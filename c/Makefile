# C translation of timberwoof/PDP11
#
#   make          build the library, runpdp11, hello_world and the tests
#   make test     build and run every test program
#   make run      run the emulator (M9301-YA console) on this terminal
#   make hello    run the hello-world program
#
# The programs open config_default.json and source/M9301-YA.txt by relative
# path, as the Python does, so `make test`, `make run` and `make hello` run
# them from the repository root (the parent of this directory).

CC      ?= cc

# Warnings this code deliberately triggers, turned off only where the
# compiler knows them (GCC and clang name the initializer one differently,
# and older clang has no format-truncation warning).  Each is tested in its
# positive -W form, because GCC silently accepts unknown -Wno- options.
cc_warning = $(shell $(CC) -Werror -W$(1) -x c -c /dev/null -o /dev/null >/dev/null 2>&1 && echo -Wno-$(1))
WARN_OFF := $(call cc_warning,override-init) $(call cc_warning,initializer-overrides) \
            $(call cc_warning,format-truncation)

# macOS hides clock_gettime and CLOCK_MONOTONIC when only _POSIX_C_SOURCE is set.
ifeq ($(shell uname -s),Darwin)
PLATFORM_DEFS := -D_DARWIN_C_SOURCE
endif

CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Werror $(WARN_OFF) -D_POSIX_C_SOURCE=200809L $(PLATFORM_DEFS)
LDLIBS  += -lpthread

ROOT    := ..
BUILD   := build

LIB_SRC := src/pdp11_logger.c src/pdp11_util.c src/pdp11_config.c src/stopwatches.c \
           src/pdp11_hardware.c src/pdp11_br_ops.c src/pdp11_cc_ops.c src/pdp11_noopr_ops.c \
           src/pdp11_other_ops.c src/pdp11_ss_ops.c src/pdp11_ssdd_ops.c src/pdp11_rss_ops.c \
           src/pdp11_boot.c src/pdp11_dl11.c src/pdp11_m9301.c src/pdp11_rk11.c \
           src/pdp11_terminal.c src/pdp11.c
LIB_OBJ := $(LIB_SRC:src/%.c=$(BUILD)/%.o)
LIB     := $(BUILD)/libpdp11.a

TESTS   := test_utils test_hardware_reg test_hardware_ram test_hardware_psw test_address_modes \
           test_condition_code_ops test_stack test_rss_ops test_ss_ops test_ssdd_ops test_jump \
           test_config test_DL11 test_hello_world test_implemented
TEST_BIN := $(TESTS:%=$(BUILD)/%)

PROGRAMS := $(BUILD)/runpdp11 $(BUILD)/hello_world

all: $(LIB) $(PROGRAMS) $(TEST_BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c src/*.h | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

$(BUILD)/%.o: tests/%.c tests/test.h src/*.h | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -Itests -c $< -o $@

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/runpdp11: $(BUILD)/runpdp11.o $(LIB)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/hello_world: $(BUILD)/hello_world.o $(LIB)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/test_%: $(BUILD)/test_%.o $(LIB)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test: $(TEST_BIN)
	@status=0; for t in $(TESTS); do (cd $(ROOT) && c/$(BUILD)/$$t) || status=1; done; exit $$status

run: $(BUILD)/runpdp11
	cd $(ROOT) && c/$(BUILD)/runpdp11

hello: $(BUILD)/hello_world
	cd $(ROOT) && c/$(BUILD)/hello_world

clean:
	rm -rf $(BUILD)

.PHONY: all test run hello clean
.SECONDARY:

# Differential test against the Python: random single instructions through both.
# Matches exactly at tag c-translation-faithful; on the fixed code it lists
# the cases the bug fixes change.
#   make difftest            (needs python3; run from this directory)
$(BUILD)/diff_ops: tests/differential/diff_ops.c tests/fixture.h $(LIB)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(LIB) $(LDLIBS) -o $@

DIFF_CASES ?= 3000
DIFF_SEED  ?= 1
difftest: $(BUILD)/diff_ops
	cd $(ROOT) && python3 c/tests/differential/gen_cases.py $(DIFF_CASES) $(DIFF_SEED) > c/$(BUILD)/cases.txt
	$(BUILD)/diff_ops < $(BUILD)/cases.txt

.PHONY: difftest

# Comparison with SIMH (an independent PDP-11 simulator, 11/03 + EIS):
#   make simhcheck SIMH=/path/to/simh/BIN/pdp11
$(BUILD)/simh_ops: tests/differential/simh_ops.c tests/fixture.h $(LIB)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(LIB) $(LDLIBS) -o $@

SIMH_CASES ?= 3000
SIMH_SEED  ?= 1
simhcheck: $(BUILD)/simh_ops
	@test -n "$(SIMH)" || { echo "set SIMH=/path/to/simh/BIN/pdp11"; exit 2; }
	python3 tests/differential/simh_check.py $(SIMH) $(BUILD)/simh_ops $(SIMH_CASES) $(SIMH_SEED)

.PHONY: simhcheck
