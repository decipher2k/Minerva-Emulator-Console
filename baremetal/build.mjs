#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import crypto from "node:crypto";
import fs from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

const repoRoot = path.resolve(path.join(path.dirname(fileURLToPath(import.meta.url)), ".."));
const projectRoot = path.join(repoRoot, "baremetal");

const cli = new Map();
for (const arg of process.argv.slice(2)) {
  const match = arg.match(/^--([^=]+)=(.*)$/);
  if (match) cli.set(match[1], match[2]);
}

const selectedCore = (cli.get("core") || process.env.CORE || "pattern").toLowerCase();
const selectedN64FrameSkip = cli.get("n64-frameskip") || process.env.N64_FRAMESKIP || "3";
const selectedN64PresentDivisor = cli.get("n64-vi-divisor")
  || process.env.N64_VI_DIVISOR
  || String(Math.max(1, Number.parseInt(selectedN64FrameSkip, 10) + 1 || 1));

const config = {
  circleHome: path.resolve(process.env.CIRCLEHOME || path.join(repoRoot, "circle")),
  toolchain: path.resolve(
    process.env.TOOLCHAIN ||
      "C:/Users/dennis/Documents/GitHub/MINERVA_closed/fantasy-pi/toolchain/aarch64-none-elf",
  ),
  core: selectedCore,
  romPath: cli.get("rom") || process.env.ROM_PATH || (selectedCore === "fceumm" || selectedCore === "multi" ? "GAME.NES" : selectedCore === "n64" ? "GAME.Z64" : "GAME.GB"),
  buildDir: path.join(projectRoot, "build-node", selectedCore),
  target: "kernel_2712",
  optimize: process.env.OPTIMIZE || "-O3",
  noUsb: selectedCore === "fceumm" && (cli.get("usb") === "0" || process.env.USB === "0"),
  kernelMaxSize: cli.get("kernel-max-size") || process.env.KERNEL_MAX_SIZE || (selectedCore === "n64" || selectedCore === "multi" ? "0x08000000" : selectedCore === "fceumm" ? "0x800000" : "0x200000"),
  hdmiPhysicalWidth: cli.get("hdmi-physical-width") || process.env.HDMI_PHYSICAL_WIDTH || "1920",
  hdmiPhysicalHeight: cli.get("hdmi-physical-height") || process.env.HDMI_PHYSICAL_HEIGHT || "1080",
  n64FrameSkip: selectedN64FrameSkip,
  n64ViDivisor: selectedN64PresentDivisor,
};

const bin = path.join(config.toolchain, "bin");
const tools = {
  cc: path.join(bin, "aarch64-none-elf-gcc.exe"),
  cxx: path.join(bin, "aarch64-none-elf-g++.exe"),
  ld: path.join(bin, "aarch64-none-elf-ld.exe"),
  ar: path.join(bin, "aarch64-none-elf-ar.exe"),
  objcopy: path.join(bin, "aarch64-none-elf-objcopy.exe"),
  objdump: path.join(bin, "aarch64-none-elf-objdump.exe"),
  cxxfilt: path.join(bin, "aarch64-none-elf-c++filt.exe"),
};

function fail(message) {
  console.error(`\nERROR: ${message}`);
  process.exit(1);
}

function ensureFile(file) {
  if (!fs.existsSync(file)) {
    fail(`Missing file: ${file}`);
  }
}

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function run(label, exe, args, options = {}) {
  ensureFile(exe);
  console.log(`${label.padEnd(7)} ${options.display || ""}`);
  const result = spawnSync(exe, args, {
    cwd: options.cwd || repoRoot,
    encoding: "utf8",
    stdio: options.capture ? "pipe" : "inherit",
    maxBuffer: 64 * 1024 * 1024,
  });

  if (result.status !== 0) {
    if (options.capture) {
      if (result.stdout) process.stdout.write(result.stdout);
      if (result.stderr) process.stderr.write(result.stderr);
    }
    fail(`${label.trim()} failed: ${exe} ${args.join(" ")}`);
  }

  return result.stdout || "";
}

function outputOf(exe, args) {
  return run("QUERY", exe, args, { capture: true }).trim().replace(/^"|"$/g, "");
}

function newer(target, inputs) {
  if (!fs.existsSync(target)) return true;
  const targetTime = fs.statSync(target).mtimeMs;
  return inputs.some((input) => fs.statSync(input).mtimeMs > targetTime);
}

function relFromRoot(file) {
  return path.relative(repoRoot, file).replace(/\\/g, "/");
}

function objPathFor(source) {
  const rel = relFromRoot(source).replace(/[:]/g, "");
  const hash = crypto.createHash("sha1").update(rel).digest("hex").slice(0, 12);
  const base = path.basename(source).replace(/[^A-Za-z0-9_.-]/g, "_");
  return path.join(config.buildDir, "obj", `${hash}_${base}.o`);
}

function archivePath(name) {
  return path.join(config.buildDir, "lib", name);
}

function prefixed(base, items, ext = ".cpp") {
  return items.map((item) => path.join(base, item.endsWith(".S") || item.endsWith(".c") || item.endsWith(".cpp") ? item : `${item}${ext}`));
}

function existingSources(base, names) {
  return names.map((name) => {
    const source = path.join(base, name);
    ensureFile(source);
    return source;
  });
}

