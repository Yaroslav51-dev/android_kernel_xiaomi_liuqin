/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2022, Arm Limited.
 *
 * Adapted from the original at:
 * https://github.com/ARM-software/optimized-routines/blob/189dfefe37d54c5b/string/aarch64/strncmp.S
 */

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * compare two strings
 *
 * ARMv8-a, AArch64.
 * MTE compatible.
 */

#define REP8_01 0x0101010101010101
#define REP8_7f 0x7f7f7f7f7f7f7f7f

/* Parameters and result.  */
src1		.req	x0
src2		.req	x1
limit		.req	x2
result		.req	x0

/* Internal variables.  */
#define data1		x3
#define data1w		w3
#define data2		x4
#define data2w		w4
#define has_nul		x5
#define diff		x6
#define syndrome	x7
#define tmp1		x8
#define tmp2		x9
#define tmp3		x10
#define zeroones	x11
#define pos		x12
#define mask		x13
#define endloop		x14
#define count		mask
#define offset		pos
#define neg_offset	x15

/* Define endian dependent shift operations.
   On big-endian early bytes are at MSB and on little-endian LSB.
   LS_FW means shifting towards early bytes.
   LS_BK means shifting towards later bytes.
   */
#ifdef __AARCH64EB__
#define LS_FW lsl
#define LS_BK lsr
#else
#define LS_FW lsr
#define LS_BK lsl
#endif

SYM_FUNC_START_WEAK_PI(strncmp)
	cbz	limit, .Lret0
	eor	tmp1, src1, src2
	mov	zeroones, #REP8_01
	tst	tmp1, #7
	and	count, src1, #7
	b.ne	L(misaligned8)
	cbnz	count, L(mutual_align)

	/*
	* NUL detection works on the principle that (X - 1) & (~X) & 0x80
	* (=> (X - 1) & ~(X | 0x7f)) is non-zero iff a byte is zero, and
	* can be done in parallel across the entire word.
	*/
.Lloop_aligned:
	ldr	data1, [src1], #8
	ldr	data2, [src2], #8
L(start_realigned):
	subs	limit, limit, #8
	sub	tmp1, data1, zeroones
	orr	tmp2, data1, #REP8_7f
	eor	diff, data1, data2	/* Non-zero if differences found.  */
	csinv	endloop, diff, xzr, hi	/* Last Dword or differences.  */
	bics	has_nul, tmp1, tmp2	/* Non-zero if NUL terminator.  */
	ccmp	endloop, #0, #0, eq
	b.eq	.Lloop_aligned

L(full_check):
#ifndef __AARCH64EB__
	orr	syndrome, diff, has_nul
	add	limit, limit, 8	/* Rewind limit to before last subs. */
L(syndrome_check):
	/* Limit was reached. Check if the NUL byte or the difference
	   is before the limit. */
	rev	syndrome, syndrome
	rev	data1, data1
	clz	pos, syndrome
	rev	data2, data2
	lsl	data1, data1, pos
	cmp	limit, pos, lsr #3
	lsl	data2, data2, pos
	/* But we need to zero-extend (char is unsigned) the value and then
	   perform a signed 32-bit subtraction.  */
	lsr	data1, data1, #56
	sub	result, data1, data2, lsr #56
	csel result, result, xzr, hi
	ret
#else
	/* Not reached the limit, must have found the end or a diff.  */
	tbz	limit, #63, L(not_limit)
	add	tmp1, limit, 8
	cbz	limit, L(not_limit)

	lsl	limit, tmp1, #3	/* Bits -> bytes.  */
	mov	mask, #~0
	lsr	mask, mask, limit
	bic	data1, data1, mask
	bic	data2, data2, mask

	/* Make sure that the NUL byte is marked in the syndrome.  */
	orr	has_nul, has_nul, mask

L(not_limit):
	/* For big-endian we cannot use the trick with the syndrome value
	   as carry-propagation can corrupt the upper bits if the trailing
	   bytes in the string contain 0x01.  */
	/* However, if there is no NUL byte in the dword, we can generate
	   the result directly.  We can't just subtract the bytes as the
	   MSB might be significant.  */
	cbnz	has_nul, 1f
	cmp	data1, data2
	cset	result, ne
	cneg	result, result, lo
	ret
