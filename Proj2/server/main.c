#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h> 
#include <stdbool.h>
#include <signal.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "clients_manager.h"

pthread_mutex_t mutex_whorker_threads ;

sem_t semaphore_sessions;

struct Worker_Thread all_worker_threads[MAX_SESSION_COUNT];

int wait_signal = 1 ;

pthread_cond_t signal_cond ;
pthread_t signal_thread;

static void sig_handler(int sig) {
  if (sig == SIGUSR1) {
    pthread_cond_signal(&signal_cond);

    if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
      exit(EXIT_FAILURE);
    }
  }
}

void *wait_for_signal() {   //Thread to wait for signal SIGUSR1 to show EMS using block waiting 
  sigset_t mask, og_mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);

  if (pthread_sigmask(SIG_BLOCK, &mask, &og_mask) != 0) {
    fprintf(stderr, "Error masking SIGUSR1\n");
    return NULL;
  }
  
  while (wait_signal) {                
    pthread_cond_wait(&signal_cond, &all_worker_threads[0].mutex);
    pthread_mutex_lock(&mutex_whorker_threads);
    show_EMS();
    pthread_mutex_unlock(&mutex_whorker_threads);
  }
  
  if (pthread_sigmask(SIG_SETMASK, &og_mask, NULL) != 0) {
    fprintf(stderr, "Error unmasking SIGUSR1\n");
    return NULL;
  }
  exit(EXIT_SUCCESS);
}
  
void *worker_thread(void *arg) {
  
  sigset_t mask, og_mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);

  if (pthread_sigmask(SIG_BLOCK, &mask, &og_mask) != 0) {
    perror("Error masking SIGUSR1");
    return NULL;
  }

  struct Worker_Thread *worker_thread = (struct Worker_Thread *)arg;

  while (1) {
    
    pthread_cond_wait(&worker_thread->start_cond, &worker_thread->mutex);
    sleep(3);
    int reset = 0;
    // Open request pipe
    int fd_request = open(worker_thread->path_request, O_RDONLY);
    if (fd_request == -1) {
      fprintf(stderr, "Error opening request pipe\n");
      exit(EXIT_FAILURE);
    } 
    // Open response pipe
    int fd_response = open(worker_thread->path_response, O_WRONLY);
    if (fd_response == -1) {
      fprintf(stderr, "Error opening response pipe\n");
      exit(EXIT_FAILURE);
    }
    // Send id_session to client
    if (check_write(fd_response, &worker_thread->id_session, sizeof(int))) {
      fprintf(stderr, "Error writing to response pipe\n");
      exit(EXIT_FAILURE);
    }
    
    while (!reset) {
  
      char char_op_code ;
      if (check_read(fd_request, &char_op_code, sizeof(char_op_code))) {
        fprintf(stderr, "Error reading from request pipe\n");
        exit(EXIT_FAILURE);
      }       

      int op_code = char_op_code - '0';
      int return_type = 0;
      unsigned int event_id;
    
      switch (op_code) {
        case 2 : //ems_quit 
          // Close request and response pipes
          if (close(fd_request) == -1) {
            fprintf(stderr, "Error closing request pipe\n");
            exit(EXIT_FAILURE);
          }
          if (close(fd_response) == -1) {
            fprintf(stderr, "Error closing response pipe\n");
            exit(EXIT_FAILURE);
          }

          if (unlink(worker_thread->path_request) == -1) {
            fprintf(stderr, "Error unlinking request pipe\n");
            perror("Error unlinking request pipe");
            exit(EXIT_FAILURE);
          }
          if (unlink(worker_thread->path_response) == -1) {
            fprintf(stderr, "Error unlinking response pipe\n");
            perror("Error unlinking response pipe");
            exit(EXIT_FAILURE);
          }
          
          // Reset session
          reset_WorkerThread(worker_thread);

          if (sem_post(&semaphore_sessions) == -1) {
            fprintf(stderr, "Error posting semaphore\n");
            continue;
          }
          reset = 1;
          break;

        case 3 : //ems_create
          size_t num_rows ;
          size_t num_cols ;

          if (check_read(fd_request, &event_id, sizeof(event_id))) {
            fprintf(stderr, "Error reading from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if (check_read(fd_request, &num_rows, sizeof(num_rows))) {
            fprintf(stderr, "Error reading from request pipe\n");
            exit(EXIT_FAILURE);
          }
          if (check_read(fd_request, &num_cols, sizeof(num_cols))) {
            fprintf(stderr, "Error reading from request pipe\n");
            exit(EXIT_FAILURE);
          }
          
          return_type = ems_create(event_id, num_rows, num_cols);
    
          if (check_write(fd_response, &return_type, sizeof(return_type))) {
            fprintf(stderr, "Error writing to response pipe\n");
            exit(EXIT_FAILURE);
          }
          break;

        case 4 : //reserve    
          size_t num_seats ;
          size_t xs[MAX_RESERVATION_SIZE] = {0};
          size_t ys[MAX_RESERVATION_SIZE] = {0};
          if (check_read(fd_request, &event_id, sizeof(event_id))) {
            fprintf(stderr, "Error reading from request pipe1\n");
            exit(EXIT_FAILURE);
          }
          if (check_read(fd_request, &num_seats, sizeof(num_seats))) {
            fprintf(stderr, "Error reading from request pipe2\n");
            exit(EXIT_FAILURE);
          }
          if (check_read(fd_request, &xs, sizeof(size_t) * num_seats)) {
            fprintf(stderr, "Error reading from request pipe3\n");
            exit(EXIT_FAILURE);
          }
          if (check_read(fd_request, &ys, sizeof(size_t) * num_seats)) {
            fprintf(stderr, "Error reading from request pipe4\n");
            exit(EXIT_FAILURE);
          }

          return_type = ems_reserve(event_id, num_seats, xs, ys);
          
          // Send response
          if (check_write(fd_response, &return_type, sizeof(return_type))) {
            fprintf(stderr, "Error writing to response pipe\n");
            exit(EXIT_FAILURE);
          }
          break;

        case 5 : //show
          if (check_read(fd_request, &event_id, sizeof(event_id))) {
            fprintf(stderr, "Error reading from request pipe\n");
            exit(EXIT_FAILURE);
          }
          ems_show(fd_response, event_id);
          break;

        case 6 : //ems_list
          ems_list_events(fd_response);
          break;
      }
    }
  }
  if (pthread_sigmask(SIG_SETMASK, &og_mask, NULL) != 0) {
    perror("pthread_sigmask");
    return NULL;
  }
  exit(EXIT_SUCCESS); 
}

