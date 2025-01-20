CC=clang
CFLAGS=-Wall -Wextra -O2
LDFLAGS=-lreadline -lncurses

TARGET=xsh
SRCS=src/main.c src/shell.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
    rm -f $(TARGET) $(OBJS)

.PHONY: all clean