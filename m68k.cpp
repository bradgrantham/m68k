#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <set>
#include <chrono>
#include <thread>
#include <ratio>
#include <iostream>
#include <deque>
#include <map>
#include <thread>
#include <signal.h>

extern "C" {
#include "musashi/m68k.h"
}

using namespace std;

#include "emulator.h"

const unsigned int DEBUG_ERROR = 0x01;
const unsigned int DEBUG_WARN = 0x02;
const unsigned int DEBUG_DECODE = 0x04;
const unsigned int DEBUG_STATE = 0x08;
const unsigned int DEBUG_RW = 0x10;
const unsigned int DEBUG_BUS = 0x20;
const unsigned int DEBUG_FLOPPY = 0x40;
const unsigned int DEBUG_SWITCH = 0x80;
volatile unsigned int debug = DEBUG_ERROR | DEBUG_WARN ; // | DEBUG_DECODE | DEBUG_STATE | DEBUG_RW;

volatile bool exit_on_memory_fallthrough = true;
volatile bool run_fast = false;
volatile bool pause_cpu = false;

typedef unsigned long long clk_t;
struct system_clock
{
    clk_t value = 0;
    operator clk_t() const { return value; }
    clk_t operator+=(clk_t i) { return value += i; }
    clk_t operator++(int) { clk_t v = value; value ++; return v; }
} clk;

bool read_blob(char *name, unsigned char *b, size_t sz, bool short_okay = false)
{
    FILE *fp = fopen(name, "rb");
    if(fp == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", name);
        fclose(fp);
        return false;
    }
    size_t length = fread(b, 1, sz, fp);
    if(length < sz) {
        fprintf(stderr, "File read from %s was unexpectedly short (%zd bytes, expected %zd)\n", name, length, sz);
	if(!short_okay) {
	    perror("read_blob");
	    fclose(fp);
	    return false;
	}
    }
    fclose(fp);
    return true;
}

struct SoftSwitch
{
    string name;
    int clear_address;
    int set_address;
    int read_address;
    bool read_also_changes;
    bool enabled = false;
    bool implemented;
    SoftSwitch(const char* name_, int clear, int on, int read, bool read_changes, vector<SoftSwitch*>& s, bool implemented_ = false) :
        name(name_),
        clear_address(clear),
        set_address(on),
        read_address(read),
        read_also_changes(read_changes),
        implemented(implemented_)
    {
        s.push_back(this);
    }
    operator bool() const
    {
        return enabled;
    }
};

struct region
{
    string name;
    int base;
    int size;
    bool contains(int addr) const
    {
        return addr >= base && addr < base + size;
    }
};

typedef std::function<bool()> enabled_func;

enum MemoryType {RAM, ROM};

struct backed_region : region
{
    vector<unsigned char> memory;
    MemoryType type;
    enabled_func read_enabled;
    enabled_func write_enabled;

    backed_region(const char* name, int base, int size, MemoryType type_, vector<backed_region*>* regions, enabled_func enabled_) :
        region{name, base, size},
        memory(size),
        type(type_),
        read_enabled(enabled_),
        write_enabled(enabled_)
    {
        std::fill(memory.begin(), memory.end(), 0x00);
        if(regions)
            regions->push_back(this);
    }

    backed_region(const char* name, int base, int size, MemoryType type_, vector<backed_region*>* regions, enabled_func read_enabled_, enabled_func write_enabled_) :
        region{name, base, size},
        memory(size),
        type(type_),
        read_enabled(read_enabled_),
        write_enabled(write_enabled_)
    {
        std::fill(memory.begin(), memory.end(), 0x00);
        if(regions)
            regions->push_back(this);
    }

    bool contains(int addr) const
    {
        return addr >= base && addr < base + size;
    }

    bool read(int addr, unsigned char& data)
    {
        if(contains(addr) && read_enabled()) {
            data = memory[addr - base];
            return true;
        }
        return false;
    }

    bool write(int addr, unsigned char data)
    {
        if((type == RAM) && contains(addr) && write_enabled()) {
            memory[addr - base] = data;
            return true;
        }
        return false;
    }
};

const region io_region = {"io", 0xC000, 0x100};

