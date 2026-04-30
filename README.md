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

This works because of `op = *pc++` - we have incremented to the next slot after the `op`, which is the address to jump to - so we can then simply redefine the program counter as the current thing in memory, and this effectively jumps the counter.

#### JZ/JNZ

This is like a conditional `JMP`. We have two, `JZ` to jump if `ax` is 0 or `JNZ` to jump if `ax` is not zero.
```c
else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;}                   // jump if ax is zero
else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;}                   // jump if ax is not zero
```

#### Function Calls

When a function is called, the VM needs an isolated workspace to hold its local variables and remember where to return when it finishes. It builds this workspace on the stack.

Remember we have `sp` which points to the top of the stack, and `bp` which acts as a fixed anchor for the current function. It allows the function to say 'my first arg is 2 slots above my `bp`' etc.

We can implement the `CALL <addr>` instruction to call the function whose starting point is `<addr>` and `RET` to fetch the bookeeping information to return previous excecution.
```c
else if (op == CALL) {*--sp = (int)(pc+1); pc = (int *)*pc;} // call subroutine
// else if (op == RET) {pc = (int *)*sp++;} // return from subroutine
```

Also, `RET` is commented out as we will replace in a second.

For various reasons, we cannot introduce function calls as they are implemented in C with this VM - it is too simple. We will deal with this as we did before, by simply adding more instructions.

`ENT <size>` is called when we are about to enter the function to make a 'new calling frame'. It stores the current `PC` value onto the stack, and save some space `<size>` bytes to store the local variables for the function. This is
```c
else if (op == ENT) {*--sp = (int)bp; bp = sp; sp = sp - *pc++;} // make new stack frame
```
1. `*--sp = (int)bp;`: push the *old* `bp` (the callers anchor) onto the stack. We have to save this so we don't destroy the callers workspace.
2. `bp = sp;`: set the new `bp` to the current top of the stack. This is the anchor for our new function.
3. `sp = sp - *pc++;`: reserve space for local variables. It reads the `<size>` argument in `pc` and moves `sp` down by that amount. This carves out a block in memory that will not be overwritten.

`ADJ <size>` is to adjust the stack, to remove arguments from the frame. We need this instruction mainly because our `ADD` doesn't have enough power. So, we treat it as a special `ADD` instance
```c
else if (op == ADJ) {sp = sp + *pc++}
```
1. `sp = sp + ...`: we move the stack pointer *up* (shrinking the stack).
2. `*pc++`: the argument here is the number of arguments that we originally pushed. If we pushed two arguments, `ADJ 2` will move `sp` up by 2, effectivelly binning those old arguments.

`LEA` allows us to read arguments or local variables. Because `bp` is our anchor, everything is calculated as an offset from `bp`
```c
else if (op == LEA)  {ax = (int)(bp + *pc++);}
```
1. `*pc++`: Read the offset argument (e.g. +2 for an argument, -1 for a local variable)
2. `bp + ...`: Add that offset to the base pointer to find the exact memory location for the variable or argument
3. `ax = (int)...`: save the memory address to the `ax` register. Note, this does not read the value, just writes its location. We then use `LI` or `SI` to actually read the value.

This can be visualised as 
```
sub_function(arg1, arg2, arg3);

|    ....       | high address
+---------------+
| arg: 1        |    new_bp + 4
+---------------+
| arg: 2        |    new_bp + 3
+---------------+
| arg: 3        |    new_bp + 2
+---------------+
|return address |    new_bp + 1
+---------------+
| old BP        | <- new BP
+---------------+
| local var 1   |    new_bp - 1
+---------------+
| local var 2   |    new_bp - 2
+---------------+
|    ....       |  low address
```

`LEV` tears the workspace down in reverse order (reverse of how `ENT` and `CALL` built it).
```c
else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}
```
1. `sp = bp;`: Instantly destroy all local variables. We abandon the reserved space.
2. `bp = (int *)*sp++;`: pop off the old base pointer from the stack and restore it.
3. `pc = (int *)*sp++;`: pop the return address of the stack and put it back into `pc`. The VM now goes back to the line of code after the original `CALL`.

#### Mathematical instructions

These are fairly obvious, so will just paste them. After a calculation is done, the argument on the stack will be popped out and the result stored in `ax`. 

```c
else if (op == OR)  ax = *sp++ | ax;
else if (op == XOR) ax = *sp++ ^ ax;
else if (op == AND) ax = *sp++ & ax;
else if (op == EQ)  ax = *sp++ == ax;
else if (op == NE)  ax = *sp++ != ax;
else if (op == LT)  ax = *sp++ < ax;
else if (op == LE)  ax = *sp++ <= ax;
else if (op == GT)  ax = *sp++ >  ax;
else if (op == GE)  ax = *sp++ >= ax;
else if (op == SHL) ax = *sp++ << ax;
else if (op == SHR) ax = *sp++ >> ax;
else if (op == ADD) ax = *sp++ + ax;
else if (op == SUB) ax = *sp++ - ax;
else if (op == MUL) ax = *sp++ * ax;
else if (op == DIV) ax = *sp++ / ax;
else if (op == MOD) ax = *sp++ % ax;
```

#### Built in instructions

Besides our core logic, we need an IO mechanism. `printf` is commonly used in C, but is hard to implement in this assembly, so we just steal it from C itself. We can take a few other functions as well.

```c
else if (op == EXIT) { printf("exit(%d)", *sp); return *sp;}
else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
else if (op == CLOS) { ax = close(*sp);}
else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
else if (op == MALC) { ax = (int)malloc(*sp);}
else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp);}
```

We have no written a primitive assembly language that we can write programs in. For example

```c
int i = 0;
text[i++] = IMM;
text[i++] = 10;
text[i++] = PUSH;
text[i++] = IMM;
text[i++] = 20;
text[i++] = ADD;
text[i++] = PUSH;
text[i++] = EXIT;

pc = text;

program();
return eval();
```

returns 

```
exit(30)
```

as expected

## Lexer

Lexical analysis 
