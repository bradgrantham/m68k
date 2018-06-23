TOOLROOT	=	/home/grantham/x-tools/m68k-none-elf

hello.bin: hello.hex
	hex2bin hello.hex hello.bin

hello.hex: hello.elf
	$(TOOLROOT)/bin/m68k-none-elf-objcopy -O ihex $< $@

hello.elf: hello.o
	$(TOOLROOT)/bin/m68k-none-elf-gcc -Tgeneric_platform.ld -Wl,--gc-sections $^ -o $@

	# $(TOOLROOT)/bin/m68k-none-elf-gcc $(OPT) $(CORTEX_M4_HWFP_CC_FLAGS)  -L$(CORTEX_M4_HWFP_LIB_PATH) -TSTM32F415RG_FLASH.ld -lm -Wl,--gc-sections $^ -o $@

hello.o: hello.c
	$(TOOLROOT)/bin/m68k-none-elf-gcc -Wall -Os -c hello.c

dis: hello.o
	$(TOOLROOT)/bin/m68k-none-elf-objdump -d hello.o
