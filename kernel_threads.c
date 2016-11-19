#include <assert.h>
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_threads.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
  Mutex_Lock(&kernel_mutex);
  //symposiumofThreads argl -> number of philosopher
  TCB* temp = spawn_thread(CURPROC,task,argl);
  if(temp!=NULL){
    wakeup(temp);
    Mutex_Unlock(& kernel_mutex);
   return temp;
  }
  else {
    Mutex_Unlock(&kernel_mutex);
    return NOTHREAD;
  } 

}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval)
{
  if(tid == ThreadSelf())
	 return -1;
  else 
   return Wait_Thread(tid,exitval);
   //join 
}

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval)
{

  release_TCB(CURTHREAD);
}


/**
  @brief Awaken the thread, if it is sleeping.

  This call will set the interrupt flag of the
  thread.

  */
int ThreadInterrupt(Tid_t tid)
{
	return -1;
}


/**
  @brief Return the interrupt flag of the 
  current thread.
  */
int ThreadIsInterrupted()
{
	return 0;
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{

}