function compile(source, extraFlags = [], options = {}) {
  const object = objPathFor(source);
  ensureDir(path.dirname(object));

  const ext = path.extname(source).toLowerCase();
  const isCxx = ext === ".cpp";
  const isAsm = ext === ".s";
  const exe = isCxx ? tools.cxx : tools.cc;
  const std = isCxx ? ["-std=c++17", "-fno-exceptions", "-fno-rtti", "-nostdinc++"] : ext === ".c" ? ["-std=gnu99"] : [];
  const args = [
    ...(options.prependFlags || []),
    ...commonCompileFlags,
    ...std,
    ...extraFlags,
    "-c",
    "-o",
    object,
    source,
  ];

  const stamp = `${object}.cmd`;
  const commandKey = JSON.stringify([exe, args]);
  if (!newer(object, [source]) && fs.existsSync(stamp) && fs.readFileSync(stamp, "utf8") === commandKey) {
    return object;
  }

  run(isAsm ? "AS" : isCxx ? "CXX" : "CC", exe, args, { display: relFromRoot(source) });
  fs.writeFileSync(stamp, commandKey);
  return object;
}

function archive(name, sources, extraFlags = [], options = {}) {
  const output = archivePath(name);
  ensureDir(path.dirname(output));
  const objects = sources.map((source) => compile(source, extraFlags, options));

  if (!newer(output, objects)) {
    return output;
  }

  if (fs.existsSync(output)) fs.unlinkSync(output);
  const chunkSize = 80;
  for (let i = 0; i < objects.length; i += chunkSize) {
    const chunk = objects.slice(i, i + chunkSize);
    run("AR", tools.ar, [i === 0 ? "cr" : "r", output, ...chunk], {
      display: i === 0 ? relFromRoot(output) : `${relFromRoot(output)} +${i}`,
    });
  }
  return output;
}

function parseLibs() {
  const libgcc = outputOf(tools.cc, ["-mcpu=cortex-a76", "-mlittle-endian", "-print-file-name=libgcc.a"]);
  const libm = outputOf(tools.cc, ["-mcpu=cortex-a76", "-mlittle-endian", "-print-file-name=libm.a"]);
  const libc = path.join(config.toolchain, "aarch64-none-elf", "lib", "libc.a");
  const libnosys = path.join(config.toolchain, "aarch64-none-elf", "lib", "libnosys.a");
  ensureFile(libgcc);
  ensureFile(libm);
  ensureFile(libc);
  ensureFile(libnosys);
  return { libgcc, libm, libc, libnosys };
}

function listSources(dir, extension = ".c") {
  return fs.readdirSync(dir)
    .filter((file) => file.endsWith(extension))
    .sort()
    .map((file) => path.join(dir, file));
}

function libretroSymbolDefines(prefix) {
  return [
    "retro_init",
    "retro_deinit",
    "retro_api_version",
    "retro_get_system_info",
    "retro_get_system_av_info",
    "retro_set_environment",
    "retro_set_video_refresh",
    "retro_set_audio_sample",
    "retro_set_audio_sample_batch",
    "retro_set_input_poll",
    "retro_set_input_state",
    "retro_set_controller_port_device",
    "retro_reset",
    "retro_run",
    "retro_serialize_size",
    "retro_serialize",
    "retro_unserialize",
    "retro_cheat_reset",
    "retro_cheat_set",
    "retro_load_game",
    "retro_load_game_special",
    "retro_unload_game",
    "retro_get_region",
    "retro_get_memory_data",
    "retro_get_memory_size",
  ].map((symbol) => `-D${symbol}=${prefix}_${symbol}`);
}

function prefixedGlobalDefines(prefix, symbols) {
  return symbols.map((symbol) => `-D${symbol}=${prefix}_${symbol}`);
}

const multiCoreSharedSymbolDefines = [
  "md5_finish",
  "option_cats_us",
  "option_defs_us",
  "options_us",
  "string_trim_whitespace",
  "string_trim_whitespace_left",
  "string_trim_whitespace_right",
  "strlcat_retro__",
  "strlcpy_retro__",
];

const includeFlags = [
  "-I", path.join(config.circleHome, "include"),
  "-I", path.join(config.circleHome, "addon"),
  "-I", path.join(config.circleHome, "app", "lib"),
  "-I", path.join(config.circleHome, "addon", "vc4"),
  "-I", path.join(config.circleHome, "addon", "vc4", "interface", "khronos", "include"),
  "-I", projectRoot,
  "-I", path.join(projectRoot, "kernel"),
  "-I", path.join(projectRoot, "libretro"),
  "-I", path.join(projectRoot, "platform", "circle"),
  "-I", path.join(repoRoot, "src", "libretro-common", "include"),
];

const defines = [
  "-DAARCH=64",
  "-DRASPPI=5",
  "-D__circle__=510000",
  "-DSTDLIB_SUPPORT=1",
  "-D__VCCOREVER__=0x04000000",
  "-DDEPTH=16",
  `-DKERNEL_MAX_SIZE=${config.kernelMaxSize}`,
  `-DRA_BAREMETAL_HDMI_PHYSICAL_WIDTH=${config.hdmiPhysicalWidth}`,
  `-DRA_BAREMETAL_HDMI_PHYSICAL_HEIGHT=${config.hdmiPhysicalHeight}`,
  `-DRA_BAREMETAL_ROM_PATH="${config.romPath}"`,
  `-DRA_BAREMETAL_N64_FRAMESKIP=${config.n64FrameSkip}`,
  `-DRA_BAREMETAL_N64_VI_PRESENT_DIVISOR=${config.n64ViDivisor}`,
  `-DRA_BAREMETAL_MAX_ROM_SIZE=${config.core === "n64" || config.core === "multi" ? "0x4000000" : "0x1000000"}`,
  ...(config.core === "fceumm" ? ["-DRA_BAREMETAL_FCEUMM=1"] : []),
  ...(config.core === "n64" ? ["-DRA_BAREMETAL_N64=1"] : []),
  ...(config.core === "multi" ? ["-DRA_BAREMETAL_MULTI=1"] : []),
  ...(config.core === "n64" || config.core === "multi" ? ["-DARM_ALLOW_MULTI_CORE=1"] : []),
  ...(config.core === "n64" || config.core === "multi" ? ["-DRA_BAREMETAL_ENABLE_JIT=1"] : []),
  ...(config.noUsb ? ["-DRA_BAREMETAL_NO_USB=1"] : []),
  "-U__unix__",
  "-U__linux__",
];

