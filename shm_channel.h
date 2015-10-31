//In case you want to implement the shared memory IPC as a library...
#define BUFFER_LEN 4096
#define MESSAGE_KEY 9999

typedef struct msgbuf {
    long mtype;
    char mtext[4096];
}msgbuf;
