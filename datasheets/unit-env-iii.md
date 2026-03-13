# Unit ENV-III Firmware Implementation Specification

This document is a self-contained firmware implementation reference for the **Unit ENV-III (SKU: U001-C)** module. It is intended to enable automatic generation of firmware drivers, HAL integrations, BSPs, communication handlers, and test code **without requiring the original source files**.

The Unit ENV-III is a **composite I2C environmental sensing module** containing:

* **Sensirion SHT30 / SHT3x-DIS family device** for temperature and humidity
* **QST QMP6988** for absolute barometric pressure and temperature
* **HT7533 LDO** converting module input supply to the local **+3.3 V** rail
* **Two 4.7 kΩ pull-ups** on the shared I2C bus
* **HY2.0-4P** external connector carrying **GND, 5 V, SDA, SCL**

The supplied schematic shows the module is intended to be driven as a **single I2C bus peripheral assembly** with two logical devices on the same bus.

---

## 1. Purpose

This specification defines the software-visible behavior of the Unit ENV-III module and its internal sensing devices, including:

* bus addresses
* commands
* registers
* bitfields
* timing
* raw and converted data formats
* initialization
* runtime sequences
* correctness rules

It is designed so a generated implementation can produce a complete working BSP and sensor driver stack for the module.

---

## 2. Device Overview

## 2.1 Module-level function

Unit ENV-III is an environmental sensor module providing:

* **temperature**
* **relative humidity**
* **barometric pressure**

via a shared **I2C** interface.

## 2.2 Internal devices

### SHT30 / SHT3x-DIS

Functions:

* humidity measurement
* temperature measurement
* internal heater
* optional ALERT output
* reset input
* single-shot and periodic acquisition modes

Key interface facts:

* I2C only
* supported I2C clock up to **1000 kHz**
* address selectable between **0x44** and **0x45**
* on this module, schematic ties **ADDR to GND**, so the effective address is **0x44**

Key performance values for the SHT30 variant:

* supply voltage: **2.15 V to 5.5 V**
* typical temperature accuracy: **±0.2 °C** over **0 °C to 65 °C**
* humidity accuracy tolerance: **typ. ±2 %RH**
* humidity specified range: **0 to 100 %RH**
* temperature specified range: **-40 °C to 125 °C**  

### QMP6988

Functions:

* absolute pressure measurement
* temperature measurement
* OTP/NVM-stored compensation coefficients
* sleep / forced / normal modes
* configurable oversampling and IIR filtering

Key interface facts:

* supports I2C and SPI
* on this module it is wired for **I2C**
* I2C address depends on SDO/ADDR pin
* on this module, schematic ties **SDO/ADDR low**, so effective address is **0x70**

Key performance values:

* operating pressure range: **30 kPa to 110 kPa**
* pressure resolution: **0.06 Pa**
* temperature resolution: **0.0002 °C**
* relative pressure accuracy: **±3.9 Pa** in Ultra High Accuracy mode
* rms noise: **1.3 Pa** in Ultra High Accuracy mode
* operating voltage: **VDD 1.71 V to 3.6 V**, **VDDIO 1.2 V to 3.6 V**  

## 2.3 Module electrical integration visible to firmware

From the schematic:

* external connector power pin is labeled **VCC / 5 V**
* **HT7533** generates local **+3.3 V**
* SHT30 and QMP6988 are powered from **+3.3 V**
* SDA and SCL each have **4.7 kΩ** pull-ups to **VCC**
* both sensors share the same SDA/SCL bus
* QMP6988 is forced into I2C mode by tying **CSB high**
* QMP6988 I2C address select pin is tied low
* SHT30 ADDR is tied low

For firmware, treat the module as a shared bus with two targets:

* **SHT30 at 0x44**
* **QMP6988 at 0x70**

---

## 3. Communication Interfaces

## 3.1 External module interface

Connector pin map:

| HY2.0-4P Pin |  Color | Signal |
| ------------ | -----: | ------ |
| 1            |  Black | GND    |
| 2            |    Red | 5 V    |
| 3            | Yellow | SDA    |
| 4            |  White | SCL    |

Module-level protocol: **I2C**

---

## 3.2 SHT30 communication interface

| Item                       | Value                                      |
| -------------------------- | ------------------------------------------ |
| Bus                        | I2C                                        |
| Address A                  | 0x44                                       |
| Address B                  | 0x45                                       |
| Module address             | 0x44                                       |
| Max clock frequency        | 1000 kHz                                   |
| Address selection          | ADDR low = 0x44, ADDR high = 0x45          |
| Bus type                   | open-drain SDA/SCL                         |
| Command width              | 16 bits                                    |
| CRC on data                | yes, 8-bit CRC after each 16-bit data word |
| Minimal inter-command wait | 1 ms                                       |

Protocol model:

* host sends **START**
* host sends **7-bit address + write bit**
* host sends **16-bit command**
* sensor ACKs each byte
* sensor measures or changes state depending on command
* results are later read using **address + read bit**

In single-shot measurement mode:

* result order is:

  1. temperature MSB
  2. temperature LSB
  3. temperature CRC
  4. humidity MSB
  5. humidity LSB
  6. humidity CRC

Clock stretching:

* supported in selected single-shot commands
* not selectable in periodic mode  

---

## 3.3 QMP6988 communication interface

| Item                      | Value                              |
| ------------------------- | ---------------------------------- |
| Supported buses           | I2C, SPI 3-wire, SPI 4-wire        |
| Module wiring             | I2C                                |
| I2C address when SDO low  | 0x70                               |
| I2C address when SDO high | 0x56                               |
| Module address            | 0x70                               |
| I2C supported rates       | 100 kbit/s, 400 kbit/s, 3.4 Mbit/s |
| SPI supported rate        | 10 Mbit/s                          |
| Address autoincrement     | yes                                |
| Read/write method         | register addressed                 |

Protocol model:

* write: device address + register address + one or more data bytes
* read: write phase with target register address, then repeated start, then read phase
* address autoincrements during burst access
* if register address increments past **0xFF**, device continues outputting **0xFF**

SPI facts, relevant only for portability of a generic QMP6988 driver:

* mode selected by CSB pin and IO_SETUP.spi3w
* CSB high at POR/reset selects I2C
* CSB low selects SPI until POR or asynchronous reset
* spi3w=0 -> 4-wire SPI
* spi3w=1 -> 3-wire SPI   

---

## 4. Device Addressing and Identification

## 4.1 Module logical devices

| Logical device | Address | Determination         |
| -------------- | ------: | --------------------- |
| SHT30          |    0x44 | ADDR tied low         |
| QMP6988        |    0x70 | QMP SDO/ADDR tied low |

