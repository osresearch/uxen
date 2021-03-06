/******************************************************************************
 * arch/x86/mm/p2m-pt.c
 *
 * Implementation of p2m datastructures as pagetables, for use by 
 * NPT and shadow-pagetable code
 *
 * Parts of this code are Copyright (c) 2009-2011 by Citrix Systems, Inc.
 * Parts of this code are Copyright (c) 2007 by Advanced Micro Devices.
 * Parts of this code are Copyright (c) 2006-2007 by XenSource Inc.
 * Parts of this code are Copyright (c) 2006 by Michael A Fetterman
 * Parts based on earlier work by Michael A Fetterman, Ian Pratt et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <asm/domain.h>
#include <asm/page.h>
#include <asm/paging.h>
#include <asm/p2m.h>
#include <xen/iommu.h>
#ifndef __UXEN__
#include <asm/mem_event.h>
#include <public/mem_event.h>
#include <asm/mem_sharing.h>
#endif  /* __UXEN__ */
#include <xen/event.h>
#include <xen/trace.h>
#include <asm/hvm/nestedhvm.h>
#include <asm/hvm/svm/amd-iommu-proto.h>

#include "mm-locks.h"

/* Override macros from asm/page.h to make them work with mfn_t */
#undef mfn_to_page
#define mfn_to_page(_m) __mfn_to_page(mfn_x(_m))
#undef mfn_valid
#define mfn_valid(_mfn) __mfn_valid(mfn_x(_mfn))
#undef mfn_valid_page
#define mfn_valid_page(_mfn) __mfn_valid_page(mfn_x(_mfn))
#undef mfn_valid_page_or_vframe
#define mfn_valid_page_or_vframe(_mfn) __mfn_valid_page_or_vframe(mfn_x(_mfn))
#undef page_to_mfn
#define page_to_mfn(_pg) _mfn(__page_to_mfn(_pg))


#ifdef __UXEN__
#define PGT_l1_page_table 1
#define PGT_l2_page_table 2
#define PGT_l3_page_table 3
#endif  /* __UXEN__ */

/* PTE flags for the various types of p2m entry */
#define P2M_BASE_FLAGS \
        (_PAGE_PRESENT | _PAGE_USER | _PAGE_DIRTY | _PAGE_ACCESSED)

static unsigned long p2m_type_to_flags(p2m_type_t t, mfn_t mfn)
{
    unsigned long flags;
#ifdef __x86_64__
    /*
     * AMD IOMMU: When we share p2m table with iommu, bit 9 - bit 11 will be
     * used for iommu hardware to encode next io page level. Bit 59 - bit 62
     * are used for iommu flags, We could not use these bits to store p2m types.
     */
    flags = (unsigned long)(t & 0x7f) << 12;
#else
    flags = (t & 0x7UL) << 9;
#endif

    switch(t)
    {
    case p2m_invalid:
    case p2m_mmio_dm:
    default:
        return flags;
    case p2m_ram_ro:
#ifndef __UXEN__
    case p2m_grant_map_ro:
#endif  /* __UXEN__ */
    case p2m_ram_logdirty:
#ifndef __UXEN__
    case p2m_ram_shared:
#endif  /* __UXEN__ */
    case p2m_ram_immutable:
        return flags | P2M_BASE_FLAGS;
    case p2m_ram_rw:
#ifndef __UXEN__
    case p2m_grant_map_rw:
#endif  /* __UXEN__ */
        return flags | P2M_BASE_FLAGS | _PAGE_RW;
    case p2m_mmio_direct:
        if ( !rangeset_contains_singleton(mmio_ro_ranges, mfn_x(mfn)) )
            flags |= _PAGE_RW;
        return flags | P2M_BASE_FLAGS | _PAGE_PCD;
    case p2m_populate_on_demand:
        return flags | (mfn_valid_page(mfn) ? P2M_BASE_FLAGS : 0);
    }
}


// Find the next level's P2M entry, checking for out-of-range gfn's...
// Returns NULL on error.
//
static l1_pgentry_t *
p2m_find_entry(void *table, unsigned long *gfn_remainder,
                   unsigned long gfn, uint32_t shift, uint32_t max)
{
    u32 index;

    index = *gfn_remainder >> shift;
    if ( index >= max )
    {
        P2M_DEBUG("gfn=0x%lx out of range "
                  "(gfn_remainder=0x%lx shift=%d index=0x%x max=0x%x)\n",
                  gfn, *gfn_remainder, shift, index, max);
        return NULL;
    }
    *gfn_remainder &= (1 << shift) - 1;
    return (l1_pgentry_t *)table + index;
}

/* Free intermediate tables from a p2m sub-tree */
static void
p2m_free_entry(struct p2m_domain *p2m, l1_pgentry_t *p2m_entry, int page_order)
{
    /* End if the entry is a leaf entry. */
    if ( page_order == PAGE_ORDER_4K 
         || !(l1e_get_flags(*p2m_entry) & _PAGE_PRESENT)
         || (l1e_get_flags(*p2m_entry) & _PAGE_PSE) )
        return;

    if ( page_order > PAGE_ORDER_2M )
    {
        l1_pgentry_t *l3_table = map_domain_page(l1e_get_pfn(*p2m_entry));
        for ( int i = 0; i < L3_PAGETABLE_ENTRIES; i++ )
            p2m_free_entry(p2m, l3_table + i, page_order - 9);
        unmap_domain_page(l3_table);
    }

    p2m_free_ptp(p2m, __mfn_to_page(l1e_get_pfn(*p2m_entry)));
}

// Walk one level of the P2M table, allocating a new table if required.
// Returns 0 on error.
//

/* AMD IOMMU: Convert next level bits and r/w bits into 24 bits p2m flags */
#define iommu_nlevel_to_flags(nl, f) ((((nl) & 0x7) << 9 )|(((f) & 0x3) << 21))

static void p2m_add_iommu_flags(l1_pgentry_t *p2m_entry,
                                unsigned int nlevel, unsigned int flags)
{
#if CONFIG_PAGING_LEVELS == 4
#ifndef __UXEN__
    if ( iommu_hap_pt_share )
        l1e_add_flags(*p2m_entry, iommu_nlevel_to_flags(nlevel, flags));
#endif  /* __UXEN__ */
#endif
}

