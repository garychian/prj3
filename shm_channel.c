#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "shm_channel.h"

size_t char_msgbuff_sizeof()
{
	//size of message to be sent is size of struct minus long field
	return sizeof(char_msgbuf) - sizeof(long);
}

void char_msgbuf_prnt(char_msgbuf *self)
{
	printf("mtext = %s\n", self->mtext);
	printf("mkey = %zd\n", self->mkey);
	printf("shmkey = %zd\n", self->shmkey);
	printf("size_seg = %zd\n", self->size_seg);
	printf("existance = %d\n", self->existance);
}
void char_msgbuf_init(char_msgbuf *self, char *mtext, key_t mkey, key_t shmkey, size_t size_seg, int existance)
{

	self->mtype = CHAR_MTYPE;
	memset(self->mtext, 0, BUFFER_LEN);
	strcpy(self->mtext, mtext);
	self->mkey = mkey;
	self->shmkey = shmkey;
	self->size_seg = size_seg;
	self->existance = existance;
}

int key_msgbuff_sizeof()
{
	//size of message to be sent is size of struct minus long field
	return sizeof(key_msgbuff) - sizeof(long);
}
void key_msgbuff_prnt(key_msgbuff *self)
{
    printf("size_seg = %zd\n", self->size_seg); //only applicable when used with sharedmemory
    printf("key_count = %d\n", self->key_count);
    printf("key_start = %d\n", self->key_start);
	printf("key_end = %d\n", self->key_end);
}

void key_msgbuff_init(key_msgbuff *self, size_t size_seg, int key_count, int key_start)
{
	//initizles struct with total number of elements
	self->mtype = KEY_MYTPE;
	self->size_seg = size_seg;
	self->key_count = key_count;
	self->key_start = key_start;
	self->key_end = key_start + key_count - 1;
}
void shm_data_init(shm_data_t *self, size_t presc_size)
{
	/*
	given self initilize a process shared mutex.
	Additionally sets:
		shm_size
		allwd_data_size
	*/

	_shm_mutex_var_init(&(self->mutex));
	_shm_cond_var_init(&(self->cond_read));
	_shm_cond_var_init(&(self->cond_write));

	//initilize data structure to 0
	shm_data_clean(self);
	//calculate sizes
	shm_data_sizecalc(self, presc_size);
}
void shm_data_sizecalc(shm_data_t *self, size_t presc_size)
{
	size_t ps;
	size_t rem;

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
	//The data is to be written past the memory contents of the struct.
	//Use pointer arithmetic to specify
	shm_data_calc_offset(self);
	//print shared memory
	shm_data_prnt(self);
}
void shm_data_calc_offset(shm_data_t *self)
{
	self->data = (char *)(self + 1);
}
void shm_data_clean(shm_data_t *self)
{
	memset(self->path, 0, 256);
	self->fexist = 0; //when read by handle_with_cache. 0 - FILENOTFOUND, 1, FILEFOUND, -1 set on error
	self->rw_status = WRITE_STATUS;
	//self->shm_size = 0;
	//self->allwd_data_size = 0;
	self->fsize = 0; //Should be set to total data size to be written (could be larger than block)
	self->data_size = 0; //size of data that currently resides in data block

}
int _shm_cond_var_init(pthread_cond_t *c)
{
	int ret;
	pthread_condattr_t c_attr;
	ret = pthread_condattr_init(&c_attr);
	if (ret != 0)
		perror("pthread_condattr_init");
	ret = pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);
	if (ret != 0)
		perror("pthread_condattr_setpshared");
	ret = pthread_cond_init(c, &c_attr);
	if (ret != 0)
		perror("pthread_cond_init");
	return ret;
}

int _shm_mutex_var_init(pthread_mutex_t *m)
{
	int ret;
	pthread_mutexattr_t m_attr;
	ret = pthread_mutexattr_init(&m_attr);
	if (ret != 0)
		perror("pthread_mutexattr_init");
	ret = pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
	if (ret != 0)
		perror("pthread_mutexattr_setpshared");
	ret = pthread_mutex_init(m, &m_attr);
	if (ret != 0)
		perror("pthread_mutex_init");
	return ret;
}

void shm_data_prnt(shm_data_t *self)
{
	printf("shm_data_object starts at %p\n", self);
	printf("shm_data.path_object starts at %p\n", self->path);
	printf("shm_data.data starts at %p\n", self->data);
	printf("shm_data object offset starts at %p\n", self + 1);
}
