These are my experiments in
* targeting C++ to bare-metal, status:
  * C++ 14 compilation - works
  * Copying data initializers from ROM to RAM (e.g. string) - works
  * constructor/destructor - works
  * static initialization - untested
  * STL containers - untested
* targeting motorola 68000 (aka m68k)
  * 68000 emulation with 3rd party emulator (Musashi) - works
  * from-scratch implementation - unimplemented
  * generic dumb platform (keyboard in/out, RAM/ROM) - partial (no input)
  * RAM/ROM soft switch - unimplemented
  * framebuffer - unimplemented
  * disk I/O - unimplemented

Here's how I targeted C++ programs to a bare-metal platform:
* I used Crosstool-NG 1.23.0
* I used arm-unknown-eabi target as my source, and created a new target ["m68-none-elf"](https://github.com/bradgrantham/m68k/blob/master/crosstool.config) including:
  * newlib
  * ELF
  * no target OS
  * C++
  * doesn't use the newlib system call stubs (e.g. read(), write(), sbrk())
* I created a linker script ["bare.ld"](https://github.com/bradgrantham/m68k/blob/master/bare.ld) based on other bare metal linker scripts which (among other things)
  * defines the vector table to be the first item in ROM
  * places .text (code) items in ROM
  * places initialization data in ROM with symbols actually in RAM
  * defines the stack pointer for later use in startup code
  * puts .bss (zero-initialized data) in RAM and defines the "heap" to follow that
  * calculates a plausible "top of stack" (lowest memory address) for error checking
* I created startup.s:
  * defines the vector table, which contains the initial processor stack pointer and program counter
  * copies the .data section initializers
  * clears the .bss
  * would call platform initialization if there was any
  * calls the C library initialization
  * calls main
* Newlib provides C library functionality (e.g. printf()) and calls stubs for the system calls.  I think I cribbed the syscalls.c from STMCube32
  * read and write call __io_putchar() and __io_getchar(), which are platform-specific.
  * sbrk() knows to start from the heap address defined in the linker script.
  
Continuing work includes:
  * flesh out the generic platform into something useful
  * write a 68000 emulator myself (and then 68030 by adding the MMU)
  * get Linux running, maybe (unrelated to C++ on bare-metal but was one of the motivators for the emulation)