static int
npt_split_super_page(struct p2m_domain *p2m, l1_pgentry_t *p2m_entry,
                     unsigned long type)
{
    l1_pgentry_t *l1_entry;
    l1_pgentry_t new_entry;
    int i;

    switch (type) {
    case PGT_l1_page_table: {
        unsigned long flags, pfn;
        struct page_info *pg;

        pg = p2m_alloc_ptp(p2m, PGT_l1_page_table);
        if ( pg == NULL )
            return 0;

        flags = l1e_get_flags(*p2m_entry);
        if (p2m_is_pod(p2m_flags_to_type(flags))) {
            /* when populating a populate on demand superpage, don't
             * "split" the mfn value since it's in most cases 0 or some
             * pod specific value, but not an actual mfn */
#ifndef NDEBUG
            pfn = l1e_get_pfn(*p2m_entry);
            if (pfn)
                WARN_ONCE();
#else  /* NDEBUG */
            pfn = 0;
#endif /* NDEBUG */
            /* new_entry is constant */
            new_entry = l1e_from_pfn(pfn, flags);
            p2m_add_iommu_flags(&new_entry, 0, 0);
        } else {
            /* New splintered mappings inherit the flags of the old superpage,
             * with a little reorganisation for the _PAGE_PSE_PAT bit. */
            pfn = l1e_get_pfn(*p2m_entry);
            if ( pfn & 1 )           /* ==> _PAGE_PSE_PAT was set */
                pfn -= 1;            /* Clear it; _PAGE_PSE becomes _PAGE_PAT */
            else
                flags &= ~_PAGE_PSE; /* Clear _PAGE_PSE (== _PAGE_PAT) */
        }
        l1_entry = __map_domain_page(pg);
        for ( i = 0; i < L1_PAGETABLE_ENTRIES; i++ )
        {
            if (!p2m_is_pod(p2m_flags_to_type(flags))) {
                new_entry = l1e_from_pfn(pfn + i, flags);
                p2m_add_iommu_flags(&new_entry, 0, 0);
            }
            p2m->write_p2m_entry(p2m, -1, l1_entry+i, _mfn(INVALID_MFN),
                                 new_entry, 1);
        }
        unmap_domain_page(l1_entry);

        new_entry = l1e_from_pfn(__page_to_mfn(pg),
                                 __PAGE_HYPERVISOR|_PAGE_USER);
        p2m_add_iommu_flags(&new_entry, 1, IOMMUF_readable|IOMMUF_writable);
        p2m->write_p2m_entry(p2m, -1, p2m_entry, _mfn(INVALID_MFN),
                             new_entry, 2);
        if (p2m_is_pod(p2m_flags_to_type(flags))) {
            ASSERT(!is_template_domain(p2m->domain));
            atomic_dec(&p2m->domain->clone.l1_pod_pages);
            atomic_add(1 << PAGE_ORDER_2M, &p2m->domain->pod_pages);
        }
        return 1;
    }
    default:
        return 0;
    }
}

static int
p2m_next_level(struct p2m_domain *p2m, mfn_t *table_mfn, void **table,
               unsigned long *gfn_remainder, unsigned long gfn, u32 shift,
               u32 max, unsigned long type)
{
    l1_pgentry_t *l1_entry;
    l1_pgentry_t *p2m_entry;
    l1_pgentry_t new_entry;
    void *next;
    int i;

    if ( !(p2m_entry = p2m_find_entry(*table, gfn_remainder, gfn,
                                      shift, max)) )
        return 0;

    /* PoD: Not present doesn't imply empty. */
    if ( !l1e_get_flags(*p2m_entry) )
    {
        struct page_info *pg;

        pg = p2m_alloc_ptp(p2m, type);
        if ( pg == NULL )
            return 0;

        new_entry = l1e_from_pfn(__page_to_mfn(pg),
                                 __PAGE_HYPERVISOR | _PAGE_USER);

        switch ( type ) {
        case PGT_l3_page_table:
            p2m_add_iommu_flags(&new_entry, 3, IOMMUF_readable|IOMMUF_writable);
            p2m->write_p2m_entry(p2m, gfn, p2m_entry, *table_mfn, new_entry, 4);
            break;
        case PGT_l2_page_table:
#if CONFIG_PAGING_LEVELS == 3
            /* for PAE mode, PDPE only has PCD/PWT/P bits available */
            new_entry = l1e_from_pfn(__page_to_mfn(pg), _PAGE_PRESENT);
#endif
            p2m_add_iommu_flags(&new_entry, 2, IOMMUF_readable|IOMMUF_writable);
            p2m->write_p2m_entry(p2m, gfn, p2m_entry, *table_mfn, new_entry, 3);
            break;
        case PGT_l1_page_table:
            p2m_add_iommu_flags(&new_entry, 1, IOMMUF_readable|IOMMUF_writable);
            p2m->write_p2m_entry(p2m, gfn, p2m_entry, *table_mfn, new_entry, 2);
            break;
        default:
            BUG();
            break;
        }
    }

    ASSERT(l1e_get_flags(*p2m_entry) & (_PAGE_PRESENT|_PAGE_PSE));

    /* split 1GB pages into 2MB pages */
    if ( type == PGT_l2_page_table && (l1e_get_flags(*p2m_entry) & _PAGE_PSE) )
    {
        unsigned long flags, pfn;
        struct page_info *pg;

        pg = p2m_alloc_ptp(p2m, PGT_l2_page_table);
        if ( pg == NULL )
            return 0;

        flags = l1e_get_flags(*p2m_entry);
        pfn = l1e_get_pfn(*p2m_entry);

        l1_entry = map_domain_page(__page_to_mfn(pg));
        for ( i = 0; i < L2_PAGETABLE_ENTRIES; i++ )
        {
            new_entry = l1e_from_pfn(pfn + (i * L1_PAGETABLE_ENTRIES), flags);
            p2m_add_iommu_flags(&new_entry, 1, IOMMUF_readable|IOMMUF_writable);
            p2m->write_p2m_entry(p2m, gfn,
                l1_entry+i, *table_mfn, new_entry, 2);
        }
        unmap_domain_page(l1_entry);
        new_entry = l1e_from_pfn(__page_to_mfn(pg),
                                 __PAGE_HYPERVISOR|_PAGE_USER); //disable PSE
        p2m_add_iommu_flags(&new_entry, 2, IOMMUF_readable|IOMMUF_writable);
        p2m->write_p2m_entry(p2m, gfn, p2m_entry, *table_mfn, new_entry, 3);
    }


    /* split single 2MB large page into 4KB page in P2M table */
    if (type == PGT_l1_page_table && (l1e_get_flags(*p2m_entry) & _PAGE_PSE)) {
        if (!npt_split_super_page(p2m, p2m_entry, PGT_l1_page_table))
            return 0;
    }

    *table_mfn = _mfn(l1e_get_pfn(*p2m_entry));
    next = map_domain_page(mfn_x(*table_mfn));
    unmap_domain_page(*table);
    *table = next;

    return 1;
}

static int
npt_split_super_page_one(struct p2m_domain *p2m, void *entry, int order)
{
    l1_pgentry_t *l1e = (l1_pgentry_t *)entry;
    int level;

    level = order / PAGETABLE_ORDER;
    if (!level)
        return 1;
    return !npt_split_super_page(p2m, l1e, level);
}

