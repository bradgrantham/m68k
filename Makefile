TOOLROOT	=	/home/grantham/x-tools/m68k-none-elf


m68k: m68k.cpp musashi/m68kcpu.o musashi/m68kops.o musashi/m68kopnz.o musashi/m68kopac.o musashi/m68kopdm.o musashi/m68kdasm.o
	g++ -g -Wall -std=c++11 $^ -o $@

hello.bin: hello.elf
	$(TOOLROOT)/bin/m68k-none-elf-objcopy -O binary $< $@

hello.elf: hello.o startup.s syscalls.o crti.S crtn.S
	$(TOOLROOT)/bin/m68k-none-elf-gcc -Tbare.ld '-Wl,--gc-sections' $^ -o $@

	# $(TOOLROOT)/bin/m68k-none-elf-gcc $(OPT) $(CORTEX_M4_HWFP_CC_FLAGS)  -L$(CORTEX_M4_HWFP_LIB_PATH) -TSTM32F415RG_FLASH.ld -lm -Wl,--gc-sections $^ -o $@

syscalls.o: syscalls.c
	$(TOOLROOT)/bin/m68k-none-elf-gcc -std=c99 -Wall -Os -c syscalls.c

hello.o: hello.c
	$(TOOLROOT)/bin/m68k-none-elf-gcc -std=c99 -Wall -Os -c hello.c

dis: hello.o
	$(TOOLROOT)/bin/m68k-none-elf-objdump -d hello.o
