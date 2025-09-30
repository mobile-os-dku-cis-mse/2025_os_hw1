CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = sish
SRCS = sish.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

sish.o: sish.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
