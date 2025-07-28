CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
STATIC_CFLAGS = -Wall -Wextra -std=c99 -O2 -static
INCLUDES = -I.
SRCDIR = .
LIBDIR = lib
TARGET = md5_scanner
STATIC_TARGET = md5_scanner_static

# Source files
MAIN_SRC = main.c
LIB_SRCS = $(LIBDIR)/calc_md5/calc_md5.c \
           $(LIBDIR)/list_file/list_file.c \
           $(LIBDIR)/cJSON/cJSON.c \
           $(LIBDIR)/json_diff/json_diff.c

# Object files
MAIN_OBJ = $(MAIN_SRC:.c=.o)
LIB_OBJS = $(LIB_SRCS:.c=.o)
ALL_OBJS = $(MAIN_OBJ) $(LIB_OBJS)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Build the static executable
static: $(STATIC_TARGET)

$(STATIC_TARGET): $(ALL_OBJS)
	$(CC) $(STATIC_CFLAGS) $(INCLUDES) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(ALL_OBJS) $(TARGET) $(STATIC_TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build the MD5 scanner"
	@echo "  static      - Build the MD5 scanner (static)"
	@echo "  clean       - Remove build artifacts"
	@echo "  help        - Show this help message"

.PHONY: all static clean help
