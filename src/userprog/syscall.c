#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static struct lock fileLock;

void
syscall_init (void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init(&fileLock);
}

static bool check (const void *uaddr) {
  return (uaddr < PHYS_BASE && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL);
}

static inline bool getUser (uint8_t *dst, const uint8_t *usrc) {
  int eax;

  asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:" : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
  
  return eax != 0;
}

static inline bool putUser (uint8_t *udst, uint8_t byte) {
  int eax;

  asm ("movl $1f, %%eax; movb %b2, %0; 1:" : "=m" (*udst), "=&a" (eax) : "q" (byte));
  
  return eax != 0;
}

static bool getData (void *dst, const void *usrc, size_t size) 
{
  uint8_t *dst_byte = (uint8_t *)dst;
  uint8_t *usrc_byte = (uint8_t *)usrc;

  for (unsigned i=0; i < size; i++) {
    if(!(check(usrc_byte+i) && getUser(dst_byte+i, usrc_byte+i))) {
      return false;
    }
  }

  return true;
}

static int write (int arg0, int arg1, int arg2)
{
  int fd = arg0;
  const void *buffer = (const void*)arg1;
  unsigned length = (unsigned)arg2;

  if(fd == 1) {
    putbuf(buffer, length);
    return length;
  }
  else return 0; 
}

static int halt(int arg0 , int arg1 , int arg2 )
{
  shutdown_power_off();
}

static int exit (int arg0, int arg1 , int arg2 )
{
  int status = arg0;

  thread_current()->p->exit_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);

  thread_exit();
}

static int exec (int arg0, int arg1 , int arg2 )
{ 
  const char *a = (const char*)arg0;

  if(!verify_string(a)) sys_exit(-1, 0, 0);
  
  return process_execute(a);
}

static int wait (int arg0, int arg1 , int arg2 )
{ 
  pid_t pid = arg0;

  return process_wait(pid); 
}

static int create (int arg0, int arg1, int arg2 )
{ 
   const char *file = (const char*)arg0;
   unsigned initial_size = (unsigned)arg1;

  return 0; 
}

static int remove (int arg0, int arg1 , int arg2 )
{ 
   const char *file = (const char *)arg0;

  return 0;
}

static int open (int arg0, int arg1 , int arg2 )
{ 
   const char *file = (const char *)arg0;

  return 0; 
}

static int filesize (int arg0, int arg1 , int arg2 )
{ 
   int fd = arg0;
  
  return 0; 
}

static int read (int arg0, int arg1, int arg2)
{ 
   int fd = arg0;
   void *buffer = (void*)arg1;
   unsigned length = (unsigned)arg2;
  
  return 0; 
}

static int seek (int arg0, int arg1, int arg2 )
{ 
   int fd = arg0;
   unsigned position = (unsigned)arg1;
  
  return 0; 
}

static int tell (int arg0, int arg1 , int arg2 )
{ 
   int fd = arg0;
  
  return 0; 
}

static int close (int arg0, int arg1 , int arg2 )
{ 
   int fd = arg0;
  
  return 0; 
}

typedef int syscall_func(int, int, int);
struct syscall {
  int argc;
  syscall_func *func;
};

static struct syscall syscall_array[] =
{
  {0, halt}, {1, exit}, {1, exec}, {1, wait}, {2, create}, {1, remove}, {1, open}, {1, filesize}, {3, read}, {3, write}, {2, seek}, {1, tell}, {1, close}
};

static void syscall_handler (struct intr_frame *f) {
  int syscall_number = 0;
  int args[3] = {0,0,0};

  if(!getData(&syscall_number, f->esp, 4)) exit(-1, 0, 0);

  struct syscall *sc = &syscall_array[syscall_number];

  switch(sc->argc){
    case 3:
      if(!getData(&args[2], f->esp+12, 4)) exit(-1, 0, 0);
    case 2:
      if(!getData(&args[1], f->esp+8, 4)) exit(-1, 0, 0);
    case 1:
      if(!getData(&args[0], f->esp+4, 4)) exit(-1, 0, 0);
  }

  f->eax = sc->func(args[0], args[1], args[2]);

  return;
}