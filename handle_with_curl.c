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
void mem_struct_init(struct MemoryStruct *mem);

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){

	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len, remaining_bytes;
	char buffer[4096];
	char *data_dir = arg;
	struct MemoryStruct data;
	CURLcode curl_ret_code;
	mem_struct_init(&data);
	strcpy(buffer,data_dir);
	strcat(buffer,path);
	CURL *easy_handle = curl_easy_init();
	curl_easy_setopt(easy_handle, CURLOPT_URL, buffer);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, write_memory_cb);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void *)&data);
	curl_ret_code = curl_easy_perform(easy_handle);
	//performed okay
	if (curl_ret_code == 0){

	}
	//couldn't resolve host
	else if (curl_ret_code == 6){
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
	//any other error, it must have been server error. gfserver library will handle
	else{
		return EXIT_FAILURE;
	}


	printf("%lu bytes retrieved\n", (long)data.size);
	gfs_sendheader(ctx, GF_OK, data.size);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < data.size){
		remaining_bytes = data.size - bytes_transferred;
		read_len = (BUFFER_LEN > remaining_bytes) ? remaining_bytes : BUFFER_LEN;
		write_len = gfs_send(ctx, data.memory + bytes_transferred, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}
	curl_easy_cleanup(easy_handle);
	free(data.memory);
	return bytes_transferred;
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
