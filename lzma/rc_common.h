/* SPDX-License-Identifier: Unlicense */
/*
 * lzma/rc_common.h - Common definitions for range coder
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 *
 * Authors: Igor Pavlov <http://7-zip.org/>
 *          Lasse Collin <lasse.collin@tukaani.org>
 *          Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_LZMA_RC_COMMON_H
#define __EZ_LZMA_RC_COMMON_H

/* Constants */
#define RC_SHIFT_BITS		8
#define RC_TOP_BITS		24
#define RC_TOP_VALUE		(1U << RC_TOP_BITS)
#define RC_BIT_MODEL_TOTAL_BITS	11
#define RC_BIT_MODEL_TOTAL	(1U << RC_BIT_MODEL_TOTAL_BITS)
#define RC_MOVE_BITS		5

/* Type definitions */

/*
 * Type of probabilities used with range coder
 *
 * This needs to be at least 12-bit, so uint16_t is a logical choice. However,
 * on some architecture and compiler combinations, a bigger type may give
 * better speed since the probability variables are accessed a lot.
 * On the other hand, bigger probability type increases cache footprint,
 * since there are 2 to 14 thousand probability variables in LZMA (assuming
 * the limit of lc + lp <= 4; with lc + lp <= 12 there would be about 1.5
 * million variables).
 *
 * I will stick unless some specific architectures are *much* faster (20-50%)
 * with uint32_t than uint16_t.
 */
typedef uint16_t probability;

static inline uint32_t rc_bound(uint32_t range, probability prob)
{
	return (range >> RC_BIT_MODEL_TOTAL_BITS) * prob;
}

#endif

