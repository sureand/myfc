CFLAGS=-Wall -Wno-strict-aliasing -std=gnu11 -g -I -O0
OBJS=load_rom.o disasm.o
LIBS=-lm

fc: load_rom.h disasm.h main.o $(OBJS)
	cc -o $@ main.o $(OBJS) $(CFLAGS) $(LIBS)

$(OBJS) main.o: load_rom.h

clean:
	rm *.o fc
