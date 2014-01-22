#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <errno.h>

#define MAXDATASIZE 100000
#define NUMTHREADS 1

//need a lock
int fd[2];
int out_sock;
pthread_mutex_t mutex_pipe; //lock to write thing to pipe
pthread_mutex_t mutex_task; //lock to access task list to get task
pthread_cond_t cv_task; //condition variable to show the task list can be accessed now

int someone_writing = 0;

typedef struct
{
	int sock;
	char fName[256];
} task_t;

struct task
{
	int sock;
	char fName[256];
	struct task *next;
};

struct task *taskList = NULL; //a queue that stores the info of remaining tasks accessable by all the threads, protected by lock

/* function that return a task from taskList, protected by lock */

struct task * getTask(){  
	struct task *currPtr = taskList;
	struct task *prevPtr = taskList;
	if(currPtr){
		while(currPtr->next){
			prevPtr = currPtr;
			currPtr = currPtr-> next;
		}
		prevPtr->next = NULL;
		return currPtr;
	}else{
		return NULL;
	}

}

/* this function adds a new task to the taskList */

void addTask(int adding_sock, char* adding_fName){ 
	struct task *headPtr = taskList;
	struct task *newTaskPtr;
	newTaskPtr = (struct task *) malloc(sizeof(struct task));
	newTaskPtr->sock = adding_sock;
	strcpy(newTaskPtr->fName, adding_fName);
	newTaskPtr->next = taskList;
	taskList = newTaskPtr;
	return;
}

/*print the tasks in the taskList, for testing */

void printTasks(){
	struct task *ptr = taskList;
	printf("starting task list:---\n");
	while(ptr!=NULL){
		printf("task for fName %s\n", ptr->fName);
		ptr = ptr->next;
	}
} 

void* worker(void * ptr)
{

	while(1){
		printf("tp: initialize thread\n");
		char ch;
		char *message;
		message = (char *)malloc((MAXDATASIZE)*sizeof(char));
		int current_len = 0;
		int current_size = MAXDATASIZE;
		struct task * current_task;
		pthread_mutex_lock (&mutex_task);
		pthread_cond_wait(&cv_task, &mutex_task);
		//check if there is something to do
		printf("tp: waking up!!!\n");
		//printTasks(); 
		if(taskList){ //yes, get the task
			current_task = getTask();
			if(!current_task){
				continue;
			}
			pthread_mutex_unlock (&mutex_task); //finished accessing tasklist, release the lock
			//if(taskList){ //check it again to see if there is still something else to do. if yes, send signal to other threads
			//	pthread_cond_signal(&cv_task);
			//}			
		}else{ // no, keep sleeping
			continue;
		}

		//sleep 5 seconds for testing
		printf("starting thread for %d\n", current_task->sock);
		sleep(5);

		/* reset message */
		message[0] = '\0';
		printf("tp0\n");
		FILE *fp;
		fp=fopen(current_task->fName, "r");
		if(fp){
			
			while( ( ch = fgetc(fp) ) != EOF ){
				//note:error might occur when reallocate

				if(current_len >= current_size-10){
					current_size*=2;
					message = (char*)realloc(message, current_size * sizeof(char));
				}
				message[current_len] = ch;
				current_len++;
			}
			message[current_len] = '\0';
			printf("current_len strlen %d %d\n", current_len, strlen(message));


			fclose(fp);
			pthread_mutex_lock (&mutex_pipe);
			while(someone_writing){
				printf("someone writing\n");
				//busy waiting for a bit;
			}
			someone_writing = 1;
			out_sock = current_task->sock;

			printf("message to write to pipe: %s\n", message);
			printf("out_sock %d\n", out_sock);

			write(fd[1],message,current_len+1);
			pthread_mutex_unlock (&mutex_pipe);
			free(message);
			free(current_task);
			printf("after unlock\n");
		}else{
			printf("tp no file\n");
			write(fd[1],'\0',1);
			free(current_task);
		}
	}

	pthread_exit(0);
}

/**
 * Set a file descriptor to non-blocking mode.
 */
int setnonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

