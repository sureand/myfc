#ifndef __CPU__HEADER__
#define __CPU__HEADER__
#include "common.h"

typedef struct {
    WORD IP;
    BYTE SP;
    BYTE A;
    BYTE X;
    BYTE Y;

    BYTE reversed;

}_CPU;

#endif