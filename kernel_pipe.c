
#include "tinyos.h"
#include "ringbuf.h"
#include "ringbuf.c"

#include <assert.h>
#include "kernel_cc.h"
#include "kernel_dev.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_proc.h"


#include "util.h"


int Pipe(pipe_t* pipe)
{
	return -1;
}

typedef struct serial_pipe_device_control_block {
  //uint devno;
  Mutex spinlock;
	/*our stuff*/
  CondVar pipe_has_stuff_to_read;
  CondVar pipe_has_space_to_write;
  ringbuf_t buffer;
  Fid_t fid_reader;
  Fid_t fid_writer;
} serial_pipe_dcb_t;

/*
  Read from the pipe (device), sleeping if needed.
 */
int serial_pipe_read(void* dev, char *buf, unsigned int size){
	
  serial_pipe_dcb_t* dcb = (serial_pipe_dcb_t*)dev;

  //preempt_off;            /* Stop preemption */
  Mutex_Lock(& dcb->spinlock);

  uint count =  0;

  if (ringbuf_is_empty(dcb->buffer)){ //do we need *? //if buffer is empty
  	if (dcb->fid_writer == NULL){ //if writer is dead
  		Mutex_Unlock(& dcb->spinlock);
  		//preempt_on;           /* Restart preemption */
  		return EOF;
  	} 
  	else //if writer is still alive, wait until there is something to read
  		Cond_Wait(&dcb->spinlock, &dcb->pipe_has_stuff_to_read); 
  }

  while(count<size) { 
    //int valid = bios_read_serial(dcb->devno, &buf[count]);
    int valid = ringbuf_read(&buf[count],dcb->buffer, 1); //1 = serial read from buffer //do i need casting? Do I need *?
    
    if (valid){ //if we successfully read something
      count++;
      //wake up writer
      Cond_Broadcast(&dcb->pipe_has_space_to_write);
    }
    else{ //if the buffer is empty
    	if (dcb->fid_writer == NULL){ //if writer is dead
  			Mutex_Unlock(& dcb->spinlock);
  			//preempt_on;           /* Restart preemption */
  			return EOF;
  		} 
  		else //if writer is still alive, wait until there is something to read
  			Cond_Wait(&dcb->spinlock, &dcb->pipe_has_stuff_to_read);
  	}	
  }

  Mutex_Unlock(& dcb->spinlock);
  //preempt_on;           /* Restart preemption */

  return count;
}

/* 
  Write call 
  returns -1 when FAIL-> reader has closed
*/
int serial_pipe_write(void* dev, const char* buf, unsigned int size)
{
  serial_pipe_dcb_t* dcb = (serial_pipe_dcb_t*)dev;


  //preempt_off;            /* Stop preemption */
  Mutex_Lock(& dcb->spinlock);

  unsigned int count = 0;
  while(count < size) {
  	if (dcb->fid_writer == NULL){ //reader has closed, all hope is lost....
  		Mutex_Unlock(& dcb->spinlock);
 		//preempt_on;           /* Restart preemption */
  		return -1;
  	}
  	else{
  		if(ringbuf_bytes_free(dcb->buffer)>0){//&* ???????
  			//there is some space to write
  			int success = ringbuf_write(&buf[count],dcb->buffer, 1); //serial write
  			assert(success >0);
  			count++;
  			//wake up reader  			
     		Cond_Broadcast(&dcb->pipe_has_stuff_to_read);
  		}
  		else{
  			//wait until reader frees us some space
  			Cond_Wait(&dcb->spinlock, &dcb->pipe_has_space_to_write); ///????
  		}

  	}

  	
  }
  Mutex_Unlock(& dcb->spinlock);
  //preempt_on;           /* Restart preemption */

  return count;  
}

void* serial_pipe_open(uint minor)
{
  return NULL;
}

int serial_pipe_close(void* dev) 
{
  return 0;
}

static file_ops serial_pipe_fops = {
  .Open = serial_pipe_open,
  .Read = serial_pipe_read,
  .Write = serial_pipe_write,
  .Close = serial_pipe_close
};

