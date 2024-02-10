#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#include "pthread.h"
#include "eventlist.h"
#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "ems_operations.h"

static struct EventList* event_list = NULL; // List of events
static unsigned int state_access_delay_ms = 0; // Delay to simulate a real system accessing a costly memory resource

int BARRIER_FLAG = 1; // Flag to control the barrier

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->Data[index].place;
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }
  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}
/// Terminates the EMS state.
/// @return  0 if successful, 1 otherwise.
int ems_terminate() {

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  event_list = NULL;
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {

  try_lock_rwmutex(&rwlock);

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->Data = malloc(sizeof(struct Data) * num_rows * num_cols);

  if (event->Data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->Data[i].place = 0;
    if ( pthread_mutex_init(&event->Data[i].mutex, NULL) != 0) {
      fprintf(stderr, "Error initializing mutex\n");
      free(event->Data);
      free(event);
      try_unlock_rwmutex(&rwlock);
      return 1;
    }
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->Data);
    free(event);
    try_unlock_rwmutex(&rwlock);
    return 1;
  }
  try_unlock_rwmutex(&rwlock);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  
  try_lock_rwmutex(&rwlock);
  
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    try_unlock_rwmutex(&rwlock);
    return 1; 
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    try_lock_mutex(&event->Data[seat_index(event, row, col)].mutex);
    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      try_unlock_mutex(&event->Data[seat_index(event, row, col)].mutex);
      break;
    }
    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
    try_unlock_mutex(&event->Data[seat_index(event, row, col)].mutex);
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    try_unlock_rwmutex(&rwlock);
    return 1;
  }
  try_unlock_rwmutex(&rwlock);
  return 0;
}

int ems_show(unsigned int event_id, int fd_output) {
  
  try_lock_rwmutex(&rwlock);
  
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_rwlock_unlock(&rwlock);
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

    for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {

      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      char* seat_str = malloc(sizeof(char) * 2);
      sprintf(seat_str, "%u", *seat);
      write(fd_output, seat_str, strlen(seat_str));
      free(seat_str);

      if (j < event->cols) {
        write(fd_output, " ", 1);
      }
    }

    write(fd_output, "\n", 1);
    fflush(stdout);
  }
  try_unlock_rwmutex(&rwlock);
  return 0;
}

int ems_list_events(int fd_output) {
  
  try_lock_rwmutex(&rwlock);
  
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    try_unlock_rwmutex(&rwlock);
    return 1;
  }

  if (event_list->head == NULL) {
    fprintf(stderr,"No events\n");
    try_unlock_rwmutex(&rwlock);
    return 0;
  }

  struct ListNode* current = event_list->head;
  while (current != NULL) {
  
    write(fd_output, "Event: ", 7);
    
    char *id = malloc(sizeof(char)*10);
    sprintf(id, "%u\n", (current->event)->id);
    write(fd_output, id, strlen(id));
    free(id);
    
    current = current->next;
  }

  try_unlock_rwmutex(&rwlock);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

void ems_process_with_threads(int fd_input, int fd_output, unsigned int num_threads) {

  struct Pthread threads[num_threads]; 
  int id_thread = 1;
  struct ThreadArgs *returnArgs;
  int exitFlag = 1;

  while (1) {
    // Initialize thread list
    set_List_Pthreads(threads, num_threads);
    BARRIER_FLAG = 1;

    for (unsigned int i = 0; i < num_threads; i++) {
      // Set arguments to be used by the thread
      struct ThreadArgs *args = (struct ThreadArgs*) malloc(sizeof(struct ThreadArgs));
      args->fd_input = fd_input;
      args->fd_output = fd_output;
      args->pthread_list = threads;
      args->max_threads = num_threads;
      args->current_thread_id = id_thread;
      // Assign thread id and wait time
      threads[i].id = id_thread;
      threads[i].wait = 0;
      // Create threads and assign them to the thread list
      if (pthread_create(threads[i].thread, NULL, &ems_process_thread, (void*)args) != 0) {
        perror("Error creating thread");
        exit(EXIT_FAILURE);
      }
      id_thread++;
    }
    // Wait for threads to finish
    for (unsigned int i = 0; i < num_threads; i++) {
      // Wait for threads to finish
      if (pthread_join(*threads[i].thread, (void**) &returnArgs) != 0) {
        perror("Error joining thread");
        exit(EXIT_FAILURE);
      }
      if (returnArgs->return_value == 0) { // EOC
        free(returnArgs);
        exitFlag = 0;
      } 
      else if (returnArgs->return_value == 1) { free(returnArgs); } // BARRIER
    }
    // Free thread list
    free_list_Pthreads(threads, num_threads);
    //Teminate EMS if EOC
    if (exitFlag == 0) {
      ems_terminate();
      break; 
    }
  }
}

