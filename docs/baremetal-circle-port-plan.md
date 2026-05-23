# RetroArch Bare-Metal Circle Port Plan

Stand: 2026-05-23. Ziel: Raspberry Pi 5, AArch64, Circle, bootbares
Circle-Kernel-Image. Hinweis: Circle nennt das Pi-5-Image offiziell
`kernel_2712.img`. Ein `kernel8.img` ist nur als bewusstes Umbenennen mit
passendem `config.txt` sinnvoll.

## 1. Kurzfazit zur Machbarkeit

RetroArch ist als Bare-Metal-Ziel realistischer als ein schwerer Emulator wie
RPCS3, weil RetroArch selbst "nur" ein Frontend und eine Runtime fuer
libretro-Cores ist. Es emuliert nicht zwingend komplexe Systeme, sondern
koordiniert Core-Lifecycle, Video, Audio, Input, Dateien, Config und Timing.

Grundsaetzlich portierbar sind der libretro-Callback-Vertrag, ein sehr kleiner
Teil des Main Loops, statisch gelinkte Cores, einfache Saves, einfache
Software-Framebuffer-Ausgabe, USB-Gamepad-Input und ein minimales
Dateisystem-Modell.

Die Hauptblocker sind POSIX-/Desktop-Abhaengigkeiten, dynamisches Laden von
Cores, pthreads, Filesystem-Konventionen, Shader/GPU-Stacks, Audio-Latenz,
USB/Bluetooth-Controllerbreite, Debugging und die Tatsache, dass viele Cores
selbst OS-nahe Annahmen haben.

Erste sinnvolle Cores: libretro-Testcores, 2048, CHIP-8, kleine Game-Boy-Cores.
Danach Gambatte, FCEUmm, SNES9x 2002/2005, Genesis Plus GX oder PicoDrive.
PlayStation, N64, PSP und Dreamcast gehoeren spaet in die Roadmap.

## 2. Architekturvergleich

| Bereich | Klassisches RetroArch | Bare-Metal-Circle-Ziel |
| --- | --- | --- |
| OS | POSIX/Win32/UWP/Konsolen-SDKs | Circle-Objekte, kein Userspace |
| Event Loop | RetroArch-Runloop plus OS-Events | CKernel::Run() ruft retro_run() |
| Threading | pthreads/Win32/SDK Threads | zunaechst single core, spaeter Circle-Scheduler |
| Datei-I/O | stdio, VFS, Pfade, Archive | FAT auf SD, anfangs Root/kurze Pfade |
| Config | retroarch.cfg, Overrides | compile-time Defaults, spaeter INI/TOML-lite |
| Dynamic Loading | dlopen/LoadLibrary | zunaechst aus, Cores statisch linken |
| Grafik | GL/Vulkan/D3D/SDL/fbdev/DRM | CScreenDevice/Framebuffer, RGB565 |
| Audio | ALSA/Pulse/SDL/CoreAudio/etc. | HDMI/PWM/I2S/USB ueber Circle-Sound |
| Input | SDL/udev/XInput/HID/etc. | Circle USB HID Gamepad, Keyboard optional |
| Timer | OS monotonic clock, vsync | CTimer/GetClockTicks64, eigene Frame-Pacing-Logik |
| Saves | Host-Filesystem | SD-FAT-Dateien, SRAM erst nach Core-Memory-API |
| Menues | Ozone/XMB/RGUI/MaterialUI | zuerst kein Menue, spaeter einfacher Picker |
| Shader | GLSL/Slang/CG/HLSL | gestrichen, spaeter CPU-Scaler oder simple LUT |
| Netzwerk | Sockets, HTTP, Netplay | zuerst aus, Circle-Netzwerk spaeter separat |
| Achievements | HTTP + Runtime | aus |

## 3. Harte technische Probleme

RetroArch erwartet normalerweise eine Plattformschicht mit OS-Services:
Dateien, dynamische Bibliotheken, Zeit, Threads, Synchronisation, Fenster- oder
Framebuffer-Kontext, Audio-Device, Input-Events und Logging. Bare Metal hat
diese Schicht nicht.

Auf dem Ziel gibt es keine SDL2-, ALSA-, PulseAudio-, OpenGL-, Vulkan-, X11-,
Wayland- oder POSIX-Umgebung. Circle bietet stattdessen direkte Geraete- und
Subsystemklassen. Das ist gut fuer deterministischen Zugriff, aber nicht
kompatibel mit RetroArchs Desktop-Annahmen.

Dynamisches Laden ist besonders schwierig. Ein libretro-Core als ELF/.so
braeuchte Relocation, Symbolauflosung, Speicherbereiche, Cache-Management und
Fehlerisolation. Fuer ein MVP ist statisches Linken der klare Weg.

Viele Cores bringen eigene Annahmen mit: `mmap`, `fopen`, `gettimeofday`,
Threads, atomics, JIT/Dynarec, SIMD-Flags, Large-File-I/O oder Host-Endianness.
Jeder Core muss einzeln geprueft werden.