unsigned char floppy_header[21] = {
	0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xDE, 0xAA, 0xFF,	0xFF, 0xFF,
	0xFF, 0xFF, 0xD5, 0xAA, 0xAD };
unsigned char floppy_doSector[16] = {
	0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4, 0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF };
unsigned char floppy_poSector[16] = {
	0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB, 0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF };

void floppy_NybblizeImage(unsigned char *image, unsigned char *nybblized, unsigned char *skew)
{
	// Format of a sector is header (23) + nybbles (343) + footer (30) = 396
	// (short by 20 bytes of 416 [413 if 48 byte header is one time only])
	// hdr (21) + nybbles (343) + footer (48) = 412 bytes per sector
	// (not incl. 64 byte track marker)

	static unsigned char footer[48] = {
		0xDE, 0xAA, 0xEB, 0xFF, 0xEB, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	static unsigned char diskbyte[0x40] = {
		0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
		0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
		0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
		0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
		0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
		0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
		0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
		0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF };

	memset(nybblized, 0xFF, 232960);					// Doesn't matter if 00s or FFs...

        unsigned char *p = nybblized;

	for(unsigned char trk=0; trk<35; trk++)
	{
		memset(p, 0xFF, 64);					// Write gap 1, 64 bytes (self-sync)
		p += 64;

		for(unsigned char sector=0; sector<16; sector++)
		{
			memcpy(p, floppy_header, 21);			// Set up the sector header

			p[5] = ((trk >> 1) & 0x55) | 0xAA;
			p[6] =  (trk       & 0x55) | 0xAA;
			p[7] = ((sector >> 1) & 0x55) | 0xAA;
			p[8] =  (sector       & 0x55) | 0xAA;
			p[9] = (((trk ^ sector ^ 0xFE) >> 1) & 0x55) | 0xAA;
			p[10] = ((trk ^ sector ^ 0xFE)       & 0x55) | 0xAA;

			p += 21;
			unsigned char * bytes = image;

                        bytes += (skew[sector] * 256) + (trk * 256 * 16);

			// Convert the 256 8-bit bytes into 342 6-bit bytes.

			for(int i=0; i<0x56; i++)
			{
				p[i] = ((bytes[(i + 0xAC) & 0xFF] & 0x01) << 7)
					| ((bytes[(i + 0xAC) & 0xFF] & 0x02) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x01) << 5)
					| ((bytes[(i + 0x56) & 0xFF] & 0x02) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x01) << 3)
					| ((bytes[(i + 0x00) & 0xFF] & 0x02) << 1);
			}

			p[0x54] &= 0x3F;
			p[0x55] &= 0x3F;
			memcpy(p + 0x56, bytes, 256);

			// XOR the data block with itself, offset by one byte,
			// creating a 343rd byte which is used as a cheksum.

			p[342] = 0x00;

			for(int i=342; i>0; i--)
				p[i] = p[i] ^ p[i - 1];

			// Using a lookup table, convert the 6-bit bytes into disk bytes.

			for(int i=0; i<343; i++)
                            p[i] = diskbyte[p[i] >> 2];
			p += 343;

			// Done with the nybblization, now for the epilogue...

			memcpy(p, footer, 48);
			p += 48;
		}
	}
}


struct DISKIIboard : board_base
{
    const int CA0 = 0xC0E0; // stepper phase 0 / control line 0
    const int CA1 = 0xC0E2; // stepper phase 1 / control line 1
    const int CA2 = 0xC0E4; // stepper phase 2 / control line 2
    const int CA3 = 0xC0E6; // stepper phase 3 / control strobe
    const int ENABLE = 0xC0E8; // disk drive off/on
    const int SELECT = 0xC0EA; // select drive 1/2
    const int Q6L = 0xC0EC; // IO strobe for read
    const int Q6H = 0xC0ED; // IO strobe for write
    const int Q7L = 0xC0EE; // IO strobe for clear
    const int Q7H = 0xC0EF; // IO strobe for shift

