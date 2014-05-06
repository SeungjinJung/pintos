#include "swap.h"
#include <bitmap.h>
#include "devices/disk.h"
#include "vm/page.h"

static struct bitmap *swap_table;
static struct disk* swap_disk;
static struct semaphore swap_sema;

void init_swap(){
	swap_disk = disk_get(1, 1);
	sema_init(&swap_sema, 1);
	swap_table = bitmap_create(disk_size(swap_disk)/SECTORS_PER_PAGE);
}
size_t swap_out(void *kaddr){
	sema_down(&swap_sema);
	size_t idx = bitmap_scan_and_flip (swap_table, 0, 1, false);
	int i;
	for(i=0; i<SECTORS_PER_PAGE; i++){
		disk_write(swap_disk, idx*SECTORS_PER_PAGE+i, kaddr+DISK_SECTOR_SIZE*i);
	}
	sema_up(&swap_sema);
	return idx;
}
void swap_in(size_t idx, void* kaddr){

	int i;
	sema_down(&swap_sema);
	for(i=0; i<SECTORS_PER_PAGE; i++){
		disk_read(swap_disk, idx*SECTORS_PER_PAGE+i, kaddr+DISK_SECTOR_SIZE*i);
	}
	bitmap_set (swap_table, idx, false);
	sema_up(&swap_sema);
}

void swap_free(size_t idx)
{
	sema_down(&swap_sema);
	bitmap_set (swap_table, idx, false);
	sema_up(&swap_sema);
}
