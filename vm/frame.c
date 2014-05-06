#include "frame.h"
#include "threads/malloc.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "userprog/pagedir.h"

#include <stdio.h>

struct list_elem *victim_cur;

void init_frame(){
	list_init(&frame_table);
	sema_init(&frame_sema, 1);
	victim_cur = NULL;
}

void insert_frame(struct fte * fte){
	sema_down(&frame_sema);
	list_push_back(&frame_table, &fte->lelem);
	sema_up(&frame_sema);
}


void delete_frame(struct fte *fte){
	palloc_free_page(fte->paddr);
	sema_down(&frame_sema);
	list_remove(&fte->lelem);
	sema_up(&frame_sema);
	free(fte);
}

struct fte *find_frame(void *paddr){
	struct list_elem *e;
	struct list *flist =  &frame_table;
	struct fte *fte;
	for (e = list_begin(flist); e != list_end (flist); e = list_next (e)){
		fte = list_entry(e, struct fte, lelem);
		if(fte->paddr == paddr){
			return fte;
		}
	}
	return NULL;
}

struct fte *make_frame_entry(void *vaddr, void *paddr, struct thread* t){
	struct fte* newfte = (struct fte *)malloc(sizeof(struct fte));
	newfte->paddr = paddr;
	newfte->vaddr = vaddr;
	newfte->owner = t;
	newfte->state = ALLOC;
	newfte->reference = 0;
	return newfte;
}
void evict_frame(){
	struct list *flist = &frame_table;
	struct fte *fte = NULL;
	if(true){
		victim_cur = list_begin(flist);
		fte = list_entry(victim_cur, struct fte, lelem);
//		printf("Victim : %x <--- %x(%x)\n",  (unsigned int)fte->vaddr, (unsigned int)fte->paddr, (unsigned int)fte->reference);
	}
	struct pte *pte = NULL;
	pte = find_page(fte->owner->page_table, fte->vaddr);

	pte->disk_ind = swap_out(fte->paddr);
	pte->loc = SWP;

	//page table에서 entry 삭제
	free_page(fte->owner->pagedir, fte->vaddr);

	//frame table에서 entry 삭제
	delete_frame(fte);
	
}

//debugging
void print_frame_table(){
	struct list_elem *e;
	struct list *flist =  &frame_table;
	struct fte *fte;
	for (e = list_begin(flist); e != list_end (flist); e = list_next (e)){
		fte = list_entry(e, struct fte, lelem);
		printf("Vaddr :  %x <--- Paddr : %x(%x)\n", (unsigned int)fte->vaddr, (unsigned int)fte->paddr, (unsigned int)fte->reference);
	}
}