const commonCompileFlags = [
  "-mcpu=cortex-a76",
  "-mlittle-endian",
  "-Wall",
  "-fsigned-char",
  "-g",
  config.optimize,
  "-ffreestanding",
  "-mstrict-align",
  ...defines,
  ...includeFlags,
];

const circleLibBase = path.join(config.circleHome, "lib");
const circleCoreSources = existingSources(circleLibBase, [
  "actled.cpp", "alloc.cpp", "assert.cpp", "display.cpp", "windowdisplay.cpp",
  "bcmframebuffer.cpp", "bcmmailbox.cpp", "bcmpropertytags.cpp", "bcmwatchdog.cpp",
  "chargenerator.cpp", "classallocator.cpp", "cputhrottle.cpp", "debug.cpp",
  "delayloop.S", "device.cpp", "devicenameservice.cpp", "dmachannel.cpp",
  "koptions.cpp", "logger.cpp", "machineinfo.cpp", "multicore.cpp",
  "nulldevice.cpp", "ptrarray.cpp", "ptrlist.cpp", "qemu.cpp", "terminal.cpp",
  "screen.cpp", "serial.cpp", "spinlock.cpp", "string.cpp", "sysinit.cpp",
  "time.cpp", "timer.cpp", "tracer.cpp", "util.cpp", "util_fast.S",
  "virtualgpiopin.cpp", "chainboot.cpp", "macaddress.cpp", "netdevice.cpp",
  "new.cpp", "heapallocator.cpp", "pageallocator.cpp", "setjmp.S",
  "numberpool.cpp", "writebuffer.cpp", "2dgraphics.cpp", "ptrlistfiq.cpp",
  "font6x7.cpp", "font8x8.cpp", "font8x10.cpp", "font8x12.cpp",
  "font8x14.cpp", "font8x16.cpp", "font12x22.cpp",
  "exceptionhandler64.cpp", "exceptionstub64.S", "memory64.cpp", "startup64.S",
  "synchronize64.cpp", "translationtable64.cpp",
  "bcmpciehostbridge.cpp", "bcmrandom200.cpp", "interruptgic.cpp",
  "dma4channel.cpp", "devicetreeblob.cpp", "southbridge.cpp",
  "dmachannel-rp1.cpp", "gpiomanager2712.cpp", "gpiopin2712.cpp",
  "gpioclock-rp1.cpp", "pwmoutput-rp1.cpp", "i2cmaster-rp1.cpp",
  "spimaster-rp1.cpp", "spimasterdma-rp1.cpp", "macb.cpp",
  "purecall.cpp", "cxa_guard.cpp",
]);

const usbSources = existingSources(path.join(circleLibBase, "usb"), [
  "lan7800.cpp", "smsc951x.cpp", "usbbluetooth.cpp", "usbcdcethernet.cpp",
  "usbfloppydevice.cpp", "usbconfigparser.cpp", "usbdevice.cpp",
  "usbdevicefactory.cpp", "usbendpoint.cpp", "usbfunction.cpp",
  "usbgamepad.cpp", "usbgamepadps3.cpp", "usbgamepadps4.cpp",
  "usbgamepadstandard.cpp", "usbgamepadswitchpro.cpp", "usbgamepadxbox360.cpp",
  "usbgamepadxboxone.cpp", "usbhiddevice.cpp", "usbhostcontroller.cpp",
  "usbkeyboard.cpp", "usbmassdevice.cpp", "usbmidi.cpp", "usbmidihost.cpp",
  "usbmouse.cpp", "usbprinter.cpp", "usbrequest.cpp", "usbstandardhub.cpp",
  "usbstring.cpp", "usbserial.cpp", "usbserialhost.cpp", "usbserialch341.cpp",
  "usbserialcp210x.cpp", "usbserialpl2303.cpp", "usbserialft231x.cpp",
  "usbserialcdc.cpp", "usbtouchscreen.cpp", "dwhciregister.cpp",
  "xhcicommandmanager.cpp", "xhcidevice.cpp", "xhciendpoint.cpp",
  "xhcieventmanager.cpp", "xhcimmiospace.cpp", "xhciring.cpp",
  "xhciroothub.cpp", "xhcirootport.cpp", "xhcisharedmemallocator.cpp",
  "xhcislotmanager.cpp", "xhciusbdevice.cpp", "usbaudiocontrol.cpp",
  "usbaudiostreaming.cpp", "usbaudiofunctopology.cpp", "usbsubsystem.cpp",
]);

const soundSources = existingSources(path.join(circleLibBase, "sound"), [
  "soundbasedevice.cpp", "pwmsounddevice.cpp", "hdmisoundbasedevice.cpp",
  "pcm512xsoundcontroller.cpp", "wm8960soundcontroller.cpp",
  "i2ssoundbasedevice-rp1.cpp", "pwmsoundbasedevice-rp1.cpp",
  "usbsoundbasedevice.cpp", "usbsoundcontroller.cpp",
]);

