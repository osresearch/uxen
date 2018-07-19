/*
 *  memcache-dm.h
 *  uxen
 *
 *  COPYRIGHT
 *
 */

#ifndef _MEMCACHE_DM_H_
#define _MEMCACHE_DM_H_

uint64_t
mdm_enter(struct domain *d, xen_pfn_t pfn, xen_pfn_t mfn);
int
mdm_clear(struct domain *d, xen_pfn_t pfn, int force);
int
_mdm_init_vm(struct domain *d);
#define mdm_init_vm(d)                                  \
    (likely((d)->mdm_map_pfns) ? 0 : _mdm_init_vm(d))
int
mdm_clear_vm(struct domain *d);
void
mdm_destroy_vm(struct domain *d);

#endif /* _MEMCACHE_DM_H_ */