    map<unsigned int, string> io = {
        {0xC0E0, "CA0OFF"},
        {0xC0E1, "CA0ON"},
        {0xC0E2, "CA1OFF"},
        {0xC0E3, "CA1ON"},
        {0xC0E4, "CA2OFF"},
        {0xC0E5, "CA2ON"},
        {0xC0E6, "CA3OFF"},
        {0xC0E7, "CA3ON"},
        {0xC0E8, "DISABLE"},
        {0xC0E9, "ENABLE"},
        {0xC0EA, "SELECT0"},
        {0xC0EB, "SELECT1"},
        {0xC0EC, "Q6L"},
        {0xC0ED, "Q6H"},
        {0xC0EE, "Q7L"},
        {0xC0EF, "Q7H"},
    };

    backed_region rom_C600 = {"rom_C600", 0xC600, 0x0100, ROM, nullptr, [&]{return true;}};

    unsigned char floppy_image[2][143360];
    unsigned char floppy_nybblized[2][232960];
    const unsigned int bytes_per_nybblized_track = 6656;
    bool floppy_present[2];

    int drive_selected = 0;
    bool drive_motor_enabled[2];
    enum {READ, WRITE} head_mode = READ;
    unsigned char data_latch = 0x00;
    int head_stepper_phase[4] = {0, 0, 0, 0};
    int head_stepper_most_recent_phase = 0;
    int track_number = 0; // physical track number - DOS and ProDOS only use even tracks
    unsigned int track_byte = 0;

    void set_floppy(int number, char *name) // number 0 or 1; name = NULL to eject
    {
        floppy_present[number] = false;
        if(name) {
            if(!read_blob(name, floppy_image[number], sizeof(floppy_image[0])))
                throw "Couldn't read floppy";
            
            floppy_present[number] = true;
            unsigned char *skew;
            if(strcmp(name + strlen(name) - 3, ".po") == 0) {
                printf("ProDOS floppy\n");
                skew = floppy_poSector;
            }
            else
                skew = floppy_doSector;
            floppy_NybblizeImage(floppy_image[number], floppy_nybblized[number], skew);
        }
    }

    typedef std::function<void (int number, bool activity)> floppy_activity_func;
    floppy_activity_func floppy_activity;

    DISKIIboard(unsigned char diskII_rom[256], char *floppy0_name, char *floppy1_name, floppy_activity_func floppy_activity_) :
        floppy_activity(floppy_activity_)
    {
        std::copy(diskII_rom, diskII_rom + 0x100, rom_C600.memory.begin());
        set_floppy(0, floppy0_name);
        set_floppy(1, floppy1_name);
    }

    unsigned char read_next_nybblized_byte()
    {
        if(head_mode != READ || !drive_motor_enabled[drive_selected] || !floppy_present[drive_selected])
            return 0x00;
        int i = track_byte;
        track_byte = (track_byte + 1) % bytes_per_nybblized_track;
        return floppy_nybblized[drive_selected][(track_number / 2) * bytes_per_nybblized_track + i];
    }

    void control_track_motor(unsigned int addr)
    {
        int phase = (addr & 0x7) >> 1;
        int state = addr & 0x1;
        head_stepper_phase[phase] = state;
        if(debug & DEBUG_FLOPPY) printf("stepper %04X, phase %d, state %d, so stepper motor state now: %d, %d, %d, %d\n",
            addr, phase, state,
            head_stepper_phase[0], head_stepper_phase[1],
            head_stepper_phase[2], head_stepper_phase[3]);
        if(state == 1) { // turn stepper motor phase on
            if(head_stepper_most_recent_phase == (((phase - 1) + 4) % 4)) { // stepping up
                track_number = min(track_number + 1, 70);
                if(debug & DEBUG_FLOPPY) printf("track number now %d\n", track_number);
            } else if(head_stepper_most_recent_phase == ((phase + 1) % 4)) { // stepping down
                track_number = max(0, track_number - 1);
                if(debug & DEBUG_FLOPPY) printf("track number now %d\n", track_number);
            } else if(head_stepper_most_recent_phase == phase) { // unexpected condition
                if(debug & DEBUG_FLOPPY) printf("track head stepper no change\n");
            } else { // unexpected condition
                if(debug & DEBUG_WARN) fprintf(stderr, "unexpected track stepper motor state: %d, %d, %d, %d\n",
                    head_stepper_phase[0], head_stepper_phase[1],
                    head_stepper_phase[2], head_stepper_phase[3]);
                if(debug & DEBUG_WARN) fprintf(stderr, "most recent phase: %d\n", head_stepper_most_recent_phase);
            }
            head_stepper_most_recent_phase = phase;
        }
    }