const fsSources = existingSources(path.join(circleLibBase, "fs"), [
  "partition.cpp", "partitionmanager.cpp",
]);

const fatSources = existingSources(path.join(circleLibBase, "fs", "fat"), [
  "fatfs.cpp", "fatcache.cpp", "fatinfo.cpp", "fat.cpp", "fatdir.cpp",
]);

const inputSources = existingSources(path.join(circleLibBase, "input"), [
  "keyboardbehaviour.cpp", "keymap.cpp", "mousebehaviour.cpp", "mouse.cpp",
  "touchscreen.cpp", "rpitouchscreen.cpp", "xpt2046touchscreen.cpp",
  "console.cpp", "keyboardbuffer.cpp", "linediscipline.cpp",
]);

const sdCardSources = existingSources(path.join(config.circleHome, "addon", "SDCard"), [
  "emmc.cpp",
]);

const appBaseSourcesNoInput = [
  "kernel/main.cpp",
  "kernel/kernel.cpp",
  "kernel/memory.cpp",
  "kernel/rom_browser.cpp",
  "libretro/libretro_runner.cpp",
  "platform/circle/circle_log.cpp",
  "platform/circle/circle_timer.cpp",
  "platform/circle/circle_video.cpp",
  "platform/circle/circle_audio.cpp",
  "platform/circle/circle_parallel.cpp",
  "platform/circle/circle_fs.cpp",
];

function appBaseSources() {
  return config.noUsb
    ? appBaseSourcesNoInput
    : [...appBaseSourcesNoInput, "platform/circle/circle_input.cpp"];
}

function patternCoreSources() {
  return existingSources(projectRoot, [
    ...appBaseSources(),
    "libretro/builtin_pattern_core.cpp",
  ]);
}

function bootTestSources() {
  return existingSources(projectRoot, [
    "kernel/boottest_main.cpp",
  ]);
}

function bootTestFceummSources() {
  return existingSources(projectRoot, [
    "kernel/boottest_fceumm_main.cpp",
  ]);
}

function fceummSources() {
  const root = path.join(projectRoot, "cores", "libretro-fceumm");
  const core = path.join(root, "src");
  const common = path.join(core, "drivers", "libretro", "libretro-common");

  ensureFile(path.join(root, "Makefile.common"));

  return [
    ...listSources(path.join(core, "boards")),
    ...listSources(path.join(core, "input")),
    ...existingSources(core, [
      "drivers/libretro/libretro.c",
      "drivers/libretro/libretro_dipswitch.c",
      "cart.c",
      "cheat.c",
      "crc32.c",
      "fceu-endian.c",
      "fceu-memory.c",
      "fceu.c",
      "fds.c",
      "fds_apu.c",
      "file.c",
      "filter.c",
      "general.c",
      "input.c",
      "md5.c",
      "nsf.c",
      "palette.c",
      "ppu.c",
      "sound.c",
      "state.c",
      "video.c",
      "vsuni.c",
      "ines.c",
      "unif.c",
      "x6502.c",
    ]),
    path.join(common, "compat", "compat_strl.c"),
    path.join(common, "streams", "memory_stream.c"),
    path.join(projectRoot, "cores", "fceumm_baremetal_compat.c"),
  ];
}

