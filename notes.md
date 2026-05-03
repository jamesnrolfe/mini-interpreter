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

We have an issue here where we have `malloc`ed some space for this stuff, but the computer has given us no guarantee that this is empty - it has just found a load of space for us to use that isn't being used by anything else. Previous data could be left here.

There are two ways to deal with this:
1. `malloc() + memset()`: We can allocate space with `malloc` and then wipe it with `memset`.
2. `calloc()`: We could just use `calloc` in place of `malloc`, which does the previous step in one function call. We will use this here, and so all `malloc`s will now become `calloc`s.

#### Registers

Registers store the running state of computers. Our VM will use 4:

1. `PC`: Program counter - stores a memory address that holds the **next** instruction to run.
2. `SP`: Stack pointer - always points to the *top* of the stack. Notice that the stack grows from high adress to low address so that when we push a new element to the stack, `SP` decreases.
3. `BP`: Base pointer - points to some elements in the stack, used for funciton calls.
4. `AX`: A general register used to store the result of an instruction.

Let's add some code to the global area:

```c
int *pc, *bp, *sp, ax, cycle; // virtual machine registers
```

and create a function to initialise them:

```c
void init_registers(int poolsize) {
    // base pointer and stack pointer both initially point to top of stack
    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;
}
```
`PC` should point to the `main` function of the program to be interpreted, but we don't have code generation yet, so we can skip.

### Instruction Set

The instruction set is a list of instructions our CPU can understand. We will design based on a simplified version of the x86 instructions set.

We start by adding an ENUM with the following instructions. 

```c
// instructions
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };
```

Let's go through some of these.

#### MOV

`MOV` moves data into registers or memory, sort of like assignment in C. There are two arguments in x86's `MOV`: `MOV dest source`. `source` can be a number, a register or a memory address.

We won't follow x86 exactly. On the one hand, our VM has only one general register `AX`, and on the other hand it is hard to determine the type of the arguments (is it a number, a reg or a MA?), so we tear `MOV` into 5 pieces:

1. `IMM <num>`: put immediate `<num>` into register `AX`.
2. `LC`: load character into `AX` from a memory address which is stored in `AX` before execution.
3. `LI`: like `LC` but with integer.
4. `SC`: load the character in `AX` into the memory whose address is stored on the top of the stack.
5. `SI`: like `SC` but with integer.

By separating this, we reduce the complexity a lot. Let's implement this in `eval`:

```c
void eval() {
    int op, *tmp;
    while (1) {
        op = *pc++; // get next operation code
        if (op == IMM)       {ax = *pc++;}                                     // load immediate value to ax
        else if (op == LC)   {ax = *(char *)ax;}                               // load character to ax, address in ax
        else if (op == LI)   {ax = *(int *)ax;}                                // load integer to ax, address in ax
        else if (op == SC)   {ax = *(char *)*sp++ = ax;}                       // save character to address, value in ax, address on stack
        else if (op == SI)   {*(int *)*sp++ = ax;}                             // save integer to address, value in ax, address on stack
    }

    ...
    return 0;
}
```

##### Syntax notes

So this syntax is hard to follow. Let's go through whats happening here.
```c
op = *pc++
```

is a common idiom in C. It does two things:

1. `*pc` (dereference): It looks at the memory address `pc` is currently pointing to, and reads the value stored there. This is saved into `op`.
2. `++` (*post*-increment): It then moves the `pc` pointer forward to the next memory location.

> If the `++` came before, e.g. `++*pc` it would increment first, and then read.

Then, for our `IMM` instruction, we effectively do the exact same thing, loading the value into `ax`.

Then, for `LC` (load character), we have 
```c
ax = *(char *)ax;
```

At this point, `ax` contains a number, and we want to treat that number as a memory address.
1. `(char *)ax` (typecast): This tells the compiler to pretend `ax` is a pointer to a single character (1B). 
2. `*(...)` (dereference): Go to the memory address we just made, and read the character stored there.
3. `ax = ...`: save that character back to `ax`.

`LI` is clearly very similar.

`SC` is more complicated. We have 
```c
ax = *(char *)*sp++ = ax;
```
1. `*sp++`: just like `*pc++`, dereference and post-increment. This reads the value off the top of the stack, and moves the stack pointer down (effectively popping a character off the stack). In this VM design, the value popped off is the target memory address. Let's call this `TARGET_ADDRESS`.
2. `(char *)TARGET_ADDRESS`: cast that popped address to a character pointer.
3. `*(char *)TARGET_ADDRESS = ax`: dereference the target address and write the value of `ax` into it. Because we cast it as a `char *`, we only are able to write 1 byte to memory.
4. `ax = ...`: In C, `A = B = C` means assign C to B, *then* assign B to A. Here, the VM writes `ax` to memory, then assigns `ax` back to itself (which does nothing, but ensures `ax` holds the correct value going forward).

`SI` is very similar, but doesn't need to reassign `ax` at the end.

#### PUSH

`PUSH` can push an immediate value or a registers value into the stack. Here, `PUSH` pushes the value in `ax` onto the stack.

```c
else if (op = PUSH) {*--sp = ax;} // push value of ax onto stack
```

Note that we now use *pre*-incrementation, to move to the place to push `ax` to first before doing it.

#### JMP

`JMP <addr>` will unconditionally set the value `PC` register to `<addr>`.
```c
else if (op == JMP) {pc = (int *)*pc;} // jump to address
```
