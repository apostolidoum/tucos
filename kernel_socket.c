
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


socket_cb* PORT_TABLE[MAX_PORT];

/* file ops for the listener */
static file_ops no_fops = {
  .Open = NULL,   
  .Read = NULL,
  .Write = NULL, 
  .Close = NULL
};
static file_ops peer_fops = {
  .Open = NULL,    
  .Read = socket_read,
  .Write = socket_write, 
  .Close = socket_close
};

void initialize_ports()
{
   for(int i=0;i<MAX_PORT;i++) 
    PORT_TABLE[i] = NULL;
}

Fid_t Socket(port_t port)
{
	//fcb reserve
	//if port = NO_PORT -> peer type
	//if port != NO_PORT -> listener type , bound to #port
	socket_cb *socket;
	/*reserve space for a socket
		union makes sure that there will be enough space for the biggest of listener/peer*/
	socket = (socket_cb*)xmalloc(sizeof(socket));
	socket->socket_spinlock = MUTEX_INIT;
	Fid_t fid;
	FCB* fcb;
	/*
	 * reserve FCB if available
	 */
	/* Since FCB_reserve allocates fids in increasing order,
	   we expect pair[0]==0 and pair[1]==1 */
	if(FCB_reserve(1, fid, fcb)==0)// || fid[0]!=0 || fid[1]!=1)
	{
		printf("Failed to allocate console Fids\n");
		return -1;
		abort();
	}
	
	fcb->streamobj =  socket;
	/*check port to determine if you created a listener or a peer*/
	if(port != NOPORT){
		//listener
		fcb->streamfunc = & no_fops; 
		socket->type = LISTENER;
		socket->type_parameters.listener_param.port = port;
		socket->type_parameters.listener_param.init_status = UNINITIALIZED;
	}
	else{
		//peer
		//initialize 
		socket->type_parameters.peer_param.sender = SOCKET_NULL_FID;
      	socket->type_parameters.peer_param.receiver = SOCKET_NULL_FID;
      	socket->type_parameters.peer_param.wait_to_be_served = COND_INIT; 
      	socket->type_parameters.peer_param.conection_status = INITIAL;  

		socket->type = PEER;
		fcb->streamfunc = & peer_fops; 

	}
	

	socket->fid = fid;


	return socket->fid;
}

int Listen(Fid_t sock)
{	//check invalid fid
	if (sock == NOFILE)
		return -1;

	FCB* fcb;

	fcb = get_fcb(sock);
	socket_cb * socket = fcb->streamobj;

	//assert(socket->type == LISTENER);
	//check correct type
	if (socket->type != LISTENER)
		return -1;
	//check if already initialized
	if (socket->type_parameters.listener_param.init_status == INITIALIZED)
		return -1;
	//check valid port
	if (socket->type_parameters.listener_param.port == NOPORT)
		return -1;
	//check port is not already taken 
	if (PORT_TABLE[socket->type_parameters.listener_param.port] != NULL)
		return -1;

	//initialize request list, cv
	rlnode_init(& socket->type_parameters.listener_param.request_list,NULL);
	socket->type_parameters.listener_param.incoming_request = COND_INIT;
	socket->type_parameters.listener_param.init_status = INITIALIZED;
	PORT_TABLE[socket->type_parameters.listener_param.port] = socket;
	//when you are not sleeping, keep listening for requests
	while(1)
		Accept(sock);
	return 0;
}


