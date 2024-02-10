CC = gcc
DIR_JOBS = jobs

# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -pthread\
		 -Wall -Werror -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -fsanitize=address -fsanitize=undefined

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: ems

ems: main2.c constants.h operations.c ems_operations.o parser.o eventlist.o 
	$(CC) $(CFLAGS) $(SLEEP) -o ems main2.c operations.c ems_operations.o parser.o eventlist.o 

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

run: ems
	@./ems

clean:
	rm -f *.o ems 
	rm -f $(DIR_JOBS)/*.out

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i *.c *.h
