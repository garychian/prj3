#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.
#define BUFFER_LEN 4096
struct MemoryStruct{
	char *memory;
	size_t size;
};

size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp);
int send_contents(gfcontext_t *ctx, struct MemoryStruct * data);
void mem_struct_init(struct MemoryStruct *mem);

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){

	char buffer[4096];
	char *data_dir = arg;
	struct MemoryStruct data;
	CURLcode curl_ret_code;

	mem_struct_init(&data);
	strcpy(buffer,data_dir);
	strcat(buffer,path);

	CURL *easy_handle = curl_easy_init();
	//fprintf(stdout, "cur_easy_perform.data_dir = %s\n", data_dir);
	//fprintf(stdout, "cur_easy_perform.path = %s\n", path);
	fprintf(stdout, "cur_easy_perform.path = %s\n", buffer);
	curl_easy_setopt(easy_handle, CURLOPT_URL, buffer);
	curl_easy_setopt(easy_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, write_memory_cb);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void *)&data);
	curl_ret_code = curl_easy_perform(easy_handle);
	curl_easy_cleanup(easy_handle);
	//performed okay
	if (curl_ret_code == 0){
		printf("%lu bytes retrieved\n", (long)data.size);
		gfs_sendheader(ctx, GF_OK, data.size);
		/* Sending the file contents chunk by chunk. */
		int ret_sc = send_contents(ctx, &data);
		free(data.memory);
		if (ret_sc == 0)
			return data.size;
		else
			return EXIT_FAILURE;


	}
	//couldn't resolve host
	else if (curl_ret_code == 22){
		free(data.memory);
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
	//any other error, it must have been server error. gfserver library will handle
	else{
		free(data.memory);
		return EXIT_FAILURE;
	}

}
int send_contents(gfcontext_t *ctx, struct MemoryStruct * data)
/*
*arguments*
gfcontext_t
MemoryStruct -> struct containing data to send and its size

*synopsis*
Sends chunks of data-> memory until bytes_transferred is less than
the size referenced by data struct.

*return*
0--If no error
1--if a gfs_send sends lses than the write_len_blk
*/
{
	ssize_t bytes_transferred = 0;
	ssize_t remaining_bytes, write_len_blk, write_len;
	while(bytes_transferred < data->size)
	{
		remaining_bytes = data->size - bytes_transferred;
		write_len_blk = (BUFFER_LEN > remaining_bytes) ? remaining_bytes : BUFFER_LEN;
		write_len = gfs_send(ctx, data->memory + bytes_transferred, write_len_blk);
		if (write_len != write_len_blk){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}
	return 0;
}

// reference for http://curl.haxx.se/libcurl/c/getinmemory.html
size_t write_memory_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}
void mem_struct_init(struct MemoryStruct *mem){
	mem->memory = malloc(1);
	mem->size = 0;
}
