# Copy this file to <circle>/Config.mk or merge it with an existing Circle config.
AARCH = 64
RASPPI = 5
STDLIB_SUPPORT = 1

PREFIX64 = C:/Users/dennis/Documents/GitHub/MINERVA_closed/fantasy-pi/toolchain/aarch64-none-elf/bin/aarch64-none-elf-

# Keep the initial video path simple. Deeper ports can switch to direct
# framebuffer access or C2DGraphics.
DEFINE += -DDEPTH=16
