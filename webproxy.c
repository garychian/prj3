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
#include "steque.h"

#include "gfserver.h"

steque_t * QUEUE;
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
  CURLcode cg_init;
  //Initialize queue
  QUEUE = malloc(sizeof(steque_t));
  steque_init(QUEUE);

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
  cg_init = curl_global_init(CURL_GLOBAL_ALL);
  if (cg_init != 0){
    log_err("curl_global_init failed to initialize properly");
  	exit(1);
  }

  /*Initializing server*/
  gfserver_init(&gfs, nworkerthreads);

  /*Setting options*/
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  //create nsegments shared memory segments. Check return value
  for (i = 0; i < nsegments; i++)
  {
	  shm_ret = shmget(i, size_segments, 0755 | IPC_CREAT);
	  //shgmget returns -1 on failure
	  if (shm_ret == -1)
		  perror('shmget');
	  else
		  steque_push(QUEUE, i);
  }
  for(i = 0; i < nworkerthreads; i++)
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);

  /*Loops forever*/
  gfserver_serve(&gfs);
}