1:
	/* Re-compute the NUL-byte detection, using a byte-reversed value.  */
	rev	tmp3, data1
	sub	tmp1, tmp3, zeroones
	orr	tmp2, tmp3, #REP8_7f
	bic	has_nul, tmp1, tmp2
	rev	has_nul, has_nul
	orr	syndrome, diff, has_nul
	clz	pos, syndrome
	/* The most-significant-non-zero bit of the syndrome marks either the
	   first bit that is different, or the top bit of the first zero byte.
	   Shifting left now will bring the critical information into the
	   top bits.  */
L(end_quick):
	lsl	data1, data1, pos
	lsl	data2, data2, pos
	/* But we need to zero-extend (char is unsigned) the value and then
	   perform a signed 32-bit subtraction.  */
	lsr	data1, data1, #56
	sub	result, data1, data2, lsr #56
	ret
#endif

L(mutual_align):
	/* Sources are mutually aligned, but are not currently at an
	   alignment boundary.  Round down the addresses and then mask off
	   the bytes that precede the start point.
	   We also need to adjust the limit calculations, but without
	   overflowing if the limit is near ULONG_MAX.  */
	bic	src1, src1, #7
	bic	src2, src2, #7
	ldr	data1, [src1], #8
	neg	tmp3, tmp1, lsl #3  /* 64 - bits(bytes beyond align). */
	ldr	data2, [src2], #8
	mov	tmp2, #~0
	LS_FW	tmp2, tmp2, tmp3	/* Shift (count & 63).  */
	/* Adjust the limit and ensure it doesn't overflow.  */
	adds	limit, limit, count
	csinv	limit, limit, xzr, lo
	orr	data1, data1, tmp2
	orr	data2, data2, tmp2
	b	L(start_realigned)

/*when src1 offset is not equal to src2 offset...*/
.Lmisaligned8:
	cmp	limit, #8
	b.lo	.Ltiny8proc /*limit < 8... */
	/*
	* Get the align offset length to compare per byte first.
	* After this process, one string's address will be aligned.*/
	and	tmp1, src1, #7
	neg	tmp1, tmp1
	add	tmp1, tmp1, #8
	and	tmp2, src2, #7
	neg	tmp2, tmp2
	add	tmp2, tmp2, #8
	subs	tmp3, tmp1, tmp2
	csel	pos, tmp1, tmp2, hi /*Choose the maximum. */
	/*
	* Here, limit is not less than 8, so directly run .Ltinycmp
	* without checking the limit.*/
	sub	limit, limit, pos
.Ltinycmp:
	ldrb	data1w, [src1], #1
	ldrb	data2w, [src2], #1
	subs	pos, pos, #1
	ccmp	data1w, #1, #0, ne  /* NZCV = 0b0000.  */
	ccmp	data1w, data2w, #0, cs  /* NZCV = 0b0000.  */
	b.eq	.Ltinycmp
	cbnz	pos, 1f /*find the null or unequal...*/
	cmp	data1w, #1
	ccmp	data1w, data2w, #0, cs
	b.eq	.Lstart_align /*the last bytes are equal....*/
1:
	sub	result, data1, data2
	ret

.Lstart_align:
	lsr	limit_wd, limit, #3
	cbz	limit_wd, .Lremain8
	/*process more leading bytes to make str1 aligned...*/
	ands	xzr, src1, #7
	b.eq	.Lrecal_offset
	add	src1, src1, tmp3	/*tmp3 is positive in this branch.*/
	add	src2, src2, tmp3
	ldr	data1, [src1], #8
	ldr	data2, [src2], #8

	sub	limit, limit, tmp3
	lsr	limit_wd, limit, #3
	subs	limit_wd, limit_wd, #1

	sub	tmp1, data1, zeroones
	orr	tmp2, data1, #REP8_7f
	eor	diff, data1, data2  /* Non-zero if differences found.  */
	csinv	endloop, diff, xzr, ne/*if limit_wd is 0,will finish the cmp*/
	bics	has_nul, tmp1, tmp2
	ccmp	endloop, #0, #0, eq /*has_null is ZERO: no null byte*/
	b.ne	.Lunequal_proc
	/*How far is the current str2 from the alignment boundary...*/
	and	tmp3, tmp3, #7
.Lrecal_offset:
	neg	pos, tmp3