// Returns 0 on error (out of memory)
static int
p2m_set_entry(struct p2m_domain *p2m, unsigned long gfn, mfn_t mfn, 
              unsigned int page_order, p2m_type_t p2mt, p2m_access_t p2ma)
{
    // XXX -- this might be able to be faster iff current->domain == d
    mfn_t table_mfn;
    void *table;
    unsigned long i, gfn_remainder = gfn;
    l1_pgentry_t *p2m_entry;
    l1_pgentry_t entry_content;
    l2_pgentry_t l2e_content;
    l3_pgentry_t l3e_content;
    int rv=0;
    unsigned int iommu_pte_flags = p2m_is_ram_rw(p2mt) ?
                                   IOMMUF_readable|IOMMUF_writable:
                                   0; 
#ifndef __UXEN__
    unsigned long old_mfn = 0;
#endif  /* __UXEN__ */
    struct domain *d = p2m->domain;
    union p2m_l1_cache *l1c = &this_cpu(p2m_l1_cache);

    if ( tb_init_done )
    {
        struct {
            u64 gfn, mfn;
            int p2mt;
            int d:16,order:16;
        } t;

        t.gfn = gfn;
        t.mfn = mfn_x(mfn);
        t.p2mt = p2mt;
        t.d = p2m->domain->domain_id;
        t.order = page_order;

        __trace_var(TRC_MEM_SET_P2M_ENTRY, 0, sizeof(t), &t);
    }

    if (!page_order && mfn_valid_page(l1c->se_l1_mfn) &&
        p2m_l1_prefix(gfn, p2m) == l1c->se_l1_prefix) {
        perfc_incr(p2m_set_entry_cached);
        table_mfn = l1c->se_l1_mfn;
        table = map_domain_page(mfn_x(table_mfn));
        gfn_remainder = gfn & ((1UL << PAGETABLE_ORDER) - 1);
        goto cont_l1;
    }
    l1c->se_l1_mfn = _mfn(0);

    perfc_incr(p2m_set_entry_walk);

    table_mfn = pagetable_get_mfn(p2m_get_pagetable(p2m));
    table = map_domain_page(mfn_x(table_mfn));

#if CONFIG_PAGING_LEVELS >= 4
    if ( !p2m_next_level(p2m, &table_mfn, &table, &gfn_remainder, gfn,
                         L4_PAGETABLE_SHIFT - PAGE_SHIFT,
                         L4_PAGETABLE_ENTRIES, PGT_l3_page_table) )
        goto out;
#endif
    /*
     * Try to allocate 1GB page table if this feature is supported.
     */
    if ( page_order == PAGE_ORDER_1G )
    {
        l1_pgentry_t old_entry = l1e_empty();
        p2m_entry = p2m_find_entry(table, &gfn_remainder, gfn,
                                   L3_PAGETABLE_SHIFT - PAGE_SHIFT,
                                   L3_PAGETABLE_ENTRIES);
        ASSERT(p2m_entry);

        /* Non-l1 update -- invalidate the get_entry cache */
        if (l1e_get_flags(*p2m_entry) & _PAGE_PRESENT)
            p2m_ge_l1_cache_invalidate(p2m, gfn, page_order);

        if ( (l1e_get_flags(*p2m_entry) & _PAGE_PRESENT) &&
             !(l1e_get_flags(*p2m_entry) & _PAGE_PSE) )
        {
            /* We're replacing a non-SP page with a superpage.  Make sure to
             * handle freeing the table properly. */
            old_entry = *p2m_entry;
        }

        ASSERT(!mfn_valid(mfn) || !p2m_is_mmio_direct(p2mt));
        l3e_content = (mfn_valid(mfn) || p2m_is_pod(p2mt))
            ? l3e_from_pfn(mfn_x(mfn),
                           p2m_type_to_flags(p2mt, mfn) | _PAGE_PSE)
            : l3e_empty();
        entry_content.l1 = l3e_content.l3;

        if ( entry_content.l1 != 0 )
        {
            p2m_add_iommu_flags(&entry_content, 0, iommu_pte_flags);
#ifndef __UXEN__
            old_mfn = l1e_get_pfn(*p2m_entry);
#endif  /* __UXEN__ */
        }

        p2m->write_p2m_entry(p2m, gfn, p2m_entry, table_mfn, entry_content, 3);
        /* NB: paging_write_p2m_entry() handles tlb flushes properly */

        /* Free old intermediate tables if necessary */
        if ( l1e_get_flags(old_entry) & _PAGE_PRESENT )
            p2m_free_entry(p2m, &old_entry, page_order);
    }
    /*
     * When using PAE Xen, we only allow 33 bits of pseudo-physical
     * address in translated guests (i.e. 8 GBytes).  This restriction
     * comes from wanting to map the P2M table into the 16MB RO_MPT hole
     * in Xen's address space for translated PV guests.
     * When using AMD's NPT on PAE Xen, we are restricted to 4GB.
     */
    else if ( !p2m_next_level(p2m, &table_mfn, &table, &gfn_remainder, gfn,
                              L3_PAGETABLE_SHIFT - PAGE_SHIFT,
                              ((CONFIG_PAGING_LEVELS == 3)
                               ? (hap_enabled(p2m->domain) ? 4 : 8)
                               : L3_PAGETABLE_ENTRIES),
                              PGT_l2_page_table) )
        goto out;

    if ( page_order == PAGE_ORDER_4K )
    {
        l1_pgentry_t old_entry = l1e_empty();
        if ( !p2m_next_level(p2m, &table_mfn, &table, &gfn_remainder, gfn,
                             L2_PAGETABLE_SHIFT - PAGE_SHIFT,
                             L2_PAGETABLE_ENTRIES, PGT_l1_page_table) )
            goto out;

        l1c->se_l1_prefix = p2m_l1_prefix(gfn, p2m);
        l1c->se_l1_mfn = table_mfn;

      cont_l1:
        p2m_entry = p2m_find_entry(table, &gfn_remainder, gfn,
                                   0, L1_PAGETABLE_ENTRIES);
        ASSERT(p2m_entry);
        old_entry = *p2m_entry;

        if (mfn_valid_page(mfn) ||
            p2m_is_mmio_direct(p2mt) || p2m_is_pod(p2mt))
            entry_content = l1e_from_pfn(mfn_x(mfn),
                                         p2m_type_to_flags(p2mt, mfn));
        else
            entry_content = l1e_empty();

        if ( entry_content.l1 != 0 )
            p2m_add_iommu_flags(&entry_content, 0, iommu_pte_flags);
        /* level 1 entry */
        p2m->write_p2m_entry(p2m, gfn, p2m_entry, table_mfn, entry_content, 1);
        /* NB: paging_write_p2m_entry() handles tlb flushes properly */

        if (l1e_get_pfn(old_entry) != mfn_x(mfn)) {
            if (mfn_valid_page_or_vframe(mfn) &&
                mfn_x(mfn) != mfn_x(shared_zero_page))
                get_page_fast(mfn_to_page(mfn), NULL);
            if (__mfn_valid_page_or_vframe(l1e_get_pfn(old_entry)) &&
                l1e_get_pfn(old_entry) != mfn_x(shared_zero_page)) {
                if (p2m_flags_to_type(l1e_get_flags(old_entry)) ==
                    p2m_populate_on_demand)
                    put_page_destructor(__mfn_to_page(l1e_get_pfn(old_entry)),
                                        p2m_pod_free_page, p2m->domain, gfn);
                else
                    put_page(__mfn_to_page(l1e_get_pfn(old_entry)));
            }
        }
        if (old_entry.l1 != entry_content.l1)
            p2m_update_pod_counts(
                d, l1e_get_pfn(old_entry),
                p2m_flags_to_type(l1e_get_flags(old_entry)),
                l1e_get_pfn(entry_content),
                p2m_flags_to_type(l1e_get_flags(entry_content)));
    }
    else if ( page_order == PAGE_ORDER_2M )
    {
        l1_pgentry_t old_entry = l1e_empty();
        p2m_entry = p2m_find_entry(table, &gfn_remainder, gfn,
                                   L2_PAGETABLE_SHIFT - PAGE_SHIFT,
                                   L2_PAGETABLE_ENTRIES);
        ASSERT(p2m_entry);
        
        /* Non-l1 update -- invalidate the get_entry cache */
        if (l1e_get_flags(*p2m_entry) & _PAGE_PRESENT)
            p2m_ge_l1_cache_invalidate(p2m, gfn, page_order);

        /* FIXME: Deal with 4k replaced by 2meg pages */
        if ( (l1e_get_flags(*p2m_entry) & _PAGE_PRESENT) &&
             !(l1e_get_flags(*p2m_entry) & _PAGE_PSE) )
        {
            /* We're replacing a non-SP page with a superpage.  Make sure to
             * handle freeing the table properly. */
            old_entry = *p2m_entry;
        }
        
        ASSERT(!mfn_valid(mfn) || !p2m_is_mmio_direct(p2mt));
        if (mfn_valid(mfn) || p2m_is_pod(p2mt))
            l2e_content = l2e_from_pfn(mfn_x(mfn),
                                       p2m_type_to_flags(p2mt, mfn) |
                                       _PAGE_PSE);
        else
            l2e_content = l2e_empty();
        
        entry_content.l1 = l2e_content.l2;

        if ( entry_content.l1 != 0 )
        {
            p2m_add_iommu_flags(&entry_content, 0, iommu_pte_flags);
#ifndef __UXEN__
            old_mfn = l1e_get_pfn(*p2m_entry);
#endif  /* __UXEN__ */
        }

        p2m->write_p2m_entry(p2m, gfn, p2m_entry, table_mfn, entry_content, 2);
        /* NB: paging_write_p2m_entry() handles tlb flushes properly */

        /* Free old intermediate tables if necessary */
        if ( l1e_get_flags(old_entry) & _PAGE_PRESENT )
            p2m_free_entry(p2m, &old_entry, page_order);
    }

    /* Track the highest gfn for which we have ever had a valid mapping */
    if ( mfn_x(mfn) != INVALID_MFN &&
         (gfn + (1UL << page_order) - 1 > p2m->max_mapped_pfn) )
        p2m->max_mapped_pfn = gfn + (1UL << page_order) - 1;

#ifndef __UXEN__
    if ( iommu_enabled && need_iommu(p2m->domain) )
    {
        if ( iommu_hap_pt_share )
        {
            if ( old_mfn && (old_mfn != mfn_x(mfn)) )
                amd_iommu_flush_pages(p2m->domain, gfn, page_order);
        }
        else
        {
            if (p2m_is_ram_rw(p2mt))
                for ( i = 0; i < (1UL << page_order); i++ )
                    iommu_map_page(p2m->domain, gfn+i, mfn_x(mfn)+i,
                                   IOMMUF_readable|IOMMUF_writable);
            else
                for ( int i = 0; i < (1UL << page_order); i++ )
                    iommu_unmap_page(p2m->domain, gfn+i);
        }
    }
#else  /* __UXEN__ */
    (void)i;
#endif  /* __UXEN__ */

    /* Success */
    rv = 1;

out:
    unmap_domain_page(table);
    return rv;
}


