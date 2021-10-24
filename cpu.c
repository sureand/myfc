#include "cpu.h"
#include "load_rom.h"
#include "memory.h"
#include "disasm.h"

inline void do_brk()
{
    //BYTE op = do_read_byte()
    printf("do_brk\n");
}

void show_code(ROM *rom)
{
    init_code();

    _MEM *mem = (_MEM*)malloc(sizeof(_MEM));

    mem->PROG_ROM_LOWER = rom->prg_rom;
    mem->PROG_ROM_UPPER = rom->prg_rom;

    ROM_HEADER *header = rom->header;
    if(header->prg_rom_count > 1) {
        mem->PROG_ROM_UPPER = rom->prg_rom + 0x8000;
    }

    /*

    - $FFFA-FFFB = NMI  ' 这里 - 指的是地址，不是减号'
    - $FFFC-FFFD = RESET
    - $FFFE-FFFF = IRQ/BRK

    */

    BYTE addr1 = mem_read(0xFFFE, mem);
    BYTE addr2 = mem_read(0xFFFF, mem);

    WORD addr = addr2 << 8 | addr1;

    printf("IRQ/BRK:%04X\n", addr);

    addr1 = mem_read(0xFFFC, mem);
    addr2 = mem_read(0xFFFD, mem);

    addr = addr2 << 8 | addr1;

    printf("RESET:%04X\n", addr);

    BYTE code = mem_read(addr, mem);
    printf("code:%s\n", code_maps[code].op_name);

    addr1 = mem_read(0xFFFA, mem);
    addr2 = mem_read(0xFFFB, mem);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}