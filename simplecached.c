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

#define MAX_CACHE_REQUEST_LEN 256

steque_t * QUEUE_SHM; //queue max len equal to number of shm segments
steque_t * QUEUE_MES; //queue max len equal to number of webproxy threads
pthread_mutex_t MUTEX_SHM;

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
	char *cachedir = "locals.txt";
	char option_char;
	int msqid, msgqid_glob;
	struct msgbuf msg;
	int *thread_id_list;
	int m_check;
	//key_msgbuff msg_thread;
	key_msgbuff msg_seg;

	m_check = pthread_mutex_init(&MUTEX_SHM, NULL);
	if (m_check != 0)
		perror("pthread_mutexattr_setpshared");

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
	//Receive message struct from the queue

	//Create thread pool
	  pthread_t *thread_list = (pthread_t *)malloc(nthreads * sizeof(pthread_t));
	  for (int ii =0; ii < nthreads; ii++){
		  thread_id_list[ii] = ii;
		  pthread_create(&thread_list[ii], NULL, (void *)&sc_worker, (void *)&thread_id_list[ii]);
	  }
	// get shared memory and message queue info
    msgrcv(msqid, &msg_seg, key_msgbuff_sizeof(), 0, 0);
    //msgrcv(msqid, &msg_thread, key_msgbuff_sizeof(), 0, 0);
	printf("message about shm ipc: key_count = %d, key_start = %d, key_end = %d", msg_seg.key_count, msg_seg.key_start, msg_seg.key_end);
	//printf("message about message queue ipc: key_count = %d, key_start = %d, key_end = %d", msg_thread.key_count, msg_thread.key_start, msg_thread.key_end);

	//create queues of shm keyid
  //Initialize queue  structures for message and shm
    QUEUE_SHM = malloc(sizeof(steque_t));
    QUEUE_MES = malloc(sizeof(steque_t));
    steque_init(QUEUE_SHM);
    steque_init(QUEUE_MES);
    //Add the shared memory keys to a queue
    for (int ii = msg_seg.key_start; ii <= msg_seg.key_end; i++)
    {
    	steque_push(QUEUE_SHM, ii);
    }
    //add the message keys to a queue
    //for (int ii = msg_thread.key_start; ii <= msg_thread.key_end; i++)
    //{
    //	steque_push(QUEUE_MES, ii);
    //}

	//start pulling from master message queue
	while (1)
	{
		msgrcv(msqid, &msg_thread, char_msgbuff_sizeof(), 0, 0);
		msg_thread
	}
	//an item in master queue will request a path, and message_keyid which will be listening on
	//check if path in cahce.
	//if path in cache. aquire lock and aquire shm_keyid (queue of shm keyid is unused keys0)
	//write to shared memory (fpath, tot_size, written_size). 
	//if file fits in buffer message compltion_status = 1 (finished), 0 (keep reading), -1 (error)
	//wait for read receipt = 1 (finished0) else error
	//repeat until message a completio_status = 1
	//wait for 

}


void sc_worker(void * path, void *seg_size){
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
	int shm_key;
	int fd;
	pthread_mutex_lock(&MUTEX_SHM);
		shm_key = steque_pop(QUEUE_SHM);
	pthread_mutex_unlock(&MUTEX_SHM);

	fd = simplecahce_get((char *)path);
	shm_ret = shmget(shm_key, (size_t *)seg_size, 0755 | IPC_CREAT);
	//shgmget returns -1 on failure
	if (shm_ret == -1)
	  perror("shmget");
	shm_data_p = (shm_data_t *)shmat(shm_ret, (void *)0, 0);

	//fd == -1 means couldnt be found in cache. Return FILENOTFOUND
	if (fd == -1)
	{

	}
	else
	{
		//get filesize
		fseek(fd, 0L, SEEK_END);
		shm_data_p->fsize = ftell(fd);
		//read contents and send
	    while (1)
	    {
	    	shm_data_p->data_size = read(fd, (void *)shm_data_p->data, shm_data_p->allwd_data_size);
			 = bytes_read;
			fprintf(stdout, "sc_worker: number of bytes read %d\n", bytes_read);
			if (shm_data_p->data_size == 0)
			{
				break;
			}
			else if (shm_data_p->data_size == -1)
			{
				perror("Error reading file");
			}
	    }
	}
	while (1){
		pthread_mutex_lock(&m);
			//if all jobs have been completed, break (instead
			//of blocking infinitely for a queue that won't populate.
			if (jobs_completed < tot_jobs){
				//wait while steque_isempty
				while (steque_isempty(queue) != 0){
					pthread_cond_wait(&c_cons, &m);
				}
			}
			else{
				pthread_mutex_unlock(&m);
				break;
			}
			job *job_to_exec = (job *)steque_pop(queue);
			jobs_completed++;
			printf("local_path = %s\n", job_to_exec->local_path);
		pthread_mutex_unlock(&m);

		int returncode = gfc_perform(job_to_exec->gfr);
		if ( returncode < 0){
		  fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
		  fclose(job_to_exec->file);
		  if ( 0 > unlink(job_to_exec->local_path))
			fprintf(stderr, "unlink failed on %s\n", job_to_exec->local_path);
		}
		else {
			fclose(job_to_exec->file);
		}

		if ( gfc_get_status(job_to_exec->gfr) != GF_OK){
		  if ( 0 > unlink(job_to_exec->local_path))
			fprintf(stderr, "unlink failed on %s\n", job_to_exec->local_path);
		}
		fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(job_to_exec->gfr)));
		fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(job_to_exec->gfr), gfc_get_filelen(job_to_exec->gfr));

		free(job_to_exec->gfr);
		free(job_to_exec);
	}
}
