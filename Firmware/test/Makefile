# Host-side unit test build (x86, gcc)
# Usage:
#   make              — build and run all test suites
#   make test_tmc5160 — build single suite
#   make clean        — remove build artefacts
#   make unity        — download Unity if not present

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -g \
           -DUNIT_TEST \
           -I../App/hal \
           -I../App/drv \
           -I../App/app \
           -Imocks \
           -Iunity

BUILD   := build
UNITY_C := unity/unity.c
MOCKS_C := mocks/mock_spi.c \
           mocks/mock_i2c.c \
           mocks/mock_adc.c \
           mocks/mock_pwm.c \
           mocks/mock_gpio.c \
           mocks/mock_can.c \
           mocks/mock_flash.c

# -----------------------------------------------------------------------
# Test suites — add an entry here for each new phase
# -----------------------------------------------------------------------
SUITES := test_tmc5160 \
          test_canopen_pdo \
          test_homing \
          test_fault \
          test_tof \
          test_servo \
          test_dotstar \
          test_node_config \
          test_flash

BINS := $(addprefix $(BUILD)/, $(SUITES))

.PHONY: all clean unity $(SUITES)

all: unity $(BINS)
	@echo ""
	@echo "=== Running all test suites ==="
	@mkdir -p $(BUILD); \
	failed=0; \
	for bin in $(BINS); do \
	    echo ""; \
	    echo "--- $$bin ---"; \
	    suite=$$(basename $$bin); \
	    t0=$$(date +%s%3N); \
	    ./$$bin > $(BUILD)/$${suite}.txt 2>&1; rc=$$?; \
	    t1=$$(date +%s%3N); \
	    cat $(BUILD)/$${suite}.txt; \
	    [ $$rc -eq 0 ] || failed=1; \
	    python3 unity_to_junit.py $(BUILD)/$${suite}.xml $${suite} $$((t1 - t0)) \
	        < $(BUILD)/$${suite}.txt; \
	done; \
	echo ""; \
	[ $$failed -eq 0 ] && echo "All tests passed." || (echo "SOME TESTS FAILED" && exit 1)

$(BUILD):
	mkdir -p $(BUILD)

# -----------------------------------------------------------------------
# Per-suite build rules
# -----------------------------------------------------------------------

$(BUILD)/test_node_config: test_node_config.c \
                            $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_flash: test_flash.c \
                     ../App/app/cfg_flash.c \
                     $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_tof: test_tof.c \
                   ../App/drv/drv_tof.c \
                   $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_servo: test_servo.c \
                     ../App/drv/drv_servo.c \
                     $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_dotstar: test_dotstar.c \
                       ../App/drv/drv_dotstar.c \
                       $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_fault: test_fault.c \
                     ../App/app/app_canopen.c \
                     ../App/drv/drv_tmc5160.c \
                     $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_tmc5160: test_tmc5160.c \
                        ../App/drv/drv_tmc5160.c \
                        $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_canopen_pdo: test_canopen_pdo.c \
                            ../App/app/app_canopen.c \
                            ../App/drv/drv_tmc5160.c \
                            $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_homing: test_homing.c \
                      ../App/app/app_homing.c \
                      ../App/app/app_canopen.c \
                      ../App/drv/drv_tmc5160.c \
                      $(MOCKS_C) $(UNITY_C) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# -----------------------------------------------------------------------
# Run individual suite
# -----------------------------------------------------------------------

$(SUITES): %: $(BUILD)/%
	./$<

# -----------------------------------------------------------------------
# Unity — download from GitHub if not present
# -----------------------------------------------------------------------

UNITY_URL := https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src

unity: unity/unity.c unity/unity.h unity/unity_internals.h

unity/unity.c:
	mkdir -p unity
	curl -fsSL $(UNITY_URL)/unity.c -o unity/unity.c

unity/unity.h:
	mkdir -p unity
	curl -fsSL $(UNITY_URL)/unity.h -o unity/unity.h

unity/unity_internals.h:
	mkdir -p unity
	curl -fsSL $(UNITY_URL)/unity_internals.h -o unity/unity_internals.h

# -----------------------------------------------------------------------

clean:
	rm -rf $(BUILD)
