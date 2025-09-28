TARGET = goronoSH
CC = gcc
CFLAGS = -Wall -Wextra -g

SRC_DIR = src
BIN_DIR = bin

SRC = $(SRC_DIR)/main.c
OBJ = $(BIN_DIR)/main.o

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/main.o: $(SRC_DIR)/main.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

install: $(BIN_DIR)/$(TARGET)
	cp $(BIN_DIR)/$(TARGET) $(BIN_DIR)/$(TARGET)

uninstall:
	rm -f $(BIN_DIR)/$(TARGET)

clean:
	rm -f $(OBJ) $(BIN_DIR)/$(TARGET)
