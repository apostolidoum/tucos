
#include "tinyos.h"

#include "ringbuf.c"

#include <assert.h>
#include "kernel_cc.h"
#include "kernel_dev.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_proc.h"

#include "tinyoslib.h"


#include "util.h"

file_ops __pipe_reader_fops = {
  .Open = pipe_open,    //return NULL
  .Read = pipe_read,
  .Write = pipe_dont_write, //return -1
  .Close = pipe_close_reader
};

file_ops __pipe_writer_fops = {
  .Open = pipe_open,  //return NULL
  .Read = pipe_dont_read,   //return -1
  .Write = pipe_write,
  .Close = pipe_close_writer 
};



int Pipe(pipe_t* pipe)
{
	//reserve space for pipe
	//acquire 2 fcbs
	//fcb obj = same for both
	//fcb1 func->reader file_ops reader
	//fcb
	/*
	 * allocate space for pipe struct
	 */
	pipe = (pipe_t *)xmalloc(PIPE_SIZE);
	assert(pipe !=NULL);
	Fid_t fid[2];
	FCB* fcb[2];
	/*
	 * reserve two FCBs if available
	 */
	/* Since FCB_reserve allocates fids in increasing order,
	   we expect pair[0]==0 and pair[1]==1 */
	if(FCB_reserve(2, fid, fcb)==0 || fid[0]!=0 || fid[1]!=1)
	{
		printf("Failed to allocate console Fids\n");
		abort();
	}
	//they both point to the same object
	fcb[0]->streamobj = pipe;
	fcb[1]->streamobj = pipe;

	//fcb[0] corresponds to reader
	//fcb[1] corresponds to writer
	fcb[0]->streamfunc = & __pipe_reader_fops;
	fcb[1]->streamfunc = & __pipe_writer_fops;

	pipe->read = fcb[0];
	pipe->write = fcb[1];
	return 0;
}



/*
  Read from the pipe (device), sleeping if needed.
 */
int pipe_read(void* dev, char *buf, unsigned int size){
	
  pipe_t* pipe_cb = (pipe_t*)dev;

  //preempt_off;            /* Stop preemption */
  Mutex_Lock(& pipe_cb->spinlock);

  uint count =  0;

  if (ringbuf_is_empty(pipe_cb->buffer)){ //do we need *? //if buffer is empty
  	if (pipe_cb->write == NULL){ //if writer is dead
  		Mutex_Unlock(& pipe_cb->spinlock);
  		//preempt_on;           /* Restart preemption */
  		return EOF;
  	} 
  	else //if writer is still alive, wait until there is something to read
  		Cond_Wait(&pipe_cb->spinlock, &pipe_cb->pipe_has_stuff_to_read); 
  }

  while(count<size) { 
    //int valid = bios_read_serial(dcb->devno, &buf[count]);
    int valid = ringbuf_read(&buf[count],pipe_cb->buffer, 1); //1 = serial read from buffer //do i need casting? Do I need *?
    
    if (valid){ //if we successfully read something
      count++;
      //wake up writer
      Cond_Broadcast(&pipe_cb->pipe_has_space_to_write);
    }
    else{ //if the buffer is empty
    	if (pipe_cb->write == NULL){ //if writer is dead
  			Mutex_Unlock(& pipe_cb->spinlock);
  			//preempt_on;           /* Restart preemption */
  			return EOF;
  		} 
  		else //if writer is still alive, wait until there is something to read
  			Cond_Wait(&pipe_cb->spinlock, &pipe_cb->pipe_has_stuff_to_read);
  	}	
  }

  Mutex_Unlock(& pipe_cb->spinlock);
  //preempt_on;           /* Restart preemption */

  return count;
}

/* 
  Write call 
  returns -1 when FAIL-> reader has closed
*/
int pipe_write(void* dev, const char* buf, unsigned int size)
{
  pipe_t* pipe_cb = (pipe_t*)dev;


  //preempt_off;            /* Stop preemption */
  Mutex_Lock(& pipe_cb->spinlock);

  unsigned int count = 0;
  while(count < size) {
  	if (pipe_cb->read == NULL){ //reader has closed, all hope is lost....
  		Mutex_Unlock(& pipe_cb->spinlock);
 		//preempt_on;           /* Restart preemption */
  		return -1;
  	}
  	else{
  		if(ringbuf_bytes_free(pipe_cb->buffer)>0){//&* ???????
  			//there is some space to write
  			int success = ringbuf_write(&buf[count],pipe_cb->buffer, 1); //serial write
  			assert(success >0);
  			count++;
  			//wake up reader  			
     		Cond_Broadcast(&pipe_cb->pipe_has_stuff_to_read);
  		}
  		else{
  			//wait until reader frees us some space
  			Cond_Wait(&pipe_cb->spinlock, &pipe_cb->pipe_has_space_to_write); ///????
  		}

  	}

  	
  }
  Mutex_Unlock(& pipe_cb->spinlock);
  //preempt_on;           /* Restart preemption */

  return count;  
}

void* pipe_open(uint minor)
{
  return NULL;
}

int pipe_close_reader(void* dev) 
{
	pipe_t* pipe_cb = (pipe_t*)dev;

	Mutex_Lock(& pipe_cb->spinlock);
	//pipe_cb->read = NULL;
	//wake up writer
    Cond_Broadcast(&pipe_cb->pipe_has_space_to_write);
  	Mutex_Unlock(& pipe_cb->spinlock);


  return 0;
}

int pipe_close_writer(void* dev) 
{
	pipe_t* pipe_cb = (pipe_t*)dev;

	Mutex_Lock(& pipe_cb->spinlock);
	//pipe_cb->write = NULL;
	//wake up reader
    Cond_Broadcast(&pipe_cb->pipe_has_stuff_to_read);
  	Mutex_Unlock(& pipe_cb->spinlock);

  return 0;
}

int pipe_dont_write(void* dev, const char* buf, unsigned int size){
	return -1;

}

int pipe_dont_read(void* dev, const char* buf, unsigned int size){
	return -1;
}




