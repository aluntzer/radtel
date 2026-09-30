# harness for the radtel-gtk3 spectrum widget fit-selection path
#
# builds the real widget sources out of the repository (no autotools, so no
# in-tree generated files are touched) and links them against a libFuzzer
# entry point and a deterministic scenario runner.

R      := $(abspath $(CURDIR)/../..)
BUILD  ?= build

PKGS   := gtk+-3.0 cairo glib-2.0 gio-2.0 gmodule-2.0

CFLAGS := -g -O1 -fno-omit-frame-pointer \
	  -fsanitize=address,undefined -fno-sanitize-recover=all \
	  -fno-common -std=gnu11 \
	  -I$(R)/src/include \
	  -I$(R)/src/client/include \
	  -I$(R)/src/client/widgets/include \
	  -I$(R)/src/client/widgets/spectrum \
	  -I$(R)/src/client/widgets/util \
	  -Wno-unused-const-variable

CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS := -fsanitize=address,undefined $(shell pkg-config --libs $(PKGS)) -lm

SRC := \
	$(R)/src/client/widgets/xyplot/xyplot.c \
	$(R)/src/client/widgets/spectrum/spectrum.c \
	$(R)/src/client/widgets/util/desclabel.c \
	$(R)/src/client/widgets/util/default_grid.c \
	$(R)/src/util/coordinates.c \
	$(R)/src/util/levmar.c \
	$(R)/src/util/fitfunc.c \
	$(R)/src/util/fourier_transform.c \
	$(R)/src/net/protocol.c \
	$(wildcard $(R)/src/net/cmds/*.c) \
	$(wildcard $(R)/src/net/acks/*.c) \
	$(wildcard $(R)/src/client/sig/*.c) \
	$(R)/src/client/proc/proc_pr_spec_data.c

OBJ := $(patsubst $(R)/%.c,$(BUILD)/%.o,$(SRC)) \
	$(BUILD)/harness.o $(BUILD)/harness_stubs.o

FUZZ_OBJ := $(BUILD)/fuzz_spectrum.o
REPRO_OBJ := $(BUILD)/repro.o

SCENARIOS := nan-graph nan-graph-head nan-graph-tail nan-graph-2 nan-graph-1 \
	     clear-plot clear-plot-one clear-plot-mid \
	     flat-selection zero-band few-points select-all refit-loop \
	     refit-once refit-on-packet stale-scale degenerate-fit \
	     style-sweep menu-drop persistence-churn short-packet \
	     fit-graph-leak fit-menu-refit fit-ref-stale fit-deferred

.PHONY: all clean fuzz repro check

all: fuzz repro

$(BUILD)/%.o: $(R)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/harness_stubs.o: harness_stubs.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/harness.o: harness.c harness.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/fuzz_spectrum.o: fuzz_spectrum.c harness.h
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -fsanitize=fuzzer,address,undefined \
		-fno-sanitize-recover=all -c -o $@ $<

$(BUILD)/repro.o: repro.c harness.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

fuzz: $(FUZZ_OBJ) $(OBJ)
	clang $(LDFLAGS) -fsanitize=fuzzer -o fuzz_spectrum $^

repro: $(REPRO_OBJ) $(OBJ)
	$(CC) $(LDFLAGS) -o repro $^

# needs a display; wrap in xvfb-run on a headless host, e.g.
#	make check UNDER="xvfb-run -a --server-args=-screen 0 1280x1024x24"
UNDER ?=
# leak checking is off: the widget sources are never torn down, and GTK,
# pango and fontconfig retain their own global allocations for the lifetime
# of the process
RUNENV := ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1
check: repro
	@rc=0; \
	for s in $(SCENARIOS); do \
		$(RUNENV) $(UNDER) ./repro $$s > $(BUILD)/$$s.log 2>&1; \
		if [ $$? -eq 0 ]; then \
			echo "selftests: $$s [PASS]"; \
		else \
			echo "selftests: $$s [FAIL] (see $(BUILD)/$$s.log)"; \
			rc=1; \
		fi; \
	done; \
	exit $$rc

clean:
	rm -rf $(BUILD) fuzz_spectrum repro
