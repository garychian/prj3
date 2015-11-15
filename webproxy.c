#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curl/curl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "steque.h"
#include "shm_channel.h"
#include "gfserver.h"

steque_t * QUEUE_SHM; //queue max len equal to number of shm segments
steque_t * QUEUE_MES; //queue max len equal to number of webproxy threads


#define USAGE                                                                 \
"usage:\n" \
"	webproxy [options]\n" \
"options:\n" \
"	-n number of segments to use in communication with cache.\n" \
"	-z the size (in bytes) of the segments. \n" \
"	-p port for incoming requests\n" \
"	-t number of worker threads\n" \
"	-s server address(e.g. “localhost:8080”, or “example.com”)\n" \
"	-h print a help message \n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},         
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;
key_msgbuff SHM_KEY_INFO = { .mtype = MESSAGE_KEY, .size_seg = 0, .key_start = 0, .key_end = 0 };
key_msgbuff MSG_KEY_INFO = { .mtype = MESSAGE_KEY, .size_seg = 0, .key_start = 0, .key_end = 0 };

static void _sig_handler(int signo){
	shm_struct *shm_data_p = NULL;
	int shmid = 0;
	int shm_key = 0;
	int msg_key = 0;
	int shm_ret = 0;
	int mes_ret = 0;
	if (signo == SIGINT || signo == SIGTERM){

		//Detach and destroy shared memories (Loop through all used keys
		for (shm_key = SHM_KEY_INFO.key_start; shm_key <= SHM_KEY_INFO.key_end; shm_key++)
		{
			key_msgbuff_prnt(&SHM_KEY_INFO);
			key_msgbuff_prnt(&MSG_KEY_INFO);
			shmid = shmget(shm_key, SHM_KEY_INFO.size_seg, 0777 | IPC_CREAT);
			//shgmget returns -1 on failure
			if (shmid == -1)
				  perror("shmget");

			shm_data_p = (shm_struct *)shmat(shmid, (void *)0, 0);
			if (shm_data_p == (shm_struct *)-1)
				  perror("shm_data_p");


			shm_ret = shmdt(shm_data_p);
			printf("shmdt.shm_ret %d\n", shm_ret);
			if (shm_ret == -1)
				perror("shmdt");

			shm_ret = shmctl(shmid, IPC_RMID, NULL);
			printf("shmctl.shm_ret %d\n", shm_ret);
			if (shm_ret == -1)
				perror("shmctl");
		}

		//destroy message queues
		for (msg_key = MSG_KEY_INFO.key_start; msg_key <= MSG_KEY_INFO.key_end; msg_key++)
		{
			mes_ret = msgget(msg_key, 0777 | IPC_CREAT);
			if (mes_ret == -1)
				perror("msgget");
			msgctl(mes_ret, IPC_RMID, NULL);
		}
		gfserver_stop(&gfs);
		exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv)
	{
	int i = 0;
	int option_char = 0;
	int shm_ret = 0;
	int msgsnd_ret = 0;
	size_t size_segments = 4096;
	unsigned short port = 8888;
	unsigned short nsegments = 1;
	unsigned short nworkerthreads = 1;
	char *server = "s3.amazonaws.com/content.udacity-data.com";
	int msg_key = 0;
	int shm_key = 0;
	int msqid = 0;
	thread_arg_strct *thread_args = NULL;
	shm_struct *shm_data_p = NULL;
	key_msgbuff msg_seg = { .mtype = MESSAGE_KEY, .size_seg = 0, .key_start = 0, .key_end = 0 };

	if (signal(SIGINT, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	// Parse and set command line arguments
	while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1)
	{
		switch (option_char)
		{
			case 'n':
				nsegments = atoi(optarg);
				break;
			case 'z':
				size_segments = atoi(optarg);
				break;
			case 'p': // listen-port
				port = atoi(optarg);
				break;
			case 't': // thread-count
				nworkerthreads = atoi(optarg);
				break;
			case 's': // file-path
				server = optarg;
				break;
			case 'h': // help
				fprintf(stdout, "%s", USAGE);
				exit(0);
				break;
			default:
				fprintf(stderr, "%s", USAGE);
				exit(1);
		}
	}

	/* SHM initialization...*/

	/*Initializing server*/
	gfserver_init(&gfs, nworkerthreads);

	/*Setting options*/
	gfserver_setopt(&gfs, GFS_PORT, port);
	gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
	gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
	key_msgbuff_init(&msg_seg, size_segments, nsegments, SHM_KEY);
	key_msgbuff_init(&SHM_KEY_INFO, size_segments, nsegments, SHM_KEY);
	key_msgbuff_init(&MSG_KEY_INFO, 0, nworkerthreads + 1, MESSAGE_KEY); //+1 b/c there is a global message queue as well
	//create nsegments shared memory segments. Check return value
	for (shm_key = msg_seg.key_start; shm_key <= msg_seg.key_end; shm_key++)
	{
	  printf("webproxy shared memory made with key = %d and size = %zd\n", shm_key, msg_seg.size_seg);
	  shm_ret = shmget(shm_key, size_segments, 0777 | IPC_CREAT);
	  //shgmget returns -1 on failure
	  if (shm_ret == -1)
		  perror("shmget");
	  shm_data_p = (shm_struct *)shmat(shm_ret, (void *)0, 0);
	  if (shm_data_p == (shm_struct *)-1)
		  perror("shm_data_p");
	  //initialize data constructs (mutexes, size calculations, etc)
	  shm_init(shm_data_p, size_segments);
	}
	//create global message queue
	msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msqid == -1)
	  perror("msgget");

	//send info about segments shared memory to simplecached
	printf("webproxy: send message with key %d\n", MESSAGE_KEY);
	msgsnd_ret = msgsnd(msqid, &msg_seg, key_msgbuff_sizeof(), 0);
	if (msgsnd_ret == -1)
	  perror("main.msgsnd");

	i = 0;
	thread_args = (thread_arg_strct *)malloc(nworkerthreads * sizeof(thread_arg_strct));
	for(msg_key = MESSAGE_KEY + 1; msg_key < MESSAGE_KEY + 1 + nworkerthreads; msg_key++)
	{
		//where optional argument is the thread specific message key id
		//Its offset by 1 so as not to be confused with global message key
		thread_args[i].msg_key = msg_key;
		thread_args[i].size_segs = msg_seg.size_seg;
		gfserver_setopt(&gfs, GFS_WORKER_ARG, i, &thread_args[i]);
		i++;
	}


	/*Loops forever*/
	gfserver_serve(&gfs);
}
