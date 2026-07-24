NASM ?= nasm
CC ?= gcc
CFLAGS ?= -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra
LDFLAGS ?= -Ttext 0x1000 -nostdlib -Wl,-Map,build/kernel.map,--oformat=binary
PYTHON ?= python
QEMU ?= qemu-system-i386
BUILD_DIR := build

SRCS := kernel/kernel.c kernel/render_backend.c kernel/desktop_interface_manager.c kernel/keyboard.c kernel/ui.c kernel/mouse.c kernel/disk.c kernel/fs.c kernel/system.c kernel/exec.c kernel/program_manager.c kernel/calculator.c kernel/text_editor.c kernel/file_manager.c kernel/installer.c kernel/sysver.c
OBJS := $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel_kernel.o $(BUILD_DIR)/kernel_render_backend.o $(BUILD_DIR)/kernel_desktop_interface_manager.o $(BUILD_DIR)/kernel_keyboard.o $(BUILD_DIR)/kernel_ui.o $(BUILD_DIR)/kernel_mouse.o $(BUILD_DIR)/kernel_disk.o $(BUILD_DIR)/kernel_fs.o $(BUILD_DIR)/kernel_system.o $(BUILD_DIR)/kernel_exec.o $(BUILD_DIR)/kernel_program_manager.o $(BUILD_DIR)/kernel_calculator.o $(BUILD_DIR)/kernel_text_editor.o $(BUILD_DIR)/kernel_file_manager.o $(BUILD_DIR)/kernel_installer.o $(BUILD_DIR)/kernel_sysver.o

.PHONY: all run run-hdd clean
all: $(BUILD_DIR)/eagleos.img $(BUILD_DIR)/eagleos-hdd.img

$(BUILD_DIR)/.build_tag: tools/bump_build_tag.py include/version.h | $(BUILD_DIR)
	$(PYTHON) tools/bump_build_tag.py
	touch $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.bin: boot/boot.asm | $(BUILD_DIR)
	$(NASM) -f bin boot/boot.asm -o $@

$(BUILD_DIR)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -f elf32 kernel/kernel_entry.asm -o $@

$(BUILD_DIR)/kernel_kernel.o: kernel/kernel.c $(BUILD_DIR)/.build_tag | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_render_backend.o: kernel/render_backend.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_desktop_interface_manager.o: kernel/desktop_interface_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_keyboard.o: kernel/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_ui.o: kernel/ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_mouse.o: kernel/mouse.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_disk.o: kernel/disk.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_fs.o: kernel/fs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_system.o: kernel/system.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_exec.o: kernel/exec.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_program_manager.o: kernel/program_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_calculator.o: kernel/calculator.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_text_editor.o: kernel/text_editor.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_file_manager.o: kernel/file_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_installer.o: kernel/installer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_sysver.o: kernel/sysver.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/calc_native.bin: apps/native/calc_native.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/exec.bundle: tools/build_exec_bundle.py $(BUILD_DIR)/calc_native.bin | $(BUILD_DIR)
	$(PYTHON) tools/build_exec_bundle.py $@ $(BUILD_DIR)/calc_native.bin

$(BUILD_DIR)/eagleos.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/exec.bundle tools/make_image.py | $(BUILD_DIR)
	$(PYTHON) tools/make_image.py $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin $@ $(BUILD_DIR)/exec.bundle

$(BUILD_DIR)/eagleos-hdd.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/exec.bundle tools/make_hdd_image.py | $(BUILD_DIR)
	$(PYTHON) tools/make_hdd_image.py $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin $@ $(BUILD_DIR)/exec.bundle 64

run: all
	$(QEMU) -drive format=raw,file=$(BUILD_DIR)/eagleos.img

run-hdd: all
	$(QEMU) -drive file=$(BUILD_DIR)/eagleos-hdd.img,if=ide,format=raw -boot order=c

clean:
	rm -rf $(BUILD_DIR)
