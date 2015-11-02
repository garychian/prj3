//In case you want to implement the shared memory IPC as a library...
#include <pthread.h>
#include <unistd.h>

#define BUFFER_LEN 4096
#define SHM_KEY 66666
#define MESSAGE_KEY 9999

typedef struct char_msgbuf {
    long mtype = 1;
    char mtext[BUFFER_LEN] = "";
    int mkey = 0;
    int shmkey = 0;
    size_t size_seg = 0;
}char_msgbuf;

int char_msgbuff_sizeof()
{
	//size of message to be sent is size of struct minus long field
	return sizeof(char_msgbuff_sizeof) - sizeof(long);
}

typedef struct key_msgbuff {
	//This struct is used to communicate key ids
	//key ids are a continutes set of integers
	//given key_start and the number of keys key_count
    long mtype = 2;
    size_t size_seg = 0; //only applicable when used with sharedmemory
    int key_count = 0;
    int key_start = 0;
    int key_end = 0;
}key_msgbuff;

int key_msgbuff_init(key_msgbuff *self, size_t size_seg, int key_count, int key_start)
{
	//initizles struct with total number of elements
	self->size_seg = size_seg;
	self->key_count = key_count;
	self->key_start = key_start;
	self->key_end = key_start + key_count - 1;
}
int key_msgbuff_sizeof()
{
	//size of message to be sent is size of struct minus long field
	return sizeof(key_msgbuff) - sizeof(long);
}

typedef struct shm_data_t{
	pthread_mutex_t mutex;
	pthread_cond_t cond_read;
	pthread_cond_t cond_write;
	char path[256] = ""; //initilization of path
	size_t shm_size = 0;
	size_t allwd_data_size = 0;
	size_t fsize = 0; //Should be set to total data size to be written (could be larger than block)
	size_t data_size = 0; //size of data that currently resides in data block
	char *data;
}shm_data_t;

int shm_data_init(shm_data_t *self, size_t presc_size){
	/*
	given self initilize a process shared mutex
	*/
	int ret;
	_shm_mutex_var_init(&(self->mutex));
	_shm_cond_var_init(&(self->cond_read));
	_shm_cond_var_init(&(self->cond_write));

	ps = getpagesize();
	rem = presc_size % ps;
	//if mod returns 0 then prescribed size was equal to pagesize
	//and is the size of allocated shared memory
	if (rem == 0)
		self->shm_size = presc_size;
	//if not zero do some math to take prescrbed size
	//up to nearest page size
	else
		self->shm_size = (presc_size / ps) * (ps + 1);
	//determine allowed data size by subtracting from allocated
	//shm size the size of struct
	self->allwd_data_size = self->shm_size - sizeof(shm_data_t);

}

int _shm_cond_var_init(pthread_cond_t *c){
	pthread_condattr_t c_attr;
	int ret_val = 0;
	int ret = pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);
	if (ret != 0)
		perror("pthread_condattr_setpshared");
		ret_val = -1;
	ret = pthread_cond_init(c, &c_attr);
	if (ret != 0)
		perror("pthread_cond_init");
		ret_val = -1;
	return ret_val;
}

int _shm_mutex_var_init(pthread_mutex_t *m){
	pthread_mutexattr_t m_attr;
	int ret_val = 0;
	int ret = pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
	if (ret != 0)
		perror("pthread_mutexattr_setpshared");
		ret_val = -1;
	ret = pthread_cond_init(m, &m_attr);
	if (ret != 0)
		perror("pthread_mutex_init");
		ret_val = -1;
	return ret_val;
}