int main(int argc, char ** argv)
{
	int listen_sock = -1, new_sock = -1;
	struct sockaddr_in address;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int port;
	char host_address[80];
	struct pollfd ufds[200];
	char buf1[MAXDATASIZE], buf2[MAXDATASIZE];
	int rv;
	int nfds = 2, current_size = 0, len = 1, i = 0, j = 0, n = 0;
	int close_conn = 0, end_server = 0, compress_array = 0;
	pthread_t threadPool[NUMTHREADS];
	int thread_count = 0;

	/* initialize the locks for the threads*/
	pthread_mutex_init(&mutex_pipe, NULL);
	pthread_mutex_init(&mutex_task, NULL);
	pthread_cond_init (&cv_task, NULL);

	/* initialize the threads in the threadPool */

	for(i = 0; i < NUMTHREADS; i++) {
	    pthread_create(&threadPool[i], NULL, worker, NULL);
	}	

	//create nonblock pipe
	setnonblock(fd[0]);
	setnonblock(fd[1]);
	if (pipe(fd) == -1)
	    perror("pipe error");

	/* check for command line arguments */
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s address port\n", argv[0]);
		return -1;
	}

	/* obtain host address */
	if (sscanf(argv[1], "%s", &host_address) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	/* obtain port number */

	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	/* create socket */
	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	/* bind socket to port */
	address.sin_family = AF_INET;
	inet_pton(AF_INET, host_address, &address.sin_addr);
	address.sin_port = htons(port);
	if (bind(listen_sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
		return -4;
	}

	/* listen on port */
	if (listen(listen_sock, 5) < 0)
	{
		fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
		return -5;
	}

	printf("%s: ready and listening\n", argv[0]);

	/* Set the socket to non-blocking. */
	if (setnonblock(listen_sock) < 0)
		err(1, "failed to set server socket to non-blocking");



	//add listening socket for polling
	ufds[0].fd = listen_sock;
	ufds[0].events = POLLIN;

	//add pipe read end for polling
	ufds[1].fd = fd[0];
	ufds[1].events = POLLIN;

	while(end_server == 0){

		//show all current file descriptor(test)
		printf("current file descriptors count %d\n", nfds);
		for (i = 0; i < nfds ; i++){
			printf("fd %d\n", ufds[i].fd);
		}
		printf("%s\n", "--------");

		if(taskList){
			pthread_cond_signal(&cv_task);
		}


	 	rv = poll(ufds, nfds, -1); //polling and check if there is network or file IO
		if (rv == -1) {
		    perror("poll"); // error occurred in poll()
		    break;
		}
		current_size = nfds;

	    for (i = 0; i < current_size; i++){
	  		if(ufds[i].revents == 0){
	    		continue;
	  		}
		    if(ufds[i].revents != POLLIN){
		        printf("  Error! revents = %d\n", ufds[i].revents);
		        printf("%d\n", ufds[i].fd);
		        end_server = 1;
		        break;
		    }
		    //some events happen		    
		    if(ufds[i].revents == POLLIN){ 
		    	//an event occurs at listening socket, accept all new connections
		  		if (ufds[i].fd == listen_sock){ 
			        printf("  Listening socket is readable\n");
			        do{
			          	new_sock = accept(listen_sock, NULL, NULL);
			          	if (new_sock < 0){
			            	if (errno != EWOULDBLOCK){
			              		perror("  accept() failed");
			              		end_server = 1;
			            	}
			            	break;
			          	}

				        printf("  New incoming connection - %d\n", new_sock);
				        ufds[nfds].fd = new_sock;
				        ufds[nfds].events = POLLIN;
				        nfds++;
			        } while (new_sock != -1);

			    /*event occurs at the pipe read end, read the data and send back to the client*/

		  		}else if (ufds[i].fd == fd[0]){
			        printf("  File io %d is readable\n", ufds[i].fd);
			    	//it's a file io event
			    	if ((n = read(ufds[i].fd, buf1, MAXDATASIZE)) >= 0) {
			    		buf1[n] = 0;	// terminate the string
			    		printf("read %d bytes from the pipe: \"%s\"\n", n, buf1);
						rv = send(out_sock, buf1, n+1, MSG_NOSIGNAL);
			    	}
			    	printf("outgoing socket: %d\n", out_sock);
			        close(out_sock);
			        for (j = 0; j < nfds; j++){
			        	if (ufds[j].fd == out_sock){
			        		ufds[j].fd = -1;
			        		out_sock = -1;				        		
			        	} 				        	
			        }
			        someone_writing = 0;
  			    	compress_array = 1;
  			    	break;

  			    //events occur at network IO, read the fileName and dispatch the work
		  		}else{
			        printf("  Network io %d is readable\n", ufds[i].fd);
			        close_conn = 0;
		        	rv = recv(ufds[i].fd, buf2, sizeof(buf2), 0);
		        	if (rv < 0){
			          	if (errno != EWOULDBLOCK){
			              	perror(" recv() failed");
			              	close_conn = 1;
			          	}
			          	break;
		        	}
		        	if (rv == 0){
			            printf("  Connection closed\n");
			            close_conn = 1;
		            	break;
		        	}

		        	len = rv;
		        	printf("  %d bytes received\n", len);

			        buf2[len-1] = '\0';
			        printf( "%s\n", buf2);

			        /* add a new task to the taskList, and send the signal to the threads for them to work */
			        //pthread_mutex_lock (&mutex_task);
			        addTask(ufds[i].fd, buf2);
			        //pthread_mutex_unlock (&mutex_task);
			        //printf("tp signal thread\n");
			        /*
			        if(taskList){
			        	pthread_cond_signal(&cv_task);
			        }
					*/
			        if (close_conn){
			          close(ufds[i].fd);
			          ufds[i].fd = -1;
			          compress_array = 1;
			    	}
			    	break;
			    }
			}
		}

		if (compress_array){
	  		compress_array = 0;
	  		for (i = 0; i < nfds; i++){
	    		if (ufds[i].fd == -1){
	    			for(j = i; j < nfds; j++){
	      			ufds[j].fd = ufds[j+1].fd;
	    			}
	    			nfds--;
	    		}
	  		}
		}



	}
	printf("%s\n", "program end");
	pthread_mutex_destroy(&mutex_pipe);
	pthread_mutex_destroy(&mutex_task);
	pthread_exit(NULL);
	return 0;
}
