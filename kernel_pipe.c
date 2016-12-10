
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

/*static file_ops pipe_reader_fops = {
  .Open = pipe_open,    //return NULL
  .Read = pipe_read,
  .Write = pipe_dont_write, //return -1
  .Close = pipe_close_reader
};

static file_ops pipe_writer_fops = {
  .Open = pipe_open,  //return NULL
  .Read = pipe_dont_read,   //return -1
  .Write = pipe_write,
  .Close = pipe_close_writer 
}; */

 static file_ops pipe_reader_fops = {
  .Open = pipe_open,    //return NULL
  .Read = pipe_read,
  .Write = pipe_write, //pipe_dont_write, //return -1
  .Close = pipe_close_reader
};

static file_ops pipe_writer_fops = {
  .Open = pipe_open,  //return NULL
  .Read =  pipe_read, //pipe_dont_read,   //return -1
  .Write = pipe_write,
  .Close = pipe_close_writer 
}; 

int Pipe(pipe_t* pipe)
{
	fprintf(stderr, "%s\n", "im in the Pipe!!!" );
	//reserve space for pipe
	//acquire 2 fcbs
	//fcb obj = same for both
	//fcb1 func->reader file_ops reader
	//fcb
	/*
	 * allocate space for pipe struct
	 */
	fprintf(stderr, "%s %d\n","pipe address initially",pipe );
	pipe = (pipe_t *)xmalloc(PIPE_SIZE);
	//assert(pipe !=NULL);
	if(pipe ==NULL) {
		fprintf(stderr, "%s\n","failed to create pipe! xmalloc shit" );
		return -1;
	}
	fprintf(stderr, "%s %d\n","pipe address after malloc",pipe );
	/* initialize pipe values */
	pipe->spinlock = MUTEX_INIT;
	pipe->pipe_has_stuff_to_read = COND_INIT;
  	pipe->pipe_has_space_to_write = COND_INIT;
  	pipe->buffer = ringbuf_new(8192); //size of buffer in bytes = 8KB
  	//assert(pipe->buffer != 0);
  	if(& pipe->buffer == 0) {
  		fprintf(stderr, "%s\n", "failed to create buffer....malloc??" );
  		return -1;
  	}

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

	//fcb[0] corresponds to wr
	//fcb[1] corresponds to rd
	fcb[1]->streamfunc = & pipe_reader_fops; 
	fcb[0]->streamfunc = & pipe_writer_fops;

	
	fprintf(stderr, "%s %d\n", "pipe->write", pipe->write);
	fprintf(stderr, "%s %d\n", "pipe->read", pipe->read);

	pipe->read = fcb[1];
	pipe->write = fcb[0];
	//fprintf(stderr, "%s %d\n","fcb[0]->streamobj ", fcb[0]->streamobj );
	fprintf(stderr, "%s %d\n", "fcb[0]->streamfunc", fcb[0]->streamfunc);
	fprintf(stderr, "%s %d\n", "fcb[1]->streamfunc", fcb[1]->streamfunc);
	fprintf(stderr, "%s %d\n", "fcb[0]->streamfunc->Write", fcb[0]->streamfunc->Write);
	fprintf(stderr, "%s %d\n", "fcb[1]->streamfunc->Write", fcb[1]->streamfunc->Write);
	fprintf(stderr, "%s %d\n", "fcb[0]->streamfunc->Read", fcb[0]->streamfunc->Read);
	fprintf(stderr, "%s %d\n", "fcb[1]->streamfunc->Read", fcb[1]->streamfunc->Read);
	fprintf(stderr, "%s %d\n", "fcb[0]", fcb[0]);
	fprintf(stderr, "%s %d\n", "fcb[1]", fcb[1]);
	fprintf(stderr, "%s %d\n", "pipe->write", pipe->write);
	fprintf(stderr, "%s %d\n", "pipe->read", pipe->read);
	//fprintf(stderr, "%s %d\n", "pipe->write", pipe->read);
	fprintf(stderr, "%s\n", "i am about to return 0..." );
	return 0;
}



/*
  Read from the pipe (device), sleeping if needed.
 */
int pipe_read(void* dev, char *buf, unsigned int size){
	fprintf(stderr, "%s\n", "Im in pipe read biatcheddd");
	
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

  fprintf(stderr, "%s %d\n","count",count );
  fprintf(stderr, "%s %d\n", "size",size );
  while(count<size) { 
  	fprintf(stderr, "%s\n","im looping biAtch" );
    //int valid = bios_read_serial(dcb->devno, &buf[count]);
    //---int valid = ringbuf_read(&buf[count],pipe_cb->buffer, 1); //1 = serial read from buffer //do i need casting? Do I need *?
    fprintf(stderr, "%s %d\n","buffer vsam's", buf[count] );

    int valid = ringbuf_memcpy_from(&buf[count], pipe_cb->buffer, 1); //serial write

    fprintf(stderr, "%s %d\n","buffer ours", buf[count] );

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
  	fprintf(stderr, "%s %d\n","counter in while ", count );
  }

  Mutex_Unlock(& pipe_cb->spinlock);
  //preempt_on;           /* Restart preemption */
  fprintf(stderr, "%s %d\n","counter before return ", count );
  return count;
}

/* 
  Write call 
  returns -1 when FAIL-> reader has closed
*/
int pipe_write(void* dev, const char* buf, unsigned int size)
{
	//fprintf(stderr, "%s\n", "I am about to write... pipe_write function" );
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
  			fprintf(stderr, "%s %d %s %d\n"," we are writing this ch: ",buf[count], " from buf[", count );
  			/*fprintf(stderr, "%s %c\n","buf[0] ",buf[0] );
  			int bu = ringbuf_bytes_free(pipe_cb->buffer);
  			int head = ringbuf_head( pipe_cb->buffer);
  			int tail = ringbuf_tail( pipe_cb->buffer);
  			fprintf(stderr, "%s %d\n","bytes free", bu);
  			fprintf(stderr, "%s %d \n","head ", head );
  			fprintf(stderr, "%s %d \n","tail ", tail ); 
			int bytes_used = ringbuf_bytes_used( pipe_cb->buffer); 			
    		fprintf(stderr, "%s %d\n","!!!!! bytes used", bytes_used );*/
  			int success = ringbuf_memset(pipe_cb->buffer, &buf[count],  1); //serial write
  			/*fprintf(stderr, "%s %d\n","success aka ringbuf_write return value",success );
  			fprintf(stderr, "%s %d\n","bytes free", bu);
  			fprintf(stderr, "%s %d \n","head ", head );
  			fprintf(stderr, "%s %d \n","tail ", tail );*/
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