function n64Sources() {
  const root = path.join(projectRoot, "cores", "mupen64plus-libretro-nx");
  const zlib = path.join(root, "custom", "dependencies", "libzlib");

  ensureFile(path.join(root, "Makefile.common"));

  return [
    ...existingSources(root, [
      "custom/mupen64plus-core/api/config.c",
      "mupen64plus-core/src/api/callbacks.c",
      "mupen64plus-core/src/api/debugger.c",
      "mupen64plus-core/src/api/frontend.c",
      "mupen64plus-core/src/backends/api/video_capture_backend.c",
      "mupen64plus-core/src/backends/clock_ctime_plus_delta.c",
      "mupen64plus-core/src/backends/dummy_video_capture.c",
      "mupen64plus-core/src/backends/file_storage.c",
      "mupen64plus-core/src/backends/plugins_compat/audio_plugin_compat.c",
      "mupen64plus-core/src/backends/plugins_compat/input_plugin_compat.c",
      "mupen64plus-core/src/device/cart/af_rtc.c",
      "mupen64plus-core/src/device/cart/cart.c",
      "mupen64plus-core/src/device/cart/cart_rom.c",
      "mupen64plus-core/src/device/cart/eeprom.c",
      "mupen64plus-core/src/device/cart/flashram.c",
      "mupen64plus-core/src/device/cart/is_viewer.c",
      "mupen64plus-core/src/device/cart/sram.c",
      "mupen64plus-core/src/device/controllers/game_controller.c",
      "mupen64plus-core/src/device/controllers/paks/biopak.c",
      "mupen64plus-core/src/device/controllers/paks/mempak.c",
      "mupen64plus-core/src/device/controllers/paks/rumblepak.c",
      "mupen64plus-core/src/device/controllers/paks/transferpak.c",
      "mupen64plus-core/src/device/controllers/vru_controller.c",
      "mupen64plus-core/src/device/dd/dd_controller.c",
      "mupen64plus-core/src/device/dd/disk.c",
      "mupen64plus-core/src/device/device.c",
      "mupen64plus-core/src/device/gb/gb_cart.c",
      "mupen64plus-core/src/device/gb/mbc3_rtc.c",
      "mupen64plus-core/src/device/gb/m64282fp.c",
      "mupen64plus-core/src/device/memory/memory.c",
      "mupen64plus-core/src/device/pif/bootrom_hle.c",
      "mupen64plus-core/src/device/pif/cic.c",
      "mupen64plus-core/src/device/pif/n64_cic_nus_6105.c",
      "mupen64plus-core/src/device/pif/pif.c",
      "mupen64plus-core/src/device/r4300/cached_interp.c",
      "mupen64plus-core/src/device/r4300/cp0.c",
      "mupen64plus-core/src/device/r4300/cp1.c",
      "mupen64plus-core/src/device/r4300/cp2.c",
      "mupen64plus-core/src/device/r4300/idec.c",
      "mupen64plus-core/src/device/r4300/interrupt.c",
      "mupen64plus-core/src/device/r4300/new_dynarec/new_dynarec.c",
      "mupen64plus-core/src/device/r4300/new_dynarec/arm64/linkage_arm64.S",
      "mupen64plus-core/src/device/r4300/pure_interp.c",
      "mupen64plus-core/src/device/r4300/r4300_core.c",
      "mupen64plus-core/src/device/r4300/tlb.c",
      "mupen64plus-core/src/device/rcp/ai/ai_controller.c",
      "mupen64plus-core/src/device/rcp/mi/mi_controller.c",
      "mupen64plus-core/src/device/rcp/pi/pi_controller.c",
      "mupen64plus-core/src/device/rcp/rdp/fb.c",
      "mupen64plus-core/src/device/rcp/rdp/rdp_core.c",
      "mupen64plus-core/src/device/rcp/ri/ri_controller.c",
      "mupen64plus-core/src/device/rcp/rsp/rsp_core.c",
      "mupen64plus-core/src/device/rcp/si/si_controller.c",
      "mupen64plus-core/src/device/rcp/vi/vi_controller.c",
      "mupen64plus-core/src/device/rdram/rdram.c",
      "mupen64plus-core/src/main/cheat.c",
      "mupen64plus-core/src/main/main.c",
      "mupen64plus-core/src/main/rom.c",
      "mupen64plus-core/src/main/savestates.c",
      "mupen64plus-core/src/main/util.c",
      "mupen64plus-core/src/plugin/dummy_audio.c",
      "mupen64plus-core/src/plugin/dummy_input.c",
      "mupen64plus-core/src/plugin/plugin.c",
      "mupen64plus-core/subprojects/md5/md5.c",
      "mupen64plus-core/subprojects/minizip/ioapi.c",
      "mupen64plus-core/subprojects/minizip/unzip.c",
      "mupen64plus-core/subprojects/minizip/zip.c",
      "mupen64plus-rsp-cxd4/rsp.c",
      "mupen64plus-rsp-hle/src/alist.c",
      "mupen64plus-rsp-hle/src/alist_audio.c",
      "mupen64plus-rsp-hle/src/alist_naudio.c",
      "mupen64plus-rsp-hle/src/alist_nead.c",
      "mupen64plus-rsp-hle/src/audio.c",
      "mupen64plus-rsp-hle/src/cicx105.c",
      "mupen64plus-rsp-hle/src/hle.c",
      "mupen64plus-rsp-hle/src/hvqm.c",
      "mupen64plus-rsp-hle/src/jpeg.c",
      "mupen64plus-rsp-hle/src/memory.c",
      "mupen64plus-rsp-hle/src/mp3.c",
      "mupen64plus-rsp-hle/src/musyx.c",
      "mupen64plus-rsp-hle/src/plugin.c",
      "mupen64plus-rsp-hle/src/re2.c",
      "mupen64plus-video-angrylion/interface.c",
      "mupen64plus-video-angrylion/n64video.c",
      "libretro/libretro.c",
      "libretro-common/audio/conversion/float_to_s16.c",
      "libretro-common/audio/conversion/s16_to_float.c",
      "libretro-common/audio/resampler/audio_resampler.c",
      "libretro-common/audio/resampler/drivers/nearest_resampler.c",
      "libretro-common/audio/resampler/drivers/sinc_resampler.c",
      "libretro-common/compat/compat_posix_string.c",
      "libretro-common/compat/compat_strcasestr.c",
      "libretro-common/compat/compat_strl.c",
      "libretro-common/libco/libco.c",
      "libretro-common/memmap/memalign.c",
      "libretro-common/string/stdstring.c",
      "custom/mupen64plus-core/plugin/audio_libretro/audio_backend_libretro.c",
      "custom/mupen64plus-core/plugin/emulate_game_controller_via_libretro.c",
    ]),
    ...listSources(zlib),
    ...existingSources(projectRoot, [
      "cores/n64_baremetal_gl_stubs.c",
      "cores/n64_baremetal_libretro_stubs.c",
      "cores/n64_baremetal_osal_stubs.c",
    ]),
  ];
}

const n64SpeedFlags = [
  "-Ofast",
  "-DNDEBUG",
  "-fomit-frame-pointer",
  "-fno-unwind-tables",
  "-fno-asynchronous-unwind-tables",
  "-fno-stack-protector",
];

