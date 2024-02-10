#include "api.h"
#include "common/io.h"

#include <fcntl.h> 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/types.h>

int fd_server_resquest;
int fd_server_response;

int id_session;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  
  char const *path_fifo_server = server_pipe_path;
  char const *path_fifo_request = req_pipe_path;
  char const *path_fifo_response = resp_pipe_path;

  //Create the paths
  size_t len_request =  strlen(req_pipe_path) + 1;
  char  *path_request = malloc(len_request);
  if (path_request == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    return 1;
  }
  snprintf(path_request, len_request, "%s", req_pipe_path);

  size_t len_response = strlen(resp_pipe_path) + 1;
  char *path_response = malloc(len_response);
  if (path_response == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    return 1;
  }
  snprintf(path_response, len_response, "%s", resp_pipe_path);
 
  //Connect to the server pipe
  int fd_register_request = open(path_fifo_server, O_WRONLY);
  if (fd_register_request == -1) {
    fprintf(stderr, "Error opening server pipe\n");
    return 1;
  }

  //Create the pipes
  if(mkfifo(path_request, 0777) == -1){
    fprintf(stderr, "Error creating request pipe\n");
    return 1;
  }
  if(mkfifo(path_response, 0777) == -1){
    fprintf(stderr, "Error creating response pipe\n");
    return 1;
  }

  // Set the message to send to the server
  char buffer_to_server[84] = {'\0'}, buffer_request[41] = {'\0'}, buffer_response[41] = {'\0'};
  buffer_to_server[0] = '1';
  buffer_to_server[1] = '\0';

  strncpy(buffer_request, path_request, sizeof(buffer_request) - 1);
  buffer_request[sizeof(buffer_request) - 1] = '\0';  // Ensure null termination
  if (strlen(buffer_request) < 40) {
    memset(buffer_request + strlen(buffer_request), '\0', 40 - strlen(buffer_request) + 1);
  }

  strncpy(buffer_response, path_response, sizeof(buffer_response) - 1);
  buffer_response[sizeof(buffer_response) - 1] = '\0';  // Ensure null termination
  if (strlen(buffer_response) < 40) {
    memset(buffer_response + strlen(buffer_response), '\0', 40 - strlen(buffer_response) + 1);
  }

  memcpy(buffer_to_server + 2, buffer_request, sizeof(buffer_request) - 1);
  memcpy(buffer_to_server + 42, buffer_response, sizeof(buffer_response) - 1);

  //Send the message to the server
  if (check_write(fd_register_request, buffer_to_server, sizeof(buffer_to_server))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }

  // Close the server pipe
  if (close(fd_register_request) == -1) {
    fprintf(stderr, "Error closing server pipe\n");
    return 1;
  }

  free(path_request);
  free(path_response);
 
  //Open the request pipe
  fd_server_resquest = open(path_fifo_request, O_WRONLY);
  if (fd_server_resquest == -1) {
    fprintf(stderr, "Error opening request pipe1111\n");
    return 1;
  }
  
  //Open the response pipe
  fd_server_response = open(path_fifo_response, O_RDONLY);
  if (fd_server_response == -1) {
    fprintf(stderr, "Error opening response pipe\n");
    return 1;
  }

  //Read the response from the server
  int buffer_from_server[1];
  
  if (check_read(fd_server_response, buffer_from_server, sizeof(buffer_from_server))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }

  id_session = buffer_from_server[0];

  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  char op_code = '2';
  // Make request to server
  if (check_write(fd_server_resquest, &op_code, sizeof(op_code))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  // Close request pipe
  if (close(fd_server_resquest) == -1) {
    fprintf(stderr, "Error closing server pipe\n");
    return 1;
  }
  // Close response pipe
  if (close(fd_server_response) == -1) {
    fprintf(stderr, "Error closing server pipe\n");
    return 1;
  }

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  
  char op_code = '3';

  if (check_write(fd_server_resquest, &op_code, sizeof(op_code))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &event_id, sizeof(event_id))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &num_rows, sizeof(num_rows))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &num_cols, sizeof(num_cols))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  // Read response
  int buffer_from_server[1];
  if (check_read(fd_server_response, buffer_from_server, sizeof(buffer_from_server))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  
  return buffer_from_server[0];
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {

  char op_code = '4';
  size_t buffer_to_server_xs[num_seats];
  size_t buffer_to_server_ys[num_seats];
  memset(buffer_to_server_xs, 0, sizeof(buffer_to_server_xs));
  memset(buffer_to_server_ys, 0, sizeof(buffer_to_server_ys));

  size_t count = 0;
  for (size_t i = 0; i < num_seats; i++) {
    buffer_to_server_xs[count] = xs[i];
    count++;
  }
  count = 0;
  for (size_t i = 0; i < num_seats; i++) {
    buffer_to_server_ys[count] = ys[i];
    count++;
  }
  // Send request
  if (check_write(fd_server_resquest, &op_code, sizeof(op_code))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &event_id, sizeof(event_id))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &num_seats, sizeof(num_seats))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, buffer_to_server_xs, sizeof(buffer_to_server_xs))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, buffer_to_server_ys, sizeof(buffer_to_server_ys))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }

  // Read response
  int buffer_from_server[1];
  if (check_read(fd_server_response, buffer_from_server, sizeof(buffer_from_server))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }

  return buffer_from_server[0];
}

