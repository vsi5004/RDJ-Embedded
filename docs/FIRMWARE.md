# Firmware Architecture

## Overview

All five controller boards run the same firmware binary. Behavior is selected by the 3-bit DIP switch (node ID) read at boot. The firmware is a bare-metal super-loop with CANopenNode as the communication stack. There is no RTOS. MCUs are dumb I/O nodes — they receive commands over CAN, actuate peripherals, read sensors, and report status. All operational intelligence lives on the SBC.

## Development Methodology — Test-Driven

All firmware modules are developed test-first. Unit tests run on the host machine (x86, gcc) without any target hardware. A thin HAL abstraction seam separates driver logic from STM32 register access. Driver code is identical on host and target — only the lowest-level HAL implementation differs.

**Test framework**: Unity (single-header C test framework). Mocks are hand-written — the HAL seam is thin enough that auto-generation (CMock) is unnecessary.

**Development cycle for each module**:
1. Write failing tests that define the expected behavior
2. Implement the driver/module to pass the tests
3. Run `make -C test/` to verify (builds and runs in under a second)
4. Flash to Nucleo for hardware validation

**Development phase order** (each builds on the previous):

| Phase | Tests | Module | Validates |
|-------|-------|--------|-----------|
| 1 | Register r/w, target, status, StallGuard, init | `drv_tmc5160.c` | SPI protocol, bit packing, all register access |
| 2 | RPDO staging, SYNC application, TPDO latching, status word | `app_canopen.c` | Two-stage staging, atomic SYNC, status bits |
| 3 | Homing state machine, pot direction, endstop zero | `app_homing.c` | Per-axis homing, A axis pot logic |
| 4 | Fault detection, fault-blocks-motion, clear-fault, EMCY | fault paths in `app_canopen.c` | EMCY generation, status word, recovery |
| 5 | ToF ranging, servo clamping, DotStar framing | `drv_tof.c`, `drv_servo.c`, `drv_dotstar.c` | I2C protocol, PWM limits, LED format |
| 6 | Node config macros, flash roundtrip, corruption recovery | `node_config.h`, `cfg_flash.c` | Population logic, double-buffer integrity |

## Build System and Project Structure

Two build targets from the same source tree:
- **Target build**: arm-none-eabi-gcc / STM32CubeIDE — produces the binary for the STM32G431
- **Test build**: host gcc — compiles driver logic + mocks + Unity, runs on x86

```
firmware/
├── Core/                        # CubeMX generated HAL init (target only)
├── Drivers/                     # STM32 HAL + CMSIS (target only)
├── Lib/
│   └── CANopenNode/             # git submodule
├── App/
│   ├── hal/                     # Hardware abstraction seam
│   │   ├── hal_spi.h            # SPI interface (function pointer struct)
│   │   ├── hal_spi_stm32.c      # Real STM32 implementation (target only)
│   │   ├── hal_i2c.h
│   │   ├── hal_i2c_stm32.c
│   │   ├── hal_adc.h
│   │   ├── hal_adc_stm32.c
│   │   ├── hal_pwm.h
│   │   ├── hal_pwm_stm32.c
│   │   ├── hal_gpio.h
│   │   ├── hal_gpio_stm32.c
│   │   ├── hal_can.h
│   │   └── hal_can_stm32.c
│   ├── drv/                     # Drivers — pure logic, testable on host
│   │   ├── drv_tmc5160.c        # SPI register r/w, ramp config, StallGuard
│   │   ├── drv_tmc5160.h
│   │   ├── drv_tof.c            # I2C ranging start/read (non-blocking)
│   │   ├── drv_tof.h
│   │   ├── drv_servo.c          # PWM pulse width control
│   │   ├── drv_servo.h
│   │   ├── drv_dotstar.c        # SPI2 LED frame output
│   │   ├── drv_dotstar.h
│   │   └── drv_pot_adc.c        # ADC read + oversampling + filtering
│   ├── app/                     # Application logic — testable on host
│   │   ├── app_main.c           # Super-loop, boot sequence
│   │   ├── app_canopen.c        # OD definition, PDO callbacks, NMT hooks
│   │   ├── app_canopen.h
│   │   ├── app_homing.c         # Per-axis homing state machine
│   │   ├── app_homing.h
│   │   ├── cfg_flash.c          # Double-buffer flash config storage
│   │   └── node_config.h        # Node ID → peripheral enable mapping
│   └── OD/
│       ├── OD.h                 # Generated from EDS or hand-written
│       └── OD.c
├── test/                        # Host-side unit tests (x86, gcc)
│   ├── unity/                   # Unity test framework (vendored or submodule)
│   │   ├── unity.c
│   │   └── unity.h
│   ├── mocks/                   # Mock HAL implementations for testing
│   │   ├── mock_spi.c           # Records SPI writes, returns controlled data
│   │   ├── mock_spi.h
│   │   ├── mock_i2c.c
│   │   ├── mock_i2c.h
│   │   ├── mock_adc.c
│   │   ├── mock_adc.h
│   │   ├── mock_can.c
│   │   ├── mock_pwm.c
│   │   ├── mock_gpio.c
│   │   └── mock_flash.c
│   ├── test_tmc5160.c           # TMC5160 SPI driver tests (Phase 1)
│   ├── test_canopen_pdo.c       # PDO staging + SYNC application (Phase 2)
│   ├── test_homing.c            # Per-axis homing state machine (Phase 3)
│   ├── test_fault.c             # Fault detection and EMCY (Phase 4)
│   ├── test_tof.c               # ToF sensor I2C tests (Phase 5)
│   ├── test_servo.c             # Servo PWM tests (Phase 5)
│   ├── test_dotstar.c           # DotStar LED tests (Phase 5)
│   ├── test_pot_adc.c           # Potentiometer ADC tests (Phase 5)
│   ├── test_node_config.c       # Node ID mapping tests (Phase 6)
│   ├── test_flash.c             # Flash double-buffer tests (Phase 6)
│   └── Makefile                 # Builds and runs all tests on host
├── Makefile                     # Target build (arm-none-eabi-gcc)
└── vinyl_robot.ioc              # STM32CubeMX project
```

