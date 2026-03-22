# Vinyl Robot — System Architecture

## Project Overview

An automated vinyl record handling robot that manages a queue of 5–6 records, picks them from a shelved stack, places them on a Pioneer PL-400 turntable, plays them, flips or swaps records, and returns them. The robot lives in a domestic living room environment with pets and humans nearby, requiring layered safety systems.

## Mechanical Architecture

The robot has a 1-meter linear stage (X axis) with two stations:
- **Left end (x=0)**: Shelved record queue stack — 5 slots with horizontal shelf floors between them, ~40mm pitch, ~20mm clearance per slot
- **Right end (x=1000mm)**: Pioneer PL-400 turntable connected to an audio system

### Axis Chain

**X axis (linear, 1000mm travel)** — Slides the entire upper assembly along a linear rail between the record stack and turntable. Driven by NEMA 17 stepper via TMC5160 in SPI motion controller mode.

**Z axis (vertical, ~150mm travel)** — Lead-screw driven vertical stage mounted on the X carriage. Lifts the end effector to clear the turntable platter and record stack shelves. Driven by NEMA 17 stepper via TMC5160.

**A axis (rotary, 300° travel)** — Belt-driven rotary stage mounted on the Z carriage. Rotates the pincher arm to reach records from different angles. Driven by NEMA 17 stepper via TMC5160. A 10-turn potentiometer provides coarse absolute position for safe homing on power-up. The pot is NOT used for closed-loop control — the TMC5160 microstep counter is the position authority during operation.

**Pincher (end effector)** — Two-jaw gripper that clamps a vinyl record from both sides. Driven by two RC servos: one for grip open/close, one for record flip (180° rotation). A VL53L4CD ToF sensor measures height above the surface below.

### Key Mechanical Constraints

- The record stack is a **shelved structure** with floors between slots. When the arm is inside a slot, Z movement is dangerous in both directions — shelves above and below with only ~20mm clearance.
- The A axis has physical hard stops at 0° and 300° to protect the cable wrap.
- The turntable is an open surface — Z up is always safe when near it.
- Furniture and walls constrain the A axis safe rotation window (configurable, default 30°–330°).

## Electrical Architecture

### Power Distribution

- **24V input** from a power supply in the robot base
- 24V rail routed through the full axis chain to each controller board
- Each board has:
  - 24V → TMC5160 VM (motor power) directly
  - 24V → 5V buck converter (3A, for servos and DotStar LEDs)
  - 5V → 3.3V LDO (MCU, CAN transceiver, sensors)

### CAN Bus

- Single CAN 2.0B bus at 1Mbit/s
- 5 nodes + SBC master
- 120Ω termination at both physical ends (X axis board and Player Control board) via solder jumpers
- SN65HVD230 transceivers on each node
- TVS protection (PESD2CAN) at each node's connector
- 4-pin connectors per board: 24V, GND, CANH, CANL

### Controller Boards

Five identical PCBs differentiated by populated components. See [PCB_DESIGN.md](./PCB_DESIGN.md) for full details.

| Node ID | Board | Populated Peripherals |
|---------|-------|----------------------|
| 0x01 | X axis | TMC5160, endstop on stepstick, VL53L1X ToF |
| 0x02 | Z axis | TMC5160, endstop on stepstick, VL53L4CD ToF |
| 0x03 | A axis | TMC5160, endstop on stepstick, VL53L4CD ToF, 10-turn pot (ADC), DotStar ring (20 LEDs) |
| 0x04 | Pincher | 2× RC servos (grip + flip), VL53L4CD ToF |
| 0x05 | Player Control | 2× RC servos (play/stop + speed), DotStar strip (~1m, along X rail) |

### SBC (System on Module)

**Toradex Verdin iMX8M Plus (4GB RAM)** on **Mallow carrier board**

Selected for:
- Native dual CAN-FD (FlexCAN) — no USB adapter needed
- Gigabit Ethernet for SICK TIM571 LiDAR
- MIPI-CSI camera interface with ISP
- 2.3 TOPS NPU for future CV upgrades
- Industrial temp range and long-term availability
- Torizon OS with Docker-based ROS 2 deployment

### Sensors

**LiDAR**: SICK TIM571-2050101 (preferred) — 270° FoV, 15Hz, Ethernet TCP/IP. Mounted on top of Z column. Acts as a safety curtain.

**Camera**: USB or CSI camera for tonearm position tracking on the turntable. 640×480 @ 15fps is sufficient.

**ToF sensors**:
- X axis: VL53L1X (long range, 25mm–1000mm, focused beam via ROI) — startup collision avoidance
- Z axis: VL53L4CD (short range, 10–150mm, ±1mm) — height confirmation
- Pincher: VL53L4CD — height above surface below for grip confirmation
- All on I2C, one per board, no address conflicts

**Potentiometer**: 10-turn pot on A axis belt drive. Read via MCU ADC with 16× hardware oversampling. Used only for:
1. Coarse absolute angle on power-up (±2° accuracy, enough for safe homing direction)
2. Runtime sanity check against TMC5160 XACTUAL (every few seconds, ±5° divergence triggers EMCY)

## Safety Architecture (Layered)

| Layer | Mechanism | Response Time | Catches |
|-------|-----------|---------------|---------|
| 1 — Mechanical | Current limits on TMC5160, rubber bumpers, A axis hard stops | Instant | Force limits on all failures |
| 2 — StallGuard | TMC5160 sg_stop bit — driver stops motor internally | ~1ms | Collisions with objects, pets, humans |
| 3 — LiDAR | SICK TIM571 → ROS safety node → CAN halt | ~100ms | Humans approaching the robot |
| 4 — Watchdog | CAN heartbeat (100ms) + ROS lifecycle monitoring | 200–500ms | Software crashes, bus faults, node lockups |
| 5 — E-stop | Hardware power cut to 24V motor rail (relay/contactor) | Instant | Everything else |

## Software Architecture

### Firmware (this repo)

All five controller boards run the same firmware binary (STM32 bare-metal + CANopenNode). Behavior is selected by DIP switch node ID at boot. MCUs are dumb I/O nodes — no operational logic, no motion planning, no state machines. They receive commands and report status.

See [FIRMWARE.md](./FIRMWARE.md) and [CANOPEN.md](./CANOPEN.md) for details.

### ROS 2 (separate repo)

ROS 2 Humble on Torizon OS (Docker container). 7 nodes: CANopen master, motion coordinator, LiDAR safety, turntable monitor, state machine (BehaviorTree.CPP), LED controller, diagnostics.

All operational intelligence lives on the SBC. The SBC knows the mechanical geometry, safety zones, homing sequences, and the full record-handling workflow.
