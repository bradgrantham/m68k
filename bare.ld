/* ENTRY(main) */ /* code to call on reset */

MEMORY
{
    ROM (rx)      : ORIGIN = 0x000000, LENGTH = 1024K
    RAM (xrw)      : ORIGIN = 0x800000, LENGTH = 2048K
}

SECTIONS
{
    .isr_vector :
    {
	. = ALIGN(4);
	KEEP(*(.isr_vector)) /* interrupt vector table */
	. = ALIGN(4);
    } >ROM

    .text :
    {
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text* sections (code) */
        /* *(.eh_frame) */
    
        KEEP (*(.init))
        KEEP (*(.fini))
    
        . = ALIGN(4);
        _etext = .;        /* define a global symbols at end of code */
    } >ROM
}
