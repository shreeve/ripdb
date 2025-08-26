CC = cc
AR = ar
THREADS = -pthread
OPT = -O2 -g
WARN = -Wall -Wextra -Wno-unused-parameter -Wuninitialized
CFLAGS = $(THREADS) $(OPT) $(WARN) -Iinclude
LDFLAGS =

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

SRCS = $(SRC_DIR)/ripdb.c
OBJS = $(BUILD_DIR)/ripdb.o

STATIC_LIB = $(BUILD_DIR)/libripdb.a
DYLIB = $(BUILD_DIR)/libripdb.dylib

UNAME_S := $(shell uname -s)

all: $(STATIC_LIB) $(DYLIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/ripdb.o: $(SRC_DIR)/ripdb.c $(INCLUDE_DIR)/ripdb.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(STATIC_LIB): $(OBJS)
	$(AR) rs $@ $(OBJS)

ifeq ($(UNAME_S),Darwin)
  SOFLAGS = -dynamiclib -Wl,-install_name,@rpath/libripdb.dylib
else
  SOFLAGS = -shared
endif

$(DYLIB): $(OBJS)
	$(CC) $(SOFLAGS) $(THREADS) -o $@ $(OBJS) $(LDFLAGS)


.PHONY: tools
tools: $(BUILD_DIR) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/ripdb_stat tools/ripdb_stat.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/ripdb_dump tools/ripdb_dump.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/ripdb_load tools/ripdb_load.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/ripdb_copy tools/ripdb_copy.c $(STATIC_LIB)

.PHONY: tests

tests: clean tools $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-01 tests/cursors_basic.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-02 tests/secondary_db.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-03 tests/cursors_random.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-04 tests/put_get.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test-05 tests/cursors_delete.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -Iinclude -o $(BUILD_DIR)/test-06 tests/btree_split_merge.c $(STATIC_LIB)

	# reset tests/db and run tests
	rm -rf tests/db && mkdir -p tests/db
	./build/test-01 && ./build/ripdb_stat tests/db
	./build/test-02
	./build/test-03
	./build/test-04
	./build/test-05
	./build/test-06

	# tools round-trip: dump -> load -> copy -> stat
	rm -rf tests/db_loaded tests/db_copy && mkdir -p tests/db_loaded tests/db_copy
	./build/ripdb_dump tests/db > $(BUILD_DIR)/dump.txt
	./build/ripdb_load -f $(BUILD_DIR)/dump.txt tests/db_loaded
	./build/ripdb_copy tests/db tests/db_copy
	./build/ripdb_stat tests/db_loaded
	./build/ripdb_stat tests/db_copy

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) tests/db tests/db_loaded tests/db_copy
