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

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  
  //tings for exit duno yolo
  p->exitcode = exitcode;
  p->exited = true;

  for (i=0; i < PID_MAX; i++;){
    if (curproc->pid == pidarray[i].pid){
        p->exitcode = exitcode;
        p->exited = true;
        lock_release(pidarray[i].lock); 
    }
  }

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

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
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

    struct proc *newproc = proc_create_runprogram(curproc->p_name);
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
    err = thread_fork(curthread->t_name, newproc, enter_forked_process, newtrapframe, 0);
    *retval = newproc->pid; 
    
return(0); 

}