## 4.2 SHT30 identification

No conventional chip-ID register is exposed in the command set included here.

Discovery procedure:

1. probe address **0x44**
2. issue a valid command such as **read status (0xF32D)** or a measurement command
3. verify ACK and valid response format including CRC

## 4.3 QMP6988 identification

| Register | Address | Expected value |
| -------- | ------: | -------------: |
| CHIP_ID  |    0xD1 |           0x5C |

Discovery procedure:

1. probe address **0x70**
2. read register **0xD1**
3. verify returned value equals **0x5C** 

---

## 5. Communication Protocol

## 5.1 SHT30 command-based protocol

### Write-only command transaction

`START -> [ADDR|W] -> CMD_MSB -> CMD_LSB -> STOP`

### Read measurement transaction

`START -> [ADDR|R] -> T_MSB -> T_LSB -> T_CRC -> RH_MSB -> RH_LSB -> RH_CRC -> NACK -> STOP`

### Notes

* 16-bit commands are already protected by an integrated 3-bit command CRC internal to the command encoding
* all transmitted or received **data words** are followed by an **8-bit CRC**
* in write direction, write data CRC is mandatory when applicable
* at least **1 ms** must pass after sending a command before another command can be received 

---

## 5.2 QMP6988 register protocol

### I2C single-register write

`START -> [ADDR|W] -> REG_ADDR -> DATA -> STOP`

### I2C burst write

`START -> [ADDR|W] -> REG_ADDR -> DATA0 -> DATA1 -> ... -> STOP`

### I2C register read

`START -> [ADDR|W] -> REG_ADDR -> RESTART -> [ADDR|R] -> DATA0 -> DATA1 -> ... -> NACK -> STOP`

### Notes

* register address autoincrements during reads and writes
* if incremented register address exceeds **0xFF**, output remains **0xFF**
* soft reset occurs only when **0xE6** is written to **RESET (0xE0)**  

---

## 6. Register Map

## 6.1 QMP6988 full register map

| Register      | I2C Addr | SPI Addr | Access |                                                    Reset | Description                                          |
| ------------- | -------: | -------: | ------ | -------------------------------------------------------: | ---------------------------------------------------- |
| TEMP_TXD0     |     0xFC |     0x7C | R      |                                                     0x00 | Temperature raw data bits [7:0] / low byte           |
| TEMP_TXD1     |     0xFB |     0x7B | R      |                                                     0x00 | Temperature raw data middle byte                     |
| TEMP_TXD2     |     0xFA |     0x7A | R      |                                                     0x00 | Temperature raw data high byte                       |
| PRESS_TXD0    |     0xF9 |     0x79 | R      |                                                     0x00 | Pressure raw data low byte                           |
| PRESS_TXD1    |     0xF8 |     0x78 | R      |                                                     0x00 | Pressure raw data middle byte                        |
| PRESS_TXD2    |     0xF7 |     0x77 | R      |                                                     0x00 | Pressure raw data high byte                          |
| IO_SETUP      |     0xF5 |     0x75 | R/W    |                                                     0x00 | standby, SPI mode, SDI output mode                   |
| CTRL_MEAS     |     0xF4 |     0x74 | R/W    |                                                     0x00 | temp oversampling, pressure oversampling, power mode |
| DEVICE_STAT   |     0xF3 |     0x73 | R      |                                                     0x00 | measurement busy and OTP access status               |
| I2C_SET       |     0xF2 |     0x72 | R/W    | 0x01 / 0x00 in snippets; use power-on readback if needed | I2C HS master code setting                           |
| IIR_CNT / IIR |     0xF1 |     0x71 | R/W    |                                                     0x00 | IIR filter coefficient                               |
| RESET         |     0xE0 |     0x60 | W      |                                                     0x00 | write 0xE6 for software reset                        |
| CHIP_ID       |     0xD1 |     0x51 | R      |                                                     0x5C | chip identification                                  |
| COE_b00_a0_ex |     0xB8 |     0x38 | R      |                                                        - | coefficient extension bits for b00 and a0            |
| COE_a2_0      |     0xB7 |     0x37 | R      |                                                        - | a2 low byte                                          |
| COE_a2_1      |     0xB6 |     0x36 | R      |                                                        - | a2 high byte                                         |
| COE_a1_0      |     0xB5 |     0x35 | R      |                                                        - | a1 low byte                                          |
| COE_a1_1      |     0xB4 |     0x34 | R      |                                                        - | a1 high byte                                         |
| COE_a0_0      |     0xB3 |     0x33 | R      |                                                        - | a0 middle bits                                       |
| COE_a0_1      |     0xB2 |     0x32 | R      |                                                        - | a0 high bits                                         |
| COE_bp3_0     |     0xB1 |     0x31 | R      |                                                        - | bp3 low byte                                         |
| COE_bp3_1     |     0xB0 |     0x30 | R      |                                                        - | bp3 high byte                                        |
| COE_b21_0     |     0xAF |     0x2F | R      |                                                        - | b21 low byte                                         |
| COE_b21_1     |     0xAE |     0x2E | R      |                                                        - | b21 high byte                                        |
| COE_b12_0     |     0xAD |     0x2D | R      |                                                        - | b12 low byte                                         |
| COE_b12_1     |     0xAC |     0x2C | R      |                                                        - | b12 high byte                                        |
| COE_bp2_0     |     0xAB |     0x2B | R      |                                                        - | bp2 low byte                                         |
| COE_bp2_1     |     0xAA |     0x2A | R      |                                                        - | bp2 high byte                                        |
| COE_b11_0     |     0xA9 |     0x29 | R      |                                                        - | b11 low byte                                         |
| COE_b11_1     |     0xA8 |     0x28 | R      |                                                        - | b11 high byte                                        |
| COE_bp1_0     |     0xA7 |     0x27 | R      |                                                        - | bp1 low byte                                         |
| COE_bp1_1     |     0xA6 |     0x26 | R      |                                                        - | bp1 high byte                                        |
| COE_bt2_0     |     0xA5 |     0x25 | R      |                                                        - | bt2 low byte                                         |
| COE_bt2_1     |     0xA4 |     0x24 | R      |                                                        - | bt2 high byte                                        |
| COE_bt1_0     |     0xA3 |     0x23 | R      |                                                        - | bt1 low byte                                         |
| COE_bt1_1     |     0xA2 |     0x22 | R      |                                                        - | bt1 high byte                                        |
| COE_b00_0     |     0xA1 |     0x21 | R      |                                                        - | b00 middle bits                                      |
| COE_b00_1     |     0xA0 |     0x20 | R      |                                                        - | b00 high bits                                        |

 

## 6.2 SHT30 register map

