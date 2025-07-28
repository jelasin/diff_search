CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0` -Wall -Wextra -std=c99
LIBS = `pkg-config --libs gtk+-3.0`
TARGET = diff-viewer
SOURCE = diff-ui.c
CJSON_DIR = ../lib/cJSON
CJSON_SRC = $(CJSON_DIR)/cJSON.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE) $(CJSON_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)
