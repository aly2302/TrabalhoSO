# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
BUILD_DIR = build

# Diretórios
SRC_DIR = src
INCLUDE_DIR = include

# Arquivos-fonte e cabeçalhos
SRC_FEED = $(SRC_DIR)/feed.c $(SRC_DIR)/utils.c
SRC_MANAGER = $(SRC_DIR)/manager.c $(SRC_DIR)/utils.c
INCLUDES = -I$(INCLUDE_DIR)

# Binários
MANAGER = $(BUILD_DIR)/manager
FEED = $(BUILD_DIR)/feed

# Alvo principal
all: $(BUILD_DIR) $(MANAGER) $(FEED)

# Criação do diretório de build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compilar o manager
$(MANAGER): $(SRC_MANAGER)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Compilar o feed
$(FEED): $(SRC_FEED)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Limpeza dos binários
clean:
	rm -rf $(BUILD_DIR)

# Declaração de alvos como phony
.PHONY: all clean