The SHT30 interface exposed here is **command-based**, not a standard memory-mapped register bank.

The only software-visible register-like state explicitly documented in the provided command set is the **status register**, accessed via command **0xF32D** and cleared via **0x3041**. All other operations are command initiated.

---

## 7. Register Bitfields

## 7.1 QMP6988 IO_SETUP (0xF5)

| Bits | Name           | Access | Reset | Description                |
| ---- | -------------- | ------ | ----: | -------------------------- |
| 7:5  | t_standby[2:0] | R/W    |   000 | standby time               |
| 4:3  | Reserved       | R/W    |    00 | keep 0                     |
| 2    | spi3_sdim      | R/W    |     0 | SDI output type            |
| 1    | Reserved       | R/W    |     0 | keep 0                     |
| 0    | spi3w          | R/W    |     0 | 0=SPI 4-wire, 1=SPI 3-wire |

`t_standby[2:0]` encoding:

| Value | Standby |
| ----: | ------- |
|   000 | 1 ms    |
|   001 | 5 ms    |
|   010 | 50 ms   |
|   011 | 250 ms  |
|   100 | 500 ms  |
|   101 | 1 s     |
|   110 | 2 s     |
|   111 | 4 s     |

`spi3_sdim`:

* `0`: Lo / Hi-Z output
* `1`: Lo / Hi output

`spi3w`:

* `0`: 4-wire SPI
* `1`: 3-wire SPI 

---

## 7.2 QMP6988 CTRL_MEAS (0xF4)

| Bits | Name               | Access | Reset | Description           |
| ---- | ------------------ | ------ | ----: | --------------------- |
| 7:5  | temp_average[2:0]  | R/W    |   000 | temperature averaging |
| 4:2  | press_average[2:0] | R/W    |   000 | pressure averaging    |
| 1:0  | power_mode[1:0]    | R/W    |    00 | operating mode        |

`temp_average[2:0]` and `press_average[2:0]`:

| Value | Averaging |
| ----: | --------- |
|   000 | Skip      |
|   001 | 1         |
|   010 | 2         |
|   011 | 4         |
|   100 | 8         |
|   101 | 16        |
|   110 | 32        |
|   111 | 64        |

`power_mode[1:0]`:

| Value | Mode   |
| ----: | ------ |
|    00 | Sleep  |
|    01 | Forced |
|    10 | Forced |
|    11 | Normal |



---

## 7.3 QMP6988 DEVICE_STAT (0xF3)

| Bits | Name       | Access | Reset | Description                                                 |
| ---- | ---------- | ------ | ----: | ----------------------------------------------------------- |
| 7:4  | Reserved   | R      |  0000 | keep 0                                                      |
| 3    | measure    | R      |     0 | 0=measurement complete / waiting; 1=measurement in progress |
| 2:1  | Reserved   | R      |    00 | keep 0                                                      |
| 0    | otp_update | R      |     0 | 0=no OTP access; 1=accessing OTP                            |



---

## 7.4 QMP6988 I2C_SET (0xF2)

| Bits | Name             | Access | Reset | Description                     |
| ---- | ---------------- | ------ | ----: | ------------------------------- |
| 7:3  | Reserved         | R/W    | 00000 | keep 0                          |
| 2:0  | master_code[2:0] | R/W    |   000 | I2C high-speed mode master code |

`master_code[2:0]` encoding:

| Value | Encoded HS master code |
| ----: | ---------------------- |
|   000 | 0x08                   |
|   001 | 0x09                   |
|   010 | 0x0A                   |
|   011 | 0x0B                   |
|   100 | 0x0C                   |
|   101 | 0x0D                   |
|   110 | 0x0E                   |
|   111 | 0x0F                   |

 

---

## 7.5 QMP6988 IIR_CNT / IIR (0xF1)

| Bits | Name        | Access | Reset | Description            |
| ---- | ----------- | ------ | ----: | ---------------------- |
| 7:3  | Reserved    | R/W    | 00000 | keep 0                 |
| 2:0  | filter[2:0] | R/W    |   000 | IIR filter coefficient |

`filter[2:0]` encoding:

| Value | Setting |
| ----: | ------- |
|   000 | Off     |
|   001 | N=2     |
|   010 | N=4     |
|   011 | N=8     |
|   100 | N=16    |
|   101 | N=32    |
|   110 | N=32    |
|   111 | N=32    |

Write access to this register initializes the IIR filter. Initial setting is **Off**.  

---

## 7.6 QMP6988 RESET (0xE0)

| Bits | Name       | Access | Reset | Description                          |
| ---- | ---------- | ------ | ----: | ------------------------------------ |
| 7:0  | reset[7:0] | W      |  0x00 | write 0xE6 to trigger software reset |

Any value other than **0xE6** has no effect. 

---

## 7.7 QMP6988 CHIP_ID (0xD1)

| Bits | Name         | Access | Reset | Description     |
| ---- | ------------ | ------ | ----: | --------------- |
| 7:0  | chip_id[7:0] | R      |  0x5C | fixed device ID |



---

## 7.8 QMP6988 coefficient registers

These registers are read-only OTP/NVM-backed coefficient storage. The snippets identify packing as follows:

* `a0` uses `COE_a0_1`, `COE_a0_0`, `COE_b00_a0_ex`
* `b00` uses `COE_b00_1`, `COE_b00_0`, `COE_b00_a0_ex`
* all remaining coefficients use paired `_1` high-byte and `_0` low-byte registers

The packing visible in the extracted content is:

* `a0`: 20-bit value in **20Q16** format using `[19:12]`, `[11:4]`, `[3:0]`
* `b00`: 20-bit value in **20Q16** format using `[19:12]`, `[11:4]`, `[3:0]`
* `a1`, `a2`, `bt1`, `bt2`, `bp1`, `b11`, `bp2`, `b12`, `b21`, `bp3` are packed as 16-bit values from `_1` and `_0` bytes

The provided material does not expose per-register reset values for OTP coefficients. Treat them as immutable calibration contents read after POR.  

---

## 7.9 SHT30 status register (read via command 0xF32D)

| Bits | Name                       | Reset | Description                                                          |
| ---- | -------------------------- | ----: | -------------------------------------------------------------------- |
| 15   | Alert pending status       |     1 | 0=no pending alerts, 1=at least one pending alert                    |
| 14   | Reserved                   |     0 | reserved                                                             |
| 13   | Heater status              |     0 | 0=heater off, 1=heater on                                            |
| 12   | Reserved                   |     0 | reserved                                                             |
| 11   | RH tracking alert          |     0 | 0=no alert, 1=alert                                                  |
| 10   | T tracking alert           |     0 | 0=no alert, 1=alert                                                  |
| 9:5  | Reserved                   | xxxxx | reserved                                                             |
| 4    | System reset detected      |     1 | 0=no reset since last clear-status, 1=reset detected                 |
| 3:2  | Reserved                   |    00 | reserved                                                             |
| 1    | Command status             |     0 | 0=last command ok, 1=last command invalid or failed command checksum |
| 0    | Write data checksum status |     0 | 0=last write checksum correct, 1=last write checksum failed          |