#ifndef __UXEN__
/* Read the current domain's p2m table (through the linear mapping). */
static mfn_t p2m_gfn_to_mfn_current(struct p2m_domain *p2m, 
                                    unsigned long gfn, p2m_type_t *t, 
                                    p2m_access_t *a, p2m_query_t q,
                                    unsigned int *page_order)
{
    mfn_t mfn = _mfn(0);
    p2m_type_t p2mt = p2m_mmio_dm;
    paddr_t addr = ((paddr_t)gfn) << PAGE_SHIFT;
    /* XXX This is for compatibility with the old model, where anything not 
     * XXX marked as RAM was considered to be emulated MMIO space.
     * XXX Once we start explicitly registering MMIO regions in the p2m 
     * XXX we will return p2m_invalid for unmapped gfns */

    l1_pgentry_t l1e = l1e_empty(), *p2m_entry;
    l2_pgentry_t l2e = l2e_empty();
    int ret;
#if CONFIG_PAGING_LEVELS >= 4
    l3_pgentry_t l3e = l3e_empty();
#endif

    ASSERT(gfn < (RO_MPT_VIRT_END - RO_MPT_VIRT_START) 
           / sizeof(l1_pgentry_t));

#if CONFIG_PAGING_LEVELS >= 4
    /*
     * Read & process L3
     */
    p2m_entry = (l1_pgentry_t *)
        &__linear_l2_table[l2_linear_offset(RO_MPT_VIRT_START)
                           + l3_linear_offset(addr)];
pod_retry_l3:
    ret = __copy_from_user(&l3e, p2m_entry, sizeof(l3e));

    if ( ret != 0 || !(l3e_get_flags(l3e) & _PAGE_PRESENT) )
    {
        if ((l3e_get_flags(l3e) & _PAGE_PSE) &&
            p2m_is_pod(p2m_flags_to_type(l3e_get_flags(l3e)))) {
            /* The read has succeeded, so we know that mapping exists */
            if ( q != p2m_query )
            {
                mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_1G, q,
                                              p2m_entry);
                if (!mfn_x(mfn))
                    goto pod_retry_l3;
                p2mt = p2m_invalid;
                printk("%s: Allocate 1GB failed!\n", __func__);
                goto out;
            }
            else
            {
                p2mt = p2m_populate_on_demand;
                goto out;
            }
        }
        goto pod_retry_l2;
    }

    if ( l3e_get_flags(l3e) & _PAGE_PSE )
    {
        p2mt = p2m_flags_to_type(l3e_get_flags(l3e));
        ASSERT(l3e_get_pfn(l3e) != INVALID_MFN || !p2m_is_ram(p2mt));
        if (p2m_is_valid(p2mt) )
            mfn = _mfn(l3e_get_pfn(l3e) + 
                       l2_table_offset(addr) * L1_PAGETABLE_ENTRIES + 
                       l1_table_offset(addr));
        else
            p2mt = p2m_mmio_dm;
            
        if ( page_order )
            *page_order = PAGE_ORDER_1G;
        goto out;
    }
#endif
    /*
     * Read & process L2
     */
    p2m_entry = &__linear_l1_table[l1_linear_offset(RO_MPT_VIRT_START)
                                   + l2_linear_offset(addr)];

pod_retry_l2:
    ret = __copy_from_user(&l2e,
                           p2m_entry,
                           sizeof(l2e));
    if ( ret != 0
         || !(l2e_get_flags(l2e) & _PAGE_PRESENT) )
    {
        if ((l2e_get_flags(l2e) & _PAGE_PSE) &&
            p2m_is_pod(p2m_flags_to_type(l2e_get_flags(l2e)))) {
            /* The read has succeeded, so we know that the mapping
             * exits at this point.  */
            if ( q != p2m_query )
            {
                mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_2M, q,
                                              p2m_entry);
                if (!mfn_x(mfn))
                    goto pod_retry_l2;

                /* Allocate failed. */
                p2mt = p2m_invalid;
                printk("%s: Allocate failed!\n", __func__);
                goto out;
            }
            else
            {
                p2mt = p2m_populate_on_demand;
                goto out;
            }
        }

        goto pod_retry_l1;
    }
        
    if (l2e_get_flags(l2e) & _PAGE_PSE)
    {
        p2mt = p2m_flags_to_type(l2e_get_flags(l2e));
        ASSERT(l2e_get_pfn(l2e) != INVALID_MFN || !p2m_is_ram(p2mt));

        if ( p2m_is_valid(p2mt) )
            mfn = _mfn(l2e_get_pfn(l2e) + l1_table_offset(addr));
        else
            p2mt = p2m_mmio_dm;

        if ( page_order )
            *page_order = PAGE_ORDER_2M;
        goto out;
    }

    /*
     * Read and process L1
     */

    /* Need to __copy_from_user because the p2m is sparse and this
     * part might not exist */
pod_retry_l1:
    p2m_entry = &phys_to_machine_mapping[gfn];

    ret = __copy_from_user(&l1e,
                           p2m_entry,
                           sizeof(l1e));
            
    if ( ret == 0 ) {
        p2mt = p2m_flags_to_type(l1e_get_flags(l1e));
        ASSERT(l1e_get_pfn(l1e) != INVALID_MFN || !p2m_is_ram(p2mt));

        if (p2m_is_pod(p2m_flags_to_type(l1e_get_flags(l1e)))) {
            /* The read has succeeded, so we know that the mapping
             * exits at this point.  */
            if ( q != p2m_query )
            {
                mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_4K, q,
                                              p2m_entry);
                if (!mfn_x(mfn))
                    goto pod_retry_l1;

                /* Allocate failed. */
                p2mt = p2m_invalid;
                goto out;
            }
            else
            {
                p2mt = p2m_populate_on_demand;
                goto out;
            }
        }

        if (p2m_is_valid(p2mt)
#ifndef __UXEN__
            || p2m_is_grant(p2mt)
#endif  /* __UXEN__ */
            )
            mfn = _mfn(l1e_get_pfn(l1e));
        else 
            /* XXX see above */
            p2mt = p2m_mmio_dm;
    }
    
    if ( page_order )
        *page_order = PAGE_ORDER_4K;