function buildCoreArchive() {
  if (config.core === "pattern") {
    return { appSources: patternCoreSources(), archives: [], extraLibs: [] };
  }

  if (config.core === "boottest") {
    return { appSources: bootTestSources(), archives: [], extraLibs: [] };
  }

  if (config.core === "boottest-fceumm") {
    const root = path.join(projectRoot, "cores", "libretro-fceumm");
    const core = path.join(root, "src");
    const common = path.join(core, "drivers", "libretro", "libretro-common");
    const includeFirst = [
      "-I", path.join(core, "drivers", "libretro"),
      "-I", path.join(common, "include"),
      "-I", core,
      "-I", path.join(core, "input"),
      "-I", path.join(core, "boards"),
    ];
    const flags = [
      "-D__LIBRETRO__",
      "-DSTATIC_LINKING",
      "-DFRONTEND_SUPPORTS_RGB565",
      "-DHAVE_NO_LANGEXTRA",
      "-DPATH_MAX=1024",
      "-DFCEU_VERSION_NUMERIC=9813",
      "-DGIT_VERSION=\" baremetal\"",
      "-DPRId64=\"ld\"",
      "-DPRIu64=\"lu\"",
      "-DPRIuPTR=\"lu\"",
      "-ffunction-sections",
      "-fdata-sections",
      "-Wno-unused-function",
      "-Wno-unused-variable",
      "-Wno-missing-braces",
      "-Wno-implicit-fallthrough",
    ];

    const archiveFile = archive("libfceumm.a", fceummSources(), flags, { prependFlags: includeFirst });
    return { appSources: bootTestFceummSources(), archives: [archiveFile], extraLibs: [] };
  }

  if (config.core === "multi") {
    const fceummRoot = path.join(projectRoot, "cores", "libretro-fceumm");
    const fceummCore = path.join(fceummRoot, "src");
    const fceummCommon = path.join(fceummCore, "drivers", "libretro", "libretro-common");
    const fceummIncludeFirst = [
      "-I", path.join(fceummCore, "drivers", "libretro"),
      "-I", path.join(fceummCommon, "include"),
      "-I", fceummCore,
      "-I", path.join(fceummCore, "input"),
      "-I", path.join(fceummCore, "boards"),
    ];
    const fceummFlags = [
      ...libretroSymbolDefines("fceumm"),
      ...prefixedGlobalDefines("fceumm", [
        "environ_cb",
        ...multiCoreSharedSymbolDefines,
      ]),
      "-D__LIBRETRO__",
      "-DSTATIC_LINKING",
      "-DFRONTEND_SUPPORTS_RGB565",
      "-DHAVE_NO_LANGEXTRA",
      "-DPATH_MAX=1024",
      "-DFCEU_VERSION_NUMERIC=9813",
      "-DGIT_VERSION=\" baremetal\"",
      "-DPRId64=\"ld\"",
      "-DPRIu64=\"lu\"",
      "-DPRIuPTR=\"lu\"",
      "-ffunction-sections",
      "-fdata-sections",
      "-Wno-unused-function",
      "-Wno-unused-variable",
      "-Wno-missing-braces",
      "-Wno-implicit-fallthrough",
    ];

    const n64Root = path.join(projectRoot, "cores", "mupen64plus-libretro-nx");
    const n64IncludeFirst = [
      "-I", path.join(projectRoot, "cores", "n64_compat"),
      "-I", path.join(n64Root, "libretro"),
      "-I", path.join(n64Root, "custom"),
      "-I", path.join(n64Root, "custom", "mupen64plus-core"),
      "-I", path.join(n64Root, "custom", "mupen64plus-core", "api"),
      "-I", path.join(n64Root, "custom", "mupen64plus-core", "plugin", "audio_libretro"),
      "-I", path.join(n64Root, "custom", "android", "include"),
      "-I", path.join(n64Root, "custom", "GLideN64"),
      "-I", path.join(n64Root, "custom", "dependencies", "libzlib"),
      "-I", path.join(n64Root, "mupen64plus-core", "src"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "api"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "main"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "osal"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "plugin"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "asm_defines"),
      "-I", path.join(n64Root, "mupen64plus-core", "src", "device", "r4300", "new_dynarec", "arm64"),
      "-I", path.join(n64Root, "mupen64plus-core", "subprojects", "md5"),
      "-I", path.join(n64Root, "mupen64plus-core", "subprojects", "minizip"),
      "-I", path.join(n64Root, "mupen64plus-rsp-cxd4"),
      "-I", path.join(n64Root, "mupen64plus-video-angrylion"),
      "-I", path.join(n64Root, "mupen64plus-video-angrylion", "n64video"),
      "-I", path.join(n64Root, "libretro-common", "include"),
      "-I", path.join(n64Root, "GLideN64", "src", "inc"),
      "-I", path.join(n64Root, "GLideN64", "src", "osal"),
      "-I", path.join(n64Root, "xxHash"),
    ];
    const n64Flags = [
      ...libretroSymbolDefines("n64"),
      ...prefixedGlobalDefines("n64", [
        "audio_batch_cb",
        "environ_cb",
        "environ_clear_thread_waits_cb",
        "input_cb",
        "log_cb",
        "perf_cb",
        "poll_cb",
        "retro_screen_aspect",
        "retro_screen_height",
        "retro_screen_width",
        "rumble",
        "video_cb",
        ...multiCoreSharedSymbolDefines,
      ]),
      "-std=gnu11",
      "-D__LIBRETRO__",
      "-DSTATIC_LINKING",
      "-DM64P_PLUGIN_API",
      "-DM64P_CORE_PROTOTYPES",
      "-DDYNAREC",
      "-DNEW_DYNAREC=4",
      "-D_ENDUSER_RELEASE",
      "-D__STDC_CONSTANT_MACROS",
      "-D__STDC_LIMIT_MACROS",
      "-D__STDC_FORMAT_MACROS",
      "-DUSE_FILE32API",
      "-DSINC_LOWER_QUALITY",
      "-DTXFILTER_LIB",
      "-D__VEC4_OPT",
      "-DMUPENPLUSAPI",
      "-DHAVE_THR_AL",
      "-DHAVE_LLE",
      "-DCORE_NAME=\"mupen64plus\"",
      "-DGIT_VERSION=\" baremetal\"",
      "-DPATH_MAX=1024",
      "-DPRIX64=\"lX\"",
      "-DPRIX32=\"X\"",
      "-DPRIX16=\"X\"",
      "-DPRIX8=\"X\"",
      "-DPRIxPTR=\"lx\"",
      "-DPRIuPTR=\"lu\"",
      "-DPRIu64=\"lu\"",
      "-DPRId64=\"ld\"",
      "-D_CRT_SECURE_NO_WARNINGS",
      ...n64SpeedFlags,
      "-Wno-unused-function",
      "-Wno-unused-variable",
      "-Wno-missing-braces",
      "-Wno-implicit-fallthrough",
      "-Wno-discarded-qualifiers",
      "-Wno-unknown-pragmas",
      "-ffunction-sections",
      "-fdata-sections",
    ];

    const fceummArchive = archive("libfceumm_multi.a", fceummSources(), fceummFlags, { prependFlags: fceummIncludeFirst });
    const n64Archive = archive("libmupen64plus_next_multi.a", n64Sources(), n64Flags, { prependFlags: n64IncludeFirst });
    return {
      appSources: existingSources(projectRoot, appBaseSources()),
      archives: [fceummArchive, n64Archive],
      extraLibs: ["libc"],
    };
  }

  if (config.core === "n64") {
    const root = path.join(projectRoot, "cores", "mupen64plus-libretro-nx");
    const includeFirst = [
      "-I", path.join(projectRoot, "cores", "n64_compat"),
      "-I", path.join(root, "libretro"),
      "-I", path.join(root, "custom"),
      "-I", path.join(root, "custom", "mupen64plus-core"),
      "-I", path.join(root, "custom", "mupen64plus-core", "api"),
      "-I", path.join(root, "custom", "mupen64plus-core", "plugin", "audio_libretro"),
      "-I", path.join(root, "custom", "android", "include"),
      "-I", path.join(root, "custom", "GLideN64"),
      "-I", path.join(root, "custom", "dependencies", "libzlib"),
      "-I", path.join(root, "mupen64plus-core", "src"),
      "-I", path.join(root, "mupen64plus-core", "src", "api"),
      "-I", path.join(root, "mupen64plus-core", "src", "main"),
      "-I", path.join(root, "mupen64plus-core", "src", "osal"),
      "-I", path.join(root, "mupen64plus-core", "src", "plugin"),
      "-I", path.join(root, "mupen64plus-core", "src", "asm_defines"),
      "-I", path.join(root, "mupen64plus-core", "src", "device", "r4300", "new_dynarec", "arm64"),
      "-I", path.join(root, "mupen64plus-core", "subprojects", "md5"),
      "-I", path.join(root, "mupen64plus-core", "subprojects", "minizip"),
      "-I", path.join(root, "mupen64plus-rsp-cxd4"),
      "-I", path.join(root, "mupen64plus-video-angrylion"),
      "-I", path.join(root, "mupen64plus-video-angrylion", "n64video"),
      "-I", path.join(root, "libretro-common", "include"),
      "-I", path.join(root, "GLideN64", "src", "inc"),
      "-I", path.join(root, "GLideN64", "src", "osal"),
      "-I", path.join(root, "xxHash"),
    ];
    const flags = [
      "-std=gnu11",
      "-D__LIBRETRO__",
      "-DSTATIC_LINKING",
      "-DM64P_PLUGIN_API",
      "-DM64P_CORE_PROTOTYPES",
      "-DDYNAREC",
      "-DNEW_DYNAREC=4",
      "-D_ENDUSER_RELEASE",
      "-D__STDC_CONSTANT_MACROS",
      "-D__STDC_LIMIT_MACROS",
      "-D__STDC_FORMAT_MACROS",
      "-DUSE_FILE32API",
      "-DSINC_LOWER_QUALITY",
      "-DTXFILTER_LIB",
      "-D__VEC4_OPT",
      "-DMUPENPLUSAPI",
      "-DHAVE_THR_AL",
      "-DHAVE_LLE",
      "-DCORE_NAME=\"mupen64plus\"",
      "-DGIT_VERSION=\" baremetal\"",
      "-DPATH_MAX=1024",
      "-DPRIX64=\"lX\"",
      "-DPRIX32=\"X\"",
      "-DPRIX16=\"X\"",
      "-DPRIX8=\"X\"",
      "-DPRIxPTR=\"lx\"",
      "-DPRIuPTR=\"lu\"",
      "-DPRIu64=\"lu\"",
      "-DPRId64=\"ld\"",
      "-D_CRT_SECURE_NO_WARNINGS",
      ...n64SpeedFlags,
      "-Wno-unused-function",
      "-Wno-unused-variable",
      "-Wno-missing-braces",
      "-Wno-implicit-fallthrough",
      "-Wno-discarded-qualifiers",
      "-Wno-unknown-pragmas",
      "-ffunction-sections",
      "-fdata-sections",
    ];

    const archiveFile = archive("libmupen64plus_next_baremetal.a", n64Sources(), flags, { prependFlags: includeFirst });
    return {
      appSources: existingSources(projectRoot, appBaseSources()),
      archives: [archiveFile],
      extraLibs: ["libc"],
    };
  }

  if (config.core !== "fceumm") {
    fail(`Unknown core '${config.core}'. Use 'pattern', 'boottest', 'boottest-fceumm', 'fceumm', 'n64' or 'multi'.`);
  }

  const root = path.join(projectRoot, "cores", "libretro-fceumm");
  const core = path.join(root, "src");
  const common = path.join(core, "drivers", "libretro", "libretro-common");
  const includeFirst = [
    "-I", path.join(core, "drivers", "libretro"),
    "-I", path.join(common, "include"),
    "-I", core,
    "-I", path.join(core, "input"),
    "-I", path.join(core, "boards"),
  ];
  const flags = [
    "-D__LIBRETRO__",
    "-DSTATIC_LINKING",
    "-DFRONTEND_SUPPORTS_RGB565",
    "-DHAVE_NO_LANGEXTRA",
    "-DPATH_MAX=1024",
    "-DFCEU_VERSION_NUMERIC=9813",
    "-DGIT_VERSION=\" baremetal\"",
    "-DPRId64=\"ld\"",
    "-DPRIu64=\"lu\"",
    "-DPRIuPTR=\"lu\"",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wno-unused-function",
    "-Wno-unused-variable",
    "-Wno-missing-braces",
    "-Wno-implicit-fallthrough",
  ];

  const archiveFile = archive("libfceumm.a", fceummSources(), flags, { prependFlags: includeFirst });
  return {
    appSources: existingSources(projectRoot, appBaseSources()),
    archives: [archiveFile],
    extraLibs: [],
  };
}