Flags cleared by command **0x3041**:

* bit 15
* bit 11
* bit 10
* bit 4



---

## 8. Reserved Bit Handling

### QMP6988

Reserved bits explicitly identified in the extracted register descriptions:

* IO_SETUP bits **4:3** and **1**: **keep 0**
* DEVICE_STAT bits **7:4** and **2:1**: read only; when caching, ignore except required status bits
* I2C_SET bits **7:3**: **keep 0**
* IIR_CNT bits **7:3**: **keep 0**

Rule:

* on writes, preserve only documented writable bits
* write all reserved bits as **0**
* on read-modify-write, mask out reserved fields

### SHT30

In the status register:

* reserved bits must be ignored on read
* no direct write access exists to the status register contents

---

## 9. Commands or Opcodes

## 9.1 SHT30 command table

### Single-shot measurement commands

| Command                                                              |   Code | Parameters | Description          |
| -------------------------------------------------------------------- | -----: | ---------- | -------------------- |
| Measure single shot, high repeatability, clock stretching enabled    | 0x2C06 | none       | one RH/T measurement |
| Measure single shot, medium repeatability, clock stretching enabled  | 0x2C0D | none       | one RH/T measurement |
| Measure single shot, low repeatability, clock stretching enabled     | 0x2C10 | none       | one RH/T measurement |
| Measure single shot, high repeatability, clock stretching disabled   | 0x2400 | none       | one RH/T measurement |
| Measure single shot, medium repeatability, clock stretching disabled | 0x240B | none       | one RH/T measurement |
| Measure single shot, low repeatability, clock stretching disabled    | 0x2416 | none       | one RH/T measurement |

### Periodic measurement commands

| Command                 |   Code | Parameters | Description                |
| ----------------------- | -----: | ---------- | -------------------------- |
| Periodic high 0.5 mps   | 0x2032 | none       | start periodic acquisition |
| Periodic medium 0.5 mps | 0x2024 | none       | start periodic acquisition |
| Periodic low 0.5 mps    | 0x202F | none       | start periodic acquisition |
| Periodic high 1 mps     | 0x2130 | none       | start periodic acquisition |
| Periodic medium 1 mps   | 0x2126 | none       | start periodic acquisition |
| Periodic low 1 mps      | 0x212D | none       | start periodic acquisition |
| Periodic high 2 mps     | 0x2236 | none       | start periodic acquisition |
| Periodic medium 2 mps   | 0x2220 | none       | start periodic acquisition |
| Periodic low 2 mps      | 0x222B | none       | start periodic acquisition |
| Periodic high 4 mps     | 0x2334 | none       | start periodic acquisition |
| Periodic medium 4 mps   | 0x2322 | none       | start periodic acquisition |
| Periodic low 4 mps      | 0x2329 | none       | start periodic acquisition |
| Periodic high 10 mps    | 0x2737 | none       | start periodic acquisition |
| Periodic medium 10 mps  | 0x2721 | none       | start periodic acquisition |
| Periodic low 10 mps     | 0x272A | none       | start periodic acquisition |

### Data retrieval and control

| Command                       |                                Code | Parameters | Description                                       |
| ----------------------------- | ----------------------------------: | ---------- | ------------------------------------------------- |
| Fetch Data                    |                              0xE000 | none       | fetch most recent periodic RH/T result            |
| Periodic Measurement with ART |                              0x2B32 | none       | ART mode, 4 Hz acquisition                        |
| Break                         |                              0x3093 | none       | stop periodic acquisition, enter single-shot mode |
| Soft Reset                    |                              0x30A2 | none       | reset internal controller                         |
| General Call Reset            | 0x0006 on general-call address 0x00 | none       | bus-wide reset                                    |
| Heater Enable                 |                              0x306D | none       | enable heater                                     |
| Heater Disable                |                              0x3066 | none       | disable heater                                    |
| Read Status Register          |                              0xF32D | none       | read 16-bit status + CRC                          |
| Clear Status Register         |                              0x3041 | none       | clear alert/reset flags                           |

   

---

## 9.2 QMP6988 commands

QMP6988 is primarily **register-addressed** rather than command-opcode driven.

Explicit command-like write:

| Command        |                       Code | Parameters | Description                                  |
| -------------- | -------------------------: | ---------- | -------------------------------------------- |
| Software Reset | write 0xE6 to RESET (0xE0) | none       | reset device, reload default interface state |

All other operations are register reads/writes. 

---

## 10. Data Formats

## 10.1 SHT30 measurement data

A measurement returns:

* **16-bit unsigned temperature**
* **8-bit CRC**
* **16-bit unsigned humidity**
* **8-bit CRC**

Byte order is big-endian per 16-bit word: **MSB first, then LSB**.

Conversion formulas:

* **RH[%] = 100 × SRH / 2^16**
* **T[°C] = -45 + 175 × ST / 2^16**
* **T[°F] = -49 + 315 × ST / 2^16**

Where:

* `SRH` = raw 16-bit humidity word
* `ST` = raw 16-bit temperature word

Both are unsigned integers. 

---

## 10.2 SHT30 CRC

CRC properties:

| Property        | Value               |
| --------------- | ------------------- |
| Name            | CRC-8               |
| Polynomial      | 0x31                |
| Polynomial form | x^8 + x^5 + x^4 + 1 |
| Initialization  | 0xFF                |
| Reflect input   | False               |
| Reflect output  | False               |
| Final XOR       | 0x00                |
| Example         | CRC(0xBEEF)=0x92    |

CRC covers exactly the previous **two data bytes**. 

---

## 10.3 QMP6988 raw data

Raw temperature and raw pressure are stored in 3-byte fields:

* temperature: `TEMP_TXD2:TEMP_TXD1:TEMP_TXD0`
* pressure: `PRESS_TXD2:PRESS_TXD1:PRESS_TXD0`

The extracted text labels these as 24-bit fields, while the compensation section states the usable raw value is a **20–24 bit measurement value**. Driver rule:

* assemble a 24-bit unsigned value from the three bytes
* use the resulting integer directly as `Dt` or `Dp` in the compensation equations

 

---

## 10.4 QMP6988 compensated output formulas

Temperature intermediate result:

`Tr = a0 + a1·Dt + a2·Dt²`

Where:

