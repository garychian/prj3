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
#include <errno.h>
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
	int ii = 0;
	int msqid = 0;
	int m_ret = 0;
	int *thread_id_list = NULL;
	pthread_t *thread_list = NULL;
	shm_key_strct *shm_key = NULL;
	char option_char;
	char *cachedir = "locals.txt";
	key_msgbuff msg_seg = { .mtype = MESSAGE_KEY, .size_seg = 0, .key_start = 0, .key_end = 0 };
	size_t size_segment = 0;

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
	//Initailzze mutex used to synchronize QUEUE_SHM
	m_ret = pthread_mutex_init(&MUTEX_SHM, NULL);
	if (m_ret != 0)
		perror("pthread_mutex_init");
	//Allocate thread pool
	thread_list = (pthread_t *)malloc(nthreads * sizeof(pthread_t));
	if (thread_list == (pthread_t *) NULL)
		perror("thread_list: malloc");
	thread_id_list = (int *)malloc(nthreads * sizeof(int));
	if (thread_id_list == (int *) NULL)
		perror("thread_id_list: malloc");

    //Initialize queue  structures for message and shm
    QUEUE_SHM = malloc(sizeof(steque_t));
    steque_init(QUEUE_SHM);

	/* Initializing the cache */
	simplecache_init(cachedir);
	msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msqid == -1)
		perror("msgget: ");
	// get shared memory and message queue info
	printf("simplecached: receieve message %d\n", MESSAGE_KEY);
    msgrcv(msqid, &msg_seg, key_msgbuff_sizeof(), KEY_MYTPE, 0);
    size_segment = msg_seg.size_seg;
    key_msgbuff_prnt(&msg_seg);

    //malloc shm key structs
    shm_key = (shm_key_strct *)malloc(msg_seg.key_count * sizeof(shm_key_strct));
    //Add the shared memory keys to a queue
    int count = 0;
    for (ii = msg_seg.key_start; ii <= msg_seg.key_end; ii++)
    {
    	(shm_key + count)->shm_key = ii;
    	steque_push(QUEUE_SHM, (shm_key + count));
    	count++;
    }

	for (ii =0; ii < nthreads; ii++){
	  thread_id_list[ii] = ii;
	  pthread_create(&thread_list[ii], NULL, (void *)&sc_worker, &msg_seg.size_seg);
	}
	for (ii =0; ii < nthreads; ii++){
	  pthread_join(thread_list[ii], NULL);
	}
}


void sc_worker(void *size_seg)
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
	shm_key_strct *shm_key;
	int fd = 0;
	int msgq_thd = 0;
	int msgq_glob = 0;
	int shm_ret = 0;
	int msgsend_ret = 0;
	int msgrcv_ret = 0;
	size_t size_segment = 0;
	shm_data_t * shm_data_p = NULL;
	char_msgbuf msg_thread = {.mtype = CHAR_MTYPE, .mtext = "", .mkey = 0, .shmkey = 0, .size_seg = 0, .existance = NOTEXISTS};
	size_segment =  *(size_t *)size_seg;
	puts("simplecached.c: get from global queue");
	//create global message queue within thread
	msgq_glob = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msgq_glob == -1)
		perror("msgget");
	while(1)
	{
		//grab mutex to read from queue and grab a shm resource
		pthread_mutex_lock(&MUTEX_SHM);
			shm_key = (shm_key_strct *)steque_pop(QUEUE_SHM);
		pthread_mutex_unlock(&MUTEX_SHM);

		puts("simplecached.c: receive from ");
		printf("simplecached.sc_worker: receieve message %d\n", MESSAGE_KEY);
		msgrcv_ret = msgrcv(msgq_glob, &msg_thread, char_msgbuff_sizeof(), CHAR_MTYPE, 0);
		if (msgrcv_ret == -1)
			perror("msgrcv.sc_worker");
		char_msgbuf_prnt(&msg_thread);

		//update msg_thread attribute shmkey with shm_key from queue
		msg_thread.shmkey = shm_key->shm_key;

		//create message queue individual to handle_with_cache
		msgq_thd = msgget(msg_thread.mkey, 0777 | IPC_CREAT);
		if (msgq_thd == -1)
			perror("msgget");

		//create pointer to shared memory
		printf("simplecached.shared memory made with key = %d and size = %zd\n", msg_thread.shmkey, size_segment);
		shm_ret = shmget(msg_thread.shmkey, size_segment, 0777 | IPC_CREAT);
		if (shm_ret == -1)
		  perror("shmget");
		shm_data_p = (shm_data_t *)shmat(shm_ret, (void *)0, 0);
		if (shm_data_p == (shm_data_t *)-1)
			perror("shmat");
		//clean out shared memory and set rw_status to WRITE
		puts("*******************************************");
		puts("simplecached shared memory b4 clean");
		shm_data_prnt(shm_data_p);
		shm_data_clean(shm_data_p);
		shm_data_calc_offset(shm_data_p);
		puts("*******************************************");
		puts("simplecached shared memory after clean and reset");
		shm_data_prnt(shm_data_p);
		//get file descriptor frame cache. Returns -1 if not in cache
		fd = simplecache_get((char *)msg_thread.mtext);
		if (fd == -1)
		{
			shm_data_p->fexist = NOTEXISTS;
			msg_thread.existance = NOTEXISTS;
		}
		else
		{
			shm_data_p->fexist = EXISTS;
			msg_thread.existance = EXISTS;
		}
		msg_thread.size_seg = shm_data_p->shm_size;

		//send over thread message queue char_msg_buff with shm_key and filexist
		printf("simplecached.sc_worker: send message %d\n", msg_thread.mkey);
		msgsend_ret = msgsnd(msgq_thd, &msg_thread, char_msgbuff_sizeof(), 0);
		if (msgsend_ret == -1)
			perror("msgsnd");
		//if file exists do stuff. Else skip
		if (fd != -1)
		{
			size_t tot_data_read = 0;
			//get filesize
			shm_data_p->fsize = lseek(fd, 0L, SEEK_END);
			//return file pointer to beginning of file
			lseek(fd, 0L, SEEK_SET);
			//dont break unless no file found or contents of file sent entirely
			while (1)
			{
				//skip wait block if already in write
				if (shm_data_p->rw_status != WRITE_STATUS)
				{
					pthread_mutex_lock(&shm_data_p->mutex);
						pthread_cond_wait(&shm_data_p->cond_write, &shm_data_p->mutex);
					pthread_mutex_unlock(&shm_data_p->mutex);
				}


				//read contents and send

				//try and read a block as large as allowed data size. Set data_size to amount read
				//todo: need to init pointer to be offset
				shm_data_p->data_size = read(fd, (void *)(shm_data_p + 1), shm_data_p->allwd_data_size);
				tot_data_read += shm_data_p->data_size;

				fprintf(stdout, "sc_worker: number of bytes read %zd....%zd/%zd\n", shm_data_p->data_size, tot_data_read, shm_data_p->fsize);

				if (shm_data_p->data_size == -1)
				{
					printf("errno = %d", errno);
					shm_data_p->fexist = -1;
					break;
				}
				else
				{
					//set shm status to READ_STATUS
					shm_data_p->rw_status = READ_STATUS;
					//signal reader that read can conditnue
					pthread_cond_signal(&shm_data_p->cond_read);
					//when reader signals cond_write then write can continue
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
