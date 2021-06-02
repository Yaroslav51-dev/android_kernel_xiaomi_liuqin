/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, Arm Limited.
 *
 * Adapted from the original at:
 * https://github.com/ARM-software/optimized-routines/blob/afd6244a1f8d9229/string/aarch64/memcpy.S
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/cache.h>

/*
 * Copy a buffer from src to dest (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - dest
 *	x1 - src
 *	x2 - n
 * Returns:
 *	x0 - dest
 */
	.macro ldrb1 reg, ptr, val
	ldrb  \reg, [\ptr], \val
	.endm

	.macro strb1 reg, ptr, val
	strb \reg, [\ptr], \val
	.endm

	.macro ldrh1 reg, ptr, val
	ldrh  \reg, [\ptr], \val
	.endm

	.macro strh1 reg, ptr, val
	strh \reg, [\ptr], \val
	.endm

	.macro ldr1 reg, ptr, val
	ldr \reg, [\ptr], \val
	.endm

	.macro str1 reg, ptr, val
	str \reg, [\ptr], \val
	.endm

	.macro ldp1 reg1, reg2, ptr, val
	ldp \reg1, \reg2, [\ptr], \val
	.endm

	.macro stp1 reg1, reg2, ptr, val
	stp \reg1, \reg2, [\ptr], \val
	.endm

SYM_FUNC_START_ALIAS(__memcpy)
SYM_FUNC_START_WEAK_PI(memcpy)
#include "copy_template.S"
	ret
SYM_FUNC_END_PI(memcpy)
EXPORT_SYMBOL(memcpy)
SYM_FUNC_END_ALIAS(__memcpy)
EXPORT_SYMBOL(__memcpy)
SYM_FUNC_END_ALIAS_PI(memmove)
EXPORT_SYMBOL(memmove)
SYM_FUNC_END_ALIAS(__memmove)
EXPORT_SYMBOL(__memmove)
