#include "clients_manager.h"
#include "common/constants.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void set_list_WorkerThreads(struct Worker_Thread *all_worker_threads) {
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    all_worker_threads[i].free = 1;
    all_worker_threads[i].id_session = i;
    all_worker_threads[i].path_request = malloc(sizeof(char) * 41);
    if (all_worker_threads[i].path_request == NULL) {
      fprintf(stderr, "Error allocating memory\n");
      exit(EXIT_FAILURE);
    }
    all_worker_threads[i].path_response = malloc(sizeof(char) * 41);
    if (all_worker_threads[i].path_response == NULL) {
      fprintf(stderr, "Error allocating memory\n");
      exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_init(&all_worker_threads[i].mutex, NULL) != 0) {
      fprintf(stderr, "Error initializing mutex\n");
      exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&all_worker_threads[i].start_cond, NULL) != 0) {
      fprintf(stderr, "Error initializing condition variable\n");
      exit(EXIT_FAILURE);
    }
  }
}
void reset_WorkerThread(struct Worker_Thread *worker_thread) {

  if (pthread_mutex_destroy(&worker_thread->mutex) != 0) {
    fprintf(stderr, "Error destroying mutex\n");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_destroy(&worker_thread->start_cond) != 0) {
    fprintf(stderr, "Error destroying condition variable\n");
    exit(EXIT_FAILURE);
  }

  worker_thread->free = 1;
  
  if (pthread_mutex_init(&worker_thread->mutex, NULL) != 0) {
    fprintf(stderr, "Error initializing mutex\n");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_init(&worker_thread->start_cond, NULL) != 0) {
    fprintf(stderr, "Error initializing condition variable\n");
    exit(EXIT_FAILURE);
  }
}

struct Worker_Thread *get_free_worker_thread(struct Worker_Thread *all_worker_threads) {
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (all_worker_threads[i].free) {
      return &all_worker_threads[i];
    }
  }
  return NULL;
}

