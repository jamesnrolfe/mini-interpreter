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

int *pc, *bp, *sp, ax, cycle; // virtual machine registers

// instructions
enum {
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

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
    int op, *tmp;
    while (1) {
        op = *pc++; // get the next operation code
        if (op == IMM) {
            // load immediate value to ax
            ax = *pc++;
        } else if (op == LC) {
            // load character to ax, address in ax
            ax = *(char *)ax;
        } else if (op == LI) {
            // load integer to ax, address in ax
            ax = *(int *)ax;
        } else if (op == SC) {
            // save char to address, value in ax, address on stack
            ax = *(char *)*sp++ = ax;
        } else if (op == SI) {
            // save integer to address, value in ax, address on stack
            *(int *)*sp++ = ax;
        } else if (op == PUSH) {
            // push value in ax onto stack
            *--sp = ax;
        } else if (op == JMP) {
            // unconditionally jump to <addr>
            pc = (int *)*pc;
        }
    }

    return 0;
}

/* Allocate memory and read a file to src. */
signed read_file_to_buffer(int fd, int poolsize) {
    int i;

    // if calloc return null pointer fail
    if (!(src = old_src = calloc(poolsize))) {
        printf("could not calloc(%lld) for source area\n", poolsize);
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
    if (!(text = old_text = calloc(poolsize))) {
        printf("could not calloc(%lld) for text area\n", poolsize);
        return -1;
    }

    if (!(data = calloc(poolsize))) {
        printf("could not calloc(%lld) for data area", poolsize);
        return -1;
    }

    if (!(stack = calloc(poolsize))) {
        printf("could not calloc(%lld) for stack area", poolsize);
        return -1;
    }

    return 1;
}

/* Initialise registers with base values. */
void init_registers(int poolsize) {
    // base pointer and stack pointer both initially point to top of stack
    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;
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
    init_registers(poolsize);

    program();
    return eval();
}
