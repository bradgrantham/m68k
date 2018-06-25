ENTRY(reset) /* code to call on reset */

_ROM_START = 0x000000; /* should be where ROM is in high memory... */
_ROM_LENGTH = 1024K;
_RAM_START = 0x800000;
_RAM_LENGTH = 2048K;

_estack = _RAM_START + _RAM_LENGTH; /* On M68K, past the highest stack byte */ 

_Min_Heap_Size = 0x80000;;      /* required amount of heap  */
_Min_Stack_Size = 0x400;; /* required amount of stack */

MEMORY
{
    ROM (rx)      : ORIGIN = _ROM_START, LENGTH = _ROM_LENGTH
    RAM (xrw)      : ORIGIN = _RAM_START, LENGTH = _RAM_LENGTH
}

SECTIONS
{
    .isr_vector :
    {
	. = ALIGN(4);
	KEEP(*(.isr_vector)) /* interrupt vector table */
	. = ALIGN(4);
    } >ROM

    /* code sections */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.text))
        KEEP(*(.text*))
        /* *(.eh_frame) */
    
        KEEP (*(.init))
        KEEP (*(.fini))
    
        . = ALIGN(4);
        _etext = .;        /* define a global symbols at end of code */
    } >ROM

    /* read-only data initialization */
    .rodata :
    {
        . = ALIGN(4);
        KEEP(*(.rodata))
        KEEP(*(.rodata*))
        . = ALIGN(4);
    } >ROM

    .ctors :
    {
	__CTOR_LIST__ = .;
	/* gcc uses crtbegin.o to find the start of
	   the constructors, so we make sure it is
	   first.  Because this is a wildcard, it
	   doesn't matter if the user does not
	   actually link against crtbegin.o; the
	   linker won't look for a file to match a
	   wildcard.  The wildcard also means that it
	   doesn't matter which directory crtbegin.o
	   is in.  */
	KEEP (*crtbegin.o(.ctors))
	/* We don't want to include the .ctor section from
	   from the crtend.o file until after the sorted ctors.
	   The .ctor section from the crtend file contains the
	   end of ctors marker and it must be last */
	KEEP (*(EXCLUDE_FILE (*crtend.o ) .ctors))
	KEEP (*(SORT(.ctors.*)))
	KEEP (*(.ctors))
	__CTOR_END__ = .;
    } > ROM  

    .dtors :
    {
	__DTOR_LIST__ = .;
	KEEP (*crtbegin.o(.dtors))
	KEEP (*(EXCLUDE_FILE (*crtend.o ) .dtors))
	KEEP (*(SORT(.dtors.*)))
	KEEP (*(.dtors))
	__DTOR_END__ = .;
    } > ROM

    .preinit_array     :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } >ROM

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } >ROM

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >ROM

    /* used by the startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data sections goes into RAM */
    .data : 
    {
        . = ALIGN(4);
	KEEP(*(vtable))
        _sdata = .;        /* global symbol at data start */
        KEEP(*(.data))
        KEEP(*(.data*))
        . = ALIGN(4);	/* guaranteeing align-4 here means we can loop long words in startup.s */
        _edata = .;        /* global symbol at data end */
    } >RAM AT> ROM

    /* This is used by the startup in order to initialize the .bss secion */
    .bss :
    {
	. = ALIGN(4);
        _sbss = .;         /* define a global symbol at bss start */
        __bss_start__ = _sbss;
        KEEP(*(.bss))
        KEEP(*(.bss*))
        KEEP(*(COMMON))
        . = ALIGN(4);	/* guaranteeing align-4 here means we can loop long words in startup.s */
        _ebss = .;         /* define a global symbol at bss end */
        __bss_end__ = _ebss;
    } >RAM

    heap_low = .;

    /* User_heap_stack section, used to check that there is enough RAM left */
    ._user_heap_stack :
    {
        . = ALIGN(4);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        heap_top = .;
        . = . + _Min_Stack_Size;
        . = ALIGN(4);
    } >RAM

}
