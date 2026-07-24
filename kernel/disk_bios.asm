; EagleOS 1.0 BIOS disk bridge for protected mode kernel.
; Uses fixed low-memory state block to avoid 16-bit linker relocations.
[bits 32]

global bios_disk_read_sector
global bios_disk_write_sector

KERNEL_RM_SEG equ 0x1000

STATE_BASE     equ 0x0600
STATE_LBA      equ STATE_BASE + 0   ; word
STATE_OP       equ STATE_BASE + 2   ; byte (0x02 read, 0x03 write)
STATE_STATUS   equ STATE_BASE + 3   ; byte (0 fail, 1 ok)
STATE_BUF      equ STATE_BASE + 4   ; dword linear pointer
STATE_PM_ESP   equ STATE_BASE + 8   ; dword
STATE_CYL      equ STATE_BASE + 12  ; byte

bios_disk_read_sector:
    push ebp
    mov ebp, esp

    mov ax, [ebp + 8]
    mov [STATE_LBA], ax
    mov eax, [ebp + 12]
    mov [STATE_BUF], eax
    mov byte [STATE_OP], 0x02

    call rm_int13_sector_io
    movzx eax, byte [STATE_STATUS]

    pop ebp
    ret

bios_disk_write_sector:
    push ebp
    mov ebp, esp

    mov ax, [ebp + 8]
    mov [STATE_LBA], ax
    mov eax, [ebp + 12]
    mov [STATE_BUF], eax
    mov byte [STATE_OP], 0x03

    call rm_int13_sector_io
    movzx eax, byte [STATE_STATUS]

    pop ebp
    ret

rm_int13_sector_io:
    cli
    mov [STATE_PM_ESP], esp

    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax
    jmp KERNEL_RM_SEG:real_mode_entry

[bits 16]
real_mode_entry:
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7000

    mov ax, [STATE_LBA]
    xor dx, dx
    mov bx, 36
    div bx
    mov [STATE_CYL], al

    mov ax, dx
    xor dx, dx
    mov bl, 18
    div bl
    mov dh, al
    mov cl, ah
    inc cl
    mov ch, [STATE_CYL]

    mov eax, [STATE_BUF]
    mov bx, ax
    and bx, 0x000F
    shr eax, 4
    mov es, ax

    mov dl, [0x0500]
    mov ah, [STATE_OP]
    mov al, 1
    int 0x13
    jc .io_fail

    mov byte [STATE_STATUS], 1
    jmp .back_to_pm

.io_fail:
    mov byte [STATE_STATUS], 0

.back_to_pm:
    cli
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:protected_mode_entry

[bits 32]
protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [STATE_PM_ESP]
    ret
