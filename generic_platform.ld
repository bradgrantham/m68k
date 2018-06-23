ENTRY(reset) /* code to call on reset */

_estack = 0x9FFFFFC;
_min_heap_size = 0x1000000; // 16MB
_min_stack_size = 0x100000; // 1MB

MEMORY
{
    ROM (rx)      : ORIGIN = 0x0000000, LENGTH = 1024K
    RAM (xrw)      : ORIGIN = 0x8000000, LENGTH = 32768K
}

SECTIONS
{
    .isr_vector :
    {
	. = ALIGN(4);
	KEEP(*(.isr_vector)) /* interrupt vector table */
	. = ALIGN(4);
    }

    .text :
    {
	. = ALIGN(4);
	*(.text)
	*(.text)
	*(.eh_frame)

	KEEP (*(.init))
	KEEP (*(.fini))
	. = ALIGN(4);
	_etext = .;
    } >ROM

    .rodata :
    {
	. = ALIGN(4);
	*(.rodata)
	*(.rodata*)
	. = ALIGN(4);
    } >ROM

    .preinit_array :
    {
	PROVIDE_HIDDEN (__preinit_array_start = .);
	KEEP (*(.preinit_array))
	PROVIDE_HIDDEN (__preinit_array_end = .);
    } >ROM

    .init_array :
    {
	PROVIDE_HIDDEN (__init_array_start = .);
	KEEP (*(SORT(.init_array.*)))
	KEEP (*(.init_array))
	PROVIDE_HIDDEN (__init_array_end = .);
    } >ROM

    .fini_array :
    {
	PROVIDE_HIDDEN (__fini_array_start = .);
	KEEP (*(SORT(.fini_array.*)))
	KEEP (*(.fini_array))
	PROVIDE_HIDDEN (__fini_array_end = .);
    } >ROM

    _sidata = LOADADDR(.data);

    .data :
    {
	. = ALIGN(4);
	_sdata = .;

	*(.data)
	*(.data*)

	. = ALIGN(4);
	_edata = .;
    } >RAM AT> FLASH

    . = ALIGN(4);
    .bss :
    {
	_sbss = .;
	__bss_start__ = _sbss;
	*(.bss)
	*(.bss*)
	*(COMMON)
	. = ALIGN(4);
	_ebss = .;
	__bss_end__ = _ebss;
    } >RAM

    heap_low = .;

    ._user_heap_stack :
    {
	. = ALIGN(4);
	PROVIDE(end = .);
	PROVIDE(_end = .);
	. = . + _min_heap_size;
	hap_top = .;
	. = . + _min_stack_size;
	. = ALIGN(4);
    } > RAM

    /* Remove information from the standard libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
}

