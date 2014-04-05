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

#define TOOLARGETOCONSOLE 256

static void sysexit(int);
static bool goodfileptr(void *);
static bool goodfd(int);
struct excute_elem{
		tid_t pid;
		struct semaphore wait;
		struct list_elem e_elem;
};
struct list execute_list;
int execute_num=0;

static void syscall_handler (struct intr_frame *);

	void
syscall_init (void) 
{
	list_init(&execute_list);
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
	//  printf ("system call!\n");
	//  printf ("system call num(%d)!\n", syscallnum);

	if(syscallnum == SYS_HALT){
		//this systemcall is about halt
		power_off();
	}
	else if(syscallnum == SYS_EXIT){
			//this systemcall is about exit
			int status = getaddr(f->esp+0x4);
			struct thread *cur = thread_current();
			struct list *elist = &execute_list;
			struct list_elem *e;
			struct excute_elem *ex=NULL;
			if(list_size(elist) != 0){
					for(e = list_begin(elist); e!=list_end(elist); e=list_next(e)){
							ex = list_entry(e, struct excute_elem, e_elem);
							if(ex->pid == cur->tid)
									break;
					}
					sema_up(&ex->wait);
//					free(ex);
			}
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
		struct excute_elem *e = (struct excute_elem *)malloc(sizeof(struct excute_elem));
		memset(e, 0, sizeof(struct excute_elem));
		e->pid = pid;
		sema_init(&e->wait, 1);
//		if(execute_num == 0){
//				//excute first then make list for excuting process
//				execute_list = (struct list *)malloc(sizeof(struct list));
//				memset(excute_list, 0, sizeof(struct list));
//				list_init(execute_list);
//		}
		list_push_back(&execute_list, &e->e_elem);
		sema_down(&e->wait);
		f->eax = pid;
	}
	else if(syscallnum == SYS_WAIT){
		//this systemcall is about wait
		tid_t pid = (tid_t)getaddr(f->esp+0x04);
		struct thread *cur = thread_current();
		struct list *elist = &execute_list;
		struct list_elem *e;
		struct excute_elem *ex=NULL;
		if(list_size(elist) != 0){
				for(e = list_begin(elist); e!=list_end(elist); e=list_next(e)){
						ex = list_entry(e, struct excute_elem, e_elem);
						if(ex->pid == pid)
								break;
				}
				sema_down(&ex->wait);
//				free(ex);
		}
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
		}
	}
	else if(syscallnum == SYS_OPEN){
		const char *file = (const char *)getaddr(f->esp+0x04);
		struct file *ofile = NULL;
		int i;
		struct thread *cur = thread_current();
		printf("FILE OPEN IS CALLED\n");
		if(!goodfileptr((void *)file)){	
			sysexit(-1);
			return;
		}
		if(file != NULL)
			ofile = filesys_open(file);
		else{
			//if arguments are not ok then exit(-1);
			sysexit(-1);
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
				sysexit(-1);
			}
		}
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
		//find file pointer
		ofile = (struct file*)ptov((uintptr_t)cur->fd_set[fd]);

		if(fd == 1){
			//it is write for console
			if(size < TOOLARGETOCONSOLE){
				putbuf(buf, size);
			}
			f->eax = size;
		}	
		else{
			//it is read from file_fd
			if(ofile != NULL && buf != NULL)
				f->eax = file_write(ofile, buf, (off_t)size);
			else{
				//if arguments are not ok then exit(-1);
				sysexit(-1);
			}
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
		}
		//remove the fd in fd_set
		cur->fd_set[fd] = 0;
	}
	else{//if the syscallnum is out of contol
		//*((int *)f->esp) = -1;
		thread_exit ();
		return;
	}
	//thread_exit ();
}


void sysexit(int status){
	printf("%s: exit(%d)\n", thread_name(), status);
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
	}
	else if(fileptr < (void *)0x08048000)
		return true;
	else
		return false;
	return true;
}