int ems_show(int out_fd, unsigned int event_id) {
  
  char op_code = '5';
  // Send request
  if (check_write(fd_server_resquest, &op_code, sizeof(op_code))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  if (check_write(fd_server_resquest, &event_id, sizeof(event_id))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  
  // Read first response
  int error ;
  size_t num_rows, num_cols;

  if (check_read(fd_server_response, &error, sizeof(error))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  // Check if response is valid
  if (error == 1) {
    return 1;
  }
  if (check_read(fd_server_response, &num_rows, sizeof(num_rows))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  if (check_read(fd_server_response, &num_cols, sizeof(num_cols))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }

  // Read second response
  size_t num_seats = num_rows * num_cols;
  
  unsigned int second_buffer_from_server[num_seats]; 
  memset(second_buffer_from_server, 0, sizeof(second_buffer_from_server));
  
  if (check_read(fd_server_response, second_buffer_from_server, sizeof(second_buffer_from_server))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  
  size_t i = 1;
  while (i <= num_seats) {
    char buffer[16];
    sprintf(buffer, "%d", second_buffer_from_server[i - 1]);
    if (print_str(out_fd, buffer)) {
      fprintf(stderr, "Error writing to output file\n");
      return 1;
    }
    if (i % num_cols == 0) { 
      if (print_str(out_fd, "\n")) {
        fprintf(stderr, "Error writing to output file\n");
      }
    }
    else {
      if (print_str(out_fd, " ")) {
        fprintf(stderr, "Error writing to output file\n");
      } 
    }
    i++;
  }
  return 0 ;
}

int ems_list_events(int out_fd) {

  char op_code = '6';
  // Send request
  if (check_write(fd_server_resquest, &op_code, sizeof(op_code))) {
    fprintf(stderr, "Error writing to server pipe\n");
    return 1;
  }
  // Read first response
  int error;
  if (check_read(fd_server_response, &error, sizeof(error))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  // Check if response is valid
  if (error == 1) {
    return 1;
  }
  // Read second response
  size_t num_events;
  if (check_read(fd_server_response, &num_events, sizeof(num_events))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }
  
  // Read second response
  unsigned int second_buffer_from_server[num_events];
  memset(second_buffer_from_server, 0, sizeof(second_buffer_from_server));

  if (check_read(fd_server_response, second_buffer_from_server, sizeof(second_buffer_from_server))) {
    fprintf(stderr, "Error reading from server pipe\n");
    return 1;
  }

  // Write response to out_fd
  for ( size_t i = 0; i < num_events; i++) {
    if (print_str(out_fd, "Event: ")) {
      fprintf(stderr, "Error writing to output file\n");
      return 1;
    }
    char buffer[16];
    sprintf(buffer, "%d", second_buffer_from_server[i]);
    if (print_str(out_fd, buffer)) {
      fprintf(stderr, "Error writing to output file\n");
      return 1;
    }
    if (print_str(out_fd, "\n")) {
      fprintf(stderr, "Error writing to output file\n");
      return 1;
    }
  }
  return 0;
}