void *product_consumer_queue(void *arg) {
  
  int get_pid = getpid();
  printf("PID of the process server: %d\n", get_pid);

  if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
    exit(EXIT_FAILURE);
  }
  
  if (pthread_cond_init(&signal_cond, NULL) != 0) {
    fprintf(stderr, "Failed to initialize condition variable of signal\n");
    return NULL;
  }

  //Initialize signal thread
  if (pthread_create(&signal_thread, NULL, wait_for_signal, NULL) != 0) {
    fprintf(stderr, "Failed to create thread\n");
    return NULL;
  }

  if (pthread_mutex_init(&mutex_whorker_threads, NULL) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    return NULL;
  }
 //Initialize all worker threads
  set_list_WorkerThreads(all_worker_threads);

  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&all_worker_threads[i].thread, NULL, worker_thread, &all_worker_threads[i]) != 0) {
      fprintf(stderr, "Failed to create thread\n");
      return NULL;
    }
  }
  char *path_register_FIFO = (char *)arg;
  // Initialize semaphore
  if (sem_init(&semaphore_sessions, 0, MAX_SESSION_COUNT) == -1) {
    fprintf(stderr, "Failed to initialize semaphore\n");
    return NULL;  
  }  
  // Open register pipe
  int fd_register = open(path_register_FIFO, O_RDWR);
  if (fd_register == -1) {
    fprintf(stderr, "Error opening register pipe\n");
    return NULL;
  }
  
  while(1) {

    // Read request from client
    char buffer_request[84] = {'\0'};
    ssize_t bytes_read = read(fd_register, buffer_request, sizeof(buffer_request));
    if (bytes_read == -1) {
      fprintf(stderr, "Error reading from register pipe\n");
      return NULL;
    }
                                          
    // Wait for new session
    sem_wait(&semaphore_sessions);
    //Get free session 
    struct Worker_Thread *free_worker_thread = get_free_worker_thread(all_worker_threads);
    
    char client_request[41], client_response[41];
    strncpy(client_request, buffer_request + 2, 40);
    client_request[41] = '\0';  
    strncpy(client_response, buffer_request + 42, 40);
    client_response[41] = '\0';
    //Set session as busy
    if (free_worker_thread->free) {
      memcpy(free_worker_thread->path_request, client_request, sizeof(client_request));
      memcpy(free_worker_thread->path_response, client_response, sizeof(client_response));
    }
    free_worker_thread->free = 0;
    //Unlock session
    pthread_cond_signal(&free_worker_thread->start_cond);
  
  }

  //Close register and response pipes
  if (close(fd_register) == -1) {
    fprintf(stderr, "Error closing register pipe\n");
    return NULL;
  }
  
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);
    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }
    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  char* path_register_FIFO = malloc(strlen(argv[1]) + 1);
  if (path_register_FIFO == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return 1;
  }
  strcpy(path_register_FIFO, argv[1]);
  //Create register FIFO to receive requests to register new clients
  if (mkfifo(path_register_FIFO, 0777) == -1) {
    fprintf(stderr, "Failed to create register FIFO\n");
    return 1;
  }   

  //Start the main thread to wait for new clients
  pthread_t requests_SESSIONS ;     
  char *arg = malloc(strlen(path_register_FIFO) + 1);
  if (arg == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return 1;
  }
  strcpy(arg, path_register_FIFO);

  if (pthread_create(&requests_SESSIONS, NULL, product_consumer_queue, arg)) {
    fprintf(stderr, "Failed to create thread\n");
    return 1;
  }

  if (pthread_join(requests_SESSIONS, NULL)) {
    fprintf(stderr, "Failed to join thread\n");
    return 1;
  }

  ems_terminate();
  if (unlink(path_register_FIFO) == -1) {
    fprintf(stderr, "Failed to unlink register FIFO\n");
    return 1;
  }
  free(arg);
  free(path_register_FIFO);
}