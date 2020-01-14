/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/include/ez/unaligned.h
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_UNALIGNED_H
#define __EZ_UNALIGNED_H

#include <stdint.h>

/*
 * __pack instructions are safer, but compiler specific, hence potentially
 * problematic for some compilers (workable on gcc, clang).
 */
static inline uint16_t get_unaligned16(const void *ptr)
{
	const struct { uint16_t v; } __attribute__((packed)) *unalign = ptr;

	return unalign->v;
}

static inline uint32_t get_unaligned32(const void *ptr)
{
	const struct { uint32_t v; } __attribute__((packed)) *unalign = ptr;

	return unalign->v;
}

static inline unsigned int __is_little_endian(void)
{
#ifdef __LITTLE_ENDIAN
	return 1;
#elif defined(__BIG_ENDIAN)
	return 0;
#else
	/* don't use static : performance detrimental */
	const union { uint32_t u; uint8_t c[4]; } one = { 1 };

	return one.c[0];
#endif
}

static inline uint32_t get_unaligned_le32(const void *ptr)
{
	if (!__is_little_endian()) {
		const uint8_t *p = (const uint8_t *)ptr;

		return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	}
	return get_unaligned32(ptr);
}

#endif

