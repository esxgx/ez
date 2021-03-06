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
#include <stdio.h>

#define LZMA_HASH_2_SZ		(1U << 10)
#define LZMA_HASH_3_SZ		(1U << 16)

#define LZMA_HASH_3_BASE	(LZMA_HASH_2_SZ)
#define LZMA_HASH_4_BASE	(LZMA_HASH_2_SZ + LZMA_HASH_3_SZ)

static inline uint32_t mt_calc_dualhash(const uint8_t cur[2])
{
	return crc32_byte_hashtable[cur[0]] ^ cur[1];
}

static inline uint32_t mt_calc_hash_3(const uint8_t cur[3],
				      const uint32_t dualhash)
{
	return (dualhash ^ (cur[2] << 8)) & (LZMA_HASH_3_SZ - 1);
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

static unsigned int lzma_mf_do_hc4_find(struct lzma_mf *mf,
					struct lzma_match *matches)
{
	const uint32_t cur = mf->cur;
	const uint8_t *ip = mf->buffer + cur;
	const uint32_t pos = cur + mf->offset;
	const uint32_t nice_len = mf->nice_len;
	const uint8_t *ilimit =
		ip + nice_len < mf->iend ? ip + nice_len : mf->iend;

	const uint32_t dualhash = mt_calc_dualhash(ip);
	const uint32_t hash_2 = dualhash & (LZMA_HASH_2_SZ - 1);
	const uint32_t delta2 = pos - mf->hash[hash_2];
	const uint32_t hash_3 = mt_calc_hash_3(ip, dualhash);
	const uint32_t delta3 = pos - mf->hash[LZMA_HASH_3_BASE + hash_3];
	const uint32_t hash_value = mt_calc_hash_4(ip, mf->hashbits);
	uint32_t cur_match = mf->hash[LZMA_HASH_4_BASE + hash_value];
	unsigned int bestlen, depth;
	const uint8_t *matchend;
	struct lzma_match *mp;

	mf->hash[hash_2] = pos;
	mf->hash[LZMA_HASH_3_BASE + hash_3] = pos;
	mf->hash[LZMA_HASH_4_BASE + hash_value] = pos;
	mf->chain[mf->chaincur] = cur_match;

	mp = matches;
	bestlen = 0;

	/* check the 2-byte match */
	if (delta2 <= mf->max_distance && *(ip - delta2) == *ip) {
		matchend = ez_memcmp(ip + 2, ip - delta2 + 2, ilimit);

		bestlen = matchend - ip;
		*(mp++) = (struct lzma_match) { .len = bestlen,
						.dist = delta2 };

		printf("found match2: %d %d %d\n", mf->cur, delta2, bestlen);

		if (matchend >= ilimit)
			goto out;
	}

	/* check the 3-byte match */
	if (delta2 != delta3 && delta3 <= mf->max_distance &&
	    *(ip - delta3) == *ip) {
		matchend = ez_memcmp(ip + 3, ip - delta3 + 3, ilimit);

		if (matchend - ip > bestlen) {
			bestlen = matchend - ip;
			*(mp++) = (struct lzma_match) { .len = bestlen,
							.dist = delta3 };
			printf("found match3: %d %d %d\n", mf->cur, delta3, bestlen);

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
			*(mp++) = (struct lzma_match) { .len = bestlen,
							.dist = delta };

			printf("found match4: %d %d %d\n", mf->cur, delta, bestlen);

			if (matchend >= ilimit)
				break;
		}
	}

out:
	return mp - matches;
}

/* aka. lzma_mf_hc4_skip */
void lzma_mf_skip(struct lzma_mf *mf, unsigned int bytetotal)
{
	const unsigned int hashbits = mf->hashbits;
	unsigned int unhashedskip = mf->unhashedskip;
	unsigned int bytecount = 0;

	if (unhashedskip) {
		bytetotal += unhashedskip;
		mf->cur -= unhashedskip;
		mf->lookahead -= unhashedskip;
		mf->unhashedskip = 0;
	}

	if (unlikely(!bytetotal))
		return;

	do {
		const uint8_t *ip = mf->buffer + mf->cur;
		uint32_t pos, dualhash, hash_2, hash_3, hash_value;

		if (mf->iend - ip < 4) {
			unhashedskip = bytetotal - bytecount;

			mf->unhashedskip = unhashedskip;
			mf->cur += unhashedskip;
			break;
		}

		pos = mf->cur + mf->offset;

		dualhash = mt_calc_dualhash(ip);
		hash_2 = dualhash & (LZMA_HASH_2_SZ - 1);
		mf->hash[hash_2] = pos;

		hash_3 = mt_calc_hash_3(ip, dualhash);
		mf->hash[LZMA_HASH_3_BASE + hash_3] = pos;

		hash_value = mt_calc_hash_4(ip, hashbits);

		mf->chain[mf->chaincur] =
			mf->hash[LZMA_HASH_4_BASE + hash_value];
		mf->hash[LZMA_HASH_4_BASE + hash_value] = pos;

		mf_move(mf);
	} while (++bytecount < bytetotal);

	mf->lookahead += bytetotal;
}

static int lzma_mf_hc4_find(struct lzma_mf *mf,
			    struct lzma_match *matches, bool finish)
{
	int ret;

	if (mf->iend - &mf->buffer[mf->cur] < 4) {
		if (!finish)
			return -ERANGE;

		mf->eod = true;
		if (mf->buffer + mf->cur == mf->iend)
			return -ERANGE;
	}

	if (!mf->eod) {
		ret = lzma_mf_do_hc4_find(mf, matches);
	} else {
		ret = 0;
		/* ++mf->unhashedskip; */
		mf->unhashedskip = 0;	/* bypass all lzma_mf_skip(mf, 0); */
	}
	mf_move(mf);
	++mf->lookahead;
	return ret;
}

int lzma_mf_find(struct lzma_mf *mf, struct lzma_match *matches, bool finish)
{
	const uint8_t *ip = mf->buffer + mf->cur;
	const uint8_t *iend = max((const uint8_t *)mf->iend,
				  ip + MATCH_LEN_MAX);
	unsigned int i;
	int ret;

	/* if (mf->unhashedskip && !mf->eod) */
	if (mf->unhashedskip)
		lzma_mf_skip(mf, 0);

	ret = lzma_mf_hc4_find(mf, matches, finish);
	if (ret <= 0)
		return ret;

	i = ret;
	do {
		const uint8_t *cur = ip + matches[i].len;

		--i;
		if (matches[i].len < mf->nice_len || cur >= iend)
			break;

		/* extend the candicated match as far as it can */
		matches[i].len = ez_memcmp(cur, cur - matches[i].dist,
					   iend) - ip;
	} while (i);
	return ret;
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

int lzma_mf_reset(struct lzma_mf *mf, const struct lzma_mf_properties *p)
{
	const uint32_t dictsize = p->dictsize;
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
	/*
	 * Set the initial value as mf->max_distance + 1.
	 * This would avoid hash zero initialization.
	 */
	mf->offset = mf->max_distance + 1;

	mf->nice_len = p->nice_len;
	mf->depth = p->depth;

	mf->cur = 0;
	mf->lookahead = 0;
	mf->chaincur = 0;
	return 0;
}

