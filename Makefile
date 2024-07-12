CC = gcc

CFLAGS = -Wall -g

TARGET = fc

SRCDIR = ./
DEST_DIR = example

SRCS = $(wildcard $(SRCDIR)/*.c)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(IDIR) $(LDIR) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(IDIR) -c -o $@ $<

clean:
	if exist $(TARGET) del $(TARGET)

.PHONY: all clean