out:
    *t = p2mt;
    return mfn_x(mfn) ? mfn : _mfn(INVALID_MFN);
}
#endif  /* __UXEN__ */

static mfn_t
npt_get_l1_table(struct p2m_domain *p2m, unsigned long gpfn,
                 unsigned int *page_order)
{
    paddr_t addr = ((paddr_t)gpfn) << PAGE_SHIFT;
    mfn_t mfn = _mfn(INVALID_MFN);
    l2_pgentry_t *l2e;

    if (page_order)
        *page_order = PAGE_ORDER_4K;

    /* This pfn is higher than the highest the p2m map currently holds */
    if (gpfn > p2m->max_mapped_pfn)
        return mfn;

    /* XXX use linear mapping */

    mfn = pagetable_get_mfn(p2m_get_pagetable(p2m));

#if CONFIG_PAGING_LEVELS >= 4
    {
        l4_pgentry_t *l4e = map_domain_page(mfn_x(mfn));
        l4e += l4_table_offset(addr);
        if ( (l4e_get_flags(*l4e) & _PAGE_PRESENT) == 0 )
        {
            unmap_domain_page(l4e);
            if ( page_order )
                *page_order = PAGE_ORDER_1G + PAGETABLE_ORDER;
            return _mfn(INVALID_MFN);
        }
        mfn = _mfn(l4e_get_pfn(*l4e));
        unmap_domain_page(l4e);
    }
#endif
    {
        l3_pgentry_t *l3e = map_domain_page(mfn_x(mfn));
#if CONFIG_PAGING_LEVELS == 3
        /* On PAE hosts the p2m has eight l3 entries, not four (see
         * shadow_set_p2m_entry()) so we can't use l3_table_offset.
         * Instead, just count the number of l3es from zero.  It's safe
         * to do this because we already checked that the gfn is within
         * the bounds of the p2m. */
        l3e += (addr >> L3_PAGETABLE_SHIFT);
#else
        l3e += l3_table_offset(addr);
#endif
        if ( (l3e_get_flags(*l3e) & _PAGE_PRESENT) == 0 )
        {
            unmap_domain_page(l3e);
            if ( page_order )
                *page_order = PAGE_ORDER_1G;
            return _mfn(INVALID_MFN);
        }
        else if ( (l3e_get_flags(*l3e) & _PAGE_PSE) )
        {
            unmap_domain_page(l3e);
            if ( page_order )
                *page_order = PAGE_ORDER_1G;
            return _mfn(INVALID_MFN);
        }
        mfn = _mfn(l3e_get_pfn(*l3e));
        unmap_domain_page(l3e);
    }

    l2e = map_domain_page(mfn_x(mfn));
    l2e += l2_table_offset(addr);

    if ( (l2e_get_flags(*l2e) & _PAGE_PRESENT) == 0 )
    {
        unmap_domain_page(l2e);
        if ( page_order )
            *page_order = PAGE_ORDER_2M;
        return _mfn(INVALID_MFN);
    }
    else if ( (l2e_get_flags(*l2e) & _PAGE_PSE) )
    {
        unmap_domain_page(l2e);
        if ( page_order )
            *page_order = PAGE_ORDER_2M;
        return _mfn(INVALID_MFN);
    }

    mfn = _mfn(l2e_get_pfn(*l2e));
    unmap_domain_page(l2e);

    return mfn;
}

static mfn_t
npt_parse_entry(void *table, unsigned long index,
                p2m_type_t *t, p2m_access_t *a)
{
    mfn_t mfn;
    l1_pgentry_t *l1e = (l1_pgentry_t *)table + index;

    if (p2m_is_valid(p2m_flags_to_type(l1e_get_flags(*l1e)))) {
        *t = p2m_flags_to_type(l1e_get_flags(*l1e));
        /* Not implemented except with EPT */
        *a = p2m_access_rwx;
        mfn = _mfn(l1e_get_pfn(*l1e));
    } else {
        *t = p2m_mmio_dm;
        /* Not implemented except with EPT */
        *a = p2m_access_n;
        mfn = _mfn(INVALID_MFN);
    }

    return mfn;
}

static mfn_t
p2m_gfn_to_mfn(struct p2m_domain *p2m, unsigned long gfn, 
               p2m_type_t *t, p2m_access_t *a, p2m_query_t q,
               unsigned int *page_order)
{
    mfn_t mfn = _mfn(0);
    paddr_t addr = ((paddr_t)gfn) << PAGE_SHIFT;
    l2_pgentry_t *l2e;
    l1_pgentry_t *l1e;
    int ge_l1_cache_slot = ge_l1_cache_hash(gfn, p2m);
    union p2m_l1_cache *l1c = &this_cpu(p2m_l1_cache);

    ASSERT(paging_mode_translate(p2m->domain));

    /* XXX This is for compatibility with the old model, where anything not 
     * XXX marked as RAM was considered to be emulated MMIO space.
     * XXX Once we start explicitly registering MMIO regions in the p2m 
     * XXX we will return p2m_invalid for unmapped gfns */
    *t = p2m_mmio_dm;
    /* Not implemented except with EPT */
    *a = p2m_access_rwx;
    if (page_order)
        *page_order = PAGE_ORDER_4K;

    if ( gfn > p2m->max_mapped_pfn )
        /* This pfn is higher than the highest the p2m map currently holds */
        return _mfn(INVALID_MFN);

#ifndef __UXEN__
    /* Use the fast path with the linear mapping if we can */
    if ( p2m == p2m_get_hostp2m(current->domain) )
        return p2m_gfn_to_mfn_current(p2m, gfn, t, a, q, page_order);
#endif  /* __UXEN__ */

    mfn = pagetable_get_mfn(p2m_get_pagetable(p2m));

    if (mfn_valid_page(l1c->ge_l1_mfn[ge_l1_cache_slot]) &&
        p2m_l1_prefix(gfn, p2m) == l1c->ge_l1_prefix[ge_l1_cache_slot]) {
        perfc_incr(p2m_get_entry_cached);
        l1e = map_domain_page(mfn_x(l1c->ge_l1_mfn[ge_l1_cache_slot]));
        goto cont_l1;
    }

    l1c->ge_l1_mfn[ge_l1_cache_slot] = _mfn(0);

    perfc_incr(p2m_get_entry_walk);

#if CONFIG_PAGING_LEVELS >= 4
    {
        l4_pgentry_t *l4e = map_domain_page(mfn_x(mfn));
        l4e += l4_table_offset(addr);
        if ( (l4e_get_flags(*l4e) & _PAGE_PRESENT) == 0 )
        {
            unmap_domain_page(l4e);
            if (page_order)
                *page_order = PAGE_ORDER_1G + PAGETABLE_ORDER;
            return _mfn(INVALID_MFN);
        }
        mfn = _mfn(l4e_get_pfn(*l4e));
        unmap_domain_page(l4e);
    }
#endif
    {
        l3_pgentry_t *l3e = map_domain_page(mfn_x(mfn));
#if CONFIG_PAGING_LEVELS == 3
        /* On PAE hosts the p2m has eight l3 entries, not four (see
         * shadow_set_p2m_entry()) so we can't use l3_table_offset.
         * Instead, just count the number of l3es from zero.  It's safe
         * to do this because we already checked that the gfn is within
         * the bounds of the p2m. */
        l3e += (addr >> L3_PAGETABLE_SHIFT);
#else
        l3e += l3_table_offset(addr);
#endif
#ifndef __UXEN__
pod_retry_l3:
#endif  /* __UXEN__ */
        if ( (l3e_get_flags(*l3e) & _PAGE_PRESENT) == 0 )
        {
#ifndef __UXEN__
            if (p2m_is_pod(p2m_flags_to_type(l3e_get_flags(*l3e)))) {
                if ( q != p2m_query )
                {
                    mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_1G, q,
                                                  l3e);
                    if (mfn_x(mfn))
                        goto pod_retry_l3;
                }
                else
                    *t = p2m_populate_on_demand;
            }
#else  /* __UXEN__ */
            ASSERT(!p2m_is_pod(p2m_flags_to_type(l3e_get_flags(*l3e))));
#endif  /* __UXEN__ */
            unmap_domain_page(l3e);
            if (page_order)
                *page_order = PAGE_ORDER_1G;
            return _mfn(INVALID_MFN);
        }
#ifndef __UXEN__
        else if ( (l3e_get_flags(*l3e) & _PAGE_PSE) )
        {
            mfn = _mfn(l3e_get_pfn(*l3e) +
                       l2_table_offset(addr) * L1_PAGETABLE_ENTRIES +
                       l1_table_offset(addr));
            *t = p2m_flags_to_type(l3e_get_flags(*l3e));
            unmap_domain_page(l3e);

            ASSERT(mfn_valid(mfn) || !p2m_is_ram(*t));
            if (page_order)
                *page_order = PAGE_ORDER_1G;
            return (p2m_is_valid(*t)) ? mfn : _mfn(INVALID_MFN);
        }
#endif  /* __UXEN__ */

        mfn = _mfn(l3e_get_pfn(*l3e));
        unmap_domain_page(l3e);
    }

    l2e = map_domain_page(mfn_x(mfn));
    l2e += l2_table_offset(addr);