    virtual bool write(int addr, unsigned char data)
    {
        if(addr < 0xC0E0 || addr > 0xC0EF)
            return false;
        if(debug & DEBUG_WARN) printf("DISK II unhandled write of %02X to %04X (%s)\n", data, addr, io[addr].c_str());
        return false;
    }
    virtual bool read(int addr, unsigned char &data)
    {
        if(rom_C600.read(addr, data)) {
            if(debug & DEBUG_RW) printf("DiskII read 0x%04X -> %02X\n", addr, data);
            return true;
        }

        if(addr < 0xC0E0 || addr > 0xC0EF) {
            return false;
        }

        if(addr >= CA0 && addr <= (CA3 + 1)) {
            if(debug & DEBUG_FLOPPY) printf("floppy control track motor\n");
            control_track_motor(addr);
            data = 0;
            return true;
        } else if(addr == Q6L) { // 0xC0EC
            data = read_next_nybblized_byte();
            if(debug & DEBUG_FLOPPY) printf("floppy read byte : %02X\n", data);
            return true;
        } else if(addr == Q6H) { // 0xC0ED
            if(debug & DEBUG_FLOPPY) printf("floppy read latch\n");
            data = data_latch; // XXX do something with the latch - e.g. set write-protect bit
            data = 0;
            return true;
        } else if(addr == Q7L) { // 0xC0EE
            if(debug & DEBUG_FLOPPY) printf("floppy set read\n");
            head_mode = READ;
            data = 0;
            return true;
        } else if(addr == Q7H) { // 0xC0EF
            if(debug & DEBUG_FLOPPY) printf("floppy set write\n");
            head_mode = WRITE;
            data = 0;
            return true;
        } else if(addr == SELECT) {
            if(debug & DEBUG_FLOPPY) printf("floppy select first drive\n");
            drive_selected = 0;
            return true;
        } else if(addr == SELECT + 1) {
            if(debug & DEBUG_FLOPPY) printf("floppy select second drive\n");
            drive_selected = 1;
            return true;
        } else if(addr == ENABLE) {
            if(debug & DEBUG_FLOPPY) printf("floppy switch off\n");
            drive_motor_enabled[drive_selected] = false;
            floppy_activity(drive_selected, false);
            // go disable reading
            // disable other drive?
            return true;
        } else if(addr == ENABLE + 1) {
            if(debug & DEBUG_FLOPPY) printf("floppy switch on\n");
            drive_motor_enabled[drive_selected] = true;
            floppy_activity(drive_selected, true);
            // go enable reading
            // disable other drive?
            return true;
        }
        if(debug & DEBUG_WARN) printf("DISK II unhandled read from %04X (%s)\n", addr, io[addr].c_str());
        data = 0;
        return true;
    }
    virtual void reset(void) {}
};

const int waveform_length = 44100 / 1000 / 2; // half of a wave at 4000 Hz
const float waveform_max_amplitude = .35f;
static unsigned char waveform[waveform_length];

static void initialize_audio_waveform() __attribute__((constructor));
void initialize_audio_waveform()
{
    for(int i = 0; i < waveform_length; i++) {
        float theta = (float(i) / (waveform_length - 1) -.5f) * M_PI;

        waveform[i] = 127.5 + waveform_max_amplitude * 127.5 * sin(theta);
    }
}

#define IO_BASE			0xC00000
#define OUTPUT_BYTE		(IO_BASE + 0)

struct MAINboard : board_base
{
    const int rom_base = 0x0; // XXX
    const int rom_size = 32768;
    std::array<unsigned char, 32768> rom;

    const int ram_base = 0x800000;
    const int ram_size = 2 * 1024 * 1024;
    std::array<unsigned char, 2 * 1024 * 1024> ram;

    void sync()
    {
    }

    MAINboard(unsigned char rom_image[32768])
    {
        std::copy(rom_image, rom_image + rom_size, rom.begin());
    }

    virtual ~MAINboard()
    {
    }

    virtual void reset()
    {
    }

