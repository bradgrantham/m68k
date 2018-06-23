const unsigned int RESET_ROM_BASE = 0x4000000;

const unsigned int RESET_RAM_BASE = 0x8000000;

const unsigned int IO_BASE = 0xC000000;
const unsigned int OUTPUT_BYTE = IO_BASE + 0;
const unsigned int INPUT_READY_BYTE = IO_BASE + 1;
const unsigned int INPUT_BYTE = IO_BASE + 2;
const unsigned int RAM_AT_0 = IO_BASE + 3;

int main()
{
    volatile unsigned char *p = (unsigned char *)OUTPUT_BYTE;
    *p = 'H';
    *p = 'E';
    *p = 'L';
    *p = 'L';
    *p = 'O';
    *p = '!';
    *p = '\n';
    for(;;);
}
