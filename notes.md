# Notes

Notes based on the content from [this repo](https://github.com/lotabout/write-a-C-interpreter/blob/master/tutorial/en/1-Skeleton.md).

## Structure

We are building an interpreter, and refer to it as both the interpreter and compiler. 

There are three phases:

1. **Lexical analysis:** convert a source string into a stream of tokens.
2. **Parsing:** consume token stream and construct a syntax tree.
3. **Code generation:** walk through the syntax tree and generate code for a target platform.

### Skeleton of the compiler

The compiler will contain four main functions:

1. `next()` for lexical analysis; get the next token, and ignore spaces, tabs etc.
2. `program()` main entrance for the parser.
3. `expression(level)`: parser expression; level explained later
4. `eval()`: the entrance for the virtual machine; to interpret target instructions

This looks like the following:

```c
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

signed main(signed argc, char **argv) {
    int i, fd;

    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitrary
    line = 1;

    // try and open the file
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

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

    src[i] = 0; // add EOF char
    close(fd);

    program();
    return eval();
}
```

This has been slightly modified from the tutorial to keep up with modern standards, but the behaviour is identical.

## Virtual Machines

We now build a VM and design an instruction set to run on such a VM.

### How does a computer work internally?

We care about CPU, registers and memory. Code (or assembly instructions) are stored in memory as binary data: the CPU will retreive this information one by one and execute them. The running states of the machine is stored in registers.

#### Memory

Memory can be used to store data. By data we mean code. All stored in binary.

Modern OS's have *virtual memory* which maps memory addresses used by a program called *virtual addresses* into physical addresses in computer memory.

The benefit of this is that it can hide the details of physical memory from the programs. For example, in a 32bit machine, all the available memory addresses are `2^32 = 4G` while actual physical memory may be much smaller. 

The programs usable memory is partitioned into several segments:

1. `text`: for storing code (instructions)
2. `data`: for storing initialised data e.g. `int i = 10;` will need to use this segment
3. `bss`: for storing uninitialised data e.g. `int i[1000];` doesn't need to occupy `1000*4` bytes, because the actual values in the array don't matter, so we can store them here to save space 
4. `stack`: used for handling the states of function called, such as calling frames and local variables of a function
5. `heap`: used to allocate memory dynamically for the program

An example layout is

```
+------------------+
|    stack   |     |      high address
|    ...     v     |
|                  |
|                  |
|                  |
|                  |
|    ...     ^     |
|    heap    |     |
+------------------+
| bss  segment     |
+------------------+
| data segment     |
+------------------+
| text segment     |      low address
+------------------+
```

Our virtual machine will be very simple:
- We dont give a shit about `bss` or `heap`. 
- Our interpreter won't support the initialisation of data, so we can merge the `data` and `bss` segments.
- We only use `data` for storing string literals.

> We can feel justified in dropping the heap because our computer is running this VM program, which has given it a heap itself. In this sense, we have one anyway, but generally we would need to include it.

We will add the following code to the global area:

```c
int *text,          // text segment
    *old_text,      // for dump text segment
    *stack;         // stack 
char *data;         // data segment
```

> Note the `int` here - we should actually use `unsigned` because we store unsigned data, like pointers, in the `text` segment. Since we want to bootstrap our interpreter, so we don't want to introduce unsigned. Finally, the `data` is `char *` because we use it to store string literals only.

> **Bootstrapping**: a compiler that compiles itself.

So now we can allocate this memory. We will turn this into a function called `allocate_virtual_memory(int poolsize)`:

```c
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
```


