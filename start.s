	.section .text.start
	.global _start
_start:
	mov.l stack_end_ptr,r15

        /* mask all interrupts */
        mov.l   imask_all,r0
        mov.l   zero_rb,r2
        stc     sr,r1
        or      r1,r0
        and     r0,r2
        ldc     r2,sr

	mov.l main_ptr,r0
	jmp @r0
	nop

	.align 4
imask_all:
        .long 0xf0
zero_rb:
        .long ~(1 << 29)
main_ptr:
	.long _main
stack_end_ptr:
	.long 0xacffc000