* `Tr` is temperature calculation result in **[256 degreeC]**
* `Dt` is raw temperature reading
* `a0`, `a1`, `a2` are compensation coefficients

Pressure result:

`Pr = b00 + bt1·Tr + bp1·Dp + b11·Tr·Dp + bt2·Tr² + bp2·Dp² + b12·Dp·Tr² + b21·Dp²·Tr + bp3·Dp³`

Where:

* `Pr` is pressure result in **Pa**
* `Tr` is the above temperature result in **[256 degreeC]**
* `Dp` is raw pressure reading
* `b00`, `bt1`, `bp1`, `b11`, `bt2`, `bp2`, `b12`, `b21`, `bp3` are compensation coefficients

Coefficient packing and scaling visible in the extracted content:

| Coefficient | Storage                                        | Scaling data visible    |
| ----------- | ---------------------------------------------- | ----------------------- |
| a0          | 20Q16 from COE_a0_1, COE_a0_0, COE_b00_a0_ex   | offset value            |
| b00         | 20Q16 from COE_b00_1, COE_b00_0, COE_b00_a0_ex | offset value            |
| a1          | COE_a1_1, COE_a1_0                             | A=-6.30E-03, S=4.30E-04 |
| a2          | COE_a2_1, COE_a2_0                             | A=-1.90E-11, S=1.20E-10 |
| bt1         | COE_bt1_1, COE_bt1_0                           | A=1.00E-01, S=9.10E-02  |
| bt2         | COE_bt2_1, COE_bt2_0                           | A=1.20E-08, S=1.20E-06  |
| bp1         | COE_bp1_1, COE_bp1_0                           | A=3.30E-02, S=1.90E-02  |
| b11         | COE_b11_1, COE_b11_0                           | A=2.10E-07, S=1.40E-07  |
| bp2         | COE_bp2_1, COE_bp2_0                           | A=-6.30E-10, S=3.50E-10 |
| b12         | COE_b12_1, COE_b12_0                           | A=2.90E-13, S=7.60E-13  |
| b21         | COE_b21_1, COE_b21_0                           | A=2.10E-15, S=1.20E-14  |
| bp3         | COE_bp3_1, COE_bp3_0                           | A=1.30E-16, S=7.90E-17  |

The extracted snippet gives the coefficient conversion factors but does not fully expose the textual algebraic reconstruction rule. A safe generated implementation should therefore:

1. read the exact OTP bytes listed above
2. preserve the exact packed bit positions for `a0` and `b00`
3. use the documented polynomial forms for `Tr` and `Pr`
4. treat coefficient reconstruction as fixed factory calibration logic, not user-configurable state

 

---

## 11. Timing Requirements

## 11.1 SHT30 timing

### System timing

For **2.4 V to 5.5 V**:

| Parameter                                           | Min |  Typ |  Max | Units |
| --------------------------------------------------- | --: | ---: | ---: | ----- |
| Power-up time `tPU`                                 |   - |  0.5 |  1.0 | ms    |
| Soft reset time `tSR`                               |   - |  0.5 |  1.5 | ms    |
| Reset pulse width `tRESETN`                         |   1 |    - |    - | µs    |
| Measurement duration low repeatability `tMEAS,l`    |   - |  2.5 |  4.0 | ms    |
| Measurement duration medium repeatability `tMEAS,m` |   - |  4.5 |  6.0 | ms    |
| Measurement duration high repeatability `tMEAS,h`   |   - | 12.5 | 15.0 | ms    |

For **2.15 V to <2.4 V**:

| Parameter                                 | Min |  Typ |  Max | Units |
| ----------------------------------------- | --: | ---: | ---: | ----- |
| Power-up time `tPU`                       |   - |  0.5 |  1.5 | ms    |
| Measurement duration low repeatability    |   - |  2.5 |  4.5 | ms    |
| Measurement duration medium repeatability |   - |  4.5 |  6.5 | ms    |
| Measurement duration high repeatability   |   - | 12.5 | 15.5 | ms    |

Additional protocol timing:

* minimal wait after sending a command before another command can be received: **1 ms**

### I2C timing

| Parameter                          |  Min |  Max | Units |
| ---------------------------------- | ---: | ---: | ----- |
| SCL frequency                      |    0 | 1000 | kHz   |
| Hold time repeated START `tHD;STA` | 0.24 |    - | µs    |
| SCL LOW period `tLOW`              | 0.53 |    - | µs    |
| SCL HIGH period `tHIGH`            | 0.26 |    - | µs    |
| SDA hold time tx                   |    0 |  250 | ns    |
| SDA hold time rx                   |    0 |    - | ns    |
| SDA setup `tSU;DAT`                |  100 |    - | ns    |
| rise time `tR`                     |    - |  300 | ns    |
| fall time `tF`                     |    - |  300 | ns    |
| SDA valid `tVD;DAT`                |    - |  0.9 | µs    |
| repeated START setup `tSU;STA`     | 0.26 |    - | µs    |
| STOP setup `tSU;STO`               | 0.26 |    - | µs    |
| bus capacitance `CB`               |    - |  400 | pF    |

 

---

## 11.2 QMP6988 timing

| Parameter                             | Value          |
| ------------------------------------- | -------------- |
| Power-on startup time `Tstart`        | 10 ms          |
| Asynchronous reset pulse width `Trst` | 100 µs minimum |
| I2C standard mode                     | 100 kbit/s     |
| I2C fast mode                         | 400 kbit/s     |
| I2C high-speed mode                   | 3.4 Mbit/s     |
| SPI                                   | 10 Mbit/s      |

Forced-mode typical measurement time by oversampling:

| Pres OSR | Temp OSR | Typ measurement time |
| -------: | -------: | -------------------: |
|        2 |        1 |               5.5 ms |
|        4 |        1 |               7.2 ms |
|        8 |        1 |              10.6 ms |
|       16 |        2 |              18.3 ms |
|       32 |        4 |              33.7 ms |



---

## 12. Operating Modes

## 12.1 Module operating model

The module has no module-level state machine beyond shared I2C connectivity. Each internal sensor has its own mode model.

---

## 12.2 SHT30 modes

* idle
* single-shot acquisition
* periodic acquisition
* ART periodic acquisition
* heater enabled / disabled

Behavior:

* after power-up, sensor enters idle after `tPU`
* in single-shot mode, one command produces one RH/T pair
* in periodic mode, one command starts a stream of measurements
* `Fetch Data` retrieves the current periodic result and clears the data buffer
* `Break` stops periodic acquisition and returns to single-shot mode
* heater is separate functional state, used for plausibility checking only  

---

## 12.3 QMP6988 modes

* sleep mode
* forced mode
* normal mode

