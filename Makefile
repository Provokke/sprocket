CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g
SAN := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
INC := -Iinclude -Itests

LIB_SRC := $(wildcard src/*.c)
TEST_SRC := $(wildcard tests/*.c)

all: build/sprocket_tests

build/sprocket_tests: $(LIB_SRC) $(TEST_SRC) include/sprocket.h tests/sptest.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) $(LIB_SRC) $(TEST_SRC) -o $@

build/sprocket_tests_san: $(LIB_SRC) $(TEST_SRC) include/sprocket.h tests/sptest.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SAN) $(INC) $(LIB_SRC) $(TEST_SRC) -o $@

test: build/sprocket_tests
	./build/sprocket_tests

sanitize: build/sprocket_tests_san
	./build/sprocket_tests_san

clean:
	rm -rf build

.PHONY: all test sanitize clean
