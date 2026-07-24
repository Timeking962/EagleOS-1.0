# EagleOS 1.0

A retro graphical OS demo inspired by Windows 1.0. This workspace contains a minimal bootloader, protected-mode kernel, and a simple VGA graphics shell with window decorations.

## Goals
- Boot from a raw disk image
- Enter 320x200 VGA mode
- Draw windows, title bars, and a taskbar
- Run built-in programs through a small executable runtime format
- Keep the codebase small and easy to extend

## Project structure
- `boot/boot.asm` — 16-bit bootloader that initializes VGA mode and jumps to protected mode
- `kernel/kernel_entry.asm` — 32-bit entry stub for the C kernel
- `kernel/kernel.c` — OS desktop logic and main entry point
- `kernel/exec.c` + `include/exec.h` — executable runtime and program launching
- `tools/build_exec_bundle.py` — builds phase-2 executable bundle (`build/exec.bundle`)
- `kernel/program_manager.c` — program manager application
- `kernel/calculator.c` — calculator application
- `kernel/text_editor.c` — text editor application
- `kernel/render_backend.c` — VGA drawing primitives and render backend
- `kernel/desktop_interface_manager.c` — shared desktop/window/dialog chrome
- `include/render_backend.h` — low-level render backend API
- `tools/make_image.py` — builds a raw floppy image for QEMU
- `Makefile` — build and run rules

## Requirements
- `nasm`
- 32-bit cross compiler or GCC with `-m32` support (e.g. `i686-elf-gcc`, `gcc -m32`)
- `python`
- `qemu-system-i386`

## Build
```bash
make
```

## Run
```bash
make run
```

### Windows helper scripts
- `build\\run_qemu.bat` - headless run with serial logging
- `build\\run_qemu_interactive.bat` - opens a QEMU window for manual keyboard testing and logs serial output
- `build\\run_qemu_keytest.bat` - runs headless and injects `Up`, `Down`, `Enter`, `Escape` via QEMU monitor `sendkey`

### Input controls
- Keyboard:
	- `Up` / `Down` navigate the active program UI
	- `Enter` activates in-program actions
	- `Tab` returns to Program Manager inside apps
- Mouse:
	- Move mouse to move the software cursor
	- Left click interacts with active program UI controls
	- Right click returns to Program Manager in app windows

### Program model
- The kernel launches `PROGMAN` at boot.
- Built-in programs are registered with a small executable header (`EEXE` magic/version/name + callbacks).
- Program Manager launches Calculator and Text Editor through this executable registry.

### Disk-backed executable loader
- Build step generates `build/exec.bundle` and appends it to `build/eagleos.img`.
- Bundle format:
	- `ECAT` v2 catalog header (count, bundle size, blob base offset)
	- catalog entries (`program_id`, display name, blob offset/size, flags)
	- `EAPP` app blobs
- At boot, `exec_init()` scans loaded image memory for `ECAT` v2, registers catalog entries, and loads each app blob through a fixed allocator (`0x00200000`..`0x00300000`).
- `EAPP` blobs support image+bss allocation and relocation patching (32-bit absolute offsets table).
- Current programs still bridge to built-in callbacks; blob loading/relocation is now in place for native app-entry execution in the next step.

## Notes
This scaffold is intentionally minimal. You can expand it with:
- keyboard input
- window management
- simple file viewer UI
- mouse support
- palette and icon rendering

## Notes
- On Windows, install `nasm`, `gcc` with `-m32` support, and `qemu-system-i386`, or build inside WSL with these tools.
- If using a cross toolchain, set `CC=i686-elf-gcc` when running `make`.
