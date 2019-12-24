/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ez/lzma/mf.c - LZMA matchfinder
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 */
#include <stdlib.h>
#include <ez/unaligned.h>
#include <ez/bitops.h>
#include "mf.h"
#include "bytehash.h"

#define LZMA_HASH_2_SZ		(1U << 8)
#define LZMA_HASH_3_SZ		(1U << 16)

#define LZMA_HASH_3_BASE	(LZMA_HASH_2_SZ)
#define LZMA_HASH_4_BASE	(LZMA_HASH_2_SZ + LZMA_HASH_3_SZ)

static inline uint32_t mt_calc_hash_2(const uint8_t cur[2])
{
	return (crc32_byte_hashtable[cur[0]] ^ cur[1]) & (LZMA_HASH_2_SZ - 1);
}

static inline uint32_t mt_calc_hash_3(const uint8_t cur[3],
				      const uint32_t hash_2_value)
{
	return (hash_2_value ^ (cur[2] << 8)) & (LZMA_HASH_3_SZ - 1);
}

static inline uint32_t mt_calc_hash_4(const uint8_t cur[4], unsigned int nbits)
{
	const uint32_t golden_ratio_32 = 0x61C88647;

	return (get_unaligned_le32(cur) * golden_ratio_32) >> (32 - nbits);
}

/* Mark the current byte as processed from point of view of the match finder. */
static void mf_move(struct lzma_mf *mf)
{
	if (mf->chaincur + 1 > mf->max_distance)
		mf->chaincur = 0;
	else
		++mf->chaincur;

	++mf->cur;
	DBG_BUGON(mf->buffer + mf->cur > mf->iend);
}

static void mf_nop(struct lzma_mf *mf)
{
	++mf->cur;
	DBG_BUGON(mf->buffer + mf->cur > mf->iend);

	++mf->nops;
}

static unsigned int lzma_mf_do_hc4_find(struct lzma_mf *mf,
					struct lzma_match *matches)
{
	const uint32_t cur = mf->cur;
	const uint8_t *ip = mf->buffer + cur;
	const uint32_t pos = cur + mf->offset;
	const uint32_t nice_len = mf->nice_len;
	const uint8_t *ilimit =
		ip + nice_len <= mf->iend ? ip + nice_len : mf->iend;

	const uint32_t hash_2 = mt_calc_hash_2(ip);
	const uint32_t delta2 = pos - mf->hash[hash_2];
	const uint32_t hash_3 = mt_calc_hash_3(ip, hash_2);
	const uint32_t hash_value = mt_calc_hash_4(ip, mf->hashbits);
	uint32_t cur_match = mf->hash[LZMA_HASH_4_BASE + hash_value];
	unsigned int count, bestlen, depth;
	uint32_t delta3;
	const uint8_t *matchend;

	mf->hash[hash_2] = pos;
	mf->hash[LZMA_HASH_3_BASE + hash_3] = pos;
	mf->hash[LZMA_HASH_4_BASE + hash_value] = pos;

	count = 0;
	bestlen = 0;

	/* check the 2-byte match */
	if (delta2 <= mf->max_distance && *(ip - delta2) == *ip) {
		matchend = ez_memcmp(ip + 2, ip - delta2 + 2, ilimit);

		bestlen = matchend - ip;
		matches[0].len = bestlen;
		matches[0].dist = delta2;
		count = 1;
		if (matchend >= ilimit)
			goto out;
	}

	delta3 = pos - mf->hash[LZMA_HASH_3_BASE + hash_3];

	/* check the 3-byte match */
	if (delta2 != delta3 && delta3 <= mf->max_distance &&
	    *(ip - delta3) == *ip) {
		matchend = ez_memcmp(ip + 3, ip - delta3 + 3, ilimit);

		if (matchend - ip > bestlen) {
			bestlen = matchend - ip;
			matches[count].len = bestlen;
			matches[count].dist = delta3;
			++count;
			if (matchend >= ilimit)
				goto out;
		}
	}

