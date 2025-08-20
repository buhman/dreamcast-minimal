	.section .text.start
	.global _start
_start:
	mov.l stack_end_ptr,r15
	mov.l main_ptr,r0
	jmp @r0
	nop

	.align 4
main_ptr:
	.long _main
stack_end_ptr:
	.long 0xacffc000
