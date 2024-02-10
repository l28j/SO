/// Redirects the standard input and output to the given file descriptors or vice versa.
/// @param fd_input File descriptor to read the commands from.
/// @param fd_output File descriptor to write the output to.
/// @param saved_stdin Standard input file descriptor.
/// @param saved_stdout Standard output file descriptor.
/// @param flag Determines whether to redirect the standard input and output to the given file descriptors or vice versa.
void redirectStdinStdout(int fd_input, int fd_output, int saved_stdin, int saved_stdout, char *flag);
