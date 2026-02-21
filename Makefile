CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS := -lncurses

SRC := modern/sillyballs_modern.c
BUILD_DIR := build-modern
BIN := $(BUILD_DIR)/sillyballs_modern

.PHONY: all run clean help

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: $(BIN)
	@echo "Running modern SillyBalls clone. Press any key to quit."
	@$(BIN)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  make        Build modern terminal version in $(BUILD_DIR)/"
	@echo "  make run    Build and run"
	@echo "  make clean  Remove build artifacts"