	/* check 4 or more byte matches, traversal the whole hash chain */
	for (depth = mf->depth; depth; --depth) {
		const uint32_t delta = pos - cur_match;
		const uint8_t *match = ip - delta;
		uint32_t nextcur;

		if (delta > mf->max_distance)
			break;

		nextcur = (mf->chaincur >= delta ? mf->chaincur - delta :
			   mf->max_distance + 1 + mf->chaincur - delta);
		cur_match = mf->chain[nextcur];

		if (get_unaligned32(match) == get_unaligned32(ip) &&
		    match[bestlen + 1] == match[bestlen + 1]) {
			matchend = ez_memcmp(ip + 4, match + 4, ilimit);

			if (matchend - ip <= bestlen)
				continue;

			bestlen = matchend - ip;
			matches[count].len = bestlen;
			matches[count].dist = delta;
			++count;
			if (matchend >= ilimit)
				break;
		}
	}

out:
	mf->chain[mf->chaincur] = cur_match;
	mf_move(mf);
	++mf->lookahead;
	return count;
}

unsigned int lzma_mf_find(struct lzma_mf *mf, struct lzma_match *matches)
{
	const uint8_t *ip = mf->buffer + mf->cur;
	const uint8_t *iend = max((const uint8_t *)mf->iend,
				  ip + MATCH_LEN_MAX);
	const unsigned int count = lzma_mf_hc4_find(mf, matches);
	unsigned int i;

	i = count;
	while (i) {
		const uint8_t *cur = ip + matches[i].len;

		--i;
		if (matches[i].len < mf->nice_len || cur >= iend)
			break;

		/* extend the candicated match as far as it can */
		matches[i].len = ez_memcmp(cur, cur - matches[i].dist,
					   iend) - ip;
	}
	return count;
}

/* aka. lzma_mf_hc4_skip */
void lzma_mf_skip(struct lzma_mf *mf, unsigned int bytetotal)
{
	const unsigned int hashbits = mf->hashbits;
	unsigned int bytecount = 0;

	do {
		const uint8_t *ip = mf->buffer + mf->cur;
		uint32_t pos, hash_2, hash_3, hash_value;

		if (mf->iend - ip < 4) {
			mf_nop(mf);
			continue;
		}

		pos = mf->cur + mf->offset;

		hash_2 = mt_calc_hash_2(ip);
		mf->hash[hash_2] = pos;

		hash_3 = mt_calc_hash_3(ip, hash_2);
		mf->hash[LZMA_HASH_3_BASE + hash_3] = pos;

		hash_value = mt_calc_hash_4(ip, hashbits);

		mf->chain[mf->chaincur] =
			mf->hash[LZMA_HASH_4_BASE + hash_value];
		mf->hash[LZMA_HASH_4_BASE + hash_value] = pos;

		mf_move(mf);
	} while (++bytecount < bytetotal);

	mf->lookahead += bytecount;
}

void lzma_mf_fill(struct lzma_mf *mf, const uint8_t *in, unsigned int size)
{
	DBG_BUGON(mf->buffer + mf->cur > mf->iend);

	/* move the sliding window in advance if needed */
	//if (mf->cur >= mf->size - mf->keep_size_after)
	//	move_window(mf);

	memcpy(mf->iend, in, size);
	mf->iend += size;
}

int lzma_mf_reset(struct lzma_mf *mf, unsigned int dictsize)
{
	unsigned int new_hashbits;

	if (!dictsize) {
		return -EINVAL;
	} if (dictsize < UINT16_MAX) {
		new_hashbits = 16;
	/* most significant set bit + 1 of distsize to derive hashbits */
	} else {
		const unsigned int hs = fls(dictsize);

		new_hashbits = hs - (1 << (hs - 1) == dictsize);
		if (new_hashbits > 31)
			new_hashbits = 31;
	}

	if (new_hashbits != mf->hashbits ||
	    mf->max_distance != dictsize - 1) {
		if (mf->hash)
			free(mf->hash);
		if (mf->chain)
			free(mf->chain);

		mf->hashbits = 0;
		mf->hash = calloc(LZMA_HASH_4_BASE + (1 << new_hashbits),
				  sizeof(mf->hash[0]));
		if (!mf->hash)
			return -ENOMEM;

		mf->chain = malloc(sizeof(mf->chain[0]) * (dictsize - 1));
		if (!mf->chain) {
			free(mf->hash);
			return -ENOMEM;
		}
		mf->hashbits = new_hashbits;
	}

	mf->max_distance = dictsize - 1;
	mf->offset = 0;
	mf->cur = 0;
	mf->nice_len = 32;
	mf->lookahead = 0;
	mf->chaincur = 0;
	mf->depth = 4;
	return 0;
}

