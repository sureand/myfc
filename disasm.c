#include "disasm.h"

void display(BYTE *data, size_t count)
{
    size_t c = 0;
    for(c = 0; c < count; ++c){
        printf("%02X", data[c]);
        if((c + 1) % 32 == 0) { printf("\n"); continue; }
        if((c + 1) % 4 == 0) printf(" ");
    }
}