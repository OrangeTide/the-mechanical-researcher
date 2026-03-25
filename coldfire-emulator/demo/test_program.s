#NO_APP
	.file	"test_program.c"
	.text
	.align	2
	.type	fibonacci, @function
fibonacci:
	lea (-116,%sp),%sp
	mov3q.l #1,%d0
	move.l 120(%sp),%a0
	movem.l #31996,(%sp)
	cmp.l %a0,%d0
	jeq .L37
	move.l %a0,%d0
	moveq #-2,%d1
	subq.l #1,%d0
	and.l %d0,%d1
	move.l %a0,%d2
	move.l %a0,60(%sp)
	clr.l 64(%sp)
	move.l %d0,%a0
	sub.l %d1,%d2
	move.l %d2,100(%sp)
	move.l 100(%sp),%d1
	cmp.l 60(%sp),%d1
	jeq .L46
.L3:
	subq.l #2,60(%sp)
	moveq #-2,%d0
	and.l 60(%sp),%d0
	move.l %a0,%d1
	clr.l 68(%sp)
	sub.l %d0,%d1
	move.l %d1,104(%sp)
	move.l %a0,%d1
.L9:
	move.l %d1,%a0
	subq.l #1,%a0
	cmp.l 104(%sp),%d1
	jeq .L47
	subq.l #2,%d1
	moveq #-2,%d0
	and.l %d1,%d0
	move.l %a0,%d2
	move.l %d1,88(%sp)
	clr.l 72(%sp)
	move.l %a0,%d1
	sub.l %d0,%d2
	move.l %d2,108(%sp)
.L13:
	move.l %d1,%a1
	subq.l #1,%a1
	cmp.l 108(%sp),%d1
	jeq .L48
	subq.l #2,%d1
	moveq #-2,%d0
	and.l %d1,%d0
	move.l %a1,%d2
	clr.l 76(%sp)
	move.l %d1,92(%sp)
	sub.l %d0,%d2
	move.l %d2,112(%sp)
.L17:
	move.l %a1,%d4
	subq.l #1,%d4
	cmp.l 112(%sp),%a1
	jeq .L49
	subq.l #2,%a1
	moveq #-2,%d1
	move.l %a1,%d0
	move.l %d4,%d2
	and.l %d1,%d0
	clr.l %d1
	move.l %d1,%a5
	move.l %a1,96(%sp)
	sub.l %d0,%d2
	move.l %d2,80(%sp)
.L21:
	move.l %d4,%d3
	subq.l #1,%d3
	cmp.l 80(%sp),%d4
	jeq .L50
	subq.l #2,%d4
	moveq #-2,%d0
	clr.l %d6
	and.l %d4,%d0
	move.l %d3,%a1
	sub.l %d0,%a1
.L25:
	move.l %d3,%d5
	subq.l #1,%d5
	cmp.l %d3,%a1
	jeq .L51
	move.l %d3,%d7
	moveq #-2,%d0
	subq.l #2,%d3
	and.l %d3,%d0
	subq.l #3,%d7
	move.l %d7,%d2
	sub.l %a4,%a4
	sub.l %d0,%d2
	move.l %d2,84(%sp)
.L29:
	move.l %d5,%d2
	subq.l #1,%d2
	cmp.l 84(%sp),%d7
	jeq .L52
	moveq #-2,%d0
	sub.l %a2,%a2
	move.l %d2,%a6
	and.l %d7,%d0
	sub.l %d0,%a6
.L33:
	move.l %d2,%a0
	subq.l #1,%a0
	cmp.l %d2,%a6
	jeq .L53
	move.l %a0,%d1
	sub.l %a3,%a3
	move.l %a0,44(%sp)
.L30:
	move.l %d1,%a0
	pea -1(%a0)
	move.l %d1,60(%sp)
	move.l %a1,56(%sp)
	jsr fibonacci
	addq.l #4,%sp
	add.l %d0,%a3
	move.l 56(%sp),%d1
	mov3q.l #1,%d0
	subq.l #2,%d1
	move.l 52(%sp),%a1
	cmp.l %d1,%d0
	jcs .L30
	move.l 44(%sp),%a0
	move.l %d2,%d0
	moveq #-2,%d1
	subq.l #3,%d0
	and.l %d1,%d0
	lea -2(%a0,%a2.l),%a2
	subq.l #2,%d2
	sub.l %d0,%a2
	add.l %a3,%a2
	mov3q.l #1,%d0
	cmp.l %d2,%d0
	jne .L33
	lea (1,%a2),%a3
	subq.l #2,%d5
	add.l %a3,%a4
	subq.l #2,%d7
	mov3q.l #1,%d0
	cmp.l %d5,%d0
	jne .L29
.L56:
	move.l %a4,%d2
	addq.l #1,%d2
	add.l %d2,%d6
	mov3q.l #1,%d1
	cmp.l %d3,%d1
	jcs .L25
.L55:
	move.l %d6,%d5
	add.l %d3,%d5
	add.l %d5,%a5
	mov3q.l #1,%d0
	cmp.l %d4,%d0
	jcs .L21
.L54:
	move.l %a5,%d0
	add.l %d4,%d0
	move.l 96(%sp),%a1
	add.l %d0,76(%sp)
	mov3q.l #1,%d0
	cmp.l %a1,%d0
	jcs .L17
.L57:
	move.l 76(%sp),%d0
	add.l %a1,%d0
	move.l 92(%sp),%d1
	add.l %d0,72(%sp)
	mov3q.l #1,%d0
	cmp.l %d1,%d0
	jcs .L13
.L58:
	move.l %d1,%a0
	mov3q.l #1,%d2
	move.l 88(%sp),%d1
	move.l 72(%sp),%d0
	add.l %a0,%d0
	add.l %d0,68(%sp)
	cmp.l %d1,%d2
	jcs .L9
