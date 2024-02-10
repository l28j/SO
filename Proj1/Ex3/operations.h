// Arguments that are passed to the threads to hold information needed to execute the operations.
struct ThreadArgs {
    int fd_input, fd_output, return_value, current_thread_id;
    unsigned int event_id, delay, thread_id, max_threads;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    struct Pthread *pthread_list;
};

// Auxiliar structure to hold information about the threads.
struct Pthread{
    int id ;
    pthread_t *thread;   
    unsigned int wait ;
};

/// Redirects the standard input and output to the given file descriptors or vice versa.
/// @param fd_input File descriptor to read the commands from.
/// @param fd_output File descriptor to write the output to.
/// @param saved_stdin Standard input file descriptor.
/// @param saved_stdout Standard output file descriptor.
/// @param flag Determines whether to redirect the standard input and output to the given file descriptors or vice versa.
void redirectStdinStdout(int fd_input, int fd_output, int saved_stdin, int saved_stdout, char *flag);

/// Set the list of threads.
/// @param Pthread_list List of threads.
/// @param max_threads Maximum number of threads.
void set_List_Pthreads(struct Pthread *Pthread_list, unsigned int max_threads);

/// Free the list of threads.
/// @param Pthread_list  List of threads.
/// @param max_threads  Maximum number of threads.
void free_list_Pthreads(struct Pthread *Pthread_list, unsigned int max_threads);

/// Get the index on the list of threads of the thread with the given id.
/// @param Pthread_list List of threads.
/// @param max_threads Maximum number of threads.
/// @param id Thread id.
/// @return Index of the thread with the given id.
unsigned int get_index_thread(struct Pthread *Pthread_list, unsigned int max_threads, unsigned int *id);

/// Try to unlock the given mutex.
/// @param mutex Mutex to unlock.
void try_unlock_mutex(pthread_mutex_t *mutex);

/// Try to unlock the given mutex.
/// @param mutex Mutex to unlock.
void try_lock_mutex(pthread_mutex_t *mutex);

///Try to unlock the given read write mutex.
/// @param mutex Read write mutex to unlock.
void try_unlock_rwmutex(pthread_rwlock_t *mutex);

/// Try to lock the given mutex.
/// @param mutex Mutex to lock.
void try_lock_rwmutex(pthread_rwlock_t *mutex); 

/// Try to lock the given mutex.
/// @param mutex Mutex to lock.
void try_lock_readmutex(pthread_rwlock_t *mutex);

