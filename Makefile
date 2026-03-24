CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -Iinclude
SRC := \
	src/main.c \
	src/config.c \
	src/logger.c \
	src/error.c \
	src/socket_utils.c \
	src/proxy_server.c
OBJ := $(SRC:.c=.o)
TARGET := proxy_server

ifeq ($(OS),Windows_NT)
TARGET_BIN := $(TARGET).exe
LDLIBS := -lws2_32
else
RM := rm -f
TARGET_BIN := $(TARGET)
LDLIBS :=
endif

.PHONY: all clean

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET_BIN) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-cmd /C "if exist src\\main.o del /Q src\\main.o"
	-cmd /C "if exist src\\config.o del /Q src\\config.o"
	-cmd /C "if exist src\\logger.o del /Q src\\logger.o"
	-cmd /C "if exist src\\error.o del /Q src\\error.o"
	-cmd /C "if exist src\\socket_utils.o del /Q src\\socket_utils.o"
	-cmd /C "if exist src\\proxy_server.o del /Q src\\proxy_server.o"
	-cmd /C "if exist $(TARGET_BIN) del /Q $(TARGET_BIN)"
else
	$(RM) $(OBJ) $(TARGET_BIN)
endif
