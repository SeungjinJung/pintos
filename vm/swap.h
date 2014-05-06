#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/page.h"

#define SECTORS_PER_PAGE (PAGE_SIZE/DISK_SECTOR_SIZE)

void init_swap(void);
size_t swap_out(void *);
void swap_in(size_t, void *);
void swap_free(size_t);
#endif
