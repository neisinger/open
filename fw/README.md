# Firmware for SoundSlide Device
This repository contains the firmware for the SoundSlide device, powered by the SAMD11D14A microcontroller.

## Prerequisites

Install the necessary dependencies. On Ubuntu, run:

```sh
sudo apt install git make gcc-arm-none-eabi openocd npm
sudo npm install -g burgrp/silicon
```

## Building the Firmware

To build the firmware, first clone the repository and resolve firmware dependencies.

```sh
git clone https://github.com/soundslide/open.git soundslide
cd soundslide/fw
make deps
```

The `make deps` command installs dependencies and builds the initial firmware image. The ELF file will be located at `build/build.elf`.

For subsequent builds, simply run:
```sh
make build
```

## Flashing

You have two options for flashing the firmware:

### Option 1: Using SWD

For unprogrammed chips, use SWD via a JTAG connection to the C (clock) and D (data) pads on the PCB.

In one terminal, start `openocd` (assuming a CMSIS-DAP adapter is connected):

```sh
openocd -f interface/cmsis-dap.cfg -f target/at91samdXX.cfg
```

In a separate terminal, run:
```sh
make flash
```

### Option 2: Using USB

For subsequent updates, use the [SoundSlide CLI](../cli) utility:

```sh
ssc upgrade build/build.elf
```

## Gesture Controls

SoundSlide supports the following touch gestures:

### Slide Gestures

Slide your finger along the touch strip to control volume, brightness, or scroll (configurable via CLI):

| Function | Slide Up/Right | Slide Down/Left |
|----------|----------------|-----------------|
| Volume (default) | Increase volume | Decrease volume |
| Brightness | Increase brightness | Decrease brightness |
| Scroll | Scroll up | Scroll down |

Configure the slide function using the CLI:
```sh
ssc set function volume    # Default
ssc set function brightness
ssc set function scroll
```

### Tap Gestures

| Gesture | Action | Description |
|---------|--------|-------------|
| **Single Tap** | Mute Microphone | Quick tap and release (< 300ms, no movement) toggles microphone mute |
| **Double Tap** | Lock Workstation | Two quick taps within 400ms sends Win+L to lock your computer (Windows) |

#### How Tap Detection Works

1. **Single Tap**: Touch the sensor briefly (under 300ms) without sliding. If no second tap follows within 400ms, the microphone mute command is sent.

2. **Double Tap**: Perform two single taps in quick succession (within 400ms of each other). This sends the Windows+L keyboard shortcut to lock your workstation.

#### Notes

- Tap gestures are always enabled and work independently of the configured slide function
- The double-tap lock feature uses the Win+L keyboard shortcut, which works on Windows systems
- On macOS, the Win+L combination may not lock the screen by default (use system preferences to configure)
- On Linux, the behavior depends on your desktop environment