pod_retry_l2:
    if ( (l2e_get_flags(*l2e) & _PAGE_PRESENT) == 0 )
    {
        mfn = _mfn(INVALID_MFN);
        /* PoD: Try to populate a 2-meg chunk */
        if (p2m_is_pod(p2m_flags_to_type(l2e_get_flags(*l2e)))) {
            if (q == p2m_query) {
                mfn = _mfn(l2e_get_pfn(*l2e));
                *t = p2m_populate_on_demand;
                goto out_l2;
            }

            mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_2M, q, l2e);
            if (mfn_x(mfn))
                goto out_l2;
            goto pod_retry_l2;
        }

      out_l2:
        unmap_domain_page(l2e);
        if (page_order)
            *page_order = PAGE_ORDER_2M;
        return mfn_x(mfn) ? mfn : _mfn(INVALID_MFN);
    }
#ifndef __UXEN__
    else if ( (l2e_get_flags(*l2e) & _PAGE_PSE) )
    {
        mfn = _mfn(l2e_get_pfn(*l2e) + l1_table_offset(addr));
        *t = p2m_flags_to_type(l2e_get_flags(*l2e));
        unmap_domain_page(l2e);
        
        ASSERT(mfn_valid(mfn) || !p2m_is_ram(*t));
        if (page_order)
            *page_order = PAGE_ORDER_2M;
        return (p2m_is_valid(*t)) ? mfn : _mfn(INVALID_MFN);
    }
#endif  /* __UXEN__ */

    mfn = _mfn(l2e_get_pfn(*l2e));
    unmap_domain_page(l2e);

    if (!mfn_valid_page(l1c->ge_l1_mfn[ge_l1_cache_slot])) {
        l1c->ge_l1_prefix[ge_l1_cache_slot] = p2m_l1_prefix(gfn, p2m);
        l1c->ge_l1_mfn[ge_l1_cache_slot] = mfn;
    }

    l1e = map_domain_page(mfn_x(mfn));
  cont_l1:
    l1e += l1_table_offset(addr);

    mfn = _mfn(INVALID_MFN);

    if (is_p2m_zeroing_any(q)) {
        if (p2m_pod_zero_share(p2m, gfn, q, l1e))
            goto out;
        /* set t/mfn below */

    } else if (p2m_is_pod(p2m_flags_to_type(l1e_get_flags(*l1e))) &&
               q != p2m_query) {
        /* PoD: Try to populate */

        if (q == p2m_alloc_r &&
            (p2m->domain->clone_of || mfn_zero_page(l1e_get_pfn(*l1e)))) {
            *t = p2m_populate_on_demand;
            goto out;
        }

        mfn = p2m_pod_demand_populate(p2m, gfn, PAGE_ORDER_4K, q, l1e);
        if (mfn_x(mfn))
            goto out;
    }

    if (p2m_is_valid(p2m_flags_to_type(l1e_get_flags(*l1e)))) {
        *t = p2m_flags_to_type(l1e_get_flags(*l1e));
        mfn = _mfn(l1e_get_pfn(*l1e));
    }

  out:
    unmap_domain_page(l1e);
    if (page_order)
        *page_order = PAGE_ORDER_4K;
    return mfn_x(mfn) ? mfn : _mfn(INVALID_MFN);
}

/* Walk the whole p2m table, changing any entries of the old type
 * to the new type.  This is used in hardware-assisted paging to 
 * quickly enable or diable log-dirty tracking */
