#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"

#define PAGE_SIZE 4096

#define SWP 10
#define MEM 11

//page table entry ±¸Á¶Ã¼
struct pte{
	void *vaddr;
	void *paddr;
	size_t disk_ind;
	int loc;
	bool writable;
	struct hash_elem helem;
};

//page table
struct pt{
	struct hash page_table;
	struct semaphore pt_sema;
};

struct pt* init_page_table(void);
void destroy_page_table(struct pt*);


struct pte *make_page_entry(void *, void *);

void insert_page(struct pt *, struct pte *);
bool delete_page(struct pt *, struct pte *);
struct pte *find_page(struct pt *, void *);

void *get_page(enum palloc_flags);
void free_page(void *, void *);

//this function for debugging
void print_page_table(struct pt*);

#endif