## HAL Abstraction Seam

The seam between testable logic and hardware is a struct of function pointers for each peripheral:

```c
// hal/hal_spi.h
typedef struct {
    int (*transfer)(uint8_t *tx, uint8_t *rx, uint16_t len);
} hal_spi_t;

extern hal_spi_t hal_spi1;  // Global instance — real or mock
```

On target, `hal_spi_stm32.c` sets the function pointer to the real STM32 HAL call. In tests, `mock_spi.c` provides a version that records what was transmitted and returns controlled responses.

Drivers accept the HAL instance at init and never include STM32 headers directly:

```c
// drv/drv_tmc5160.c
#include "drv_tmc5160.h"
#include "hal_spi.h"

static hal_spi_t *spi;

void tmc5160_init(hal_spi_t *spi_instance) {
    spi = spi_instance;
}

void tmc5160_write_register(uint8_t reg, uint32_t value) {
    uint8_t tx[5] = {
        reg | 0x80,
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    };
    uint8_t rx[5];
    spi->transfer(tx, rx, 5);
}
```

The `UNIT_TEST` preprocessor flag (set by the test Makefile, not by the target build) guards any remaining target-specific includes:

```c
#ifndef UNIT_TEST
#include "stm32g4xx_hal.h"
#endif
```

## Test Build

```bash
# Run all tests (from repo root)
make -C test/

# Run a single test suite
make -C test/ test_tmc5160 && test/test_tmc5160

# Clean
make -C test/ clean
```

The test Makefile compiles driver sources + mock HAL + Unity + test files with host gcc. Each test binary runs independently and exits with 0 on pass, non-zero on failure. Total test execution is under 1 second.

## Node Configuration

```c
// App/app/node_config.h
#define NODE_HAS_TMC(id)    ((id) >= 1 && (id) <= 3)
#define NODE_HAS_SERVO(id)  ((id) >= 4)
#define NODE_HAS_TOF(id)    ((id) != 5)
#define NODE_HAS_POT(id)    ((id) == 3)
#define NODE_HAS_LED(id)    ((id) == 3 || (id) == 5)
```

## Boot Sequence

```
1. HAL_Init() — clocks to 170MHz via HSI/PLL, SysTick
2. Read DIP switch (PC13/14/15) → node_id
3. Conditional peripheral init based on node_id:
   - if (node_id <= 0x03)  init_spi1_tmc5160();
   - if (node_id == 0x03)  init_adc_pot();
   - if (node_id == 0x03 || node_id == 0x05)  init_spi2_dotstar();
   - if (node_id >= 0x04)  init_pwm_servos();
   - if (node_id != 0x05)  init_i2c_tof();
4. load_config_from_flash(node_id);
5. CO_init() — start CANopenNode stack, enter NMT pre-operational
6. Start IWDG (100ms timeout)
7. Enter main super-loop
```

