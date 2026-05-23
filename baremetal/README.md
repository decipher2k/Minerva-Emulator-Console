# RetroArch/libretro Bare-Metal Circle MVP

This directory is the first practical slice of a Raspberry Pi 5 bare-metal
RetroArch direction: a small Circle application that acts as a static libretro
runner. It is intentionally not the full RetroArch frontend yet.

## Status

- Target: Raspberry Pi 5, AArch64, Circle.
- Output: Circle normally emits `kernel_2712.img` for Pi 5.
- Core loading: static only.
- Included test core: `libretro/builtin_pattern_core.cpp`.
- Video: Circle screen framebuffer path, RGB565-oriented.
- Audio: HDMI sound device path, signed 16-bit stereo batches.
- Input: Circle USB HID gamepad mapped to RetroPad.
- Filesystem: SD card `emmc1-1` mounted through Circle native FAT.

## Build outline with Node.js

Clone Circle next to this repository as `circle` or pass
`CIRCLEHOME=/path/to/circle`. The Node build script compiles the required
Circle libraries and this runner directly; it does not call `make`.

From the repository root:

```sh
npm run build:baremetal
```

Der NES-Prototyp mit statisch gelinktem FCEUmm-Core wird so gebaut:

```sh
npm run build:baremetal:nes
```

Oder direkt mit eigenen Parametern:

```sh
node baremetal/build.mjs --core=fceumm --rom=GAME.NES
```

The default output on Raspberry Pi 5 is:

```text
baremetal/build-node/<core>/kernel_2712.img
```

For a `kernel8.img` experiment, rename the output and set `kernel=kernel8.img`
in `config.txt`. The Circle/Pi-5-native filename remains `kernel_2712.img`.

## SD card

Minimum files on the FAT boot partition:

```text
kernel_2712.img
bcm2712-rpi-5-b.dtb
overlays/bcm2712d0.dtbo
config.txt
cmdline.txt
```

The current built-in pattern core does not require a ROM. For the NES/FCEUmm
slice, put a short-name file such as `GAME.NES` into the FAT root and build
with `npm run build:baremetal:nes` or pass `--rom=<name>`.
