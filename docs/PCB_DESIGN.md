# PCB Design — Unified Controller Board

## MCU Selection

**STM32G431CBU6** (UFQFPN48, 7×7mm)

- Cortex-M4F @ 170MHz with FPU
- 128KB flash / 32KB RAM
- Native FDCAN peripheral
- 12-bit ADC with hardware 16× oversampling
- Multiple SPI, I2C, UART, timer peripherals
- Upgrade path: STM32G473CBU6 (same package, 256KB flash, 128KB RAM, pin-compatible)

### Why G431 over alternatives

- **vs STM32G0B1**: G0 has CAN-FD but Cortex-M0+ without FPU. ADC scaling and pot angle calculations benefit from FPU. Extra $2 is justified.
- **vs STM32F446**: G4 has native CAN-FD (not just CAN 2.0B) and better ADC oversampling.
- **vs RP2040**: No native CAN — would need external MCP2515 adding complexity and latency.
- **vs ESP32-S3**: TWAI (CAN 2.0B) works but FreeRTOS overhead is unnecessary for a dumb I/O node.

### Prototyping

Two Nucleo boards available:
- **Nucleo-G431RB** (LQFP64) — primary dev board, same silicon as G431CB
- **Nucleo-G474RE** (LQFP64) — backup/upgrade testing, 512KB flash

Pin remapping from Nucleo 64-pin to production 48-pin is a CubeMX regeneration — all peripherals in the allocation exist on both packages.

## Pin Allocation (21 of 37 usable GPIOs)

### SPI1 — TMC5160 Stepstick (4 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PA5 | SPI1_SCK | Stepstick clock |
| PA6 | SPI1_MISO | TMC5160 data out |
| PA7 | SPI1_MOSI | MCU data out |
| PA4 | SPI1_CS | Active low chip select |

The TMC5160 operates in full SPI motion controller mode. STEP/DIR pins on the stepstick are unused and remapped as REFL/REFR reference switch inputs. The optical endstop connects directly to the stepstick REFL pin — not to the MCU. StallGuard stop is enabled internally via the sg_stop bit in SWMODE. The EN pin on the stepstick header is tied permanently low (always enabled). Motor disable is done via SPI by writing TOFF=0 in CHOPCONF.

**This means the MCU's only connection to the stepper subsystem is 4 SPI pins.** Endstop, StallGuard, and enable are all handled by the TMC5160 internally. The MCU polls RAMPSTAT to learn about events after the fact.

### SPI2 — DotStar LEDs (2 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PB13 | SPI2_SCK | DotStar clock |
| PB15 | SPI2_MOSI | DotStar data (unidirectional) |

SPI2 is separate from SPI1 to avoid blocking TMC5160 reads during LED updates. DotStar frames are long (32 bits × up to 60 LEDs = 1920 bits) and would block SPI1 for ~50μs per update.

### FDCAN1 (2 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PA11 | FDCAN1_RX | From SN65HVD230 transceiver |
| PA12 | FDCAN1_TX | To SN65HVD230 transceiver |

### I2C1 — ToF Sensor (2 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PB8 | I2C1_SCL | 400kHz, 4.7kΩ pullups to 3.3V |
| PB9 | I2C1_SDA | VL53L4CD or VL53L1X at default 0x29 |

One ToF per board, one I2C bus per MCU — no address conflicts.

### TIM3 — RC Servo PWM (2 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PB4 | TIM3_CH1 | Servo 1 (grip/play) — 50Hz, 500–2500μs pulse |
| PB5 | TIM3_CH2 | Servo 2 (flip/speed) — 50Hz, 500–2500μs pulse |

On PB4/PB5 to avoid conflict with PA0 (ADC) on A axis board.

### ADC1 — Potentiometer (1 pin)

| Pin | Function | Notes |
|-----|----------|-------|
| PA0 | ADC1_IN1 | 10-turn pot, 16× hardware oversampling |

With 16× oversampling, effective resolution improves from 12-bit to ~16-bit. Over 300° of travel using ~8.3% of one turn, this gives approximately 0.05° effective resolution — adequate for the coarse homing application.

### USART2 — Debug UART (2 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PA2 | USART2_TX | Debug output, 115200 baud |
| PA3 | USART2_RX | Debug input |

Exposed on 3-pin JST-SH connector. Optional — can be omitted in production.

### SWD + Status (3 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PA13 | SWDIO | Tag-Connect TC2030 footprint |
| PA14 | SWCLK | Pogo-pin, no permanent connector |
| PB3 | GPIO out | Status LED — heartbeat toggle |

### Node ID DIP Switch (3 pins)

| Pin | Function | Notes |
|-----|----------|-------|
| PB0 | NODE_ID_0 | Internal pullup, switch to GND |
| PB1 | NODE_ID_1 | 3-bit = 8 addresses (5 used) |
| PB2 | NODE_ID_2 | Read once at boot |

PB0/PB1/PB2 are adjacent on the bottom edge of the LQFP48 package (pins 16/17/18), which is convenient for PCB routing.

