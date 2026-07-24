@echo off
set "ROOT=%~dp0"
if exist "%ROOT%build\eagleos-hdd.img" (
    "%ROOT%qemu\qemu-system-i386.exe" -drive file="%ROOT%build\eagleos-hdd.img",if=ide,format=raw -boot order=c
) else (
    echo HDD image not found at "%ROOT%build\eagleos-hdd.img".
    echo Run build.bat first.
)
