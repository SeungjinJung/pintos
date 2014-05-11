#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);

#include <list.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
//mmap structure
struct mmap_elem{
	int map_pid;
	int mfd;
	struct file* mfile;
	void * mstart;
	unsigned int msize;
	struct list_elem lelem;
};

void sys_unmmap(struct thread*, struct mmap_elem *); 
#endif /* userprog/syscall.h */
