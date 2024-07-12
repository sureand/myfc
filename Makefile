# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

TARGET = fc

# Source directory
SRCDIR = ./
DEST_DIR = example

# Source files
SRCS = $(wildcard $(SRCDIR)/*.c)

# Main target (all)
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(IDIR) $(LDIR) -o $@ $^

# Rule for compiling .c files to .o files (for executable)
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(IDIR) -c -o $@ $<

# Clean target
clean:
	if exist $(TARGET) del $(TARGET)

# Phony targets to avoid conflicts with files named 'all' or 'clean'
.PHONY: all clean