Speicherverwaltung muss hart begrenzt werden. Ein Core kann auf Desktop-Systemen
viel Heap erwarten. Ohne OS gibt es keine Uebercommit- oder Prozessgrenze.
SD-Karten-I/O muss robust und sparsam sein, besonders bei Savegames.

USB-Controller funktionieren fuer bekannte HID-Gamepads ueber Circle gut, aber
Bluetooth ist laut aktuellem Circle-Support kein brauchbares erstes Ziel.
Video geht zunaechst ueber Firmware-/Framebuffer-Pfade; Pi 5 hat dort
Einschraenkungen. Audio ueber HDMI ist realistisch, PWM/I2S/USB sind weitere
Backends. Frame-Pacing muss mit CTimer statt OS-vsync geloest werden. Logging
geht ueber UART/Screen und darf die Framezeit nicht ruinieren.

## 4. Portierungsstrategie

Phase 0: RetroArch-Quellen analysieren. Gefunden: Plattformtreiber unter
`src/frontend/drivers`, Video unter `src/gfx`, Audio unter `src/audio`, Input
unter `src/input`, dynamische Core-Schnittstelle in `src/dynamic.h`.

Phase 1: minimale Circle-Bootumgebung. In diesem Repo wurde unter
`baremetal/` ein Circle-App-Scaffold angelegt.

Phase 2: UART-/Framebuffer-Logging. `CKernel` initialisiert `CSerialDevice`,
`CScreenDevice`, `CLogger`.

Phase 3: Runtime/Speicher. Zunaechst Circle-Standardruntime und klare
Maximalgroessen fuer ROM-Buffer.

Phase 4: Plattform-Layer. Angelegt unter `baremetal/platform/circle`.

Phase 5: Dynamic Core Loading deaktivieren. Der Runner erwartet statische
`retro_*`-Symbole.

Phase 6: Einzelnen Core statisch linken. Angelegt ist ein eigener
`builtin_pattern_core.cpp` als No-ROM-Testcore.

Phase 7: Null-Video/Audio/Input. Der Runner toleriert fehlende Audioausgabe und
fehlende Gamepads.

Phase 8: Framebuffer-Video. `circle_video.cpp` kopiert libretro-Pixel nach
`CScreenDevice::SetPixel()`.

Phase 9: SD-Dateizugriff. `circle_fs.cpp` nutzt `CFATFileSystem` auf `emmc1-1`.

Phase 10: USB-Gamepad. `circle_input.cpp` bindet `CUSBGamePadDevice` an
RetroPad-IDs.

Phase 11: Audio. `circle_audio.cpp` schreibt signed-16-stereo an
`CSoundBaseDevice`, Kernel nutzt HDMI als erstes Backend.

Phase 12: Einfaches Menue. Noch nicht umgesetzt; Empfehlung: Root-Dateiliste
und feste Core-Auswahl statt RetroArch-Ozone/XMB.

Phase 13: Weitere Cores. Jeden Core als statische Library mit eigener
Port-Konfiguration ziehen.

Phase 14: Optimierung. Direkter Framebuffer-Zugriff, Audio-Ringbuffer,
Frame-Pacing, SRAM-Flush-Policy, Crash-Logging.

## 5. Konkrete technische Umbauten

Zu ersetzen oder zu reduzieren:

- OS-Abstraktion: neue Circle-Frontend-Schicht statt Unix/Win32/UWP.
- Main Loop: `CKernel::Run()` statt `main(argc, argv)` plus OS-Loop.
- Video Driver: RGB565/XRGB1555/XRGB8888 nach Circle-Framebuffer.
- Audio Driver: signed 16-bit batch nach HDMI/PWM/I2S/USB.
- Input Driver: Circle-HID-Gamepad nach RetroPad.
- Filesystem Driver: CFATFileSystem/FatFs statt stdio/VFS.
- Timer: CTimer statt `clock_gettime`.
- Config: compile-time und sehr kleine SD-Config.
- Logging: CLogger/UART/Screen.
- Memory: feste Budgets, keine unkontrollierten Megabyte-Allokationen.
- Dynamic Loader: aus, statisches Linken.
- Threading: aus, spaeter Circle-Scheduler pro Core pruefen.
- Menu Driver: aus, spaeter minimaler Picker.
- Shader Pipeline: aus.
- Network Features: aus.
- Buildsystem: Circle `Rules.mk`, AArch64, RASPPI=5.
- Startup: Circle `main.cpp` + `CKernel`, Image `kernel_2712.img`.

## 6. Circle-spezifische Umsetzung

Die angelegte Struktur ist ein kleiner libretro-Runner:

- `kernel/main.cpp` startet Circle.
- `kernel/kernel.cpp` initialisiert UART, Screen, Interrupts, Timer, USB, SD und
  HDMI-Audio.
