ENTRY(reset) /* code to call on reset */

_ROM_START = 0x000000; /* should be where ROM is in high memory... */
_ROM_LENGTH = 64K;
_RAM_START = 0x800000; /* should be where ROM is in high memory... */
_RAM_LENGTH = 2048K;

_estack = _RAM_START + _RAM_LENGTH; /* On M68K, past the highest stack byte */ 

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
        *(.text)
        *(.text*)
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
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >ROM

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
        _sdata = .;        /* global symbol at data start */
        *(.data)
        *(.data*)
      . = ALIGN(4);
      _edata = .;        /* global symbol at data end */
    } >RAM

    /* This is used by the startup in order to initialize the .bss secion */
    .bss :
    {
	. = ALIGN(4);
        _sbss = .;         /* define a global symbol at bss start */
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;         /* define a global symbol at bss end */
        __bss_end__ = _ebss;
    } >RAM

    heap_low = .;
}
