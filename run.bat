@echo off
set "ROOT=%~dp0"
if exist "%ROOT%build\eagleos.img" (
    "%ROOT%qemu\qemu-system-i386.exe" -fda "%ROOT%build\eagleos.img" -boot a
) else (
    echo Image not found at "%ROOT%build\eagleos.img". Run build.bat first.
)