Mode encoding in `CTRL_MEAS.power_mode[1:0]`:

* `00` sleep
* `01` forced
* `10` forced
* `11` normal

Behavior:

* after POR or async reset, default interface mode is I2C
* forced mode performs one measurement then returns to sleep
* normal mode repeatedly measures with standby gaps determined by `t_standby`
* sleep mode performs no measurements, but registers remain accessible  

---

## 13. Reset Behavior

## 13.1 SHT30 reset behavior

Supported resets:

* power-up reset
* soft reset `0x30A2`
* general-call reset `0x0006` on address `0x00`
* hardware reset via `nRESET` low pulse for at least **1 µs**
* interface-only reset by toggling SCL **nine or more times** while leaving SDA high, followed by a transmission start

Effects:

* during reset the sensor will not process commands
* soft reset reloads calibration data
* `nRESET` is recommended for a full reset without removing supply
* hard reset requires removing VDD and also removing voltage from SDA, SCL, and ADDR to avoid back-powering through ESD diodes 

---

## 13.2 QMP6988 reset behavior

Supported resets:

* power-on reset
* asynchronous reset on dedicated pin
* software reset by writing **0xE6** to `RESET (0xE0)`

Facts:

* default mode after POR or async reset is **I2C**
* once CSB is pulled low and SPI mode is entered, communication mode will not change until POR or async reset
* power-on startup time: **10 ms**
* asynchronous reset pulse width minimum: **100 µs**   

---

## 14. Status and Diagnostics

## 14.1 SHT30 diagnostics

Primary diagnostic source: status register via **0xF32D**

Useful flags:

* reset detected
* command checksum failure
* write data checksum failure
* heater state
* alert status

Error handling rules:

* if command status bit is set, discard last operation result and reinitialize transaction state
* if write checksum status bit is set, resend the write operation
* after reset, status bit 4 will indicate reset detected until cleared with `0x3041` 

---

## 14.2 QMP6988 diagnostics

Primary diagnostic source: `DEVICE_STAT`

Useful flags:

* `measure=1` measurement in progress
* `otp_update=1` OTP access in progress

Driver use:

* poll these bits when coordinating initialization and measurement reads
* do not read raw/compensated results until measurement activity has completed 

---

## 15. Interrupts

## 15.1 Module-level interrupts

No separate module interrupt line is exposed on the external 4-pin connector.

## 15.2 SHT30 ALERT

SHT30 has an **ALERT** pin:

* output goes high when alert conditions are met
* if unused, must be left floating
* on the provided module schematic, ALERT is not routed to the external connector

Because the extracted command material does not include alert-threshold programming commands, generated module BSP code may safely omit ALERT support unless the hardware is respun. 

## 15.3 QMP6988 interrupts

No interrupt mechanism is described in the extracted content.

---

## 16. Operational Sequences

## 16.1 Module initialization sequence

1. bring up host I2C controller
2. configure I2C bus for standard or fast mode initially
3. probe **0x44** and **0x70**
4. initialize SHT30
5. initialize QMP6988
6. expose composite readings:

   * temperature_humidity from SHT30
   * pressure from QMP6988
   * optional QMP temperature as secondary diagnostic channel

---

## 16.2 SHT30 single-shot read sequence

1. send single-shot command
2. wait for measurement completion:

   * no clock stretching: wait by repeatability-specific time
   * clock stretching: read can be attempted and SCL may be held low by sensor
3. read 6 bytes
4. verify both CRC bytes
5. convert temperature and humidity

---

## 16.3 SHT30 periodic read sequence

1. send periodic-start command
2. wait until data is available
3. send `Fetch Data (0xE000)`
4. read 6 bytes
5. verify CRC
6. when leaving periodic mode, send `Break (0x3093)` and wait **1 ms**

---

## 16.4 QMP6988 initialization sequence

1. wait **10 ms** after power valid
2. read `CHIP_ID`; expect `0x5C`
3. read OTP coefficient block once after POR:

   * addresses `0xA0` through `0xB8`
4. configure `IO_SETUP` if needed
5. configure `IIR_CNT`
6. configure `CTRL_MEAS`
7. enter desired mode:

   * forced for on-demand reads
   * normal for periodic sampling

---

## 16.5 QMP6988 forced measurement sequence

1. write `CTRL_MEAS` with desired oversampling and `power_mode=forced`
2. wait or poll until `DEVICE_STAT.measure=0`
3. read `TEMP_TXD2..0`
4. read `PRESS_TXD2..0`
5. apply compensation using cached coefficients

---

## 17. Driver Initialization Sequence

Recommended startup sequence for a generated BSP:

1. initialize bus
2. delay until module supply stable
3. probe SHT30 at `0x44`
4. probe QMP6988 at `0x70`
5. SHT30:

   * optionally issue `0x30A2`
   * wait at least reset time
   * optionally read and clear status
6. QMP6988:

   * optionally write `RESET=0xE6`
   * wait startup/reset settle time
   * verify `CHIP_ID=0x5C`
   * read all OTP coefficients and cache them
   * program filter and oversampling defaults
7. expose unified module API

Suggested defaults:

* SHT30 single-shot, high repeatability, clock stretching disabled
* QMP6988 forced mode, pressure oversampling **16** or **32**, temperature oversampling **2** or **4**, IIR filter per noise/bandwidth needs

---

## 18. Runtime Operation Sequences

## 18.1 Read environment sample

Recommended composite read:

1. trigger SHT30 single-shot measurement
2. trigger QMP6988 forced measurement
3. wait for the slower of the two operations
4. read SHT30 result and validate CRC
5. read QMP6988 raw values
6. compensate QMP6988 pressure and temperature
7. publish:

   * ambient temperature from SHT30
   * relative humidity from SHT30
   * pressure from QMP6988

Rationale:

* SHT30 temperature is the primary ambient temperature channel
* QMP6988 temperature is required internally for pressure compensation and may be exposed as a secondary value

## 18.2 Reset recovery

If SHT30 stops responding:

1. toggle SCL nine or more times with SDA high
2. send start condition
3. retry command
4. if still failing, use `0x30A2`
5. if still failing and hardware permits, assert nRESET or power-cycle

If QMP6988 fails ID or bus access:

1. write `RESET=0xE6`
2. wait startup time
3. reread `CHIP_ID`
4. if recovered, reread OTP coefficients

---

## 19. Driver State Model

The generated driver should track at minimum:

### Module state

* I2C bus handle
* SHT30 presence
* QMP6988 presence

### SHT30 state

* device address
* acquisition mode: idle / single-shot / periodic / ART
* selected repeatability
* heater enabled flag
* last status word
* last valid temperature/humidity
* optional timestamp of last reading

