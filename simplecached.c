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
	int msqid;
	struct msgbuf msg;
	int *thread_id_list;

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
	msgrcv(msqid, &msg, sizeof(msgbuf) - sizeof(long), 0, 0);
	printf("message from ipc: %s", msg.mtext);

}


void sc_worker(void *thread_id){
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
