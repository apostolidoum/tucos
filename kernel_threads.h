#ifndef __KERNEL_THREADS_H
#define __KERNEL_THREADS_H

#include "kernel_cc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args);

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf();

/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval);

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid);

/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval);

#endif