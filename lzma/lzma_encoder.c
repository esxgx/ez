/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/lzma/lzma_encoder.c
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 */
#include "lzma_common.h"
#include "mf.h"

struct lzma_encoder {
	struct lzma_mf mf;

	int32_t reps[REPS];

	struct {
		struct lzma_match matches[MATCH_LEN_MAX];
		unsigned int matches_count;
	} fast;
};

#define change_pair(smalldist, bigdist) (((bigdist) >> 7) > (smalldist))

static unsigned int lzma_get_optimum_fast(struct lzma_encoder *lzma,
					  uint32_t *back_res, uint32_t *len_res)
{
	struct lzma_mf *const mf = &lzma->mf;
	const uint32_t nice_len = mf->nice_len;

	struct lzma_match matches[MATCH_LEN_MAX + 1];
	unsigned int matches_count, i, nlits;
	unsigned int longest_match_length, longest_match_back;
	unsigned int best_replen, best_rep;
	const uint8_t *ip, *ilimit;

	if (!mf->lookahead) {
		matches_count = lzma_mf_find(mf, lzma->fast.matches);
	} else {
		matches_count = lzma->fast.matches_count;
	}

	ip = mf->buffer + mf->cur - mf->lookahead;

	/* no valid match found by matchfinder */
	if (!matches_count ||
	/* not enough input left to encode a match */
	   mf->iend - ip <= 2)
		goto out_literal;

	ilimit = (mf->iend <= ip + MATCH_LEN_MAX ?
		  mf->iend : ip + MATCH_LEN_MAX);

	/* look for all valid repeat matches */
	for (i = 0; i < REPS; ++i) {
		const uint8_t *const repp = ip - lzma->reps[i];
		uint32_t len;

		/* the first two bytes (MATCH_LEN_MIN == 2) do not match */
		if (get_unaligned16(ip) != get_unaligned16(repp))
			continue;

		len = ez_memcmp(ip + 2, repp + 2, ilimit) - ip;
		/* a repeated match at least nice_len, return it immediately */
		if (len >= nice_len) {
			*back_res = i;
			*len_res = len;
			lzma_mf_skip(mf, len - 1);
			return 0;
		}

		if (len > best_replen) {
			best_rep = i;
			best_replen = len;
		}
	}

	/*
	 * although we didn't find a long enough repeated match,
	 * the normal match is long enough to use directly.
	 */
	longest_match_length = lzma->fast.matches[matches_count - 1].len;
	longest_match_back = lzma->fast.matches[matches_count - 1].len;
	if (longest_match_length >= nice_len) {
		*back_res = longest_match_back;
		*len_res = longest_match_length;
		lzma_mf_skip(mf, longest_match_length - 1);
		return 0;
	}

	while (matches_count > 1) {
		const struct lzma_match *const victim =
			&lzma->fast.matches[matches_count - 2];

		/* only (longest_match_length - 1) would be considered */
		if (longest_match_length > victim->len + 1)
			break;

		if (!change_pair(victim->dist, longest_match_back))
			break;

		--matches_count;
		longest_match_length = victim->len;
		longest_match_back = victim->dist;
	}

	nlits = 0;
	while ((lzma->fast.matches_count =
		lzma_mf_find(mf, lzma->fast.matches))) {
		const struct lzma_match *const victim =
			&lzma->fast.matches[lzma->fast.matches_count - 1];

		if (victim->len + nlits + 1 < longest_match_length)
			break;

		if (victim->len + nlits + 1 == longest_match_length &&
		    !change_pair(victim->dist + nlits, longest_match_back))
			break;

		if (victim->len + nlits == longest_match_length &&
		    victim->dist + nlits >= longest_match_back)
			break;
		++nlits;
	}
	if (nlits) {
		*len_res = 0;
	} else {
		*back_res = REPS + longest_match_back;
		*len_res = longest_match_length;
		lzma_mf_skip(mf, longest_match_length - 2);
	}
	return nlits;
out_literal:
	*len_res = 0;
	return 1;
}

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	struct lzma_encoder lzmaenc = {0};
	unsigned int back_res = 0, len_res = 0;
	unsigned int nliterals;

	lzmaenc.mf.buffer = malloc(65536);
	lzmaenc.mf.iend = lzmaenc.mf.buffer + 65536;

	memcpy(lzmaenc.mf.buffer, "abcde", sizeof("abcde"));

	lzma_mf_reset(&lzmaenc.mf, 65536);
	nliterals = lzma_get_optimum_fast(&lzmaenc, &back_res, &len_res);
	printf("nlits %d (%d %d)\n", nliterals, back_res, len_res);
	nliterals = lzma_get_optimum_fast(&lzmaenc, &back_res, &len_res);
	printf("nlits %d (%d %d)\n", nliterals, back_res, len_res);
	nliterals = lzma_get_optimum_fast(&lzmaenc, &back_res, &len_res);
	printf("nlits %d (%d %d)\n", nliterals, back_res, len_res);
	nliterals = lzma_get_optimum_fast(&lzmaenc, &back_res, &len_res);
	printf("nlits %d (%d %d)\n", nliterals, back_res, len_res);

}


void lzma_encode(struct lzma_encoder *lzma,
		 const uint8_t *in, unsigned int size)
{

}