PC13/PC14/PC15 are reserved for future use: PC14 (OSC32_IN) and PC15 (OSC32_OUT) for an optional LSE 32.768kHz crystal, and PC13 for RTC tamper/wakeup if an RTC is added. Neither is required by the current firmware — the IWDG uses the internal LSI and there is no RTC.

### Spare Pins (13 available)

PA1 (ADC — rail voltage monitoring), PA8 (TIM1 — buzzer), PA9/PA10 (USART1 — LiDAR passthrough if needed), PA15, PB6, PB7, PB10, PB11, PB12, PB14

## Population Matrix

| Component | X axis | Z axis | A axis | Pincher | Player |
|-----------|--------|--------|--------|---------|--------|
| STM32G431CBU6 | ● | ● | ● | ● | ● |
| SN65HVD230 (CAN) | ● | ● | ● | ● | ● |
| 5V buck (3A) | ● | ● | ● | ● | ● |
| 3.3V LDO | ● | ● | ● | ● | ● |
| DIP switch (3-bit) | ● | ● | ● | ● | ● |
| TMC5160 stepstick socket | ● | ● | ● | ○ | ○ |
| Optical endstop connector | ● | ● | ● | ○ | ○ |
| ToF sensor connector (I2C) | ● | ● | ● | ● | ○ |
| Pot connector (ADC) | ○ | ○ | ● | ○ | ○ |
| Servo connectors (×2) | ○ | ○ | ○ | ● | ● |
| DotStar connector (SPI2) | ○ | ○ | ● | ○ | ● |
| 120Ω CAN termination jumper | ● | ○ | ○ | ○ | ● |

## PCB Layout Guidelines

- **Target board size**: ~50×40mm (similar to common stepper driver carriers)
- **Layers**: 2-layer, unbroken ground plane on bottom
- **Stepstick socket**: Standard 2×8 pin headers at 2.54mm pitch along one edge. Motor wires route away from signal traces.
- **Power entry**: 4-pin connector on one edge. 24V trace ≥1mm wide for 3A. Buck converter input/output caps close to IC.
- **CAN transceiver**: Between connector and MCU, near board edge. TVS diode (PESD2CAN) at connector pads.
- **Optional peripheral connectors**: JST-PH or JST-SH along remaining edges. 3-pin (VCC/GND/SIG) for simple sensors, 4-pin (VCC/GND/SCL/SDA) for I2C. Unpopulated variants leave connectors off.
- **SWD**: Tag-Connect TC2030 footprint (no permanent connector).
- **Do not let 24V trace split the ground plane.**

## Stepper Driver Selection

**TMC5160** in SPI motion controller mode (stepstick format).

The TMC5160's internal ramp generator handles all real-time step timing. The MCU sends target position, velocity, and acceleration parameters over SPI. The driver does the rest autonomously.

Key registers used:
- XTARGET: target position (written by MCU from CANopen RPDO)
- XACTUAL: current position (polled by MCU, reported in TPDO)
- VMAX, AMAX, DMAX: ramp parameters
- RAMPSTAT: status flags (position reached, stall detected, endstop hit)
- SWMODE: reference switch config (stop_l_enable for endstop, sg_stop for StallGuard)
- CHOPCONF: motor driver config (TOFF=0 to disable motor)
- SG_RESULT: StallGuard load measurement

### Stepstick compatibility note

Verify that the specific TMC5160 stepstick model remaps STEP/DIR pins as REFL/REFR when in SPI mode. Confirmed compatible: BTT TMC5160 V1.3, Watterott TMC5160 BOB. Some cheaper clones don't expose reference pins.

## Component Selection Summary

| Component | Part | Package | Notes |
|-----------|------|---------|-------|
| MCU | STM32G431CBU6 | UFQFPN48 | 128KB flash, M4F, FDCAN |
| CAN transceiver | SN65HVD230 | SOIC-8 | 3.3V, 1Mbit capable |
| CAN TVS | PESD2CAN | SOT-23 | Bidirectional TVS on CANH/CANL |
| 5V buck | MP2359 or TPS54331 | SOT-23-5/6 | 24V input, 3A output, for servos+LEDs |
| 3.3V LDO | AP2112K-3.3 | SOT-23-5 | 600mA, low dropout |
| ToF (short range) | VL53L4CD | module | 1–200mm, I2C, 18° FoV |
| ToF (long range) | VL53L1X | module | 4m max, I2C, programmable ROI |
| Status LED | 0603 green | 0603 | On PB3 via 470Ω |

## Flash Storage Layout

Last 4KB of 128KB flash reserved for persistent configuration:

| Address | Size | Purpose |
|---------|------|---------|
| 0x0801F000 | 2KB | Config slot A (active) |
| 0x0801F800 | 2KB | Config slot B (mirror) |

Double-buffer scheme: write new config to slot A, validate, then erase slot B. Next write goes to B, erase A. Atomic updates with power-loss protection. CANopenNode's OD_storage callback handles serialization.

Stored data per node: TMC5160 ramp defaults, StallGuard thresholds, pot calibration offset (A axis), node-specific config. ~256 bytes total. At 10,000 write cycle endurance, daily recalibration lasts 27 years.
