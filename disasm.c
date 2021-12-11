#include "disasm.h"
#include "memory.h"
#include "cpu.h"

void display(BYTE *data, size_t count)
{
    size_t c = 0;
    for(c = 0; c < count; ++c){
        printf("%02X", data[c]);
        if((c + 1) % 32 == 0) { printf("\n"); continue; }
        if((c + 1) % 4 == 0) printf(" ");
    }
}

void parse_code()
{
    BYTE addr1 = read_byte(0xFFFC);
    BYTE addr2 = read_byte(0xFFFD);

    WORD addr = addr2 << 8 | addr1;
    PC = addr;

    cpu.P = 0x24;
    cpu.cycle = 7;

    addr = 0xC004;
    PC = 0xC004;

    size_t c = 0;

    while(addr) {

       // if(c == 8992) break;

        BYTE code = read_byte(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }

        printf("%04X %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%zu\n", \
               PC, code, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP, cpu.cycle);

        code_maps[code].op_func(code);
        cpu.cycle += code_maps[code].cycle;

        addr = PC;

        ++c;
    }

}

void show_code(ROM *rom)
{
    init_cpu();

    init_code();

    _MEM *mem = (_MEM*)malloc(sizeof(_MEM));

    mem->PROG_ROM_LOWER = rom->prg_rom;
    mem->PROG_ROM_UPPER = rom->prg_rom;

    ROM_HEADER *header = rom->header;
    if(header->prg_rom_count > 1) {
        mem->PROG_ROM_UPPER = rom->prg_rom + 0x8000;
    }

    mem_init(mem);

    parse_code();

    if(1) return;


    BYTE addr1 = read_byte(0xFFFE);
    BYTE addr2 = read_byte(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    printf("IRQ/BRK:%04X\n", addr);

    addr1 = read_byte(0xFFFC);
    addr2 = read_byte(0xFFFD);

    addr = addr2 << 8 | addr1;

    printf("RESET:%04X\n", addr);

    BYTE code = read_byte(addr);
    printf("code:%02X\n", code);
    getchar();

    printf("code:%s\n", code_maps[code].op_name);

    addr1 = read_byte(0xFFFA);
    addr2 = read_byte(0xFFFB);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}