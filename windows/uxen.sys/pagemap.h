/*
 *  pagemap.h
 *  uxen
 *
 *  COPYRIGHT
 *
 */

#ifndef _PAGEMAP_H_
#define _PAGEMAP_H_

int pagemap_init(int max_pfn);
int pagemap_free(void);
void *pagemap_map_page(xen_pfn_t mfn);
uint64_t pagemap_unmap_page_va(const void *va);
void pagemap_check_space(void);

#endif  /* _PAGEMAP_H_ */
