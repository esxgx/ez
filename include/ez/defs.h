/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/include/ez/defs.h
 *
 * Copyright (C) 2019-2020 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_DEFS_H
#define __EZ_DEFS_H

#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

/*
 * ..and if you can't take the strict types, you can specify one yourself.
 * Or don't use min/max at all, of course.
 */
#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((arr)[0]))

#ifndef __OPTIMIZE__
#define BUILD_BUG_ON(condition)	((void)sizeof(char[1 - 2 * !!(condition)]))
#else
#define BUILD_BUG_ON(condition)	assert(condition)
#endif

#define BUG_ON(cond)	assert(!(cond))

#ifdef NDEBUG
#define DBG_BUGON(condition)	((void)(condition))
#else
#define DBG_BUGON(condition)	BUG_ON(condition)
#endif

#ifndef __maybe_unused
#define __maybe_unused		__attribute__((__unused__))
#endif

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

#endif

