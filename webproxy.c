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
//The following debug macros are directly from
//http://c.learncodethehardway.org/book/ex20.html
#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)
#define log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)
#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define check(A, M, ...) if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto error; }
#define sentinel(M, ...)  { log_err(M, ##__VA_ARGS__); errno=0; goto error; }
#define check_mem(A) check((A), "Out of memory.")
#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__); errno=0; goto error; }

//


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

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i, option_char = 0;
  int shm_ret;
  size_t size_segments = 4096;
  unsigned short port = 8888;
  unsigned short nsegments = 1;
  unsigned short nworkerthreads = 1;
  char *server = "s3.amazonaws.com/content.udacity-data.com";
  int msg_key, msqid;
  shm_data_t *shm_data_p;
  key_msgbuff msg_thread;
  key_msgbuff msg_seg;


  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
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
  key_msgbuff_init(&msg_thread, 0, nworkerthreads, MESSAGE_KEY);
  key_msgbuff_init(&msg_seg, size_segments, nsegments, SHM_KEY);

  //create nsegments shared memory segments. Check return value
  for (msg_key = msg_seg.key_start; msg_key <= msg_seg.key_end; msg_key++)
  {
	  shm_ret = shmget(msg_key, msg_seg.size_seg, 0755 | IPC_CREAT);
	  //shgmget returns -1 on failure
	  if (shm_ret == -1)
		  perror("shmget");
	  shm_data_p = (shm_data_t *)shmat(shm_ret, (void *)0, 0);
	  if (shm_data_p == (shm_data_t *)-1)
		  perror("shm_data_p");
	  //initialize data constructs (mutexes, size calculations, etc)
	  shm_data_init(shm_data_p, size_segments);

  }
  //create global message queue
  msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
  if (msqid == -1)
	  perror("msgget");
  //send info about segments shared memory to simplecached
  msgsnd(msqid, &msg_seg, key_msgbuff_sizeof(), 0);
  //send info about thread message
  //msgsnd(msqid, &msg_thread, key_msgbuff_sizeof(), 0);
  i = 0;
  for(msg_key = msg_thread.key_start; msg_key < msg_thread.key_end; msg_key++)
  {
    //where optional argument is the thread specific message key id
    //Its offset by 1 so as not to be confused with global message key 
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, msg_key);
    i++;
  }


  /*Loops forever*/
  gfserver_serve(&gfs);
}