.Lloopcmp_proc:
	/*
	* Divide the eight bytes into two parts. First,backwards the src2
	* to an alignment boundary,load eight bytes from the SRC2 alignment
	* boundary,then compare with the relative bytes from SRC1.
	* If all 8 bytes are equal,then start the second part's comparison.
	* Otherwise finish the comparison.
	* This special handle can garantee all the accesses are in the
	* thread/task space in avoid to overrange access.
	*/
	ldr	data1, [src1,pos]
	ldr	data2, [src2,pos]
	sub	tmp1, data1, zeroones
	orr	tmp2, data1, #REP8_7f
	bics	has_nul, tmp1, tmp2 /* Non-zero if NUL terminator.  */
	eor	diff, data1, data2  /* Non-zero if differences found.  */
	csinv	endloop, diff, xzr, eq
	cbnz	endloop, .Lunequal_proc

	/*The second part process*/
	ldr	data1, [src1], #8
	ldr	data2, [src2], #8
	subs	limit_wd, limit_wd, #1
	sub	tmp1, data1, zeroones
	orr	tmp2, data1, #REP8_7f
	eor	diff, data1, data2  /* Non-zero if differences found.  */
	csinv	endloop, diff, xzr, ne/*if limit_wd is 0,will finish the cmp*/
	bics	has_nul, tmp1, tmp2
	ccmp	endloop, #0, #0, eq /*has_null is ZERO: no null byte*/
	b.eq	.Lloopcmp_proc

.Lunequal_proc:
	orr	syndrome, diff, has_nul
	cbz	syndrome, .Lremain8
.Lcal_cmpresult:
	/*
	* reversed the byte-order as big-endian,then CLZ can find the most
	* significant zero bits.
	*/
CPU_LE( rev	syndrome, syndrome )
CPU_LE( rev	data1, data1 )
CPU_LE( rev	data2, data2 )
	/*
	* For big-endian we cannot use the trick with the syndrome value
	* as carry-propagation can corrupt the upper bits if the trailing
	* bytes in the string contain 0x01.
	* However, if there is no NUL byte in the dword, we can generate
	* the result directly.  We can't just subtract the bytes as the
	* MSB might be significant.
	*/
CPU_BE( cbnz	has_nul, 1f )
CPU_BE( cmp	data1, data2 )
CPU_BE( cset	result, ne )
CPU_BE( cneg	result, result, lo )
CPU_BE( ret )
CPU_BE( 1: )
	/* Re-compute the NUL-byte detection, using a byte-reversed value.*/