Fid_t Accept(Fid_t lsock)
{	
	//check invalid fid
	if (lsock == NOFILE)
		return -1;

	FCB* fcb;

	fcb = get_fcb(lsock);
	socket_cb * socket = fcb->streamobj;
	//check if not already initialized
	if (socket->type_parameters.listener_param.init_status != INITIALIZED){
		fprintf(stderr, "%s\n","listener has not been initialized" );
		return -1;
	}
	//check there are enough FIDs that is more than 5
	// 2*2 for pipes
	// 1 for the second peer that will be created
	PCB* cur = CURPROC;
	int tempcount = 0;
	for(uint i = 0; i<MAX_FILEID; i++) 
		if(cur->FIDT[i] == NULL) tempcount++;
	if(tempcount<5) return -1;


	//if request list is empty sleep 
	if (rlist_len(& socket->type_parameters.listener_param.request_list ) == 0 )
		Cond_Wait(& socket->socket_spinlock, &socket->type_parameters.listener_param.incoming_request);
	//when you are awakened serve that request
	//pop request from list
	Mutex_Lock(& socket->socket_spinlock);
	Request * req = rlist_pop_front(& socket->type_parameters.listener_param.request_list );
	Mutex_Unlock(& socket->socket_spinlock);
	Fid_t fid_peer1 = req->requesting_peer;
	FCB* fcb_peer1;

	fcb_peer1 = get_fcb(fid_peer1);
	socket_cb * peer1 = fcb_peer1->streamobj;


	//make 2nd peer
	Fid_t fid_peer2 =  Socket(NOPORT);
	FCB* fcb_peer2;

	fcb_peer2 = get_fcb(fid_peer2);
	socket_cb * peer2 = fcb_peer2->streamobj;
	//make pipes
	pipe_t pipe1;
	pipe_t pipe2;
	
	//check for errors, pipes were not created
	 if (Pipe(&pipe1) == -1 || Pipe(&pipe2) == -1){
	 	peer1->type_parameters.peer_param.conection_status = UNSUCCESSFUL;
		peer2->type_parameters.peer_param.conection_status = UNSUCCESSFUL;
		//wake up peer 1
		Cond_Broadcast(& peer1->type_parameters.peer_param.wait_to_be_served);
		return -1;
	 }

	//establish connection
	peer1->type_parameters.peer_param.sender = pipe1.write;
	peer1->type_parameters.peer_param.receiver = pipe2.read;
	peer2->type_parameters.peer_param.sender = pipe2.write;
	peer2->type_parameters.peer_param.receiver = pipe1.read;

	peer1->type_parameters.peer_param.conection_status = SUCCESSFUL;
	peer2->type_parameters.peer_param.conection_status = SUCCESSFUL;
	//Broadcast to peer that he has been served
	Cond_Broadcast(& peer1->type_parameters.peer_param.wait_to_be_served);
	return fid_peer2;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	//check legal fid
	if (sock == NOFILE)
		return -1;
	FCB* fcb;

	fcb = get_fcb(sock);
	socket_cb * socket = fcb->streamobj;
	//check legal port
	if (port<0 || port > MAX_PORT)
		return -1;
	//check port has listener
	if (PORT_TABLE[port] == NULL)
		return -1;
	//check timeout....

	//make request
	socket->type_parameters.peer_param.request.requesting_peer = sock;
	
	do{
	//push to listener's list
	Mutex_Lock(& socket->socket_spinlock);
	rlist_push_back(& PORT_TABLE[port]->type_parameters.listener_param.request_list ,& socket->type_parameters.peer_param.request);
	Mutex_Unlock(& socket->socket_spinlock);
	//wake up listener
	Cond_Broadcast(& PORT_TABLE[port]->type_parameters.listener_param.incoming_request);
	//wait until served
		Cond_Wait(& socket->socket_spinlock, & socket->type_parameters.peer_param.wait_to_be_served);
	//check status of connection
	}while(socket->type_parameters.peer_param.conection_status == UNSUCCESSFUL);

	return  0;
}


int ShutDown(Fid_t sock, shutdown_mode how)
{
	//check legal fid
	if (sock == NOFILE)
		return -1;

	FCB* fcb;

	fcb = get_fcb(sock);
	socket_cb * socket = fcb->streamobj;

	switch (how)
	{
		case SHUTDOWN_READ:
			Close(socket->type_parameters.peer_param.receiver);
			socket->type_parameters.peer_param.receiver = SOCKET_NULL_FID;
			break;
		case SHUTDOWN_WRITE:
			Close(socket->type_parameters.peer_param.sender);
			socket->type_parameters.peer_param.sender = SOCKET_NULL_FID;
			break;
		case SHUTDOWN_BOTH:
			socket_close(socket);
			break;
		default:
      		fprintf(stderr, "%s\n","Invalid Shutdown arguments" );
      		return -1;
	}
	return 0;
}

int socket_read(void * dev, char* buf, unsigned int size){
	socket_cb* socket = (socket_cb*)dev;
	int sizeread = Read(socket->type_parameters.peer_param.receiver, buf, size);
	return sizeread;
}

int socket_write(void * dev, char* buf, unsigned int size){
	socket_cb* socket = (socket_cb*)dev;
	int sizewrite = Write(socket->type_parameters.peer_param.sender, buf, size);
	return sizewrite;
}

int socket_close(void* dev){
	socket_cb* socket = (socket_cb*)dev;
	Close(socket->type_parameters.peer_param.receiver);
	socket->type_parameters.peer_param.receiver = SOCKET_NULL_FID;
	Close(socket->type_parameters.peer_param.sender);
	socket->type_parameters.peer_param.sender = SOCKET_NULL_FID;

	return 0;
}



