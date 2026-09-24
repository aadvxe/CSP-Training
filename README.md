# CSP-Training (CubeSat Space Protocol Training & Experiments)

A comprehensive development and training environment for experimenting with **CubeSat Space Protocol (CSP / libcsp)**, **CAN bus communication**, and **STM32 embedded integration**.

---

## 🛰️ Project Overview

**CubeSat Space Protocol (CSP)** is a small network-layer delivery protocol designed specifically for CubeSats and small satellite subsystems. It provides a lightweight transport mechanism similar to TCP/IP but optimized for microcontrollers, constrained memory, and lossy radio links (CAN bus, I2C, UART, KISS/AX.25).

This repository folder consolidates the core CSP source code, build toolchains, PCAN USB interface drivers, and dual STM32 hardware reference implementations (STM32H7 and STM32L4).

---

## 📁 Folder Structure

```
CSP-Training/
├── can_bus/                     # Custom CAN bus driver interface
│   ├── inc/                     # Header files for CAN operations
│   └── src/                     # Source files for CAN bus abstraction
├── csp/                         # Compiled CSP library artifacts & public headers
│   ├── build/                   # Compiled static/shared objects
│   ├── include/csp/             # Core CSP headers (csp.h, csp_buffer.h, etc.)
│   └── src/                     # CSP source implementation
├── CSP_test/                    # STM32CubeIDE project for STM32H753ZI
│   ├── Core/                    # Application source code (main.c, freertos.c)
│   ├── Drivers/                 # STM32H7 HAL & CMSIS drivers
│   ├── Middlewares/             # FreeRTOS middleware
│   └── CSP_test.ioc             # STM32CubeMX configuration (FDCAN1 + FreeRTOS)
├── il_CSPL4R5/                  # STM32CubeIDE project for STM32L4R5ZI
│   ├── Core/                    # Application source code (main.c, freertos.c)
│   ├── Drivers/                 # STM32L4 HAL & CMSIS drivers
│   ├── Middlewares/             # FreeRTOS middleware
│   └── il_CSPL4R5.ioc           # STM32CubeMX configuration (CAN1 + FreeRTOS)
├── libcsp/                      # Upstream Git submodule/tree of libcsp v1.x / v2.x
│   ├── contrib/                 # OS-specific bindings and helpers
│   ├── doc/                     # CSP protocol documentation
│   ├── examples/                # Example clients, servers, and routers
│   ├── include/                 # Full libcsp header tree
│   ├── src/                     # Source tree (interfaces, arch, crypto)
│   └── utils/                   # Utilities and test scripts
├── meson-1.6.0rc2/              # Meson build system package for compiling libcsp
├── PCBUSB/                      # macOS PEAK PCAN-USB user-space driver
│   ├── Examples/                # C/C++ examples for PCAN-USB
│   ├── libPCBUSB.dylib          # Dynamic library for macOS
│   ├── PCBUSB.h                 # C header API for PCAN-USB
│   └── install.sh               # Installation script for PCBUSB
├── macOS_Library_for_PCANUSB_v0.13.tar  # Distribution archive for PCAN-USB on macOS
└── meson-1.6.0rc2.tar                   # Archive of Meson build system
```

---

## 🎯 Target Hardware & Platforms

| Subproject | Target MCU / Board | Peripherals Used | RTOS |
| :--- | :--- | :--- | :--- |
| **`CSP_test`** | STM32H753ZITx (NUCLEO-H753ZI) | FDCAN1, USART, SysTick | FreeRTOS (CMSIS-RTOS v2) |
| **`il_CSPL4R5`** | STM32L4R5ZITx (NUCLEO-L4R5ZI) | CAN1 (bxCAN), LPUART1, USART3 | FreeRTOS (CMSIS-RTOS v2) |
| **`PCBUSB`** | macOS (x86_64 / arm64) | PEAK PCAN-USB hardware dongle | N/A |
| **`libcsp`** | Cross-platform (POSIX, Linux, FreeRTOS, Embedded ARM) | CAN, I2C, UART | POSIX / FreeRTOS |

---

## 🚀 Getting Started

### 1. Embedded Firmware Development (STM32)
1. Open **STM32CubeIDE**.
2. Select **File > Open Projects from File System...** and navigate to either:
   - `CSP-Training/CSP_test` (for STM32H7)
   - `CSP-Training/il_CSPL4R5` (for STM32L4)
3. Ensure ST-LINK is connected to the corresponding NUCLEO board.
4. Click **Project > Build Project** (`Ctrl+B`).
5. Run or Debug (`F11`).

### 2. Host-Side CAN Testing (macOS / PCAN-USB)
If developing host ground station software using a PEAK-System PCAN-USB adapter on macOS:
```bash
cd PCBUSB
./install.sh
# Compile and run examples
cd Examples/C++
make
./pcan_test
```

### 3. Compiling libcsp on Host
Libcsp utilizes the **Meson** / **Ninja** build system:
```bash
cd libcsp
meson setup builddir --buildtype=release
ninja -C builddir
```

---

## 🔗 Related Projects
- [H753ZI_FreeRTOSnCAN](file:///c:/Users/Rangga/Documents/Old%20Project%20Archive/H753ZI_FreeRTOSnCAN): Standalone FreeRTOS + FDCAN project for NUCLEO-H753ZI.
- [L4R5ZI_Transmit](file:///c:/Users/Rangga/Documents/Old%20Project%20Archive/L4R5ZI_Transmit): Standalone CAN & UART transmit project for NUCLEO-L4R5ZI.
