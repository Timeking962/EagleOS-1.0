;EagleOS 1.0 Bootloader.
[bits 16]
org 0x7c00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    mov [boot_drive], dl
    mov [0x0500], dl

    ; Keep VGA in BIOS text mode so early boot traces remain visible in QEMU nographic/serial setups.
    ; Initialize COM1 (16-bit early serial) and print a boot message
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al

    mov si, msg_boot
    call print_serial_text

    ; Re-establish baseline real-mode context before any call instruction.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Switch to 320x200x256 for the graphical desktop before loading the kernel.
    mov ax, 0x0013
    int 0x10

    ; BIOS interrupts may clobber segment/stack registers. Re-establish boot context.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov word [read_count], 180
    mov dword [load_addr], 0x00010000

    mov dl, [boot_drive]
    test dl, 0x80
    jnz read_kernel_lba

    ; Floppy path: CHS reads are broadly compatible for drive 0x00.
    mov dh, 0
    mov ch, 0
    mov cl, 2

read_kernel_chs:
    mov eax, [load_addr]
    mov bx, ax
    and bx, 0x000F
    shr eax, 4
    mov es, ax

    mov dl, [boot_drive]
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc disk_error_chs

    add dword [load_addr], 512

    dec word [read_count]
    jz load_complete

    inc cl
    cmp cl, 19
    jne read_kernel_chs
    mov cl, 1
    inc dh
    cmp dh, 2
    jne read_kernel_chs
    mov dh, 0
    inc ch
    jmp read_kernel_chs

read_kernel_lba:
    mov dword [lba_low], 1
    mov dword [lba_high], 0

read_kernel:
    mov eax, [load_addr]
    mov bx, ax
    and bx, 0x000F
    shr eax, 4
    mov [dap_offset], bx
    mov [dap_segment], ax

    mov eax, [lba_low]
    mov [dap_lba_low], eax
    mov eax, [lba_high]
    mov [dap_lba_high], eax

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error_lba

    add dword [load_addr], 512
    add dword [lba_low], 1
    adc dword [lba_high], 0

    dec word [read_count]
    jnz read_kernel

load_complete:

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_entry

[bits 32]
protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    push word 0x08
    push dword 0x10000
    retf

[bits 16]

disk_error_chs:
    mov si, msg_err_chs
    call print_serial_text
    jmp halt

disk_error_lba:
    mov si, msg_err_lba
    call print_serial_text
    jmp halt

halt:
    hlt
    jmp halt

print_serial_text:
    push ax
    push dx
.next:
    mov al, [si]
    cmp al, 0
    je .done
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, 0x3F8
    mov al, [si]
    out dx, al
    inc si
    jmp .next
.done:
    pop dx
    pop ax
    ret

boot_drive: db 0
read_count: dw 0
load_addr: dd 0
lba_low: dd 0
lba_high: dd 0

dap:
    db 0x10
    db 0x00
    dw 0x0001
dap_offset:  dw 0x0000
dap_segment: dw 0x0000
dap_lba_low:  dd 0
dap_lba_high: dd 0

msg_boot: db "[b] boot",13,10,0
msg_err_chs: db "[b] ERR CHS",13,10,0
msg_err_lba: db "[b] ERR LBA",13,10,0

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

gdt_start:
    dq 0

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

times 510 - ($ - $$) db 0
    dw 0xAA55
