/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/include/ez/bitops.h
 *
 * Copyright (C) 2020 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_BITOPS_H
#define __EZ_BITOPS_H

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline int fls(unsigned int x)
{
	return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}

#endif