function main() {
  for (const file of Object.values(tools)) ensureFile(file);
  ensureFile(path.join(config.circleHome, "circle.ld"));
  ensureDir(config.buildDir);

  console.log(`Node build for Raspberry Pi 5 Circle image`);
  console.log(`Circle:   ${config.circleHome}`);
  console.log(`Toolchain:${config.toolchain}`);
  console.log(`Core:     ${config.core}`);
  console.log(`ROM:      ${config.romPath}`);
  console.log(`USB:      ${config.noUsb ? "disabled" : "enabled"}`);
  console.log(`Kernel max size: ${config.kernelMaxSize}`);
  if (config.core === "n64" || config.core === "multi") {
    console.log(`N64 skip: ${config.n64FrameSkip} frames; present divisor ${config.n64ViDivisor}`);
  }
  console.log(`Out:      ${config.buildDir}`);

  const coreBuild = buildCoreArchive();
  const libcircle = archive("libcircle.a", circleCoreSources, ["-DNO_SANITIZE=1"]);
  const libusb = archive("libusb.a", usbSources);
  const libsound = archive("libsound.a", soundSources);
  const libfs = archive("libfs.a", fsSources);
  const libfatfs = archive("libfatfs.a", fatSources);
  const libinput = archive("libinput.a", inputSources);
  const libsdcard = archive("libsdcard.a", sdCardSources);
  const appObjects = coreBuild.appSources.map((source) => compile(source));
  const { libgcc, libm, libc, libnosys } = parseLibs();
  const coreArchives = coreBuild.archives;
  const extraLibs = coreBuild.extraLibs.includes("libc") ? [libc, libnosys] : [];

  const ldHelp = spawnSync(tools.ld, ["--help"], { encoding: "utf8" });
  const noWarnRwx = `${ldHelp.stdout || ""}${ldHelp.stderr || ""}`.includes("no-warn-rwx-segments")
    ? ["--no-warn-rwx-segments"]
    : [];
  const gcSections = config.core === "fceumm" || config.core === "n64" || config.core === "multi" || config.core === "boottest" || config.core === "boottest-fceumm" ? ["--gc-sections"] : [];

  const elf = path.join(config.buildDir, `${config.target}.elf`);
  const map = path.join(config.buildDir, `${config.target}.map`);
  const img = path.join(config.buildDir, `${config.target}.img`);
  const lst = path.join(config.buildDir, `${config.target}.lst`);
  const allLinkInputs = [
    ...appObjects,
    ...coreArchives,
    libsdcard, libsound, libusb, libinput, libfatfs, libfs, libcircle, libgcc, libm, ...extraLibs,
    path.join(config.circleHome, "circle.ld"),
  ];

  if (newer(elf, allLinkInputs)) {
    run("LD", tools.ld, [
      "-o", elf,
      "-Map", map,
      "--section-start=.init=0x80000",
      ...noWarnRwx,
      ...(config.core === "n64" || config.core === "multi" ? ["--allow-multiple-definition"] : []),
      ...gcSections,
      "-T", path.join(config.circleHome, "circle.ld"),
      ...appObjects,
      "--start-group",
      ...coreArchives,
      libsdcard, libsound, libusb, libinput, libfatfs, libfs, libcircle, libgcc, libm, ...extraLibs,
      "--end-group",
    ], { display: relFromRoot(elf) });
  }

  if (newer(img, [elf])) {
    run("COPY", tools.objcopy, [elf, "-O", "binary", img], { display: relFromRoot(img) });
  }

  const dump = spawnSync(tools.objdump, ["-d", elf], { encoding: "utf8", maxBuffer: 256 * 1024 * 1024 });
  if (dump.status === 0) {
    const filt = spawnSync(tools.cxxfilt, [], { input: dump.stdout, encoding: "utf8", maxBuffer: 256 * 1024 * 1024 });
    if (filt.status === 0) {
      fs.writeFileSync(lst, filt.stdout);
      console.log(`DUMP    ${relFromRoot(lst)}`);
    }
  }

  const size = fs.statSync(img).size;
  console.log(`\nBuilt ${img}`);
  console.log(`Size: ${size.toLocaleString("en-US")} bytes`);
  console.log(`Copy it to the Pi 5 boot partition as kernel_2712.img.`);
}

main();