    virtual bool read(int addr, unsigned char &data)
    {
        if(debug & DEBUG_RW) printf("MAIN board read\n");
	if((addr >= rom_base) && (addr < rom_base + rom_size)) {
	    data = rom[addr - rom_base];
	    if(debug & DEBUG_BUS) printf("rom address %08X, offset %08X = %02X\n", addr, addr - rom_base, data);
	    return true;
	}
	if((addr >= ram_base) && (addr < ram_base + ram_size)) {
	    data = ram[addr - ram_base];
	    return true;
	}
        if(debug & DEBUG_WARN) printf("unhandled memory read from %04X\n", addr);
        if(exit_on_memory_fallthrough) {
            printf("unhandled memory read at %04X, aborting\n", addr);
            exit(1);
        }
        return false;
    }
    virtual bool write(int addr, unsigned char data)
    {
        if(debug & DEBUG_RW) printf("MAIN board read\n");
	if(addr == OUTPUT_BYTE) {
	    putchar(data);
	    return true;
	}
	if((addr >= ram_base) && (addr < ram_base + ram_size)) {
	    ram[addr - ram_base] = data;
	    return true;
	}
        if(debug & DEBUG_WARN) printf("unhandled memory write to %04X\n", addr);
        if(exit_on_memory_fallthrough) {
            printf("unhandled memory write to %04X, exiting\n", addr);
            exit(1);
        }
        return false;
    }
};

struct bus_frontend
{
    board_base* board;
    map<int, vector<unsigned char> > writes;
    map<int, vector<unsigned char> > reads;

    unsigned char read(int addr)
    {
        unsigned char data = 0xAA;
        if(board->read(addr, data)) {
            if(debug & DEBUG_BUS) printf("read %04X returned %02X\n", addr, data);
            // reads[addr].push_back(data);
            return data;
        }
        if(debug & DEBUG_ERROR)
            fprintf(stderr, "no ownership of read at %04X\n", addr);
        return 0xAA;
    }
    void write(int addr, unsigned char data)
    {
        if(board->write(addr, data)) {
            if(debug & DEBUG_BUS) printf("write %04X %02X\n", addr, data);
            // writes[addr].push_back(data);
            return;
        }
        if(debug & DEBUG_ERROR)
            fprintf(stderr, "no ownership of write %02X at %04X\n", data, addr);
    }

    void reset()
    {
        board->reset();
    }
};

bus_frontend bus;

struct CPU68000
{
    unsigned long d[8], a[8], pc, sp, ccr;

    static const unsigned char X = 0x10;
    static const unsigned char N = 0x08;
    static const unsigned char Z = 0x04;
    static const unsigned char V = 0x02;
    static const unsigned char C = 0x01;
    CPU68000()
    {
	for(int i = 0; i < 8; i++) {
	    d[i] = 0x0;
	    a[i] = 0x0;
	}
	pc = 0;
	sp = 0;
	ccr = 0;
    }
    void reset(bus_frontend& bus)
    {
        pc = bus.read(0xFFFC) + bus.read(0xFFFD) * 256;
    }
    enum Operand {
    };
    void cycle(bus_frontend& bus)
    {
    }
};

void usage(char *progname)
{
    printf("\n");
    printf("usage: %s ROM.bin\n", progname);
    printf("\n");
    printf("\n");
}

void cleanup(void)
{
    fflush(stdout);
    fflush(stderr);
}

const bool use_other_68000 = true;

