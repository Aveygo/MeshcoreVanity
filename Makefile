CC = gcc
CFLAGS = -std=c11 -g -O3

TARGET = meshcore_vanity
SOURCES = main.c src/*.c

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)