## Main Super-Loop

```c
while (1) {
    // 1. Service CANopen stack
    CO_process();  // NMT, SDO, heartbeat, emergency — ~5-20μs

    // 2. On SYNC callback (set by ISR flag)
    if (sync_received) {
        latch_tpdo_data();      // Copy sensor readings into TPDO buffers
        apply_queued_rpdo();    // Write staged commands to actuators
        sync_received = false;
    }

    // 3. TMC5160 polling (stepper nodes only)
    if (NODE_HAS_TMC(node_id)) {
        tmc5160_poll();  // Read XACTUAL, RAMPSTAT, SG_RESULT via SPI ~30μs
        if (rampstat_has_event()) {
            update_status_word();
            if (stallguard_triggered() || unexpected_endstop()) {
                send_emcy();
            }
        }
    }

    // 4. Sensor polling (10Hz, triggered by TIM7 flag)
    if (sensor_poll_flag) {
        if (NODE_HAS_TOF(node_id))  tof_read_distance();   // I2C ~100μs
        if (NODE_HAS_POT(node_id))  pot_read_angle();       // ADC register read
        if (NODE_HAS_LED(node_id))  dotstar_update();       // SPI2 if pattern changed
        sensor_poll_flag = false;
    }

    // 5. Servo update (servo nodes only)
    if (NODE_HAS_SERVO(node_id) && servo_target_changed) {
        servo_update_pwm();  // Write TIM3 CCR registers
        servo_target_changed = false;
    }

    // 6. Watchdog and diagnostics
    IWDG_kick();
    toggle_status_led();  // Heartbeat blink
    if (NODE_HAS_POT(node_id))  pot_sanity_check();  // Compare pot vs XACTUAL
}
```

Worst-case loop iteration: ~400μs → natural loop rate ~2.5kHz, well above 50Hz SYNC.

## Interrupt Context

Minimal — ISRs set flags, all heavy work in main loop.

| ISR | Handler | Purpose |
|-----|---------|---------|
| FDCAN1_IT0 | CO_CANinterrupt() | CAN frame reception, CANopenNode processing |
| SysTick | HAL_IncTick() + CO timing | 1ms tick for HAL delays and CANopenNode timers |
| TIM7 | Set sensor_poll_flag | 10Hz trigger for ToF/pot/LED polling |

**Design rule**: ISRs set flags only. All SPI, I2C, and CAN TX happen in the main loop. No peripheral bus access from ISR context.

## TMC5160 SPI Driver (App/drv/drv_tmc5160)

The driver communicates with the TMC5160 via SPI1 at 4MHz. All register access is 40-bit transactions (8-bit address + 32-bit data). The driver uses the `hal_spi_t` abstraction — it never touches STM32 HAL directly, making it fully testable on the host.

### Key Operations

```c
// Write a register
void tmc5160_write(uint8_t reg, uint32_t value);

// Read a register (returns 32-bit value, requires two transactions)
uint32_t tmc5160_read(uint8_t reg);

// Configure motion parameters
void tmc5160_set_target(int32_t position);   // Write XTARGET
void tmc5160_set_velocity(uint32_t vmax);    // Write VMAX
void tmc5160_set_accel(uint16_t amax, uint16_t dmax);  // Write AMAX, DMAX
void tmc5160_set_rampmode(uint8_t mode);     // 0=position, 1=velocity+, 2=velocity-

// Read status
int32_t tmc5160_get_position(void);          // Read XACTUAL
uint8_t tmc5160_get_rampstat(void);          // Read RAMPSTAT
uint16_t tmc5160_get_stallguard(void);       // Read SG_RESULT

// Motor control
void tmc5160_disable(void);  // Write TOFF=0 in CHOPCONF
void tmc5160_enable(void);   // Restore TOFF from config
```

### Initialization Sequence

