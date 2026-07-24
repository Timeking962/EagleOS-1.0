@echo off
setlocal

pushd "%~dp0"

set NASM=C:\Progra~2\NASM\nasm.exe
set USE_CROSS=0
set "GCC="
set "LD="
set "OBJCOPY="

where i686-elf-gcc >nul 2>&1
if %errorlevel%==0 (
    set "GCC=i686-elf-gcc"
    set "LD=i686-elf-ld"
    set "OBJCOPY=i686-elf-objcopy"
    set "USE_CROSS=1"
)

if not defined GCC (
    if exist "C:\gcc\bin\i686-elf-gcc.exe" (
        set "GCC=C:\gcc\bin\i686-elf-gcc.exe"
        set "LD=C:\gcc\bin\i686-elf-ld.exe"
        set "OBJCOPY=C:\gcc\bin\i686-elf-objcopy.exe"
        set "USE_CROSS=1"
    )
)

if not defined GCC (
    where gcc >nul 2>&1
    if %errorlevel%==0 (
        set "GCC=gcc"
        where objcopy >nul 2>&1
        if %errorlevel%==0 (
            set "OBJCOPY=objcopy"
        ) else (
            echo Error: objcopy not found on PATH.
            exit /b 1
        )
    ) else (
        echo Error: no GCC compiler found on PATH.
        echo Install either i686-elf-gcc or a native GCC toolchain with objcopy.
        exit /b 1
    )
)

if not exist "%NASM%" (
    echo Error: nasm not found at "%NASM%".
    echo Update the NASM path in build.bat if needed.
    exit /b 1
)

if not exist build mkdir build

python tools\bump_build_tag.py
if errorlevel 1 exit /b 1

"%NASM%" -f bin boot\boot.asm -o build\boot.bin
if errorlevel 1 exit /b 1

"%NASM%" -f elf32 kernel\kernel_entry.asm -o build\kernel_entry.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\kernel.c -o build\kernel_kernel.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\render_backend.c -o build\kernel_render_backend.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\desktop_interface_manager.c -o build\kernel_desktop_interface_manager.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\keyboard.c -o build\kernel_keyboard.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\ui.c -o build\kernel_ui.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\mouse.c -o build\kernel_mouse.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\disk.c -o build\kernel_disk.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\fs.c -o build\kernel_fs.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\system.c -o build\kernel_system.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\exec.c -o build\kernel_exec.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\program_manager.c -o build\kernel_program_manager.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\calculator.c -o build\kernel_calculator.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\text_editor.c -o build\kernel_text_editor.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\file_manager.c -o build\kernel_file_manager.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\installer.c -o build\kernel_installer.o
if errorlevel 1 exit /b 1

%GCC% -m32 -ffreestanding -fno-pie -O2 -Wall -Wextra -c kernel\sysver.c -o build\kernel_sysver.o
if errorlevel 1 exit /b 1

"%NASM%" -f bin apps\native\calc_native.asm -o build\calc_native.bin
if errorlevel 1 exit /b 1

if %USE_CROSS%==1 (
    "%LD%" -m elf_i386 -Ttext 0x10000 -e kernel_main_entry build\kernel_entry.o build\kernel_kernel.o build\kernel_render_backend.o build\kernel_desktop_interface_manager.o build\kernel_keyboard.o build\kernel_ui.o build\kernel_mouse.o build\kernel_disk.o build\kernel_fs.o build\kernel_system.o build\kernel_exec.o build\kernel_program_manager.o build\kernel_calculator.o build\kernel_text_editor.o build\kernel_file_manager.o build\kernel_installer.o build\kernel_sysver.o -o build\kernel.elf
    if errorlevel 1 exit /b 1
) else (
    "%GCC%" -m32 -ffreestanding -nostdlib -Ttext 0x10000 build\kernel_entry.o build\kernel_kernel.o build\kernel_render_backend.o build\kernel_desktop_interface_manager.o build\kernel_keyboard.o build\kernel_ui.o build\kernel_mouse.o build\kernel_disk.o build\kernel_fs.o build\kernel_system.o build\kernel_exec.o build\kernel_program_manager.o build\kernel_calculator.o build\kernel_text_editor.o build\kernel_file_manager.o build\kernel_installer.o build\kernel_sysver.o -o build\kernel.elf
    if errorlevel 1 exit /b 1
)

"%OBJCOPY%" --adjust-vma=-0x10000 -O binary build\kernel.elf build\kernel.bin
if errorlevel 1 exit /b 1

python tools\build_exec_bundle.py build\exec.bundle build\calc_native.bin
if errorlevel 1 exit /b 1

python tools\make_image.py build\boot.bin build\kernel.bin build\eagleos.img build\exec.bundle
if errorlevel 1 exit /b 1

python tools\make_hdd_image.py build\boot.bin build\kernel.bin build\eagleos-hdd.img build\exec.bundle 64
if errorlevel 1 exit /b 1

popd
endlocal
exit /b 0
