#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "common/io.h"
#include "eventlist.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  event_list->num_events++;
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

void ems_show(int fd_response, unsigned int event_id) {
  
  int error = 0;
  size_t num_rows, num_cols;

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    return;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    return;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  num_rows = event->rows;
  num_cols = event->cols;

  if (check_write(fd_response, &error, sizeof(error))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_mutex_unlock(&event->mutex);
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }
  if (check_write(fd_response, &num_rows, sizeof(num_rows))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_mutex_unlock(&event->mutex);
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }
  if (check_write(fd_response, &num_cols, sizeof(num_cols))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_mutex_unlock(&event->mutex);
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  size_t num_seats = event->rows * event->cols;
  unsigned int response_seats[num_seats];
  memset(response_seats, 0, sizeof(response_seats));

  pthread_rwlock_unlock(&event_list->rwl);

  int count = 0;
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      size_t index = seat_index(event, i, j);
      response_seats[count] = event->data[index];
      count++;
    }
  }
  
  if (check_write(fd_response, response_seats, sizeof(response_seats))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_mutex_unlock(&event->mutex);
    return;
  }

  pthread_mutex_unlock(&event->mutex);
  return;
}

void ems_list_events(int fd_response) {

  int error = 0;
  size_t num_events;
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    return ;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    return ;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  if (current == NULL) {
    error = 1;
    if (check_write(fd_response, &error, sizeof(error))) {
      fprintf(stderr, "Error writing to file descriptor\n");
    }
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  error = 0;  
  if (check_write(fd_response, &error, sizeof(error))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  num_events = event_list->num_events;
  if (check_write(fd_response, &num_events, sizeof(num_events))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  unsigned int events_ids[event_list->num_events];
  memset(events_ids, 0, sizeof(events_ids));

  int count = 0;
  while (1) {
    events_ids[count] = (current->event)->id;
    if (current == to) {
      break;
    }
    count++;
    current = current->next;
  }

  if (check_write(fd_response, events_ids, sizeof(events_ids))) {
    fprintf(stderr, "Error writing to file descriptor\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return ;
}

void show_EMS() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  if (current == NULL) {
    fprintf(stdout, "No events\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return;
  }

  while (1) {
    struct Event* event = current->event;
    fprintf(stdout, "Event %u\n", event->id);
    for (size_t i = 0; i < event->rows; i++) {
      for (size_t j = 0; j < event->cols; j++) {
        fprintf(stdout, "%u ", event->data[i * event->cols + j]);
      }
      fprintf(stdout, "\n");
    }
    if (current == to) {
      break;
    }
    current = current->next;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return;
}