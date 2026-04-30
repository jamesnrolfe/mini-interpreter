#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>  // for open and file control
#include <unistd.h> // for read and close

#define int long long // work with 64bit target

int token;           // current token
char *src, *old_src; // pointer to the source code string
int poolsize;        // default size of text/data/stack
int line;            // line number

int *text,     // text segment
    *old_text, // for dump text segment
    *stack;    // stack
char *data;    // data segment

void next() {
    token = *src++;
    return;
}

void expression(int level) {
    // do nothing
}

void program() {
    next(); // get the next token, which saves to global variable
    while (token > 0) {
        printf("token is: %c\n", (char)token);
        next();
    }
}

int eval() {
    return 0; // do nothing yet
}

/* Allocate memory and read a file to src. */
signed read_file_to_buffer(int fd, int poolsize) {
    int i;

    // if malloc return null pointer fail
    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%lld) for source area\n", poolsize);
        return -1;
    }

    // read source file, returns num bytes read if success
    if ((i = read(fd, src, poolsize - 1)) <= 0) {
        printf("read() returned %lld\n", i);
        return -1;
    }

    src[i] = 0;
    return 1;
}

/* Allocate memory for the text, data and stack area, of size `poolsize`. */
signed allocate_virtual_memory(int poolsize) {
    if (!(text = old_text = malloc(poolsize))) {
        printf("could not malloc(%lld) for text area\n", poolsize);
        return -1;
    }

    if (!(data = malloc(poolsize))) {
        printf("could not malloc(%lld) for data area", poolsize);
        return -1;
    }

    if (!(stack = malloc(poolsize))) {
        printf("could not malloc(%lld) for stack area", poolsize);
        return -1;
    }

    return 1;
}

signed main(signed argc, char **argv) {
    int fd;

    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitrary
    line = 1;

    // try and open the file
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    if (read_file_to_buffer(fd, poolsize) < 0) {
        printf("file read failure\n");
        return -1;
    }

    close(fd);

    // allocate memory for vm
    if (allocate_virtual_memory(poolsize) < 0) {
        printf("memory allocation error\n");
        return -1;
    }

    program();
    return eval();
}
