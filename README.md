# Vinyl Robot — EE & Firmware

Unified PCB design and STM32 firmware for a 5-node CANopen-based vinyl record handling robot.

## Documentation

- [System Architecture](docs/SYSTEM_ARCHITECTURE.md) — Mechanical overview, electrical architecture, safety layers
- [PCB Design](docs/PCB_DESIGN.md) — STM32G431 pin allocation, component selection, layout guidelines
- [Firmware Architecture](docs/FIRMWARE.md) — Bare-metal super-loop, CANopenNode integration, driver details
- [CANopen Protocol](docs/CANOPEN.md) — Object dictionary, PDO mapping, NMT lifecycle, bus analysis

## Hardware

- **MCU**: STM32G431CBU6 (UFQFPN48, 7×7mm)
- **Stepper driver**: TMC5160 stepstick (SPI motion controller mode)
- **CAN transceiver**: SN65HVD230
- **ToF sensors**: VL53L4CD (short range), VL53L1X (long range)
- **5 identical PCBs**, differentiated by populated components

## Development

Test-driven development with host-side unit tests (Unity framework, gcc). A HAL abstraction seam allows all driver and application logic to run on x86 with mock peripherals.

```bash
# Run all unit tests on host
make -C test/
```

See `docs/FIRMWARE.md` for the full TDD methodology, development phases, and HAL seam design.

## Prototyping

- Nucleo-G431RB for primary firmware development

## Related

ROS 2 control software lives in a separate repository. The CANopen object dictionary (EDS files) is the interface contract between firmware and ROS.
