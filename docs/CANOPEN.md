# CANopen Protocol Specification

## Network Configuration

- **Bus speed**: 1Mbit/s CAN 2.0B (classic CAN, not CAN-FD)
- **SYNC period**: 20ms (50Hz), broadcast by SBC master
- **Heartbeat producer**: 100ms interval on all nodes
- **Heartbeat consumer**: SBC detects node loss within 200ms (2 missed heartbeats)
- **SDO timeout**: 500ms
- **Termination**: 120Ω at node 0x01 (X axis) and node 0x05 (Player) — solder jumper on PCB

## Node Addressing

| Node ID | Board | DIP Switch | CAN ID Range (hex) |
|---------|-------|------------|-------------------|
| 0x01 | X axis | 001 | TPDO: 0x181, RPDO: 0x201, SDO TX: 0x581, SDO RX: 0x601 |
| 0x02 | Z axis | 010 | TPDO: 0x182, RPDO: 0x202, SDO TX: 0x582, SDO RX: 0x602 |
| 0x03 | A axis | 011 | TPDO: 0x183, RPDO: 0x203, SDO TX: 0x583, SDO RX: 0x603 |
| 0x04 | Pincher | 100 | TPDO: 0x284, RPDO: 0x304, SDO TX: 0x584, SDO RX: 0x604 |
| 0x05 | Player | 101 | TPDO: 0x285, RPDO: 0x305, SDO TX: 0x585, SDO RX: 0x605 |

SYNC COB-ID: 0x080 (broadcast)
NMT COB-ID: 0x000 (broadcast)
Heartbeat COB-ID: 0x700 + node_id

## Object Dictionary

### Mandatory CANopen Objects (all nodes)

| Index | Sub | Type | Name | Default |
|-------|-----|------|------|---------|
| 0x1000 | 0 | UINT32 | Device type | 0x000F0191 |
| 0x1001 | 0 | UINT8 | Error register | 0x00 |
| 0x1017 | 0 | UINT16 | Producer heartbeat time (ms) | 100 |
| 0x1018 | 1 | UINT32 | Vendor ID | TBD |
| 0x1018 | 2 | UINT32 | Product code | TBD |
| 0x1018 | 3 | UINT32 | Revision number | firmware version |

### Common Application Objects (all nodes)

| Index | Sub | Type | Name | Access | Notes |
|-------|-----|------|------|--------|-------|
| 0x2000 | 0 | UINT8 | Node role | RO | Read from DIP switch at boot |
| 0x2001 | 0 | UINT16 | Firmware version | RO | Major.minor packed |
| 0x2002 | 0 | UINT16 | Board temperature | RO | Internal ADC, °C × 10 |
| 0x2003 | 0 | UINT8 | Status word | RO | See bit definitions below |
| 0x2004 | 0 | UINT8 | Control word | RW | See bit definitions below |
| 0x2005 | 0 | UINT32 | Uptime (ms) | RO | Since boot |

#### Status Word (0x2003) Bit Definitions

| Bit | Name | Description |
|-----|------|-------------|
| 0 | homed | Axis has been homed since boot |
| 1 | moving | Ramp generator is active (RAMPSTAT velocity ≠ 0) |
| 2 | in_position | Target position reached (RAMPSTAT position_reached) |
| 3 | fault | Fault active — motion commands ignored |
| 4 | homing | Currently executing homing sequence |
| 5 | stallguard_active | StallGuard monitoring enabled |
| 6–7 | reserved | |

#### Control Word (0x2004) Bit Definitions

| Bit | Name | Description |
|-----|------|-------------|
| 0 | enable | Enable motor driver (write TOFF in CHOPCONF) |
| 1 | home | Start homing sequence |
| 2 | halt | Emergency halt — decelerate to stop |
| 3 | clear_fault | Clear fault state, resume normal operation |
| 4–7 | reserved | |

### Motion Objects — Stepper Nodes (0x01, 0x02, 0x03)

| Index | Sub | Type | Name | Access | Notes |
|-------|-----|------|------|--------|-------|
| 0x2100 | 0 | INT32 | Target position | RW | Microsteps → TMC5160 XTARGET |
| 0x2101 | 0 | INT32 | Actual position | RO | TMC5160 XACTUAL |
| 0x2102 | 0 | UINT32 | Max velocity (pps) | RW | → TMC5160 VMAX |
| 0x2103 | 0 | UINT16 | Acceleration | RW | → TMC5160 AMAX |
| 0x2104 | 0 | UINT16 | Deceleration | RW | → TMC5160 DMAX |
| 0x2105 | 0 | UINT8 | Ramp status | RO | TMC5160 RAMPSTAT bits |
| 0x2106 | 0 | UINT16 | StallGuard result | RO | TMC5160 SG_RESULT |
| 0x2107 | 0 | UINT16 | ToF distance (mm) | RO | VL53L reading |
| 0x2108 | 0 | UINT8 | Ramp mode | RW | 0=position, 1=velocity+, 2=velocity- |

### A Axis Extras (node 0x03 only)

| Index | Sub | Type | Name | Access | Notes |
|-------|-----|------|------|--------|-------|
| 0x2200 | 0 | INT16 | Pot angle (0.1° units) | RO | Coarse absolute — homing + sanity check |
| 0x2220 | 0 | UINT32 | LED pattern | RW | DotStar ring state |

### Servo Objects — Pincher (0x04) and Player (0x05)