```c
void tmc5160_init(void) {
    // 1. Write GCONF — enable SPI mode, internal ramp generator
    // 2. Write CHOPCONF — set TOFF, HSTRT, HEND for motor tuning
    // 3. Write IHOLD_IRUN — set run current, hold current, hold delay
    // 4. Write SWMODE — enable stop_l_enable (endstop), sg_stop (StallGuard)
    // 5. Write SGTHRS — StallGuard threshold (from flash config)
    // 6. Write VMAX, AMAX, DMAX — default ramp parameters (from flash config)
    // 7. Write RAMPMODE = 0 — position mode
    // 8. Write XACTUAL = 0 — assume unhomed
}
```

### Homing (Stepper Nodes)

Homing is initiated by the SBC setting the "home" bit in the control word (OD 0x2004). The MCU runs the homing sequence locally:

**X and Z axes:**
1. Set TMC5160 to velocity mode at slow speed, toward endstop
2. TMC5160 hits endstop on REFL → stops internally (SWMODE stop_l_enable)
3. MCU reads XACTUAL, writes it to zero
4. Set status word "homed" bit
5. Return to position mode

**A axis:**
1. Read pot via ADC → determine which direction is shortest path to endstop
2. Set TMC5160 to velocity mode toward endstop in that direction
3. TMC5160 hits endstop → stops
4. Zero XACTUAL
5. Set "homed" bit

The SBC orchestrates the ORDER of homing (which axis homes first, moving to safe zones between steps). But the actual endstop-seek-and-zero on each individual axis is handled by the MCU firmware.

## RPDO/TPDO Callback Pattern

CANopenNode uses callbacks for OD entry writes. The firmware implements a two-stage staging pattern:

1. **RPDO write callback**: Copy received value into a shadow buffer. Set "pending" flag. Do NOT actuate yet.
2. **SYNC callback**: If pending flag is set, copy shadow buffer to live target. Execute actuation (SPI write to TMC5160, or PWM update for servos). Clear pending flag.

This ensures:
- Multi-field commands (target + velocity + accel) all apply atomically on the same SYNC
- No partial updates if the master sends fields across multiple CAN frames
- SYNC-gated start for coordinated multi-axis moves

## Fault Handling

When a fault is detected (StallGuard, unexpected endstop, sensor timeout, SPI failure):

1. Set fault bit in status word (OD 0x2003)
2. Send CANopen EMCY frame with error code
3. Hold current position (don't disable motor — maintain holding torque)
4. Ignore new motion commands until fault is cleared
5. Wait for SBC to send clear-fault via control word (OD 0x2004)

### EMCY Error Codes

| Code | Meaning |
|------|---------|
| 0x2100 | StallGuard triggered |
| 0x2200 | Unexpected endstop hit (not during homing) |
| 0x2300 | ToF reading out of expected range |
| 0x2400 | Pot vs XACTUAL divergence exceeds 5° (A axis) |
| 0x2500 | TMC5160 SPI communication failure |
| 0x2600 | Servo position feedback timeout |
| 0x5000 | CAN bus-off recovery |

## FDCAN Configuration

CAN 2.0B (classic CAN) at 1Mbit/s with 170MHz PCLK1:

- Prescaler: 10
- Time segment 1: 13
- Time segment 2: 3
- Sync jump width: 1
- → 17 time quanta per bit, 76.5% sample point

Two RX filters:
1. Accept standard frames matching node ID range (SDO, NMT directed)
2. Accept broadcast frames (SYNC, NMT broadcast, emergency)

## Development Notes

### Getting CANopenNode Running First

Before adding any motion code, get the CANopen stack running in isolation:
1. Init FDCAN with correct bit timing
2. Init CANopenNode with minimal OD (just mandatory objects)
3. Verify heartbeat visible on CAN bus (use Nucleo + CAN transceiver breakout)
4. Test SDO read/write of a single OD entry from a second Nucleo or PC with CAN adapter
5. Then add TMC5160 SPI driver, sensors, etc. incrementally

FDCAN filter configuration and timing register setup are the most common gotchas on STM32G4.

### StallGuard Tuning

StallGuard sensitivity needs different thresholds for:
- **Homing**: Very sensitive (high SGTHRS) — moving slowly with no load, any resistance = obstacle
- **Normal operation**: Less sensitive — carrying a record adds load, shouldn't false-trigger
- **Near turntable**: May need adjusted sensitivity due to different friction characteristics

Log SG_RESULT values during normal operations to find the threshold window. Store per-node thresholds in flash config. The SBC can adjust via SDO during operation if needed.
