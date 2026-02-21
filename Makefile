CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS := -lncurses

SRC := \
	modern/connect-four-virus.c \
	modern/connect_four.c \
	modern/connect_four_ai.c
BUILD_DIR := build-modern
BIN := $(BUILD_DIR)/connect-four-virus

.PHONY: all run clean help

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: $(BIN)
	@echo "Running Connect Four Virus. Press q to quit."
	@$(BIN)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  make        Build modern terminal game in $(BUILD_DIR)/"
	@echo "  make run    Build and play Connect Four Virus"
	@echo "  make clean  Remove build artifacts"
