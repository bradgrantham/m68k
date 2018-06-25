	.cpu 68000

.global  g_vectors

.section  .isr_vector,"a",%progbits
	.type  g_vectors, %object
	.size  g_vectors, .-g_vectors
    
g_vectors:
	.long  0xA00000
	.long  reset
	.fill  254,4,0xdeadbeef

/*
reset is compiled to assembly from the following C code,
labels changed by hand for clarity.

extern unsigned int *_sidata;
extern unsigned int *_sdata;
extern unsigned int *_edata;

extern unsigned int *_sbss;
extern unsigned int *_ebss;

void reset()
{
    unsigned int *datainit;
    unsigned int *datastore;

    datainit = _sidata;
    datastore = _sdata;
    while (datastore != _edata) {
	*datastore++ = *datainit++;
    }

    unsigned int *_bss;
    
    bss = _sbss;
    while(bss != _ebss) {
	*bss++ = 0;
    }
}
*/

	.section  .text.reset
	.text
	.align	2
	.global  reset
	.weak  reset
	.type  reset, @function
reset:
	move.l #_estack,%a7

	move.l #_sidata,%a1
	move.l #_edata,%d0
	move.l #_sdata,%a0
check_idata_copied:
	cmp.l %d0,%a0
	jne copy_more_idata
	move.l #_sbss,%a0
	move.l #_ebss,%d0
check_bss_cleared:
	cmp.l %a0,%d0
	jne clear_more_bss

	/* Call the system intitialization function.*/
	/* jsr  init_board  */

	/* Call static constructors */
	jsr __libc_init_array

	/* Call the application's entry point.*/
        jsr  main

	/* halt on 68000 */
	stop #2700

copy_more_idata:
	move.l (%a1)+,(%a0)+
	jra check_idata_copied

clear_more_bss:
	clr.l (%a0)+
	jra check_bss_cleared

	.size	reset, .-reset
