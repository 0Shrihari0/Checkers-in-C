CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -I./include
LDFLAGS =
SRCS = src/main.c src/functions.c
TARGET = checkers

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

debug: CFLAGS += -g -fsanitize=address,undefined -fno-omit-frame-pointer
debug: $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all debug clean