- `platform/circle/circle_fs.cpp` mountet FAT und liest optional eine ROM.
- `libretro/libretro_runner.cpp` setzt alle libretro-Callbacks.
- `retro_init()` und `retro_load_game()` werden statisch aus dem gelinkten Core
  aufgerufen.
- `retro_run()` laeuft in der Hauptschleife.
- Video-, Audio- und Input-Callbacks leiten auf Circle-Backends weiter.

## 7. Sinnvolle erste libretro-Cores

| Core | Realismus | Kommentar |
| --- | --- | --- |
| Test-/Pattern-Core | sehr hoch | Kein ROM, prueft Boot/Video/Input/Audio |
| 2048 | sehr hoch | Kleine Logik, kaum OS-Abhaengigkeiten |
| CHIP-8 | hoch | Kleine ROMs, simple Video/Input |
| Gambatte/Game Boy | mittel | Gute zweite Stufe, aber Datei/SRAM sauber machen |
| FCEUmm/NES | mittel | Realistisch nach Audio/Timing-Stabilisierung |
| SNES9x 2002/2005 | mittel bis schwer | CPU-Last, Timing, Speicher |
| Genesis Plus GX | mittel bis schwer | Mehr Audio/Timing-Komplexitaet |
| PicoDrive | mittel | ARM-nahe Optimierungen moeglich |
| PS1/N64/PSP/Dreamcast | spaet | JIT, GPU, Threads, grosse Assets, Timing |

## 8. Beispielhafte Projektstruktur

```text
baremetal/
  Makefile
  README.md
  config.txt
  circle-config-rpi5.mk
  kernel/
    main.cpp
    kernel.h
    kernel.cpp
    memory.cpp
  libretro/
    libretro_runner.h
    libretro_runner.cpp
    builtin_pattern_core.cpp
  platform/circle/
    circle_platform.h
    circle_video.cpp
    circle_audio.cpp
    circle_input.cpp
    circle_fs.cpp
    circle_timer.cpp
    circle_log.cpp
```

## 9. Beispielcode

Der Beispielcode ist im Repo bewusst als konkrete Dateien abgelegt, nicht nur
als Snippet:

- Minimalistisches Circle-`kernel.cpp`: `baremetal/kernel/kernel.cpp`
- libretro-Callback-Initialisierung: `baremetal/libretro/libretro_runner.cpp`
- Statischer Core: `baremetal/libretro/builtin_pattern_core.cpp`
- Video-Callback nach Framebuffer: `baremetal/platform/circle/circle_video.cpp`
- Audio-Callback: `baremetal/platform/circle/circle_audio.cpp`
- Gamepad-Input: `baremetal/platform/circle/circle_input.cpp`
- Datei-I/O-Wrapper: `baremetal/platform/circle/circle_fs.cpp`
- Makefile: `baremetal/Makefile`
- Node.js-Build ohne Make: `baremetal/build.mjs`

## 10. Risiken und Alternativen

Ein kompletter RetroArch-Port ist sehr aufwendig, weil RetroArch ueber Jahre um
portable OS-Schichten, dynamische Cores, Menues, Shader, Netzwerk, Tasks und
unterschiedliche Audio-/Video-Kontexte gewachsen ist. Bare Metal nimmt fast
alles davon weg.

Ein minimalistischer libretro-Runner ist realistischer, weil er nur den
libretro-Vertrag erfuellt. Statisch gelinkte Cores sind einfacher, weil der
Linker alle Symbole und Relocations beim Build loest. Ein kleiner eigener
Bare-Metal-libretro-Prototyp liefert schneller echte Hardware-Ergebnisse und
kann spaeter gezielt RetroArch-Subsysteme zurueckholen.

Zunaechst streichen: Online-Updater, Achievements, Netplay, komplexe Menues,
Shader, Vulkan/OpenGL, dynamische Core-Auswahl und Multithreading.

## 11. Minimal realistisches Ziel

MVP:

- Raspberry Pi 5 bootet `kernel_2712.img` oder bewusst umbenanntes
  `kernel8.img`.
- Circle initialisiert UART, Framebuffer, USB, SD und Timer.
- Ein statisch gelinkter einfacher libretro-Core startet.
- Optional wird eine ROM/Homebrew-Datei aus der SD-Root gelesen.
- Video erscheint per Framebuffer.
- USB-Gamepad liefert RetroPad-Buttons.
- Einfache SRAM-Saves werden spaeter ueber `retro_get_memory_data()` auf SD
  geschrieben.
- Keine Menues, kein Netzwerk, keine Shader, kein dynamisches Core-Loading.

## 12. Ergebnis

Dieses Repo enthaelt jetzt die erste umsetzbare Basis: einen Circle-MVP-Runner
plus Portierungsplan. Der naechste harte Schritt ist kein weiterer Text, sondern
ein echter Hardware-Boot mit aktuellem Circle und danach das Ersetzen des
Pattern-Cores durch 2048 oder CHIP-8.
