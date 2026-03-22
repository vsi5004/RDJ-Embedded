# CLAUDE.md — Project Context for AI Assistant

## What This Project Is

Unified PCB design and STM32G431 bare-metal firmware for a 5-node CANopen vinyl record handling robot. The five controller boards share one PCB layout, differentiated by populated components. Firmware is a single binary with behavior gated by DIP switch node ID.

## Key Design Documents

Read these before making changes:

- `docs/SYSTEM_ARCHITECTURE.md` — Full system overview, mechanical constraints, safety layers
- `docs/PCB_DESIGN.md` — Pin allocation (21 of 37 used), component selection, population matrix
- `docs/FIRMWARE.md` — TDD methodology, HAL seam, super-loop architecture, driver specs, homing, faults
- `docs/CANOPEN.md` — Object dictionary, PDO mapping, SYNC coordination, bus bandwidth analysis

## Development Methodology — Test-Driven

**Every firmware module is developed test-first.** Write failing tests, then implement the module to pass them.

Tests run on the host machine (x86, gcc) via Unity framework. A HAL abstraction seam (function pointer structs in `App/hal/`) separates driver logic from STM32 register access. Mock implementations in `test/mocks/` record what was written and return controlled data.

```bash
# Run all tests
make -C test/

# Run a single test suite
make -C test/ test_tmc5160 && test/test_tmc5160
```

### When adding a new feature or fixing a bug:

1. **Write a test first** in the appropriate `test/test_*.c` file
2. Run `make -C test/` — confirm it fails
3. Implement the change in `App/drv/` or `App/app/`
4. Run `make -C test/` — confirm it passes
5. Flash to Nucleo for hardware validation

### Development phase order:

| Phase | Tests | Module |
|-------|-------|--------|
| 1 | `test_tmc5160.c` | `drv/drv_tmc5160.c` — SPI register access, motion, StallGuard |
| 2 | `test_canopen_pdo.c` | `app/app_canopen.c` — RPDO staging, SYNC, TPDO, status word |
| 3 | `test_homing.c` | `app/app_homing.c` — Per-axis homing, pot direction |
| 4 | `test_fault.c` | fault paths in `app/app_canopen.c` — EMCY, fault blocking, clear |
| 5 | `test_tof.c`, `test_servo.c`, `test_dotstar.c` | `drv/drv_tof.c`, `drv/drv_servo.c`, `drv/drv_dotstar.c` |
| 6 | `test_node_config.c`, `test_flash.c` | `app/node_config.h`, `app/cfg_flash.c` |

## Critical Design Decisions

1. **MCUs are dumb I/O nodes.** No operational logic, no motion planning, no state machines on the MCU. All intelligence lives on the SBC (separate repo).
2. **TMC5160 in SPI motion controller mode.** The driver's internal ramp generator handles all step timing. MCU sends targets and polls status. STEP/DIR pins unused — remapped as endstop inputs on the stepstick.
3. **No PID on the A axis.** TMC5160 microstep counter is the position authority. The 10-turn potentiometer is only for coarse homing direction and runtime sanity checks.
4. **Endstops wire to the stepstick, not the MCU.** The TMC5160 handles endstop and StallGuard stops internally. MCU polls RAMPSTAT after the fact.
5. **MCU pin budget: 4 SPI pins to TMC5160.** That's the entire stepper interface. EN tied low, endstop on stepstick REFL, StallGuard via sg_stop bit.
6. **CANopenNode** (C library) for the CANopen stack, not Lely. Lighter footprint for bare-metal.
7. **SYNC-gated RPDO application.** Commands are staged in shadow buffers and applied atomically on SYNC for coordinated multi-axis moves.
8. **HAL abstraction seam for testability.** Drivers use `hal_spi_t`, `hal_i2c_t`, etc. function pointer structs — never call STM32 HAL directly. This allows full unit testing on the host.

## Project Structure — Key Directories

```
App/hal/          → Hardware abstraction (function pointer interfaces)
App/hal/*_stm32.c → Real STM32 implementations (target only)
App/drv/          → Driver logic (testable, no HW dependency)
App/app/          → Application logic (testable, no HW dependency)
test/mocks/       → Mock HAL implementations for testing
test/test_*.c     → Unit tests (one file per module)
test/Makefile     → Host test build (gcc + Unity)
```

## Tech Stack

- STM32G431CBU6 (Cortex-M4F, 170MHz, 128KB flash, FDCAN)
- STM32 HAL (CubeMX generated)
- CANopenNode (git submodule)
- Unity test framework (vendored in test/unity/)
- TMC5160 stepstick (SPI, motion controller mode)
- VL53L4CD / VL53L1X ToF sensors (I2C)
- SN65HVD230 CAN transceiver
- arm-none-eabi-gcc / STM32CubeIDE (target build)
- gcc (host test build)

## What NOT to Do

- Don't add RTOS — the super-loop handles everything within timing constraints
- Don't add operational logic to firmware — that belongs in the ROS 2 repo
- Don't change the object dictionary indices without updating the EDS files in the ROS repo
- Don't use STEP/DIR mode on the TMC5160 — SPI motion controller mode is the architecture
- Don't wire endstops to MCU GPIOs — they go to the stepstick REFL pin
- Don't call STM32 HAL functions from `App/drv/` or `App/app/` — use the `hal_*` interfaces
- Don't skip writing tests — every driver and app module gets tests before implementation
- Don't write tests that depend on execution order — each test function must be independent
- Don't mock CANopenNode internals — test at the OD read/write boundary instead
