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
    BYTE addr1 = bus_read(0xFFFC);
    BYTE addr2 = bus_read(0xFFFD);

    WORD addr = addr2 << 8 | addr1;
    PC = addr;

    cpu.P = 0x24;
    cpu.cycle = 7;

    addr = 0xC000;
    PC = 0xC000;

    size_t c = 0;

    while(addr) {

        BYTE code = bus_read(addr);
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

void display_IRQ()
{
    BYTE addr1 = bus_read(0xFFFE);
    BYTE addr2 = bus_read(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    printf("IRQ/BRK:%04X\n", addr);

    addr1 = bus_read(0xFFFC);
    addr2 = bus_read(0xFFFD);

    addr = addr2 << 8 | addr1;

    printf("RESET:%04X\n", addr);

    BYTE code = bus_read(addr);
    printf("code:%02X\n", code);
    getchar();

    printf("code:%s\n", code_maps[code].op_name);

    addr1 = bus_read(0xFFFA);
    addr2 = bus_read(0xFFFB);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}

void show_code(ROM *rom)
{
    init_cpu();

    //mem_init(rom);

    parse_code();

    // 显示部分IRQ 指令
    // display_IRQ();
}