static void p2m_change_type_global(struct p2m_domain *p2m,
                                   p2m_type_t ot, p2m_type_t nt)
{
    unsigned long mfn, gfn, flags;
    l1_pgentry_t l1e_content;
    l1_pgentry_t *l1e;
    l2_pgentry_t *l2e;
    mfn_t l1mfn, l2mfn, l3mfn;
    unsigned long i1, i2, i3;
    l3_pgentry_t *l3e;
#if CONFIG_PAGING_LEVELS == 4
    l4_pgentry_t *l4e;
    unsigned long i4;
#endif /* CONFIG_PAGING_LEVELS == 4 */

#ifndef __UXEN__
    BUG_ON(p2m_is_grant(ot) || p2m_is_grant(nt));
#endif  /* __UXEN__ */
    BUG_ON(ot != nt && (p2m_is_mmio_direct(ot) || p2m_is_mmio_direct(nt)));

    if ( !paging_mode_translate(p2m->domain) )
        return;

    if ( pagetable_get_pfn(p2m_get_pagetable(p2m)) == 0 )
        return;

    ASSERT(p2m_locked_by_me(p2m));

#if CONFIG_PAGING_LEVELS == 4
    l4e = map_domain_page(mfn_x(pagetable_get_mfn(p2m_get_pagetable(p2m))));
#else /* CONFIG_PAGING_LEVELS == 3 */
    l3mfn = _mfn(mfn_x(pagetable_get_mfn(p2m_get_pagetable(p2m))));
    l3e = map_domain_page(mfn_x(pagetable_get_mfn(p2m_get_pagetable(p2m))));
#endif

#if CONFIG_PAGING_LEVELS >= 4
    for ( i4 = 0; i4 < L4_PAGETABLE_ENTRIES; i4++ )
    {
        if ( !(l4e_get_flags(l4e[i4]) & _PAGE_PRESENT) )
        {
            continue;
        }
        l3mfn = _mfn(l4e_get_pfn(l4e[i4]));
        l3e = map_domain_page(l4e_get_pfn(l4e[i4]));
#endif
        for ( i3 = 0;
              i3 < ((CONFIG_PAGING_LEVELS==4) ? L3_PAGETABLE_ENTRIES : 8);
              i3++ )
        {
            if ( !(l3e_get_flags(l3e[i3]) & _PAGE_PRESENT) )
            {
                continue;
            }
            if ( (l3e_get_flags(l3e[i3]) & _PAGE_PSE) )
            {
                flags = l3e_get_flags(l3e[i3]);
                if ( p2m_flags_to_type(flags) != ot )
                    continue;
                mfn = l3e_get_pfn(l3e[i3]);
#ifndef __UXEN__
                gfn = get_gpfn_from_mfn(mfn);
#else  /* __UXEN__ */
                gfn = ((i3
#if CONFIG_PAGING_LEVELS >= 4
                    + (i4 * L3_PAGETABLE_ENTRIES)
#endif
                    )
                    * L2_PAGETABLE_ENTRIES) * L1_PAGETABLE_ENTRIES;
#endif  /* __UXEN__ */
                flags = p2m_type_to_flags(nt, _mfn(mfn));
                l1e_content = l1e_from_pfn(mfn, flags | _PAGE_PSE);
                p2m->write_p2m_entry(p2m, gfn,
                                     (l1_pgentry_t *)&l3e[i3],
                                     l3mfn, l1e_content, 3);
                continue;
            }

            l2mfn = _mfn(l3e_get_pfn(l3e[i3]));
            l2e = map_domain_page(l3e_get_pfn(l3e[i3]));
            for ( i2 = 0; i2 < L2_PAGETABLE_ENTRIES; i2++ )
            {
                if ( !(l2e_get_flags(l2e[i2]) & _PAGE_PRESENT) )
                {
                    continue;
                }

                if ( (l2e_get_flags(l2e[i2]) & _PAGE_PSE) )
                {
                    flags = l2e_get_flags(l2e[i2]);
                    if ( p2m_flags_to_type(flags) != ot )
                        continue;
                    mfn = l2e_get_pfn(l2e[i2]);
                    /* Do not use get_gpfn_from_mfn because it may return 
                       SHARED_M2P_ENTRY */
                    gfn = (i2 + (i3
#if CONFIG_PAGING_LEVELS >= 4
				   + (i4 * L3_PAGETABLE_ENTRIES)
#endif
				)
                           * L2_PAGETABLE_ENTRIES) * L1_PAGETABLE_ENTRIES; 
                    flags = p2m_type_to_flags(nt, _mfn(mfn));
                    l1e_content = l1e_from_pfn(mfn, flags | _PAGE_PSE);
                    p2m->write_p2m_entry(p2m, gfn,
                                         (l1_pgentry_t *)&l2e[i2],
                                         l2mfn, l1e_content, 2);
                    continue;
                }

                l1mfn = _mfn(l2e_get_pfn(l2e[i2]));
                l1e = map_domain_page(mfn_x(l1mfn));

                for ( i1 = 0; i1 < L1_PAGETABLE_ENTRIES; i1++, gfn++ )
                {
                    flags = l1e_get_flags(l1e[i1]);
                    if ( p2m_flags_to_type(flags) != ot )
                        continue;
                    mfn = l1e_get_pfn(l1e[i1]);
                    gfn = i1 + (i2 + (i3
#if CONFIG_PAGING_LEVELS >= 4
					+ (i4 * L3_PAGETABLE_ENTRIES)
#endif
				     )
                           * L2_PAGETABLE_ENTRIES) * L1_PAGETABLE_ENTRIES; 
                    /* create a new 1le entry with the new type */
                    flags = p2m_type_to_flags(nt, _mfn(mfn));
                    l1e_content = l1e_from_pfn(mfn, flags);
                    p2m->write_p2m_entry(p2m, gfn, &l1e[i1],
                                         l1mfn, l1e_content, 1);
                }
                unmap_domain_page(l1e);
            }
            unmap_domain_page(l2e);
        }
#if CONFIG_PAGING_LEVELS >= 4
        unmap_domain_page(l3e);
    }
#endif

#if CONFIG_PAGING_LEVELS == 4
    unmap_domain_page(l4e);
#else /* CONFIG_PAGING_LEVELS == 3 */
    unmap_domain_page(l3e);
#endif

}

/* Set up the p2m function pointers for pagetable format */
void p2m_pt_init(struct p2m_domain *p2m)
{
    p2m->set_entry = p2m_set_entry;
    p2m->get_entry = p2m_gfn_to_mfn;
    p2m->get_l1_table = npt_get_l1_table;
    p2m->parse_entry = npt_parse_entry;
    p2m->change_entry_type_global = p2m_change_type_global;
    p2m->split_super_page_one = npt_split_super_page_one;
    p2m->write_p2m_entry = paging_write_p2m_entry;

    p2m->p2m_l1_cache_id = p2m->domain->domain_id;
    open_softirq(P2M_L1_CACHE_CPU_SOFTIRQ, p2m_l1_cache_flush_softirq);

    p2m->virgin = 1;
}


#if P2M_AUDIT
/* strict_m2p == 0 allows m2p mappings that don'#t match the p2m. 
 * It's intended for add_to_physmap, when the domain has just been allocated 
 * new mfns that might have stale m2p entries from previous owners */
