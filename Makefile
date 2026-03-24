CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -Iinclude
SRC := \
	src/main.c \
	src/config.c \
	src/logger.c \
	src/error.c \
	src/error_response.c \
	src/request_parser.c \
	src/cache.c \
	src/lru.c \
	src/metrics.c \
	src/socket_utils.c \
	src/thread_pool.c \
	src/response_forwarder.c \
	src/proxy_server.c
OBJ := $(SRC:.c=.o)
TARGET := proxy_server

TARGET_BIN := $(TARGET).exe
LDLIBS := -lws2_32

.PHONY: all clean test

TEST_TARGET := tests/test_request_parser.exe
TEST_OBJ := tests/test_request_parser.o src/request_parser.o
TEST_LRU_TARGET := tests/test_lru.exe
TEST_LRU_OBJ := tests/test_lru.o src/lru.o
TEST_CACHE_TARGET := tests/test_cache.exe
TEST_CACHE_OBJ := tests/test_cache.o src/cache.o src/lru.o
TEST_METRICS_TARGET := tests/test_metrics.exe
TEST_METRICS_OBJ := tests/test_metrics.o src/metrics.o src/logger.o

all: $(TARGET_BIN)

test: $(TEST_TARGET) $(TEST_LRU_TARGET) $(TEST_CACHE_TARGET) $(TEST_METRICS_TARGET)
	.\$(TEST_TARGET)
	.\$(TEST_LRU_TARGET)
	.\$(TEST_CACHE_TARGET)
	.\$(TEST_METRICS_TARGET)

$(TARGET_BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET_BIN) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $(TEST_TARGET)

$(TEST_LRU_TARGET): $(TEST_LRU_OBJ)
	$(CC) $(CFLAGS) $(TEST_LRU_OBJ) -o $(TEST_LRU_TARGET)

$(TEST_CACHE_TARGET): $(TEST_CACHE_OBJ)
	$(CC) $(CFLAGS) $(TEST_CACHE_OBJ) -o $(TEST_CACHE_TARGET)

$(TEST_METRICS_TARGET): $(TEST_METRICS_OBJ)
	$(CC) $(CFLAGS) $(TEST_METRICS_OBJ) -o $(TEST_METRICS_TARGET)

clean:
	-cmd /C "if exist src\\main.o del /Q src\\main.o"
	-cmd /C "if exist src\\config.o del /Q src\\config.o"
	-cmd /C "if exist src\\logger.o del /Q src\\logger.o"
	-cmd /C "if exist src\\error.o del /Q src\\error.o"
	-cmd /C "if exist src\\error_response.o del /Q src\\error_response.o"
	-cmd /C "if exist src\\request_parser.o del /Q src\\request_parser.o"
	-cmd /C "if exist src\\cache.o del /Q src\\cache.o"
	-cmd /C "if exist src\\lru.o del /Q src\\lru.o"
	-cmd /C "if exist src\\socket_utils.o del /Q src\\socket_utils.o"
	-cmd /C "if exist src\\thread_pool.o del /Q src\\thread_pool.o"
	-cmd /C "if exist src\\response_forwarder.o del /Q src\\response_forwarder.o"
	-cmd /C "if exist src\\proxy_server.o del /Q src\\proxy_server.o"
	-cmd /C "if exist tests\\test_request_parser.o del /Q tests\\test_request_parser.o"
	-cmd /C "if exist tests\\test_lru.o del /Q tests\\test_lru.o"
	-cmd /C "if exist tests\\test_cache.o del /Q tests\\test_cache.o"
	-cmd /C "if exist tests\\test_metrics.o del /Q tests\\test_metrics.o"
	-cmd /C "if exist tests\\test_request_parser.exe del /Q tests\\test_request_parser.exe"
	-cmd /C "if exist tests\\test_lru.exe del /Q tests\\test_lru.exe"
	-cmd /C "if exist tests\\test_cache.exe del /Q tests\\test_cache.exe"
	-cmd /C "if exist tests\\test_metrics.exe del /Q tests\\test_metrics.exe"
	-cmd /C "if exist $(TARGET_BIN) del /Q $(TARGET_BIN)"
