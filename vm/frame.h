#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include <hash.h>
#include <list.h>


#define FREE 0
#define ALLOC 1

struct fte{
	void* paddr; //Frame Number
	void* vaddr; //Page Number
	bool state; //frame�� allocated �Ǿ� �ִ��� free���� ����
	struct thread *owner; //allocate �Ǿ��ٸ� �������� �Ǿ�����
	bool reference;	//reference bit, Swap�� ���� bit
	struct list_elem lelem;
};

struct list frame_table; //global frame table
struct semaphore frame_sema;

void init_frame(void);
struct fte *make_frame_entry(void *, void *, struct thread *);
void insert_frame(struct fte *);
void delete_frame(struct fte *);
struct fte* find_frame(void *);

void evict_frame(void);

//debuggin
void print_frame_table(void);

#endif
