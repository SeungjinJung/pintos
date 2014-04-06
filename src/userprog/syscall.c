#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "threads/vaddr.h"

#include "threads/malloc.h"
#include "threads/synch.h"
#include <string.h>
#include <list.h>
#define TOOLARGETOCONSOLE 524288

static void sysexit(int);
static bool goodfileptr(void *);
static bool goodfd(int);
static int makeP_C(tid_t);

static void syscall_handler (struct intr_frame *);

struct semaphore filesys[MAXFD];
	void
syscall_init (void) 
{
	sema_init(&filesys[0],1);
	sema_init(&filesys[1],1);
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int
getaddr(void *ptr){
	return (*(int *)ptr);
}

	static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int syscallnum = *((int *)f->esp);
	// printf ("system call!\n");
	// printf ("system call num(%d)!\n", syscallnum);

	if(syscallnum == SYS_HALT){
		//this systemcall is about halt
		power_off();
	}
	else if(syscallnum == SYS_EXIT){
		//this systemcall is about exit
		int status = getaddr(f->esp+0x4);
		if(status < 0)
			status = -1;
		f->eax = status;
		//	printf("status : %x\n", status);
		sysexit(status);
	}
	else if(syscallnum == SYS_EXEC){
		//this systemcall is about exec
		const char *cmd_line = (const char *)getaddr(f->esp+0x04);
		tid_t pid = process_execute(cmd_line);
		//		int status = process_wait(pid);
		//		if(pid < 0){//it means error occurs
		//			f->eax = -1;
		//		}
		//		else
		//			printf("exit code : %d\n", thread_current()->child_exit_status);
		//make parent child relationship
		f->eax = makeP_C(pid);
	}
	else if(syscallnum == SYS_WAIT){
		//this systemcall is about wait
		tid_t pid = (tid_t)getaddr(f->esp+0x04);
		struct thread *pthread = findthread(&execute_list, pid);
		if(pthread == NULL){
			//there is no pid, that is double wait
			f->eax = -1;
			return;
		}
		else if(pthread->parent_id != thread_current()){
			//this is not my child, so no duty for wait
			f->eax=-1;
			return;
		}

		//		printf("syswait called %d,%d\n", thread_current()->tid, pid);
		//struct thread *cur = thread_current();
		f->eax = process_wait(pid); 
	}
	else if(syscallnum == SYS_CREATE){
		//this systemcall is about create
		const char *file = (const char *)getaddr(f->esp+0x04);
		unsigned initial_size = (unsigned)getaddr(f->esp+0x08);
		if(!goodfileptr((void *)file)){
			sysexit(-1);
			return;
		}
		if(file != NULL)
			f->eax = filesys_create(file, initial_size);
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
			return;
		}
	}
	else if(syscallnum == SYS_REMOVE){
		//this systemcall is about remove
		const char *file = (const char *)getaddr(f->esp+0x04);
		if(!goodfileptr((void *)file)){
			sysexit(-1);
			return;
		}
		if(file != NULL){
			f->eax = filesys_remove(file);
		}
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
			return;
		}
	}
	else if(syscallnum == SYS_OPEN){
		const char *file = (const char *)getaddr(f->esp+0x04);
		struct file *ofile = NULL;
		int i;
		struct thread *cur = thread_current();
		if(!goodfileptr((void *)file)){	
			sysexit(-1);
			return;
		}
		if(file != NULL)
			ofile = filesys_open(file);
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
			return;
		}
		if(ofile == NULL){ //if open is failed
			f->eax = -1;
			return;
		}
		//setting the file discriptor
		for(i=2; i<MAXFD; i++){
			if(cur->fd_set[i] == 0){
				f->eax = i;
				//save address of ofile to fd_set[i]
				cur->fd_set[i] = (void *)vtop(ofile);
				break;
			}
		}
		sema_init(&filesys[f->eax], 1);
	}
	else if(syscallnum == SYS_FILESIZE){
		//this systemcall is about filesize
		int fd = getaddr(f->esp+0x4);
		struct file* ofile;
		struct thread *cur = thread_current();
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}
		if(!goodfileptr(cur->fd_set[fd])){
			sysexit(-1);
			return;
		}
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);
		if(ofile != NULL)
			f->eax = file_length(ofile);
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
			return;
		}
	}
	else if(syscallnum == SYS_READ){
		//this systemcall is about read
		int fd = getaddr(f->esp+0x4);
		void * buf = (void *)getaddr(f->esp+0x8);
		unsigned size = (unsigned)getaddr(f->esp+0xc);
		struct file* ofile;
		struct thread *cur = thread_current();
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}

		if(!goodfileptr(cur->fd_set[fd]) || !goodfileptr(buf)){
			sysexit(-1);
			return;
		}
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);
		if(fd == 1)
			sysexit(-1);
		sema_down(&filesys[fd]);
		if(fd == 0){
			//it is read from console
			unsigned int i;
			for(i=0; i<size; i++)
				((char *)buf)[i] = input_getc();
			f->eax=size;
		}
		else{
			//it is read from file_fd
			if(ofile != NULL && buf != NULL)
				f->eax = file_read(ofile, buf, (off_t)size);
			else{
				//if arguments are not ok then exit(-1);
				sema_up(&filesys[fd]);
				sysexit(-1);
			}
		}
		sema_up(&filesys[fd]);
	}
	else if(syscallnum ==  SYS_WRITE){
		//this systemcall is about write
		int fd = getaddr(f->esp+0x4);
		const void * buf = (const void *)getaddr(f->esp+0x8);
		unsigned size = (unsigned)getaddr(f->esp+0xc);
		struct file* ofile;
		struct thread *cur = thread_current();
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}

		if(!goodfileptr(cur->fd_set[fd]) || !goodfileptr((void *)buf)){
			sysexit(-1);
			return;
		}
		if(fd == 0)
			sysexit(-1);
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);
		sema_down(&filesys[fd]);
		if(fd == 1){
			//it is write for console
			if(size < TOOLARGETOCONSOLE){
				putbuf(buf, size);
			}
			f->eax = size;
		}	
		else{
			//it is read from file_fd
//			file_deny_write(ofile);
			if(ofile != NULL && buf != NULL){
				f->eax = file_write(ofile, buf, (off_t)size);
			}
			else{
				//if arguments are not ok then exit(-1);
				f->eax=-1;
				sema_up(&filesys[fd]);
				sysexit(-1);
				return;
			}
		}
		sema_up(&filesys[fd]);
	}
	else if(syscallnum == SYS_SEEK){
		int fd = (int)getaddr(f->esp+0x4);
		unsigned position = (unsigned) getaddr(f->esp+0x8);
		struct thread *cur = thread_current();
		struct file *ofile;
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);
//		printf("addr %x\n", ofile);
		if(ofile != NULL){
//			printf("%x : seeking : %d\n", ofile, position);
			file_seek(ofile, position);
		}
		else{
			sysexit(-1);
			return;
		}
	}
	else if(syscallnum == SYS_TELL){
		int fd = (int)getaddr(f->esp+0x4);
		struct thread *cur = thread_current();
		struct file *ofile;
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}
		if(!goodfileptr(cur->fd_set[fd])){
			sysexit(-1);
			return;
		}
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);
		if(ofile != NULL)
			f->eax = file_tell(ofile);
		else{
			f->eax=-1;
			sysexit(-1);
			return;
		}
	}
	else if(syscallnum == SYS_CLOSE){
		//this systemcall is about close
		int fd = getaddr(f->esp+0x4);
		struct file* ofile;
		struct thread *cur = thread_current();
		if(!goodfd(fd)){
			sysexit(-1);
			return;
		}
		if(!goodfileptr(cur->fd_set[fd])){
			sysexit(-1);
			return;
		}

		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);

		if(ofile != NULL)
			file_close(ofile);
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
			return;
		}
		//remove the fd in fd_set
		cur->fd_set[fd] = 0;
	}

	else{//if the syscallnum is out of contol
		//bad sp
		if(syscallnum < 13)
			printf("Error : Now Implemented for Pj2 yet\n");
		thread_current()->child_exit_status = -1;
		thread_exit ();
		return;
	}
	//thread_exit ();
}


void sysexit(int status){
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_current()->child_exit_status = status;
	thread_exit ();
}

bool goodfd(int fd){
	if(fd < 0 || fd > MAXFD)
		return false;
	return true;
}



bool goodfileptr(void *fileptr){
	struct thread *cur = thread_current();
	//		printf("ptr : %x\n", fileptr);
	if(fileptr == NULL)
		return false;
	if(is_user_vaddr(fileptr) && fileptr >= (void *)0x08048000){
		//ASSERT(false);
		if(pagedir_get_page(cur->pagedir, fileptr) == NULL){
			//invalid file poiner
			return false;
		}
		return true;
	}
	else if(fileptr < (void *)0x08048000)
		return true;
	else
		return false;
}
int makeP_C(tid_t t){
	struct thread *cur = thread_current();
	struct list *elist = &execute_list;
	struct list_elem *e;
	struct thread *ex;
	if(!list_empty(elist)){
		for(e = list_begin(elist); e!=list_end(elist); e=list_next(e)){
			ex = list_entry(e, struct thread, e_elem);
			if(ex->tid == t && ex->child_exit_status>=0){
				ex->parent_id = cur; 
				return t;
			}
		}
	}
	return -1;
}
