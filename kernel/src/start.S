.section .text
.global _start
.global irq0_stub, irq1_stub, irq12_stub
.global default_irq_stub
.extern kernel_main
.extern timer_irq
.extern keyboard_irq
.extern mouse_irq
.extern stack_top
.extern _bss_start
.extern _bss_end

.code32
_start:
    cli
    mov     (%esp), %ebx            /* boot_info pointer left by the loader */
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss
    mov     $stack_top, %esp

    mov     $_bss_start, %edi
    mov     $_bss_end, %ecx
    sub     %edi, %ecx
    xor     %eax, %eax
    shr     $2, %ecx
    rep     stosl

    push    %ebx
    call    kernel_main
1:
    hlt
    jmp     1b

.macro IRQ_STUB handler
    pusha
    call    \handler
    popa
    iret
.endm

irq0_stub:
    IRQ_STUB timer_irq

irq1_stub:
    IRQ_STUB keyboard_irq

irq12_stub:
    IRQ_STUB mouse_irq

default_irq_stub:
    pusha
    mov     $0x20, %al
    out     %al, $0xA0
    out     %al, $0x20
    popa
    iret
