#define RESET_ROM_BASE		0x400000
#define RESET_RAM_BASE		0x800000
#define RAM_SIZE		0x200000 /* 2MB */
#define IO_BASE			0xC00000
#define OUTPUT_BYTE		(IO_BASE + 0)
#define INPUT_READY_BYTE	(IO_BASE + 1)
#define INPUT_BYTE		(IO_BASE + 2)
#define RAM_AT_0		(IO_BASE + 3)

char hello2[] = "Hello world!\n";

int main()
{
    // for(int i = 0; i < 12; i++)
	// putchar(hello2[i]);
    puts(hello2);
}

void __io_putchar( char c )
{
    volatile unsigned char *p = (unsigned char *)OUTPUT_BYTE;
    *p = c;
}

