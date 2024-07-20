#include "load_rom.h"

int run(ROM *rom)
{
    mem_init(rom);

    init_cpu();

    show_code();
}

void release_memory(ROM *rom)
{
    FREE(rom->header);
    FREE(rom);

    FREE(chr_rom);
    FREE(prg_rom);
}

int main(int argc, char *argv[])
{
    ROM * rom = load_rom("nestest.nes");

    run(rom);

    release_memory(rom);

    return 0;
}