#include "page.h"
#include "threads/malloc.h"
#include "vm/frame.h"

#include <stdio.h>

//page table관리를 위한 hash_function
static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
	struct pte *pte = hash_entry(e, struct pte, helem);
	return hash_bytes (&pte->vaddr, sizeof (pte->vaddr));
}

//page table 정렬을 위한 비교함수
static bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
		struct pte *pte1 = hash_entry(a, struct pte, helem);
		struct pte *pte2 = hash_entry(b, struct pte, helem);
		return pte1->vaddr < pte2->vaddr;
}

/* 
 * uaddr과 kaddr을 사용하여서 새로운 page table entry를 만들어서 리턴
*/
struct pte* make_page_entry(void *uaddr, void *kaddr){
	struct pte* newpte = (struct pte *)malloc(sizeof(struct pte));
	newpte->vaddr = uaddr;
	newpte->paddr = kaddr;
	return newpte;
}

/* 
 * 페이지 테이블 초기화
*/

struct pt* init_page_table(){
	struct pt* new_pt = (struct pt *)malloc(sizeof(struct pt));
	hash_init(&new_pt->page_table, page_hash, page_less, NULL);
	sema_init(&new_pt->pt_sema, 1);
	return new_pt;
}

bool delete_page(struct pt *pt, struct pte* pte){
	if(hash_delete(&pt->page_table, &pte->helem) != NULL){
		free(pte);
		return true;
	}
	return false;
}


/* 페이지 테이블 Entry 삭제 */
static void pte_destroy (struct hash_elem *e, void *aux UNUSED)
{
	free (hash_entry(e, struct pte, helem));
}

static void pte_clean (struct hash_elem *e, void *aux UNUSED)
{
	struct thread * cur = thread_current();
	struct pte *pte;
	pte = hash_entry (e, struct pte, helem);
	if(pte->loc == MEM){
		struct fte *fte = NULL;
		void *pagedir_frame = pagedir_get_page(cur->pagedir, pte->vaddr);
		//메모리 상에 있는 page일 경우
		fte = find_frame(pagedir_frame);
		if(fte){
			//페이지 디렉토리에서 삭제
			pagedir_clear_page(cur->pagedir, fte->vaddr);
			//frame 제거
			delete_frame(fte);
		}
	}
	else if(pte->loc == SWP){
		//SWAP disk안에 있는경우
		swap_free(pte->disk_ind);
	}
}

void destroy_page_table(struct pt* pt){
	if (pt == NULL)
		return;
	//먼저 frame과 페이지 디렉토리 free 시킴
	hash_apply(&pt->page_table, pte_clean);
	//그리고 page의 각 entry를 free시킴
	hash_destroy(&pt->page_table, pte_destroy);
	//마지막으로 page_table 자체를 free시킴
	free(pt);
}

/* page table에 Entry추가 */
void insert_page(struct pt *pt, struct pte *pte){
//	sema_down(&pt->pt_sema);
	hash_insert(&pt->page_table, &pte->helem);
//	sema_up(&pt->pt_sema);
}

/* find pte from vaddr */
struct pte *find_page(struct pt *pt, void *vaddr){
	struct pte for_find;
	struct pte *pte = NULL;
	for_find.vaddr = vaddr;
	struct hash_elem *temp = hash_find(&pt->page_table, &for_find.helem);
	if(temp)
		pte = hash_entry(temp, struct pte, helem);
	return pte;
}

/* page를 하나 할당받음
 * 만약 할당받을수 없을경우 evict를 한다음 할당
 */
void *get_page(enum palloc_flags flag){
	void *page = palloc_get_page(flag);
	//if now page allcated, then evict some frame
	while(page == NULL){
		evict_frame();
		page = palloc_get_page(flag);
	}
	return page;
}

/* page를 free시킴 */
void free_page(void * pd, void *uaddr){
	pagedir_clear_page (pd, uaddr);
//	palloc_free_page(kaddr);
}

//page table entry를 모두 출력
// Just For Debugging
void print_page_table(struct pt* pt){
	struct hash_iterator i;
	hash_first (&i, &pt->page_table);
	while(hash_next (&i)) {
		struct pte *pte = hash_entry (hash_cur(&i), struct pte, helem);
		printf("Vaddr :  %x ---> Paddr : %x\n", (unsigned int)pte->vaddr, (unsigned int)pte->paddr);
	}
}
