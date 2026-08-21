/* 16-bit BIOS bootloader: load kernel, set VESA LFB, enter 32-bit protected mode. */
.code16
.section .text
.global _start
_start:
    cli
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    mov     $0x7C00, %sp
    mov     %dl, boot_drive

    mov     $load_msg, %si
    call    print16

    /* Load sectors 2-8 (rest of this 4 KiB loader) to 0x7E00. */
    mov     $0x07E0, %ax
    mov     %ax, %es
    xor     %bx, %bx
    mov     $0x02, %ah
    mov     $7, %al
    xor     %ch, %ch
    mov     $0x02, %cl
    xor     %dh, %dh
    mov     boot_drive, %dl
    int     $0x13
    jc      disk_fail
    jmp     stage2

disk_fail:
    mov     $disk_msg, %si
    call    print16
hang:
    cli
1:  hlt
    jmp     1b

print16:
    lodsb
    test    %al, %al
    jz      2f
    mov     $0x0E, %ah
    mov     $0x07, %bx
    int     $0x10
    jmp     print16
2:  ret

boot_drive: .byte 0
load_msg:   .asciz "JAS OS x86..."
disk_msg:   .asciz "Disk error"

    .org 510
    .word 0xAA55

/* -------- stage 2 at 0x7E00 -------- */
stage2:
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    mov     $0x7000, %sp

    movw    $8, cur_lba
    mov     kernel_sectors, %ax
    mov     %ax, left
    movw    $0x1000, dest_seg

next_sector:
    cmpw    $0, left
    je      vesa_setup

    /* LBA -> CHS for a 1.44 MB floppy (18 spt, 2 heads). */
    mov     cur_lba, %ax
    xor     %dx, %dx
    mov     $36, %cx
    div     %cx
    mov     %al, %ch
    mov     %dx, %ax
    xor     %dx, %dx
    mov     $18, %bx
    div     %bx
    mov     %al, %dh
    inc     %dl
    mov     %dl, %cl

    mov     dest_seg, %ax
    mov     %ax, %es
    xor     %bx, %bx
    mov     $0x0201, %ax
    mov     boot_drive, %dl
    int     $0x13
    jc      disk_fail

    addw    $0x20, dest_seg
    incw    cur_lba
    decw    left
    jmp     next_sector

vesa_setup:
    /* Kernel load left ES at a high segment. VBE uses ES:DI. */
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es

    mov     $0x4F00, %ax
    mov     $vbe_info, %di
    movl    $0x32454256, vbe_info   /* "VBE2" */
    int     $0x10

    mov     $0x0118, %cx            /* 1024x768x24 */
    call    try_mode
    jnc     mode_ok
    mov     $0x0115, %cx            /* 800x600x24 */
    call    try_mode
    jnc     mode_ok
    mov     $0x0112, %cx            /* 640x480x24 */
    call    try_mode
    jnc     mode_ok
    mov     $0x011B, %cx            /* 1280x1024x24 */
    call    try_mode
    jnc     mode_ok
    mov     $0x0117, %cx            /* 1024x768x16 */
    call    try_mode
    jnc     mode_ok
    mov     $0x0114, %cx            /* 800x600x16 */
    call    try_mode
    jnc     mode_ok
    mov     $0x0111, %cx            /* 640x480x16 */
    call    try_mode
    jnc     mode_ok
    mov     $vesa_msg, %si
    call    print16
    jmp     hang

try_mode:
    push    %cx
    xor     %ax, %ax
    mov     %ax, %es
    mov     $mode_info, %di
    mov     $0x4F01, %ax
    int     $0x10
    pop     %cx
    cmp     $0x004F, %ax
    jne     try_set
try_set:
    xor     %ax, %ax
    mov     %ax, %es
    mov     $0x4F02, %ax
    mov     %cx, %bx
    or      $0x4000, %bx            /* linear framebuffer */
    int     $0x10
    cmp     $0x004F, %ax
    jne     try_fail
    xor     %ax, %ax
    mov     %ax, %es
    mov     $mode_info, %di
    mov     $0x4F01, %ax
    int     $0x10
    clc
    ret
try_fail:
    stc
    ret

mode_ok:
    movl    $0x6D696E69, boot_info
    mov     mode_info + 0x28, %eax  /* PhysBasePtr */
    mov     %eax, boot_info + 4
    mov     mode_info + 0x12, %ax   /* XResolution */
    mov     %ax, boot_info + 8
    mov     mode_info + 0x14, %ax   /* YResolution */
    mov     %ax, boot_info + 10
    mov     mode_info + 0x10, %ax   /* BytesPerScanLine */
    mov     %ax, boot_info + 12
    movzbw  mode_info + 0x19, %ax   /* BitsPerPixel */
    mov     %ax, boot_info + 14

    in      $0x92, %al
    or      $2, %al
    out     %al, $0x92

    cli
    lgdt    gdt_desc
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0
    ljmp    $0x08, $pm_entry

.code32
pm_entry:
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss
    mov     $0x9FFF0, %esp
    mov     $boot_info, %eax
    push    %eax
    mov     $0x10000, %eax
    jmp     *%eax

.code16
vesa_msg:       .asciz "VESA fail"
cur_lba:        .word 8
left:           .word 0
dest_seg:       .word 0x1000
kernel_sectors: .word 120

.align 16
gdt:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt_desc:
    .word 23
    .long gdt

.align 16
boot_info:
    .space 32
.align 16
mode_info:
    .space 256
.align 16
vbe_info:
    .space 512

    .org 4096
