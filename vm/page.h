#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define PAGE_SIZE 4096

#define SWP 0
#define MEM 1
#define ALZ 2
#define NOZ 3

//page table entry ����ü
struct pte{
	void *vaddr;
	void *paddr;
	size_t disk_ind;
	int loc;
	bool writable;
	struct hash_elem helem;
	//below variable for PJ3-2
	unsigned int ofs;
	struct file *file;
	int file_size;
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
