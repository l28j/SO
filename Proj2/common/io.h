#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <stddef.h>

/// Parses an unsigned integer from the given file descriptor.
/// @param fd The file descriptor to read from.
/// @param value Pointer to the variable to store the value in.
/// @param next Pointer to the variable to store the next character in.
/// @return 0 if the integer was read successfully, 1 otherwise.
int parse_uint(int fd, unsigned int *value, char *next);

/// Prints an unsigned integer to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param value The value to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int print_uint(int fd, unsigned int value);

/// Writes a string to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param str The string to write.
/// @return 0 if the string was written successfully, 1 otherwise.
int print_str(int fd, const char *str);

/// Try to wirte to a file descriptor.
/// @param fd The file descriptor to write to.
/// @param buf The buffer to write.
/// @param count The number of bytes to write.
int check_write(int fd, const void *buf, size_t count);

/// Try to read from a file descriptor.
/// @param fd The file descriptor to read from.
/// @param buf The buffer to read into.
/// @param count The number of bytes to read.
int check_read(int fd, void *buf, size_t count);

/// Add a value to a buffer.
/// @param buffer The buffer to add the value to.
/// @param cursor The current cursor position in the buffer.
/// @param size The size of the value to add.
/// @param value The value to add.
size_t add_to_buffer(void *buffer, size_t cursor, size_t size, const void *value);

#endif  // COMMON_IO_H