CPU_BE( rev	tmp3, data1 )
CPU_BE( sub	tmp1, tmp3, zeroones )
CPU_BE( orr	tmp2, tmp3, #REP8_7f )
CPU_BE( bic	has_nul, tmp1, tmp2 )
CPU_BE( rev	has_nul, has_nul )
CPU_BE( orr	syndrome, diff, has_nul )
	/*
	* The MS-non-zero bit of the syndrome marks either the first bit
	* that is different, or the top bit of the first zero byte.
	* Shifting left now will bring the critical information into the
	* top bits.
	*/
	clz	pos, syndrome
	lsl	data1, data1, pos
	lsl	data2, data2, pos
	/*
	* But we need to zero-extend (char is unsigned) the value and then
	* perform a signed 32-bit subtraction.
	*/
	lsr	data1, data1, #56
	sub	result, data1, data2, lsr #56
	ret

.Lremain8:
	/* Limit % 8 == 0 => all bytes significant.  */
	ands	limit, limit, #7
	b.eq	.Lret0
.Ltiny8proc:
	ldrb	data1w, [src1], #1
	ldrb	data2w, [src2], #1
	subs	limit, limit, #1

	ccmp	data1w, #1, #0, ne  /* NZCV = 0b0000.  */
	ccmp	data1w, data2w, #0, cs  /* NZCV = 0b0000.  */
	b.eq	.Ltiny8proc
	sub	result, data1, data2
	ret
	/* Align the SRC1 to a dword by doing a bytewise compare and then do
	   the dword loop.  */
L(try_misaligned_words):
	cbz	count, L(src1_aligned)

	neg	count, count
	and	count, count, #7
	sub	limit, limit, count

L(page_end_loop):
	ldrb	data1w, [src1], #1
	ldrb	data2w, [src2], #1
	cmp	data1w, #1
	ccmp	data1w, data2w, #0, cs	/* NZCV = 0b0000.  */
	b.ne	L(done)
	subs	count, count, #1
	b.hi	L(page_end_loop)

	/* The following diagram explains the comparison of misaligned strings.
	   The bytes are shown in natural order. For little-endian, it is
	   reversed in the registers. The "x" bytes are before the string.
	   The "|" separates data that is loaded at one time.
	   src1     | a a a a a a a a | b b b c c c c c | . . .
	   src2     | x x x x x a a a   a a a a a b b b | c c c c c . . .

	   After shifting in each step, the data looks like this:
	                STEP_A              STEP_B              STEP_C
	   data1    a a a a a a a a     b b b c c c c c     b b b c c c c c
	   data2    a a a a a a a a     b b b 0 0 0 0 0     0 0 0 c c c c c

	   The bytes with "0" are eliminated from the syndrome via mask.

	   Align SRC2 down to 16 bytes. This way we can read 16 bytes at a
	   time from SRC2. The comparison happens in 3 steps. After each step
	   the loop can exit, or read from SRC1 or SRC2. */
L(src1_aligned):
	/* Calculate offset from 8 byte alignment to string start in bits. No
	   need to mask offset since shifts are ignoring upper bits. */
	lsl	offset, src2, #3
	bic	src2, src2, #0xf
	mov	mask, -1
	neg	neg_offset, offset
	ldr	data1, [src1], #8
	ldp	tmp1, tmp2, [src2], #16
	LS_BK	mask, mask, neg_offset
	and	neg_offset, neg_offset, #63	/* Need actual value for cmp later. */
	/* Skip the first compare if data in tmp1 is irrelevant. */
	tbnz	offset, 6, L(misaligned_mid_loop)

L(loop_misaligned):
	/* STEP_A: Compare full 8 bytes when there is enough data from SRC2.*/
	LS_FW	data2, tmp1, offset
	LS_BK	tmp1, tmp2, neg_offset
	subs	limit, limit, #8
	orr	data2, data2, tmp1	/* 8 bytes from SRC2 combined from two regs.*/
	sub	has_nul, data1, zeroones
	eor	diff, data1, data2	/* Non-zero if differences found.  */
	orr	tmp3, data1, #REP8_7f
	csinv	endloop, diff, xzr, hi	/* If limit, set to all ones. */
	bic	has_nul, has_nul, tmp3	/* Non-zero if NUL byte found in SRC1. */
	orr	tmp3, endloop, has_nul
	cbnz	tmp3, L(full_check)

	ldr	data1, [src1], #8
L(misaligned_mid_loop):
	/* STEP_B: Compare first part of data1 to second part of tmp2. */
	LS_FW	data2, tmp2, offset
#ifdef __AARCH64EB__
	/* For big-endian we do a byte reverse to avoid carry-propagation
	problem described above. This way we can reuse the has_nul in the
	next step and also use syndrome value trick at the end. */
	rev	tmp3, data1
	#define data1_fixed tmp3
#else
	#define data1_fixed data1
#endif
	sub	has_nul, data1_fixed, zeroones
	orr	tmp3, data1_fixed, #REP8_7f
	eor	diff, data2, data1	/* Non-zero if differences found.  */
	bic	has_nul, has_nul, tmp3	/* Non-zero if NUL terminator.  */
#ifdef __AARCH64EB__
	rev	has_nul, has_nul
#endif
	cmp	limit, neg_offset, lsr #3
	orr	syndrome, diff, has_nul
	bic	syndrome, syndrome, mask	/* Ignore later bytes. */
	csinv	tmp3, syndrome, xzr, hi	/* If limit, set to all ones. */
	cbnz	tmp3, L(syndrome_check)

	/* STEP_C: Compare second part of data1 to first part of tmp1. */
	ldp	tmp1, tmp2, [src2], #16
	cmp	limit, #8
	LS_BK	data2, tmp1, neg_offset
	eor	diff, data2, data1	/* Non-zero if differences found.  */
	orr	syndrome, diff, has_nul
	and	syndrome, syndrome, mask	/* Ignore earlier bytes. */
	csinv	tmp3, syndrome, xzr, hi	/* If limit, set to all ones. */
	cbnz	tmp3, L(syndrome_check)

	ldr	data1, [src1], #8
	sub	limit, limit, #8
	b	L(loop_misaligned)

#ifdef	__AARCH64EB__
L(syndrome_check):
	clz	pos, syndrome
	cmp	pos, limit, lsl #3
	b.lo	L(end_quick)
#endif

L(ret0):
	mov	result, #0
	ret
SYM_FUNC_END_PI(strncmp)
EXPORT_SYMBOL_NOKASAN(strncmp)
