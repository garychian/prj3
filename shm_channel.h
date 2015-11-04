#ifndef SHM_CHANNEL_H
#define SHM_CHANNEL_H
#define BUFFER_LEN 4096
#define SHM_KEY 66666
#define MESSAGE_KEY 9999

typedef struct char_msgbuf {
    long mtype;
    char mtext[BUFFER_LEN];
    int mkey;
    int shmkey;
    size_t size_seg;
}char_msgbuf;

void char_msgbuf_init(char_msgbuf *self);
size_t char_msgbuff_sizeof();


typedef struct key_msgbuff
{
	//This struct is used to communicate key ids
	//key ids are a continutes set of integers
	//given key_start and the number of keys key_count
    long mtype;
    size_t size_seg; //only applicable when used with sharedmemory
    int key_count;
    int key_start;
    int key_end;
}key_msgbuff;
void key_msgbuff_init(key_msgbuff *self, size_t size_seg, int key_count, int key_start);
int key_msgbuff_sizeof();

typedef struct shm_data_t{
	pthread_mutex_t mutex;
	pthread_cond_t cond_read;
	pthread_cond_t cond_write;
	char path[256]; //initilization of path
	int fexist;
	size_t shm_size;
	size_t allwd_data_size;
	size_t fsize; //Should be set to total data size to be written (could be larger than block)
	size_t data_size; //size of data that currently resides in data block
	char *data;
}shm_data_t;

void shm_data_clean(shm_data_t *self);
void shm_data_init(shm_data_t *self, size_t presc_size);
int _shm_cond_var_init(pthread_cond_t *c);
int _shm_mutex_var_init(pthread_mutex_t *m);

#endif
