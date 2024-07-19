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
    cpu.P = 0x24;
    cpu.cycle = 7;

    WORD addr = 0xC000;
    PC = 0xC000;

    size_t c = 0;

    while(addr) {

        BYTE code = bus_read(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }

        printf("%04X %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X  PPU:  %d, %d CYC:%I64u\n", \
               PC, code, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP, ppu.ppustatus, ppu.ppuctrl, cpu.cycle);

        code_maps[code].op_func(code);
        cpu.cycle += code_maps[code].cycle;

        addr = PC;

        ++c;
    }

}

void disassemble()
{
    cpu.P = 0x24;
    cpu.cycle = 7;

    WORD addr = 0xC000;
    PC = 0xC000;

    while (addr) {

        BYTE opcode = bus_read(addr);

        printf("%04X  %02X", addr, opcode);

        BYTE operand_length = code_maps[opcode].op_len - 1;
        BYTE operand1 = 0, operand2 = 0;
        if (operand_length == 1) {
            operand1 = bus_read(addr + 1);
            printf(" %02X", operand1);
        } else if (operand_length == 2) {
            operand1 = bus_read(addr + 1);
            operand2 = bus_read(addr + 2);
            printf(" %02X %02X", operand1, operand2);
        }

        printf("  %-4s", code_maps[opcode].op_name);

        if (operand_length == 1) {
            printf(" $%02X", operand1);
        } else if (operand_length == 2) {
            WORD address = (operand2 << 8) | operand1;
            printf(" $%04X", address);
        }

        printf("    A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%I64u\n", \
               cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP,cpu.cycle);

        getchar();
        code_maps[opcode].op_func(opcode);
        cpu.cycle += code_maps[opcode].cycle;

        addr = PC;
    }
}

void display_IRQ()
{
    BYTE addr1 = bus_read(0xFFFE);
    BYTE addr2 = bus_read(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    addr1 = bus_read(0xFFFC);
    addr2 = bus_read(0xFFFD);

    addr = addr2 << 8 | addr1;

    BYTE code = bus_read(addr);

    addr1 = bus_read(0xFFFA);
    addr2 = bus_read(0xFFFB);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}

void show_code(ROM *rom)
{
    init_cpu();

    mem_init(rom);

    disassemble();

    // 显示部分IRQ 指令
    display_IRQ();
}