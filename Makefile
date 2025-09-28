TARGET = goronoSH
CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = hw1.c
OBJ = $(SRC:.c=.o)
PREFIX = $(CURDIR)/bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	mkdir -p $(PREFIX)
	cp $(TARGET) $(PREFIX)/$(TARGET)
	@echo "Installed to  $(PREFIX)/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	@echo "Uninstalled from  $(PREFIX)/$(TARGET)"

clean:
	rm -f $(OBJ) $(TARGET)
