#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/msg.h>

#include "shm_channel.h"

#include "gfserver.h"
//Replace with an implementation of handle_with_curl and any other
//functions you may need.

struct MemoryStruct{
	char *memory;
	size_t size;
};

size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp);
int send_contents(gfcontext_t *ctx, struct MemoryStruct * data);
void mem_struct_init(struct MemoryStruct *mem);

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){

	char buffer[BUFFER_LEN];
	char *data_dir = arg;
	struct MemoryStruct data;
	int msqid;
	msgbuf msg;
	CURLcode curl_ret_code;

	mem_struct_init(&data);
	strcpy(buffer,data_dir);
	strcat(buffer,path);
	msg.mtype = 2;
	strcpy(msg.mtext, buffer);

	fprintf(stdout, "cur_easy_perform.path = %s\n", buffer);
	//Create ipc message queue. Check if msgget performed okay
	msqid = msgget(MESSAGE_KEY, 0777 | IPC_CREAT);
	if (msqid == -1)
		perror("msgget: ");
	//Add message struct to the queue
	msgsnd(msqid, &msg, sizeof(msgbuf) - sizeof(long), 0);
	//performed okay
	if (curl_ret_code == 0){
		printf("%lu bytes retrieved\n", (long)data.size);
		gfs_sendheader(ctx, GF_OK, data.size);
		/* Sending the file contents chunk by chunk. */
		int ret_sc = send_contents(ctx, &data);
		if (ret_sc == 0)
			return data.size;
		else
			return EXIT_FAILURE;
	}
	//couldn't resolve host
	else if (curl_ret_code == 22){
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
	//any other error, it must have been server error. gfserver library will handle
	else{
		return EXIT_FAILURE;
	}

}

