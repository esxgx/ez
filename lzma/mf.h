/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/lzma/mf.h - header file for LZMA matchfinder
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __LZMA_MF_H
#define __LZMA_MF_H

#include <ez/util.h>
#include "lzma_common.h"

/*
 * an array used used by the LZ-based encoder to hold
 * the length-distance pairs found by LZMA matchfinder.
 */
struct lzma_match {
	unsigned int len;
	unsigned int dist;
};

struct lzma_mf {
	/* pointer to buffer with data to be compressed */
	uint8_t *buffer;

	/* size of the whole LZMA matchbuffer */
	uint32_t size;

	uint32_t offset;

	/* indicate the next byte to run through the match finder */
	uint32_t cur;

	/* maximum length of a match that the matchfinder will try to find. */
	uint32_t nice_len;

	/* indicate the first byte that doesn't contain valid input data */
	uint8_t *iend;

	/* indicate the number of bytes still not encoded */
	uint32_t lookahead;

	/* LZ matchfinder hash chain representation */
	uint32_t *hash, *chain;

	/* indicate the next byte in chain (0 ~ max_distance) */
	uint32_t chaincur;
	uint8_t hashbits;

	/* maximum number of loops in the match finder */
	uint8_t depth;

#if 0
	/*
	 * maximum length of a match supported by the LZ-based encoder.
	 * if the longest match found by the match finder is nice_len,
	 * mf_find() tries to expand it up to match_len_max bytes.
	 */
	uint32_t match_len_max;
#endif
	uint32_t max_distance;

	/* the number of bytes unhashed, and wait to roll back later */
	uint32_t unhashedskip;

	bool eod;
};

int lzma_mf_find(struct lzma_mf *mf, struct lzma_match *matches, bool finish);
void lzma_mf_skip(struct lzma_mf *mf, unsigned int n);
void lzma_mf_fill(struct lzma_mf *mf, const uint8_t *in, unsigned int size);
int lzma_mf_reset(struct lzma_mf *mf, unsigned int dictsize);

#endif

