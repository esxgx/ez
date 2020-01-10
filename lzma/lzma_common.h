/* SPDX-License-Identifier: Unlicense */
/*
 * lzma/lzma_common.h - Private definitions of LZMA encoder
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 ×
 * Authors: Igor Pavlov <http://7-zip.org/>
 *          Lasse Collin <lasse.collin@tukaani.org>
 *          Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_LZMA_LZMA_COMMON_H
#define __EZ_LZMA_LZMA_COMMON_H

#include <ez/defs.h>
#include <ez/unaligned.h>

/*
 * LZMA Matchlength
 */

/* Minimum length of a match is two bytes. */
#define MATCH_LEN_MIN 2

/*
 * Match length is encoded with 4, 5, or 10 bits.
 *
 * Length    Bits
 *    2-9     4 = (Choice = 0) + 3 bits
 *  10-17     5 = (Choice = 1) + (Choice2 = 0) + 3 bits
 * 18-273    10 = (Choice = 1) + (Choice2 = 1) + 8 bits
 */
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)

/*
 * Maximum length of a match is 273 which is a result
 * of the encoding described above.
 */
#define MATCH_LEN_MAX (MATCH_LEN_MIN + LEN_SYMBOLS - 1)

/*
 * LZMA remembers the four most recent match distances.
 * Reusing these distances tend to take less space than
 * re-encoding the actual distance value.
 */
#define REPS 4

#endif