.L59:
	move.l 68(%sp),%d0
	add.l %d1,%d0
	add.l %d0,64(%sp)
	mov3q.l #1,%d2
	cmp.l 60(%sp),%d2
	jcc .L38
.L60:
	move.l 100(%sp),%d1
	move.l 60(%sp),%d0
	subq.l #1,%d0
	move.l %d0,%a0
	cmp.l 60(%sp),%d1
	jne .L3
.L46:
	move.l 64(%sp),%a0
	add.l %d0,%a0
.L37:
	movem.l (%sp),#31996
	move.l %a0,%d0
	lea (116,%sp),%sp
	rts
.L51:
	add.l %d6,%d5
	add.l %d5,%a5
	mov3q.l #1,%d0
	cmp.l %d4,%d0
	jcs .L21
	jra .L54
.L52:
	add.l %a4,%d2
	add.l %d2,%d6
	mov3q.l #1,%d1
	cmp.l %d3,%d1
	jcs .L25
	jra .L55
.L53:
	move.l %a0,%a3
	add.l %a2,%a3
	subq.l #2,%d5
	add.l %a3,%a4
	subq.l #2,%d7
	mov3q.l #1,%d0
	cmp.l %d5,%d0
	jne .L29
	jra .L56
.L50:
	move.l %d3,%d0
	add.l %a5,%d0
	move.l 96(%sp),%a1
	add.l %d0,76(%sp)
	mov3q.l #1,%d0
	cmp.l %a1,%d0
	jcs .L17
	jra .L57
.L49:
	move.l 76(%sp),%d0
	add.l %d4,%d0
	move.l 92(%sp),%d1
	add.l %d0,72(%sp)
	mov3q.l #1,%d0
	cmp.l %d1,%d0
	jcs .L13
	jra .L58
.L48:
	move.l 88(%sp),%d1
	mov3q.l #1,%d2
	move.l 72(%sp),%d0
	add.l %a1,%d0
	add.l %d0,68(%sp)
	cmp.l %d1,%d2
	jcs .L9
	jra .L59
.L47:
	move.l 68(%sp),%d0
	add.l %a0,%d0
	add.l %d0,64(%sp)
	mov3q.l #1,%d2
	cmp.l 60(%sp),%d2
	jcs .L60
.L38:
	movem.l (%sp),#31996
	move.l 60(%sp),%a0
	add.l 64(%sp),%a0
	move.l %a0,%d0
	lea (116,%sp),%sp
	rts
	.size	fibonacci, .-fibonacci
	.section	.text.entry,"ax",@progbits
	.align	2
	.globl	_start
	.type	_start, @function
_start:
	subq.l #8,%sp
	fmovem #32,(%sp)
	move.l %d3,-(%sp)
	move.l %d2,-(%sp)
	pea 10.w
	jsr fibonacci
	mvz.w #252,%d2
	addq.l #4,%sp
	moveq #105,%d1
	move.l %d0,result_fib
.L62:
	remu.l %d1,%d3:%d2
	move.l %d1,%d0
	move.l %d0,%d2
	move.l %d3,%d1
	tst.l %d3
	jne .L62
	clr.l -(%sp);move.l #1072693248,-(%sp);fdmove.d (%sp)+,%fp0
	mvz.w #5050,%d1
	mvz.w #2645,%d3
	move.l %d0,result_gcd
	moveq #20,%d0
	move.l %d1,result_sum
	move.l %d3,result_bits
.L63:
	subq.l #1,%d0
	clr.l -(%sp);move.l #1073741824,-(%sp);fdmove.d (%sp)+,%fp1
	fddiv.d %fp0,%fp1
	fdadd.d %fp1,%fp0
	clr.l -(%sp);move.l #1071644672,-(%sp);fdmove.d (%sp)+,%fp1
	fdmul.d %fp1,%fp0
	tst.l %d0
	jne .L63
	clr.l -(%sp);move.l #1083129856,-(%sp);fdmove.d (%sp)+,%fp1
	clr.l -(%sp);move.l #1105199104,-(%sp);fdmove.d (%sp)+,%fp2
	fdmul.d %fp1,%fp0
	fcmp.d %fp2,%fp0
	fjge .L64
	fintrz.d %fp0,%fp0
	fmove.l %fp0,%d0
	move.l %d0,result_sqrt_i
#APP
| 103 "test_program.c" 1
	trap #0
| 0 "" 2
#NO_APP
.L66:
	jra .L66
.L64:
	clr.l -(%sp);move.l #1105199104,-(%sp);fdmove.d (%sp)+,%fp1
	fdsub.d %fp1,%fp0
	fintrz.d %fp0,%fp0
	fmove.l %fp0,%d0
	add.l #-2147483648,%d0
	move.l %d0,result_sqrt_i
#APP
| 103 "test_program.c" 1
	trap #0
| 0 "" 2
#NO_APP
	jra .L66
	.size	_start, .-_start
	.globl	result_sqrt_i
	.section	.results,"aw"
	.align	2
	.type	result_sqrt_i, @object
	.size	result_sqrt_i, 4
result_sqrt_i:
	.zero	4
	.globl	result_bits
	.align	2
	.type	result_bits, @object
	.size	result_bits, 4
result_bits:
	.zero	4
	.globl	result_sum
	.align	2
	.type	result_sum, @object
	.size	result_sum, 4
result_sum:
	.zero	4
	.globl	result_gcd
	.align	2
	.type	result_gcd, @object
	.size	result_gcd, 4
result_gcd:
	.zero	4
	.globl	result_fib
	.align	2
	.type	result_fib, @object
	.size	result_fib, 4
result_fib:
	.zero	4
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
