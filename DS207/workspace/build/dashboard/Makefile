CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O0 -g
SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
TESTS := $(wildcard test/test_*.c)
BIN := build-native

.PHONY: test-native build-ppc clean
test-native:
	@mkdir -p $(BIN)
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  $(CC) $(CFLAGS) -I src -I test -o $(BIN)/$$n $$t $(SRCS); \
	  ./$(BIN)/$$n; \
	done

build-ppc:
	sh scripts/build_dashboard_ppc.sh

clean:
	rm -rf $(BIN)