void * ems_process_thread(void *arg) {
  
  struct ThreadArgs *args = (struct ThreadArgs*) arg;
  enum Command cmd;

  while (1) {

    try_lock_readmutex(&rwlock);
    if (BARRIER_FLAG == 0) {
      try_unlock_rwmutex(&rwlock);  //Verify if the barrier flag is set
      args->return_value = 1;            //Stop thread
      pthread_exit((void*) args);
    }
    try_unlock_rwmutex(&rwlock);

    // Verify if the thread has to wait before processing the next command
    unsigned int current_thread_index = get_index_thread(args->pthread_list, args->max_threads, (unsigned int *)&args->current_thread_id);
    if (args->pthread_list[current_thread_index].wait > 0) {
      ems_wait(args->pthread_list[current_thread_index].wait);
      args->pthread_list[current_thread_index].wait = 0;   //Reset wait time
    }

    try_lock_mutex(&mutex);

    // Read next command
    while ((cmd = get_next(args->fd_input)) != 10) {

      switch (cmd) {

        case CMD_CREATE:
          // Process create command
          if (parse_create(args->fd_input, &args->event_id, &args->num_rows, &args->num_columns) != 0) {
            fprintf(stderr, "Invalid CREATE command. See HELP for usage\n");
            //continue;
          }
          try_unlock_mutex(&mutex);
          if (ems_create(args->event_id, args->num_rows, args->num_columns)) {
            fprintf(stderr, "Failed to create event\n");
          }
          break;

        case CMD_RESERVE:
          // Process reserve command
          args->num_coords = parse_reserve(args->fd_input, MAX_RESERVATION_SIZE, &args->event_id, args->xs, args->ys);
          try_unlock_mutex(&mutex);
          if (args->num_coords == 0) {
            fprintf(stderr, "Invalid RESERVE command. See HELP for usage\n");
            //continue;
          }
          if (ems_reserve(args->event_id, args->num_coords, args->xs, args->ys)) {
            fprintf(stderr, "Failed to reserve seats\n");
          }
          break;

        case CMD_SHOW:
          // Process show command
          if (parse_show(args->fd_input, &args->event_id) != 0) {
            fprintf(stderr, "Invalid SHOW command. See HELP for usage\n");
            //continue;
          }
          try_unlock_mutex(&mutex);
          if (ems_show(args->event_id, args->fd_output)) {
            fprintf(stderr, "Failed to show event\n");
          }
          break;

        case CMD_LIST_EVENTS:
          // Process list events command
          try_unlock_mutex(&mutex);
          if (ems_list_events(args->fd_output)) {
            fprintf(stderr, "Failed to list events\n");
          }
          break;

        case CMD_WAIT:
          // Process wait command
          unsigned int id_thread = 0;
          if (parse_wait(args->fd_input, &args->delay, &id_thread) == -1) {
            fprintf(stderr, "Invalid WAIT command. See HELP for usage\n");
            //continue;
          }
          try_unlock_mutex(&mutex);
          // If the thread id is 0, block all threads and wait to restart
          if (args->delay > 0 && id_thread == 0 ) {
            try_lock_mutex(&mutex);
            write(args->fd_output,"Waiting...\n", 11);
            ems_wait(args->delay);
            try_unlock_mutex(&mutex);
          }
          // If the thread id is not 0, set the wait time for the thread with the given id
          else if (args->delay > 0 && id_thread > 0) {
            unsigned int index = get_index_thread(args->pthread_list, args->max_threads, &id_thread);
            if (index == 0 ) {
              fprintf(stderr, "Invalid thread id\n");
              break;
            }
            args->pthread_list[index].wait = args->delay;
          }
          break;

        case CMD_INVALID:
          // Invalid command
          try_unlock_mutex(&mutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;

        case CMD_HELP:
          // Display help information
          try_unlock_mutex(&mutex); 
          fprintf(stderr,
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n"
              "  BARRIER\n"
              "  HELP\n");
          break;

        case CMD_BARRIER:
          // Wait all threads before the barrier to finish and reset all threads after the barrier
          try_lock_rwmutex(&rwlock);
          BARRIER_FLAG = 0;
          try_unlock_rwmutex(&rwlock);
          args->return_value = 1;
          try_unlock_mutex(&mutex); 
          pthread_exit((void*) args);

        case CMD_EMPTY:
          try_unlock_mutex(&mutex);
          break;

        case EOC:
          try_unlock_mutex(&mutex);
          args->return_value = 0;
          pthread_exit((void*) args);
      }
      break;
    }
  }
}
