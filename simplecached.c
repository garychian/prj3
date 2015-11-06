#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"
#define MAX_CACHE_REQUEST_LEN 256

steque_t * QUEUE_SHM; //queue max len equal to number of shm segments
steque_t * QUEUE_MES; //queue max len equal to number of webproxy threads
pthread_mutex_t MUTEX_SHM;
void sc_worker();
static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nthreads",           required_argument,      NULL,           't'},
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 1;
	int i;
	int ii;
	int msqid;
	int *thread_id_list;
	pthread_t *thread_list;
	char option_char;
	char *cachedir = "locals.txt";
	key_msgbuff msg_seg;

	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			default:
				Usage();
				exit(1);
		}
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	/* Initializing the cache */
	simplecache_init(cachedir);
	msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msqid == -1)
		perror("msgget: ");
	// get shared memory and message queue info
    msgrcv(msqid, &msg_seg, key_msgbuff_sizeof(), 0, 0);
	printf("message about shm ipc: key_count = %d, key_start = %d, key_end = %d", msg_seg.key_count, msg_seg.key_start, msg_seg.key_end);

	//Create thread pool
	thread_list = (pthread_t *)malloc(nthreads * sizeof(pthread_t));
	if (thread_list == (pthread_t *) NULL)
		perror("malloc");

	thread_id_list = (int *)malloc(nthreads * sizeof(int));
	if (thread_id_list == (int *) NULL)
		perror("malloc");
	for (ii =0; ii < nthreads; ii++){
	  thread_id_list[ii] = ii;
	  pthread_create(&thread_list[ii], NULL, (void *)&sc_worker, NULL);
	}

	//create queues of shm keyid
    //Initialize queue  structures for message and shm
    QUEUE_SHM = malloc(sizeof(steque_t));
    QUEUE_MES = malloc(sizeof(steque_t));
    steque_init(QUEUE_SHM);
    steque_init(QUEUE_MES);
    //Add the shared memory keys to a queue
    for (ii = msg_seg.key_start; ii <= msg_seg.key_end; i++)
    {
    	steque_push(QUEUE_SHM, &ii);
    }

	//start pulling from master message queue

	//an item in master queue will request a path, and message_keyid which will be listening on
	//check if path in cahce.
	//if path in cache. aquire lock and aquire shm_keyid (queue of shm keyid is unused keys0)
	//write to shared memory (fpath, tot_size, written_size). 
	//if file fits in buffer message compltion_status = 1 (finished), 0 (keep reading), -1 (error)
	//wait for read receipt = 1 (finished0) else error
	//repeat until message a completio_status = 1
	//wait for 

}


void sc_worker()
{
	/*
	 *Synopsis*
	 client_worker is dependent on the global queue object.
	 The client_worker function blocks until the queue isnt empty.
	 Once signaled that the queue is populated client_worker pops
	 job from queue which contains the gfr along with other attributes (see job struct)

	*arguments*
	thread_id (pointer to int, void bc client_worker is intended for thread use) -
		thread_id is an id for the thread worker, primarily used for debugging

	*returns*
	void

	 */
	key_t *shm_key;
	int fd;
	int msqid;
	int shm_ret;
	int msgsend_ret;
	shm_data_t * shm_data_p;
	char_msgbuf msg_thread;
	//create global message queue within thread
	msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msqid == -1)
		perror("msgget");
	while(1)
	{
		//grab mutex to read from queue and grab a shm resource
		pthread_mutex_lock(&MUTEX_SHM);
			msgrcv(msqid, &msg_thread, char_msgbuff_sizeof(), 0, 0);
			shm_key = steque_pop(QUEUE_SHM);
			msg_thread.shmkey = *shm_key;
		pthread_mutex_unlock(&MUTEX_SHM);


		shm_ret = shmget(msg_thread.shmkey, msg_thread.size_seg, 0755 | IPC_CREAT);
		//shgmget returns -1 on failure
		if (shm_ret == -1)
		  perror("shmget");
		shm_data_p = (shm_data_t *)shmat(shm_ret, (void *)0, 0);
		//clean out shared memory
		shm_data_clean(shm_data_p);
		//get file descriptor frame cache. Returns -1 if not in cache
		fd = simplecache_get((char *)msg_thread.mtext);
		//send over thread message queue char_msg_buff with shm_key
		msgsend_ret = msgsnd(msg_thread.mkey, &msg_thread, char_msgbuff_sizeof(), 0);
		if (msgsend_ret == -1)
			perror("msgsnd");
		if (fd == -1)
		{
			//sending unitialized structure (with fexist = 0) will
			//indicate no file found. Signal awake readers
			pthread_cond_signal(&shm_data_p->cond_read);
			//when reader signals cond_write then write can continue
			pthread_cond_wait(&shm_data_p->cond_write, &shm_data_p->mutex);
		}
		else
		{
			shm_data_p->fexist = 1;
			//get filesize

			shm_data_p->fsize = lseek(fd, 0L, SEEK_END);
			//read contents and send
			while (1)
			{
				//try and read a block as large as allowed data size. Set data_size to amount read
				shm_data_p->data_size = read(fd, (void *)shm_data_p->data, shm_data_p->allwd_data_size);

				fprintf(stdout, "sc_worker: number of bytes read %zd\n", shm_data_p->data_size);

				if (shm_data_p->data_size == -1)
				{
					perror("Error reading file");
					shm_data_p->fexist = -1;
				}
				else
				{
					//signal reader that read can conditnue
					pthread_cond_signal(&shm_data_p->cond_read);
					//when reader signals cond_write then write can continue
					pthread_cond_wait(&shm_data_p->cond_write, &shm_data_p->mutex);
					if (shm_data_p->data_size == 0)
						break;
				}
			}
		}
		pthread_mutex_lock(&MUTEX_SHM);
			steque_push(QUEUE_SHM, shm_key);
		pthread_mutex_unlock(&MUTEX_SHM);
	}
}
