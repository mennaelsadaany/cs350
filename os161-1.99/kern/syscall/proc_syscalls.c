#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <limits.h>
#include <synch.h>
#include <vm.h>
#include <vfs.h>
#include <kern/fcntl.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */



void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  
  //tings for exit duno yolo

  
  lock_acquire(glock);

   if (pidarray[curproc->pid].parentpid==-1) {
      //you dont have a parent you can just exit 
      pidarray[curproc->pid].pid = -5;
      pidarray[curproc->pid].exited=false; 
      pidarray[curproc->pid].exitcode= -1; 
      pidarray[curproc->pid].exitstatus=0; 
    }

    else {
        p->exitcode = exitcode;
        p->exited = true;
        pidarray[curproc->pid].exitcode = exitcode;
        pidarray[curproc->pid].exited = true;
        pidarray[curproc->pid].exitstatus = _MKWAIT_EXIT(exitcode); 
        
      for (int i=0; i < PID_MAX; i++){
         if (curproc->pid == pidarray[i].parentpid){
           pidarray[i].parentpid=-1; //yous an orphan bye felica
         }
      }
    }

  cv_broadcast(globalcv, glock); 
  lock_release(glock);


  //(void)exitcode; 
//end of my tings duno more yolo

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid; 
  //*retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

lock_acquire(glock);

if (pidarray[pid].parentpid != curproc->pid){
lock_release(glock);
  return ECHILD;
}

while(pidarray[pid].exited == false){
    cv_wait(globalcv,glock); 
}
exitstatus = pidarray[pid].exitstatus;  

lock_release(glock);

  if (options != 0) {
    return(EINVAL);
  }

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
 *retval = pid;
  return(0);
}

int
sys_fork(struct trapframe *tf, pid_t *retval) {
    int err = 0;
    lock_acquire(glock); 
    struct proc *newproc = proc_create_runprogram(curproc->p_name);
    lock_release(glock); 
        if (newproc == NULL){
          return ENOMEM; 
        } 

    struct addrspace *temp;// = (struct addrspace *)kmalloc(sizeof(addrspace));
    err= as_copy(curproc->p_addrspace, &temp);
     
        if (err){
          proc_destroy(newproc);
          kfree(newproc->p_addrspace);
          return ENOMEM; 
        } 

        
    as_activate();

    newproc->p_addrspace=temp;

    struct trapframe *newtrapframe = kmalloc(sizeof(struct trapframe));

      if(newtrapframe == NULL) {
          proc_destroy(newproc);
          kfree(newproc->p_addrspace);
          return ENOMEM;
      }
    memcpy(newtrapframe, tf, sizeof(struct trapframe));
  // *newtrapframe=*tf; //what is only shazzy dunos
    err = thread_fork(curthread->t_name, newproc, enter_forked_process, newtrapframe, 0);
    *retval = newproc->pid; 
    
return(0); 

}

int
sys_execv(userptr_t  progname, userptr_t args )
{
  struct addrspace *oldaddr;
  oldaddr=curproc_getas(); 
  int argc=0; 
  
  //how many args?  
  char ** argv = (char**)args; 

  if (argv != NULL){
    while (argv[argc] != NULL){
      argc++; 
    }
  }

  char ** argarray= NULL;
  argarray = kmalloc(argc*(sizeof(char*)));  

//copy args into array 
    for (int i=0; i<argc; i++){
      int arglen=strlen((char*)argv[i])+1; 
      argarray[i] = kmalloc((arglen) * sizeof(char)); 
      copyinstr((const_userptr_t)argv[i], argarray[i], arglen, NULL); 
    }

  ///runprogram 
  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;
 // int argc=0; 


  /* Open the file. */
  char * progtemp = kstrdup((char*)progname); 
  result = vfs_open(progtemp, O_RDONLY, 0, &v);
  kfree(progtemp); 
  if (result) {
    return result;
  }


  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr,argarray, argc);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }
//destroy old addr space
  as_destroy(oldaddr); 

  /* Warp to user mode. */
  enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}