### QMP6988 state

* device address
* cached OTP coefficients
* configured oversampling
* configured IIR coefficient
* configured standby time
* current power mode
* last raw Dt, Dp
* last compensated pressure
* last compensated temperature

---

## 20. Required Safety and Correctness Rules for Code Generation

1. Always treat Unit ENV-III as a **two-device composite module**, not as a single monolithic sensor.
2. Use **SHT30 address 0x44** and **QMP6988 address 0x70** for this board variant.
3. Verify **QMP6988 CHIP_ID == 0x5C** before enabling runtime use.
4. Read QMP6988 OTP coefficients once after POR/reset and cache them.
5. Never write nonzero values into reserved QMP6988 bits.
6. Preserve QMP6988 writable fields with explicit masking.
7. Do not read QMP6988 conversion results before measurement completion.
8. Respect SHT30 measurement timing for selected repeatability.
9. Validate every SHT30 CRC before accepting data.
10. Do not use SHT30 measurement data if CRC fails.
11. When using SHT30 periodic mode, use `Fetch Data` to read and `Break` before switching modes.
12. After SHT30 `Break`, wait **1 ms** before sending other commands.
13. After SHT30 reset, allow reset completion before next command.
14. For SHT30 hard reset, be aware that SDA/SCL/ADDR must not back-power the sensor.
15. Use SHT30 temperature conversion formula exactly as given.
16. Use SHT30 humidity conversion formula exactly as given.
17. Use the QMP6988 polynomial compensation chain exactly in the documented order.
18. Keep QMP6988 IO_SETUP reserved bits at 0.
19. Keep QMP6988 I2C_SET reserved bits at 0.
20. Keep QMP6988 IIR reserved bits at 0.
21. Treat QMP6988 `power_mode=01` and `10` as forced mode.
22. In QMP6988 forced mode, expect automatic return to sleep after the measurement finishes.
23. If a soft reset is issued to QMP6988, cached configuration must be reprogrammed and OTP coefficients reread if reset semantics require fresh initialization.
24. Use SHT30 as the primary temperature source for application-facing ambient temperature unless a system requirement says otherwise.
25. Do not enable SHT30 heater during normal environmental acquisition; it is for plausibility checking only.
26. Avoid running SHT30 periodic acquisition at highest rates unless self-heating effects are acceptable.
27. Do not assume any module-level interrupt is available externally.
28. Do not assume SPI support on this board, even though QMP6988 supports SPI generically.
29. Because the module already includes bus pull-ups, avoid adding overly strong parallel pull-ups in firmware-controlled boards.
30. Expose failure states separately for SHT30 and QMP6988.

---

## 21. Canonical Constants

```c
// Module
#define ENV3_I2C_ADDR_SHT30          0x44
#define ENV3_I2C_ADDR_QMP6988        0x70

// SHT30 commands
#define SHT30_CMD_SINGLE_H_CS        0x2C06
#define SHT30_CMD_SINGLE_M_CS        0x2C0D
#define SHT30_CMD_SINGLE_L_CS        0x2C10
#define SHT30_CMD_SINGLE_H           0x2400
#define SHT30_CMD_SINGLE_M           0x240B
#define SHT30_CMD_SINGLE_L           0x2416

#define SHT30_CMD_PERIODIC_05_H      0x2032
#define SHT30_CMD_PERIODIC_05_M      0x2024
#define SHT30_CMD_PERIODIC_05_L      0x202F
#define SHT30_CMD_PERIODIC_1_H       0x2130
#define SHT30_CMD_PERIODIC_1_M       0x2126
#define SHT30_CMD_PERIODIC_1_L       0x212D
#define SHT30_CMD_PERIODIC_2_H       0x2236
#define SHT30_CMD_PERIODIC_2_M       0x2220
#define SHT30_CMD_PERIODIC_2_L       0x222B
#define SHT30_CMD_PERIODIC_4_H       0x2334
#define SHT30_CMD_PERIODIC_4_M       0x2322
#define SHT30_CMD_PERIODIC_4_L       0x2329
#define SHT30_CMD_PERIODIC_10_H      0x2737
#define SHT30_CMD_PERIODIC_10_M      0x2721
#define SHT30_CMD_PERIODIC_10_L      0x272A

#define SHT30_CMD_FETCH_DATA         0xE000
#define SHT30_CMD_ART                0x2B32
#define SHT30_CMD_BREAK              0x3093
#define SHT30_CMD_SOFT_RESET         0x30A2
#define SHT30_CMD_HEATER_ENABLE      0x306D
#define SHT30_CMD_HEATER_DISABLE     0x3066
#define SHT30_CMD_READ_STATUS        0xF32D
#define SHT30_CMD_CLEAR_STATUS       0x3041

// QMP6988 registers
#define QMP6988_REG_TEMP_TXD0        0xFC
#define QMP6988_REG_TEMP_TXD1        0xFB
#define QMP6988_REG_TEMP_TXD2        0xFA
#define QMP6988_REG_PRESS_TXD0       0xF9
#define QMP6988_REG_PRESS_TXD1       0xF8
#define QMP6988_REG_PRESS_TXD2       0xF7
#define QMP6988_REG_IO_SETUP         0xF5
#define QMP6988_REG_CTRL_MEAS        0xF4
#define QMP6988_REG_DEVICE_STAT      0xF3
#define QMP6988_REG_I2C_SET          0xF2
#define QMP6988_REG_IIR_CNT          0xF1
#define QMP6988_REG_RESET            0xE0
#define QMP6988_REG_CHIP_ID          0xD1

#define QMP6988_REG_COE_B00_A0_EX    0xB8
#define QMP6988_REG_COE_A2_0         0xB7
#define QMP6988_REG_COE_A2_1         0xB6
#define QMP6988_REG_COE_A1_0         0xB5
#define QMP6988_REG_COE_A1_1         0xB4
#define QMP6988_REG_COE_A0_0         0xB3
#define QMP6988_REG_COE_A0_1         0xB2
#define QMP6988_REG_COE_BP3_0        0xB1
#define QMP6988_REG_COE_BP3_1        0xB0
#define QMP6988_REG_COE_B21_0        0xAF
#define QMP6988_REG_COE_B21_1        0xAE
#define QMP6988_REG_COE_B12_0        0xAD
#define QMP6988_REG_COE_B12_1        0xAC
#define QMP6988_REG_COE_BP2_0        0xAB
#define QMP6988_REG_COE_BP2_1        0xAA
#define QMP6988_REG_COE_B11_0        0xA9
#define QMP6988_REG_COE_B11_1        0xA8
#define QMP6988_REG_COE_BP1_0        0xA7
#define QMP6988_REG_COE_BP1_1        0xA6
#define QMP6988_REG_COE_BT2_0        0xA5
#define QMP6988_REG_COE_BT2_1        0xA4
#define QMP6988_REG_COE_BT1_0        0xA3
#define QMP6988_REG_COE_BT1_1        0xA2
#define QMP6988_REG_COE_B00_0        0xA1
#define QMP6988_REG_COE_B00_1        0xA0

#define QMP6988_CHIP_ID_VALUE        0x5C
#define QMP6988_RESET_CODE           0xE6
```

