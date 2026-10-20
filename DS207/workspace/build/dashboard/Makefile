CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O0 -g
SRCS := $(filter-out src/main.c,$(wildcard src/*.c))
TESTS := $(wildcard test/test_*.c)
BIN := build-native

.PHONY: test-native collector build-ppc clean
test-native:
	@mkdir -p $(BIN)
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  $(CC) $(CFLAGS) -DCOLLECTOR_NO_MAIN -I src -I test -o $(BIN)/$$n $$t $(SRCS); \
	  ./$(BIN)/$$n; \
	done

collector:
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -I src -o $(BIN)/collector $(filter-out src/uiserver.c src/auth.c src/http.c,$(SRCS))

build-ppc:
	docker run --rm -v $(abspath ../..):/w -v ds207vol:/out ds207-zig bash /w/build/scripts/build_dashboard_ppc.sh

clean:
	rm -rf $(BIN)