extern "C" {

unsigned int  m68k_read_memory_8(unsigned int address)
{
    return bus.read(address);
}

unsigned int  m68k_read_memory_16(unsigned int address)
{
    return (bus.read(address) << 8) | bus.read(address + 1);
}

unsigned int  m68k_read_memory_32(unsigned int address)
{
    return (bus.read(address + 0) << 24) |
           (bus.read(address + 1) << 16) |
           (bus.read(address + 2) <<  8) |
           (bus.read(address + 3) <<  0) ;
}

unsigned int m68k_read_disassembler_8(unsigned int address) { return m68k_read_memory_8(address); }
unsigned int m68k_read_disassembler_16(unsigned int address) { return m68k_read_memory_16(address); }
unsigned int m68k_read_disassembler_32(unsigned int address) { return m68k_read_memory_32(address); }

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    bus.write(address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    bus.write(address + 0, (value >> 8) & 0xff);
    bus.write(address + 1, (value >> 0) & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    bus.write(address + 0, (value >> 24) & 0xff);
    bus.write(address + 1, (value >> 16) & 0xff);
    bus.write(address + 2, (value >>  8) & 0xff);
    bus.write(address + 3, (value >>  0) & 0xff);
}

void make_hex(char* buff, unsigned int pc, unsigned int length)
{
	char* ptr = buff;

	for(;length>0;length -= 2)
	{
		sprintf(ptr, "%04x", m68k_read_memory_16(pc));
		pc += 2;
		ptr += 4;
		if(length > 2)
			*ptr++ = ' ';
	}
}

void cpu_instr_callback()
{
/* The following code would print out instructions as they are executed */
    static char buff[100];
    static char buff2[100];
    static unsigned int pc;
    static unsigned int instr_size;

    pc = m68k_get_reg(NULL, M68K_REG_PC);
    instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
    make_hex(buff2, pc, instr_size);
    printf("E %03x: %-20s: %s\n", pc, buff2, buff);
    fflush(stdout);
}

};

void step68000(void)
{
    m68k_execute(1);
}

void reset68000(void)
{
    m68k_pulse_reset();
}

int main(int argc, char **argv)
{
    bool debugging = false;
    char *progname = argv[0];
    argc -= 1;
    argv += 1;

    while((argc > 0) && (argv[0][0] == '-')) {
	if(strcmp(argv[0], "-debugger") == 0) {
            debugging = true;
            argv++;
            argc--;
	} else if(strcmp(argv[0], "-fast") == 0) {
            run_fast = true;
            argv += 1;
            argc -= 1;
	} else if(strcmp(argv[0], "-d") == 0) {
            debug = atoi(argv[1]);
            if(argc < 2) {
                fprintf(stderr, "-d option requires a debugger mask value.\n");
                exit(EXIT_FAILURE);
            }
            argv += 2;
            argc -= 2;
        } else if(
            (strcmp(argv[0], "-help") == 0) ||
            (strcmp(argv[0], "-h") == 0) ||
            (strcmp(argv[0], "-?") == 0))
        {
            usage(progname);
            exit(EXIT_SUCCESS);
	} else {
	    fprintf(stderr, "unknown parameter \"%s\"\n", argv[0]);
            usage(progname);
	    exit(EXIT_FAILURE);
	}
    }

    if(argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    char *romname = argv[0];
    unsigned char b[32768];

    if(!read_blob(romname, b, sizeof(b), true))
        exit(EXIT_FAILURE);

    MAINboard* mainboard;

    mainboard = new MAINboard(b);
    bus.board = mainboard;
    bus.reset();

    CPU68000 cpu;

    atexit(cleanup);

    if(use_other_68000) {
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
        reset68000();
    }

    chrono::time_point<chrono::system_clock> then = std::chrono::system_clock::now();

    while(1) {
        if(!debugging) {
	    {
                if(use_other_68000) {
                    step68000();
                } else {
                    cpu.cycle(bus);
                }
            }
            mainboard->sync();
        } else {

            printf("> ");
            char line[512];
            if(fgets(line, sizeof(line) - 1, stdin) == NULL) {
		exit(0);
	    }
            line[strlen(line) - 1] = '\0';
            if(strcmp(line, "go") == 0) {
                printf("continuing\n");
                debugging = false;
                continue;
            } else if(strcmp(line, "fast") == 0) {
                printf("run flat out\n");
                run_fast = true;
                continue;
            } else if(strcmp(line, "slow") == 0) {
                printf("run 1mhz\n");
                run_fast = false;
                continue;
            } else if(strncmp(line, "debug", 5) == 0) {
                sscanf(line + 6, "%d", &debug);
                printf("debug set to %02X\n", debug);
                continue;
            } else if(strcmp(line, "reset") == 0) {
                printf("machine reset.\n");
                bus.reset();
                cpu.reset(bus);
                continue;
            }

            if(use_other_68000) {
                step68000();
            } else {
                cpu.cycle(bus);
            }
            mainboard->sync();
        }
    }
}
