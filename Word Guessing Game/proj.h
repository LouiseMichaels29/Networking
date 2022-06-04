#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#ifdef DEBUG_FLAG
#define DEBUG_wARG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define DEBUG_MSG(msg) fprintf(stderr, "%s\n", msg);
#else
#define DEBUG_wARG(fmt, ...)
#define DEBUG_MSG(msg)
#endif

#define PRINT_wARG(fmt, ...) printf(fmt, __VA_ARGS__); fflush(stdout)
#define PRINT_MSG(msg) printf("%s", msg); fflush(stdout)

#define BUFFER_SIZE 1024

int read_stdin(char *buf, int buf_len, int *more);

/* ========= DO NOT modify anything above this. Add your code below this line. =============== */
