/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/include/ez/util.h
 *
 * Copyright (C) 2019-2020 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_UTIL_H
#define __EZ_UTIL_H

#include "defs.h"

static inline const uint8_t *ez_memcmp(const void *ptr1, const void *ptr2,
				       const void *buf1end)
{
	const uint8_t *buf1 = ptr1;
	const uint8_t *buf2 = ptr2;

	for (; buf1 != buf1end; ++buf1, ++buf2)
		if (*buf1 != *buf2)
			break;
	return buf1;
}

#endif

