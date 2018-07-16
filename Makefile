TOOLROOT	=	/home/grantham/x-tools/m68k-none-elf

default: m68k hello.bin

m68k: m68k.cpp musashi/m68kcpu.o musashi/m68kops.o musashi/m68kopnz.o musashi/m68kopac.o musashi/m68kopdm.o musashi/m68kdasm.o
	g++ -g -Wall -std=c++11 $^ -o $@

hello.bin: hello.elf
	$(TOOLROOT)/bin/m68k-none-elf-objcopy -O binary $< $@

OBJECTS		=	hello.o startup.o syscalls.o
hello.elf: $(OBJECTS) crti.o crtn.o bare.ld
	$(TOOLROOT)/bin/m68k-none-elf-g++ -specs=m68k-none-elf.specs -Tbare.ld '-Wl,--no-undefined' $(OBJECTS) -o $@
	# '-Wl,--gc-sections'

crti.o: crti.s
	$(TOOLROOT)/bin/m68k-none-elf-gcc -c crti.s

crtn.o: crtn.s
	$(TOOLROOT)/bin/m68k-none-elf-gcc -c crtn.s

startup.o: startup.s
	$(TOOLROOT)/bin/m68k-none-elf-gcc -c startup.s

syscalls.o: syscalls.c
	$(TOOLROOT)/bin/m68k-none-elf-gcc -std=c99 -Wall -Os -c syscalls.c

hello.o: hello.cpp
	$(TOOLROOT)/bin/m68k-none-elf-g++ -std=c++14 -Wall -Os -c hello.cpp

dis: hello.o
	$(TOOLROOT)/bin/m68k-none-elf-objdump -d hello.o