void audit_p2m(struct p2m_domain *p2m, int strict_m2p)
{
    struct page_info *page;
    struct domain *od;
    unsigned long mfn, gfn, m2pfn, lp2mfn = 0;
    int entry_count = 0;
    mfn_t p2mfn;
    unsigned long orphans_d = 0, orphans_i = 0, mpbad = 0, pmbad = 0;
    int test_linear;
    p2m_type_t type;
    struct domain *d = p2m->domain;

    if ( !paging_mode_translate(d) )
        return;

    //P2M_PRINTK("p2m audit starts\n");

    test_linear = ( (d == current->domain)
                    && !pagetable_is_null(current->arch.monitor_table) );
    if ( test_linear )
        flush_tlb_local();

    spin_lock(&d->page_alloc_lock);

    /* Audit part one: walk the domain's page allocation list, checking
     * the m2p entries. */
    page_list_for_each ( page, &d->page_list )
    {
        mfn = __page_to_mfn(page);

        // P2M_PRINTK("auditing guest page, mfn=%#lx\n", mfn);

        od = page_get_owner(page);

        if ( od != d )
        {
            P2M_PRINTK("wrong owner %#lx -> %p(vm%d) != %p(vm%u)\n",
                       mfn, od, (od?od->domain_id:-1), d, d->domain_id);
            continue;
        }

        gfn = get_gpfn_from_mfn(mfn);
        if ( gfn == INVALID_M2P_ENTRY )
        {
            orphans_i++;
            //P2M_PRINTK("orphaned guest page: mfn=%#lx has invalid gfn\n",
            //               mfn);
            continue;
        }

        if ( gfn == 0x55555555 || gfn == 0x5555555555555555 )
        {
            orphans_d++;
            //P2M_PRINTK("orphaned guest page: mfn=%#lx has debug gfn\n",
            //               mfn);
            continue;
        }

        if ( gfn == SHARED_M2P_ENTRY )
        {
            P2M_PRINTK("shared mfn (%lx) on domain page list!\n",
                    mfn);
            continue;
        }

        p2mfn = gfn_to_mfn_type_p2m(p2m, gfn, &type, p2m_query);
        if ( strict_m2p && mfn_x(p2mfn) != mfn )
        {
            mpbad++;
            P2M_PRINTK("map mismatch mfn %#lx -> gfn %#lx -> mfn %#lx"
                       " (-> gfn %#lx)\n",
                       mfn, gfn, mfn_x(p2mfn),
                       (mfn_valid(p2mfn)
                        ? get_gpfn_from_mfn(mfn_x(p2mfn))
                        : -1u));
            /* This m2p entry is stale: the domain has another frame in
             * this physical slot.  No great disaster, but for neatness,
             * blow away the m2p entry. */
            set_gpfn_from_mfn(mfn, INVALID_M2P_ENTRY);
        }

        if ( test_linear && (gfn <= p2m->max_mapped_pfn) )
        {
            lp2mfn = mfn_x(gfn_to_mfn_type_p2m(p2m, gfn, &type, p2m_query));
            if ( lp2mfn != mfn_x(p2mfn) )
            {
                P2M_PRINTK("linear mismatch gfn %#lx -> mfn %#lx "
                           "(!= mfn %#lx)\n", gfn, lp2mfn, mfn_x(p2mfn));
            }
        }

        // P2M_PRINTK("OK: mfn=%#lx, gfn=%#lx, p2mfn=%#lx, lp2mfn=%#lx\n",
        //                mfn, gfn, mfn_x(p2mfn), lp2mfn);
    }

    spin_unlock(&d->page_alloc_lock);

    /* Audit part two: walk the domain's p2m table, checking the entries. */
    if ( pagetable_get_pfn(p2m_get_pagetable(p2m)) != 0 )
    {
        l2_pgentry_t *l2e;
        l1_pgentry_t *l1e;
        int i1, i2;

#if CONFIG_PAGING_LEVELS == 4
        l4_pgentry_t *l4e;
        l3_pgentry_t *l3e;
        int i4, i3;
        l4e = map_domain_page(mfn_x(pagetable_get_mfn(p2m_get_pagetable(p2m))));
#else /* CONFIG_PAGING_LEVELS == 3 */
        l3_pgentry_t *l3e;
        int i3;
        l3e = map_domain_page(mfn_x(pagetable_get_mfn(p2m_get_pagetable(p2m))));
#endif

        gfn = 0;
#if CONFIG_PAGING_LEVELS >= 4
        for ( i4 = 0; i4 < L4_PAGETABLE_ENTRIES; i4++ )
        {
            if ( !(l4e_get_flags(l4e[i4]) & _PAGE_PRESENT) )
            {
                gfn += 1 << (L4_PAGETABLE_SHIFT - PAGE_SHIFT);
                continue;
            }
            l3e = map_domain_page(mfn_x(_mfn(l4e_get_pfn(l4e[i4]))));
#endif
            for ( i3 = 0;
                  i3 < ((CONFIG_PAGING_LEVELS==4) ? L3_PAGETABLE_ENTRIES : 8);
                  i3++ )
            {
                if ( !(l3e_get_flags(l3e[i3]) & _PAGE_PRESENT) )
                {
                    gfn += 1 << (L3_PAGETABLE_SHIFT - PAGE_SHIFT);
                    continue;
                }

                /* check for 1GB super page */
                if ( l3e_get_flags(l3e[i3]) & _PAGE_PSE )
                {
                    mfn = l3e_get_pfn(l3e[i3]);
                    ASSERT(__mfn_valid(mfn));
                    /* we have to cover 512x512 4K pages */
                    for ( i2 = 0; 
                          i2 < (L2_PAGETABLE_ENTRIES * L1_PAGETABLE_ENTRIES);
                          i2++)
                    {
                        m2pfn = get_gpfn_from_mfn(mfn+i2);
                        if ( m2pfn != (gfn + i2) )
                        {
                            pmbad++;
                            P2M_PRINTK("mismatch: gfn %#lx -> mfn %#lx"
                                       " -> gfn %#lx\n", gfn+i2, mfn+i2,
                                       m2pfn);
                            BUG();
                        }
                        gfn += 1 << (L3_PAGETABLE_SHIFT - PAGE_SHIFT);
                        continue;
                    }
                }

                l2e = map_domain_page(mfn_x(_mfn(l3e_get_pfn(l3e[i3]))));
                for ( i2 = 0; i2 < L2_PAGETABLE_ENTRIES; i2++ )
                {
                    if ( !(l2e_get_flags(l2e[i2]) & _PAGE_PRESENT) )
                    {
                        if ((l2e_get_flags(l2e[i2]) & _PAGE_PSE) &&
                            p2m_is_pod(
                                p2m_flags_to_type(l2e_get_flags(l2e[i2]))))
                            entry_count+=SUPERPAGE_PAGES;
                        gfn += 1 << (L2_PAGETABLE_SHIFT - PAGE_SHIFT);
                        continue;
                    }
                    
                    /* check for super page */
                    if ( l2e_get_flags(l2e[i2]) & _PAGE_PSE )
                    {
                        mfn = l2e_get_pfn(l2e[i2]);
                        ASSERT(__mfn_valid(mfn));
                        for ( i1 = 0; i1 < L1_PAGETABLE_ENTRIES; i1++)
                        {
                            m2pfn = get_gpfn_from_mfn(mfn+i1);
                            /* Allow shared M2Ps */
                            if ( (m2pfn != (gfn + i1)) &&
                                 (m2pfn != SHARED_M2P_ENTRY) )
                            {
                                pmbad++;
                                P2M_PRINTK("mismatch: gfn %#lx -> mfn %#lx"
                                           " -> gfn %#lx\n", gfn+i1, mfn+i1,
                                           m2pfn);
                                BUG();
                            }
                        }
                        gfn += 1 << (L2_PAGETABLE_SHIFT - PAGE_SHIFT);
                        continue;
                    }

                    l1e = map_domain_page(mfn_x(_mfn(l2e_get_pfn(l2e[i2]))));

                    for ( i1 = 0; i1 < L1_PAGETABLE_ENTRIES; i1++, gfn++ )
                    {
                        p2m_type_t type;

                        type = p2m_flags_to_type(l1e_get_flags(l1e[i1]));
                        if ( !(l1e_get_flags(l1e[i1]) & _PAGE_PRESENT) )
                        {
                            if (p2m_is_pod(type))
                                entry_count++;
                            continue;
                        }
                        mfn = l1e_get_pfn(l1e[i1]);
                        ASSERT(__mfn_valid(mfn));
                        m2pfn = get_gpfn_from_mfn(mfn);
                        if ( m2pfn != gfn &&
                             !p2m_is_mmio_direct(type)
#ifndef __UXEN__
                             &&
                             !p2m_is_grant(type) &&
                             !p2m_is_shared(type)
#endif  /* __UXEN__ */
                            )
                        {
                            pmbad++;
                            printk("mismatch: gfn %#lx -> mfn %#lx"
                                   " -> gfn %#lx\n", gfn, mfn, m2pfn);
                            P2M_PRINTK("mismatch: gfn %#lx -> mfn %#lx"
                                       " -> gfn %#lx\n", gfn, mfn, m2pfn);
                            BUG();
                        }
                    }
                    unmap_domain_page(l1e);
                }
                unmap_domain_page(l2e);
            }
#if CONFIG_PAGING_LEVELS >= 4
            unmap_domain_page(l3e);
        }
#endif

#if CONFIG_PAGING_LEVELS == 4
        unmap_domain_page(l4e);
#else /* CONFIG_PAGING_LEVELS == 3 */
        unmap_domain_page(l3e);
#endif

    }

    if ( entry_count != atomic_read(&d->pod_pages) )
    {
        printk("%s: refcounted entry count %d, audit count %d!\n",
               __func__,
               atomic_read(&d->pod_pages),
               entry_count);
        BUG();
    }
        
    //P2M_PRINTK("p2m audit complete\n");
    //if ( orphans_i | orphans_d | mpbad | pmbad )
    //    P2M_PRINTK("p2m audit found %lu orphans (%lu inval %lu debug)\n",
    //                   orphans_i + orphans_d, orphans_i, orphans_d);
    if ( mpbad | pmbad )
    {
        P2M_PRINTK("p2m audit found %lu odd p2m, %lu bad m2p entries\n",
                   pmbad, mpbad);
        WARN();
    }
}
#endif /* P2M_AUDIT */