---

## 22. Bitfield Constants

```c
// QMP6988 IO_SETUP
#define QMP6988_IO_SETUP_T_STANDBY_MASK      0xE0
#define QMP6988_IO_SETUP_T_STANDBY_SHIFT     5
#define QMP6988_IO_SETUP_SPI3_SDIM_MASK      0x04
#define QMP6988_IO_SETUP_SPI3_SDIM_SHIFT     2
#define QMP6988_IO_SETUP_SPI3W_MASK          0x01
#define QMP6988_IO_SETUP_SPI3W_SHIFT         0

// QMP6988 CTRL_MEAS
#define QMP6988_CTRL_MEAS_TEMP_AVG_MASK      0xE0
#define QMP6988_CTRL_MEAS_TEMP_AVG_SHIFT     5
#define QMP6988_CTRL_MEAS_PRESS_AVG_MASK     0x1C
#define QMP6988_CTRL_MEAS_PRESS_AVG_SHIFT    2
#define QMP6988_CTRL_MEAS_POWER_MODE_MASK    0x03
#define QMP6988_CTRL_MEAS_POWER_MODE_SHIFT   0

// QMP6988 DEVICE_STAT
#define QMP6988_DEVICE_STAT_MEASURE_MASK     0x08
#define QMP6988_DEVICE_STAT_MEASURE_SHIFT    3
#define QMP6988_DEVICE_STAT_OTP_UPDATE_MASK  0x01
#define QMP6988_DEVICE_STAT_OTP_UPDATE_SHIFT 0

// QMP6988 I2C_SET
#define QMP6988_I2C_SET_MASTER_CODE_MASK     0x07
#define QMP6988_I2C_SET_MASTER_CODE_SHIFT    0

// QMP6988 IIR
#define QMP6988_IIR_FILTER_MASK              0x07
#define QMP6988_IIR_FILTER_SHIFT             0

// SHT30 status bits
#define SHT30_STATUS_ALERT_PENDING_MASK      0x8000
#define SHT30_STATUS_HEATER_ON_MASK          0x2000
#define SHT30_STATUS_RH_ALERT_MASK           0x0800
#define SHT30_STATUS_T_ALERT_MASK            0x0400
#define SHT30_STATUS_RESET_DETECTED_MASK     0x0010
#define SHT30_STATUS_COMMAND_STATUS_MASK     0x0002
#define SHT30_STATUS_WRITE_CSUM_STATUS_MASK  0x0001
```

---

## 23. Enumerations

```c
typedef enum {
    ENV3_OK = 0,
    ENV3_ERR_BUS,
    ENV3_ERR_SHT30_NOT_FOUND,
    ENV3_ERR_QMP6988_NOT_FOUND,
    ENV3_ERR_SHT30_CRC,
    ENV3_ERR_QMP6988_CHIP_ID,
    ENV3_ERR_TIMEOUT,
    ENV3_ERR_INVALID_STATE
} env3_status_t;

typedef enum {
    SHT30_REPEATABILITY_LOW = 0,
    SHT30_REPEATABILITY_MEDIUM,
    SHT30_REPEATABILITY_HIGH
} sht30_repeatability_t;

typedef enum {
    SHT30_MODE_IDLE = 0,
    SHT30_MODE_SINGLE_SHOT,
    SHT30_MODE_PERIODIC,
    SHT30_MODE_ART
} sht30_mode_t;

typedef enum {
    QMP6988_POWER_SLEEP  = 0,
    QMP6988_POWER_FORCED_01 = 1,
    QMP6988_POWER_FORCED_10 = 2,
    QMP6988_POWER_NORMAL = 3
} qmp6988_power_mode_t;

typedef enum {
    QMP6988_AVG_SKIP = 0,
    QMP6988_AVG_1,
    QMP6988_AVG_2,
    QMP6988_AVG_4,
    QMP6988_AVG_8,
    QMP6988_AVG_16,
    QMP6988_AVG_32,
    QMP6988_AVG_64
} qmp6988_avg_t;

typedef enum {
    QMP6988_STANDBY_1MS = 0,
    QMP6988_STANDBY_5MS,
    QMP6988_STANDBY_50MS,
    QMP6988_STANDBY_250MS,
    QMP6988_STANDBY_500MS,
    QMP6988_STANDBY_1S,
    QMP6988_STANDBY_2S,
    QMP6988_STANDBY_4S
} qmp6988_standby_t;

typedef enum {
    QMP6988_IIR_OFF = 0,
    QMP6988_IIR_2,
    QMP6988_IIR_4,
    QMP6988_IIR_8,
    QMP6988_IIR_16,
    QMP6988_IIR_32_A,
    QMP6988_IIR_32_B,
    QMP6988_IIR_32_C
} qmp6988_iir_t;
```

---

## 24. Minimal Functional Feature Set

A minimally correct driver/BSP for Unit ENV-III must implement:

1. I2C probing for `0x44` and `0x70`
2. SHT30 single-shot measurement
3. SHT30 CRC verification
4. SHT30 raw-to-physical conversion
5. QMP6988 chip ID verification
6. QMP6988 coefficient readout
7. QMP6988 forced measurement
8. QMP6988 raw data readout
9. QMP6988 polynomial compensation
10. unified module API returning:

* temperature
* humidity
* pressure

---

## 25. Final Implementation Intent

A generated driver should behave as a **composite environmental sensor driver** that:

* initializes both internal devices
* isolates their bus transactions cleanly
* validates data integrity
* applies exact documented conversion formulas
* returns stable physical units for application use

Common implementation mistakes to avoid:

* treating the module as one I2C address instead of two
* using the wrong QMP6988 address (`0x56` instead of module’s `0x70`)
* skipping SHT30 CRC verification
* reading QMP6988 values without OTP coefficients
* forgetting that QMP6988 forced mode returns to sleep automatically
* writing reserved QMP6988 bits
* leaving SHT30 in periodic mode unintentionally
* enabling SHT30 heater during normal sensing
* ignoring required waits after reset, break, or measurement trigger

The correct BSP should expose **ambient temperature and humidity from SHT30** and **pressure from QMP6988**, while optionally exposing the QMP6988 compensated temperature as an auxiliary channel.
