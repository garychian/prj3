General overview
====================

Message Queues and Shared memory (shm) are used in combination for this project. A global message queue (key 9999)
is used to communicate from webproxy to simplecached information such as shared memory keys, shared memory sizes,
and thread specific message queues. Every thread created by webproxy has a thread specific message queue. This queue
is pinged when new data has been stored there by simplecached and is to be read. The shared memory is used to store a struct
shm_struct along with data from cached files. 



webproxy.c
---------------

1. webproxy Initializes shared memory (create nsegments of shared memory at specified size). They keys used to
   initialize shared memory is hardcoded in a macro defined in shm_channel (SHM_KEY ... SHM_KEY + nsegments - 1).
   Because the keys are incremented by 1, to inform simplecached about shm info the beginging number (SHM_KEY) and nsegments
   is the only info that must be given to recreated shm.
2. webproxy initializes a globabl message queue which shares shared memory keys with other processes (simplecached.c).
   The keying method is very similair that described in 1 (except it starts with MESSAGE_KEY 9999).

Ultimately, each thread created by gfserver_serve will have a a unique message queue. Over this message
queue information about which shared memory segment is being used is communicated, along with if the file was found

3. webproxy passes the message queue key unique to each thread as an argument to handle_with_cache.

handle_with_cache.c
------------------------

1. when a request comes in handle_with_cache sends a message with fpath to a global queue (listened to by simplecached.c)
2. handle_with_cache then listens for response (on thread specifc queue).
   The response will have info about which shared mem segment to read and if file exists
3. if file doesnt exist in cached FILE_NOT_FOUND is sent.
4. Otherwise, the  the shm rw_status is checked. If its in READ status the shm is read.
   If not handle_with_cache will wait on read condition variable.
5. After read the an attribute in the shared memory struct sets the shm to WRITE status. and a the write condition var is signaled.
   This allows handle_with_cached.c and simplecached.c to alternate reader and writer

simplecached.c
-----------------

1. Initially simplecached.c listens on globabl message queue for info about shared memory segments.
2. Once webporxy sends that info those segments are created and their keys are added to a queue (QUEUE_SHM) only seen by simplecached.c
3. simplecached.c creates worker thread.
4. (sc_worker) pull (if available) shared memory keys from QUEUE_SHM (if not avail they block)
5. Once key is popped from queue. sc_worker blocks until a message sent from handle_With_cache.c (on global message quue)
   sends info about which message queu is unique to that instance of handle_with_cache.c and which file to check cache against.
6. if file not found. this is communicated to handle_with_Cache on thread specific message queue. 
7. If it is foun then simplecached.sc_worker checks if shm is in WRITE status. If it is then data is written to shared memory.
   If its in Read then simplecached.sc_worker will block on condition variable write until signaled by handle_with_cache.
   (On first pass through shm begins in WRITE status.
8. Once written the sh_status is set to READ status and the condition variable read is signaled.
9. If entire file has been written (then process goes back to step4 ). If not simplecached.sc_worker goes back to step 7 until
   entirity of file has been written and read.