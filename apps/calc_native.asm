; EagleOS native CALC app blob (phase 2b).
[bits 32]
org 0

%define HOST_DRAW_WINDOW 0
%define HOST_TEXT        4
%define HOST_RECT        8
%define HOST_BOX         12
%define HOST_EXEC_LAUNCH 16

%define TAB_KEY          7

global app_entry

app_base:

%macro GET_BASE 1
    call %%get_base
%%get_base:
    pop %1
    sub %1, %%get_base - app_base
%endmacro

after_entry:

app_entry:
    GET_BASE eax

    mov ecx, [esp + 4]      ; host_api
    mov [eax + api_ptr - app_base], ecx

    mov edx, [esp + 8]      ; out_callbacks
    lea ecx, [eax + on_start - app_base]
    mov [edx + 0], ecx
    lea ecx, [eax + on_draw - app_base]
    mov [edx + 4], ecx
    lea ecx, [eax + on_key - app_base]
    mov [edx + 8], ecx
    lea ecx, [eax + on_mouse - app_base]
    mov [edx + 12], ecx

    xor eax, eax
    ret

on_start:
    push ebx
    GET_BASE ebx
    mov dword [ebx + value_state - app_base], 0
    pop ebx
    ret

on_draw:
    push ebx
    push esi

    GET_BASE ebx
    mov esi, [ebx + api_ptr - app_base]

    ; host->graphics_draw_window(26, 20, 268, 154, "CALC NATIVE", true)
    push dword 1
    lea eax, [ebx + str_title - app_base]
    push eax
    push dword 154
    push dword 268
    push dword 20
    push dword 26
    mov eax, [esi + HOST_DRAW_WINDOW]
    call eax
    add esp, 24

    ; small stable text-only UI (avoid previous complex draw path)
    push dword 15
    lea eax, [ebx + str_help1 - app_base]
    push eax
    push dword 42
    push dword 40
    mov eax, [esi + HOST_TEXT]
    call eax
    add esp, 16

    push dword 15
    lea eax, [ebx + str_help2 - app_base]
    push eax
    push dword 54
    push dword 40
    mov eax, [esi + HOST_TEXT]
    call eax
    add esp, 16

    pop esi
    pop ebx
    ret

draw_value:
    ; in: eax=value, uses ebx/esi and host text API
    push ebx
    push esi
    push ecx
    push edx
    push edi

    GET_BASE ebx

    lea edi, [ebx + value_buf - app_base]
    mov byte [edi], 0

    cmp eax, 0
    jne .not_zero
    mov byte [edi], '0'
    mov byte [edi + 1], 0
    jmp .emit

.not_zero:
    xor ecx, ecx
    cmp eax, 0
    jge .abs_ready
    mov ecx, 1
    neg eax
.abs_ready:
    lea edi, [ebx + value_buf - app_base + 15]
    mov byte [edi], 0

.convert_loop:
    xor edx, edx
    mov esi, 10
    div esi
    add dl, '0'
    dec edi
    mov [edi], dl
    test eax, eax
    jnz .convert_loop

    cmp ecx, 0
    je .emit
    dec edi
    mov byte [edi], '-'

.emit:
    mov esi, [ebx + api_ptr - app_base]
    push dword 14
    push edi
    push dword 66
    push dword 94
    mov eax, [esi + HOST_TEXT]
    call eax
    add esp, 16

    pop edi
    pop edx
    pop ecx
    pop esi
    pop ebx
    ret

on_key:
    push ebx

    GET_BASE ebx

    mov eax, [esp + 8]    ; key
    cmp eax, 1            ; KEY_UP
    jne .check_down
    inc dword [ebx + value_state - app_base]
    jmp .done

.check_down:
    cmp eax, 2            ; KEY_DOWN
    jne .check_tab
    dec dword [ebx + value_state - app_base]
    jmp .done

.check_tab:
    cmp eax, TAB_KEY
    jne .done

    mov edx, [ebx + api_ptr - app_base]
    lea eax, [ebx + str_progman - app_base]
    push eax
    mov eax, [edx + HOST_EXEC_LAUNCH]
    call eax
    add esp, 4

.done:
    pop ebx
    ret

on_mouse:
    push ebx

    GET_BASE ebx

    mov eax, [esp + 16]   ; left
    test eax, eax
    jz .check_right
    inc dword [ebx + value_state - app_base]

.check_right:
    mov eax, [esp + 20]   ; right
    test eax, eax
    jz .done
    dec dword [ebx + value_state - app_base]

.done:
    pop ebx
    ret

api_ptr:        dd 0
value_state:    dd 0
value_buf:      times 16 db 0

str_title:      db "CALC NATIVE", 0
str_help1:      db "UP/DOWN CHANGES VALUE", 0
str_help2:      db "TAB RETURNS TO MENU", 0
str_progman:    db "PROGMAN", 0
