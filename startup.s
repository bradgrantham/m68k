  .cpu 68000

.global  g_vectors

.section  .isr_vector,"a",%progbits
  .type  g_vectors, %object
  .size  g_vectors, .-g_vectors
    
g_vectors:
  .long  0xA00000
  .long  main
  .fill  254,4,0xdeadbeef

reset:  
  /* ldr   sp, =_estack */     /* set stack pointer */
