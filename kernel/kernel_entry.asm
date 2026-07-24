;EagleOS 1.0 Kernel Entry Point.
[bits 32]

global _start
global kernel_main_entry
global serial_write_string
extern kernel_main

_start:
kernel_main_entry:
    ; initialize serial for early debug and print an entry message
    call serial_init
    push dword msg_entering_kernel
    call serial_write_string
    add esp, 4

    call kernel_main

halt_loop:
    hlt
    jmp halt_loop

; Simple serial (COM1) initialization and write routines for early debugging.
serial_init:
    ; Disable interrupts (IER = 0)
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al

    ; Enable DLAB (set baud divisor access)
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al

    ; Set divisor to 3 (38400 baud if base 115200)
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al

    ; 8 bits, no parity, one stop bit
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al

    ; Enable FIFO (optional)
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al

    ; Modem control: RTS/DSR set
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al

    ret

; void serial_write_string(const char *s)
serial_write_string:
    push ebp
    mov ebp, esp
    push esi
    mov esi, [ebp+8]    ; pointer to string

.next_char:
    mov al, [esi]
    test al, al
    jz .serial_done

    ; Wait for transmitter holding register empty: LSR (base+5) bit 5
    mov dx, 0x3FD
.wait_lsr:
    in al, dx
    test al, 0x20
    jz .wait_lsr

    ; Write character to THR (base+0)
    mov dx, 0x3F8
    mov al, [esi]
    out dx, al
    inc esi
    jmp .next_char

.serial_done:
    pop esi
    pop ebp
    ret

section .rodata
msg_entering_kernel: db "[boot] Entering kernel...",13,10,0
