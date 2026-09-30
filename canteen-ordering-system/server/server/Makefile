CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Iinclude -I../common/include
BUILD_DIR := build
CORE_SRC := src/menu.c src/cart.c src/order_service.c
TEST_SRC := tests/test_menu.c tests/test_cart.c

.PHONY: all server test test-multi clean

all: server

server: $(BUILD_DIR)/order_server

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/order_server: $(CORE_SRC) src/server_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CORE_SRC) src/server_main.c -o $@

test: $(BUILD_DIR)/server_tests $(BUILD_DIR)/test_remove_one
	./$(BUILD_DIR)/server_tests
	./$(BUILD_DIR)/test_remove_one

test-multi: server
	bash tests/test_multi_client.sh

$(BUILD_DIR)/server_tests: $(CORE_SRC) $(TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CORE_SRC) $(TEST_SRC) -o $@

$(BUILD_DIR)/test_remove_one: src/cart.c tests/test_remove_one.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/cart.c tests/test_remove_one.c -o $@

clean:
	rm -rf $(BUILD_DIR)
