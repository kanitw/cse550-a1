#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#define MAXDATASIZE 3096
#define NUMTHREADS 4
#define NUMFD 200

int fd[2];
int out_sock;
char *output;
pthread_mutex_t mutex_pipe; //lock to write thing to pipe
pthread_mutex_t mutex_task; //lock to access task list to get task
pthread_cond_t cv_task; //condition variable to show the task list can be accessed now


/* this is used for a short time out (0.01 sec) in the code */
int self_sleep(){
	struct timespec tim;
	tim.tv_sec = 0;
	tim.tv_nsec = 10000000L;
	nanosleep(&tim , NULL);
	return 1;
}



/* get file size */
off_t fsize(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    fprintf(stderr, "Cannot determine size of %s: %s\n", filename, strerror(errno));

    return -1;
}

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
		taskList = currPtr->next;
		return currPtr;
	}else{
		return NULL;
	}

}

/* this function adds a new task to the taskList */

void addTask(int adding_sock, char* adding_fName){ 
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
	while(ptr != NULL){
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
		int current_len = 0, file_size = 0;
		struct task * current_task;
		pthread_mutex_lock (&mutex_task);
		pthread_cond_wait(&cv_task, &mutex_task);
		//check if there is something to do
		printf("tp: waking up!!!\n");
		printTasks(); 
		if(taskList){ //yes, get the task
			current_task = getTask();
			if(!current_task){
				continue;
			}
			pthread_mutex_unlock (&mutex_task); //finished accessing tasklist, release the lock

		}else{ // no, keep sleeping
			printf("WARNING: waking a thread and no task remaining\n");
			continue;
		}

		printf("starting thread for %d\n", current_task->sock);
		
		//sleep 5 seconds for concurrency testing
		//sleep(5);


		FILE *fp;
		fp=fopen(current_task->fName, "r");
		if(fp){
			file_size = fsize(current_task->fName);
			char* thread_output;
			thread_output = (char *)malloc((file_size+1)*sizeof(char));
			
			while( ( ch = fgetc(fp) ) != EOF ){
				thread_output[current_len] = ch;
				current_len++;
			}
			thread_output[current_len] = '\0';
			//printf("current_len strlen %d %d\n", current_len, strlen(message));
			printf("file size: %d\n", file_size);

			fclose(fp);
			pthread_mutex_lock (&mutex_pipe);
			printf("thread writing to the output now\n");

			out_sock = current_task->sock;
			output = thread_output;
			char* message;
			message = (char *)malloc(6*sizeof(char));
			strcpy(message, "exist\0");
			write(fd[1], message, 6);
			free(message);
		}else{
			printf("tp no file\n");
			pthread_mutex_lock (&mutex_pipe);
			out_sock = current_task->sock;
			char* message;
			message = (char *)malloc(3*sizeof(char));
			strcpy(message, "no\0");
			write(fd[1], message, 3);
			free(message);
		}
		self_sleep();// sleep 0.01 second before release the lock
		//sleep(1);
		pthread_mutex_unlock (&mutex_pipe);

		free(current_task);

	}

	pthread_exit(0);
}

/* Set a file descriptor to non-blocking mode. */
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
	struct pollfd ufds[NUMFD];
	char *bufPipe;
	char buf1[MAXDATASIZE];
	char buf2[MAXDATASIZE];
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
			printf("send signal to wake up thread\n");
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

			    	if ((n = read(ufds[i].fd, buf1, 8) >= 0)) {
			    		if(strcmp(buf1,"no")!=0){
			    			printf("output length %d\n", strlen(output));
			    			for(j=0 ; j*MAXDATASIZE < strlen(output); j++ ){
			    				strncpy(buf2, output+(j*MAXDATASIZE),MAXDATASIZE);
			    				//use MSG_NOSIGNAL to prevent SIGPIPE
								rv = send(out_sock, buf2, MAXDATASIZE, MSG_NOSIGNAL);
			    			}
			    			free(output);
			    			
			    		}else{
			    			printf("file not exist\n");
			    		}

			    	}
			    	printf("outgoing socket: %d\n", out_sock);
			        close(out_sock);
			        printf("close sock %d\n", out_sock);
			        for (j = 0; j < nfds; j++){
			        	if (ufds[j].fd == out_sock){
			        		ufds[j].fd = -1;
			        		out_sock = -1;				        		
			        	} 				        	
			        }
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

			        /* add a new task to the taskList*/
			        addTask(ufds[i].fd, buf2);

			        if (close_conn){
			          	close(ufds[i].fd);
			          	ufds[i].fd = -1;
			          	compress_array = 1;
			    	}
			    	break;
			    }
			}
		}


		/* at least one of the socket is closed, compress the ufds array here */
		if (compress_array){
			printf("remove element from ufds array\n");
	  		compress_array = 0;
	  		for (i = 0; i < nfds; i++){
	    		if (ufds[i].fd == -1){
	    			for(j = i; j < nfds; j++){
	      				ufds[j].fd = ufds[j+1].fd;
	    			}
	    			nfds--;
	    		}
	  		}
			//break;
		}



	}

	/* closing the program */
	printf("%s\n", "program ends");
	pthread_mutex_destroy(&mutex_pipe);
	pthread_mutex_destroy(&mutex_task);
	pthread_exit(NULL);
	return 0;
}