| Index | Sub | Type | Name | Access | Notes |
|-------|-----|------|------|--------|-------|
| 0x2300 | 0 | UINT16 | Servo 1 position (μs) | RW | Pulse width 500–2500μs |
| 0x2301 | 0 | UINT16 | Servo 2 position (μs) | RW | Pulse width 500–2500μs |
| 0x2302 | 0 | UINT16 | ToF distance (mm) | RO | Pincher only |
| 0x2310 | 0 | UINT32 | LED pattern | RW | Player only (X rail strip) |

## PDO Mapping

### TPDO1 — Stepper Nodes (on SYNC, 50Hz)

Transmits actual position, status, and sensor data every SYNC cycle.

| Byte(s) | OD Index | Type | Content |
|---------|----------|------|---------|
| 0–3 | 0x2101 | INT32 | Actual position (XACTUAL) |
| 4 | 0x2003 | UINT8 | Status word |
| 5 | 0x2105 | UINT8 | Ramp status |
| 6–7 | 0x2107 | UINT16 | ToF distance (mm) |

Total: 8 bytes (maximum CAN payload)

### TPDO1 — Servo Nodes (on SYNC, 50Hz)

| Byte(s) | OD Index | Type | Content |
|---------|----------|------|---------|
| 0–1 | 0x2300 | UINT16 | Servo 1 actual position |
| 2–3 | 0x2301 | UINT16 | Servo 2 actual position |
| 4–5 | 0x2302 | UINT16 | ToF distance (Pincher) or 0 |
| 6 | 0x2003 | UINT8 | Status word |

Total: 7 bytes

### RPDO1 — Stepper Nodes (event-driven from master)

| Byte(s) | OD Index | Type | Content |
|---------|----------|------|---------|
| 0–3 | 0x2100 | INT32 | Target position (microsteps) |
| 4–5 | 0x2102 | UINT16 | Max velocity (scaled) |
| 6 | 0x2004 | UINT8 | Control word |
| 7 | 0x2108 | UINT8 | Ramp mode |

Total: 8 bytes

### RPDO1 — Servo Nodes (event-driven from master)

| Byte(s) | OD Index | Type | Content |
|---------|----------|------|---------|
| 0–1 | 0x2300 | UINT16 | Servo 1 target |
| 2–3 | 0x2301 | UINT16 | Servo 2 target |
| 4–7 | 0x2310/0x2004 | UINT32/UINT8 | LED pattern (Player) or control word (Pincher) |

Total: 8 bytes

## SYNC-Gated Motion Coordination

For simultaneous multi-axis moves:

1. SBC writes RPDO to X axis (target + velocity + accel) — staged, not applied
2. SBC writes RPDO to Z axis — staged
3. SBC writes RPDO to A axis — staged
4. SBC broadcasts SYNC frame
5. All nodes' SYNC callback fires → apply staged RPDO → write to TMC5160
6. All axes start ramps within one CAN frame period (~130μs) of each other

## NMT Lifecycle

```
INITIALIZING → PRE-OPERATIONAL → OPERATIONAL → STOPPED
                                       ↑            ↓
                                       └── (clear fault) ←┘
```

- **INITIALIZING**: Boot + hardware init. Automatic transition to PRE-OPERATIONAL.
- **PRE-OPERATIONAL**: SDO configuration accessible. No PDO exchange. SBC can read/write OD entries to configure node before going operational.
- **OPERATIONAL**: PDOs active. Full real-time data exchange. Normal operating state.
- **STOPPED**: Entered on fault or SBC command. No PDO exchange. Node holds position.

SBC waits for heartbeats from all 5 nodes in PRE-OPERATIONAL, then sends NMT "start all" to transition to OPERATIONAL.

## CAN Bus Bandwidth Analysis

At 50Hz SYNC with all 5 nodes active, worst case per cycle:

| Frame Type | Count | Bytes | Bits on Wire |
|------------|-------|-------|-------------|
| SYNC | 1 | 0 | ~50 |
| TPDO1 (stepper ×3) | 3 | 8 each | 390 |
| TPDO1 (servo ×2) | 2 | 7 each | 260 |
| RPDO1 (stepper ×3) | 3 | 8 each | 390 |
| RPDO1 (servo ×2) | 2 | 8 each | 260 |
| Heartbeats | ~2.5/cycle | 1 each | 125 |

Total: ~1,475 bits per 20ms cycle = 73.75 kbit/s = **7.4% bus utilization at 1Mbit**

Ample headroom for SDO traffic, EMCY frames, and occasional diagnostic reads.

## EDS File Structure

Three EDS files needed for `ros2_canopen` configuration:

- **stepper_node.eds**: Shared by X (0x01) and Z (0x02). Motion objects 0x2100–0x2108.
- **a_axis_node.eds**: Same as stepper_node plus pot (0x2200) and DotStar (0x2220).
- **servo_node.eds**: Shared by Pincher (0x04) and Player (0x05). Servo objects 0x2300–0x2310.

## CANopenNode Integration Notes

- Use CANopenNode C library (not Lely, which is heavier)
- STM32G4 FDCAN driver available in `CANopenNode/driver/STM32/`
- Register SYNC callback via `CO_SYNC_initCallbackPre()` — fires from CAN RX interrupt
- OD storage callback for flash persistence: point at reserved flash pages
- Configure FDCAN filters: one for node-specific frames, one for broadcast
- EMCY producer: call `CO_errorReport()` to send emergency frames
