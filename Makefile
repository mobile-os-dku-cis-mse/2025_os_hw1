TARGET = goronoSH
CC = gcc
CFLAGS = -Wall -Wextra -g

SRC_DIR = src
BIN_DIR = bin

SRC = $(SRC_DIR)/main.c
OBJ = $(BIN_DIR)/goronoSH.o

PREFIX = $(BIN_DIR)

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

install: $(BIN_DIR)/$(TARGET)
	cp $(BIN_DIR)/$(TARGET) $(PREFIX)/$(TARGET)

uninstall:
	rm -f $(PREFIX)/$(TARGET)

clean:
	rm -f $(OBJ) $(BIN_DIR)/$(TARGET)
