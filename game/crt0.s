/ crt0.s - startup for a program running on a bare PDP-11: set the stack
/ below the I/O page, zero the uninitialised data (by bytes: _edata may
/ be odd), call main, and halt.
	.text
	.globl	start
	.globl	_main
start:
	mov	$0160000, sp
	mov	$_edata, r0
1:	cmp	r0, $_end
	bhis	2f
	clrb	(r0)+
	br	1b
2:	jsr	pc, _main
	halt
	br	start
