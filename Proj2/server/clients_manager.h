#ifndef CLIENTS_MANAGER_H
#define CLIENTS_MANAGER_H

#include <pthread.h>

struct Worker_Thread {
  int free ;
  int id_session;
  char *path_request;
  char *path_response;
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t start_cond;
};

void set_list_WorkerThreads(struct Worker_Thread *all_worker_threads);
struct Worker_Thread *get_free_worker_thread(struct Worker_Thread *all_worker_threads);
void reset_WorkerThread(struct Worker_Thread *worker_thread);

#endif // CLIENTS_MANAGER_H