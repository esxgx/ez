/* SPDX-License-Identifier: Unlicense */
/*
 * lzma/rc_encoder.h - Range code encoder
 *
 * Copyright (C) 2019 Gao Xiang <hsiangkao@aol.com>
 *
 * Authors: Igor Pavlov <http://7-zip.org/>
 *          Lasse Collin <lasse.collin@tukaani.org>
 *          Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_LZMA_RC_ENCODER_H
#define __EZ_LZMA_RC_ENCODER_H

#include "rc_common.h"

/*
 * Maximum number of symbols that can be put pending into lzma_range_encoder
 * structure between calls to lzma_rc_encode(). For LZMA, 52+5 is enough
 * (match with big distance and length followed by range encoder flush).
 */
#define RC_SYMBOLS_MAX 58

#define RC_BIT_0	0
#define RC_BIT_1	1
#define RC_DIRECT_0	2
#define RC_DIRECT_1	3
#define RC_FLUSH	4

struct lzma_rc_encoder {
	uint64_t low;
	uint64_t extended_bytes;
	uint32_t range;
	uint8_t firstbyte;

	/* Number of symbols in the tables */
	uint8_t count;

	/* rc_encode()'s position in the tables */
	uint8_t pos;

	/* Symbols to encode (use uint8_t so can be in a single cacheline.) */
	uint8_t symbols[RC_SYMBOLS_MAX];

	/* Probabilities associated with RC_BIT_0 or RC_BIT_1 */
	probability *probs[RC_SYMBOLS_MAX];
};

static inline void rc_reset(struct lzma_rc_encoder *rc)
{
	*rc = (struct lzma_rc_encoder) {
		.range = UINT32_MAX,
		/* .firstbyte = 0, */
	};
}

static inline void rc_bit(struct lzma_rc_encoder *rc,
			  probability *prob, uint32_t bit)
{
	rc->symbols[rc->count] = bit;
	rc->probs[rc->count] = prob;
	++rc->count;
}

static inline void rc_bittree(struct lzma_rc_encoder *rc,
			      probability *probs, uint32_t nbits,
			      uint32_t symbol)
{
	uint32_t model_index = 1;

	do {
		const uint32_t bit = (symbol >> --nbits) & 1;

		rc_bit(rc, &probs[model_index], bit);
		model_index = (model_index << 1) + bit;
	} while (nbits);
}

static inline void rc_bittree_reverse(struct lzma_rc_encoder *rc,
				      probability *probs,
				      uint32_t nbits, uint32_t symbol)
{
	uint32_t model_index = 1;

	do {
		const uint32_t bit = symbol & 1;

		symbol >>= 1;
		rc_bit(rc, &probs[model_index], bit);
		model_index = (model_index << 1) + bit;
	} while (--nbits);
}

static inline void rc_direct(struct lzma_rc_encoder *rc,
			     uint32_t val, uint32_t nbits)
{
	do {
		rc->symbols[rc->count] = RC_DIRECT_0 + ((val >> --nbits) & 1);
		++rc->count;
	} while (nbits);
}


static inline void rc_flush(struct lzma_rc_encoder *rc)
{
	unsigned int i;

	for (i = 0; i < 5; ++i)
		rc->symbols[rc->count++] = RC_FLUSH;
}

static inline bool rc_shift_low(struct lzma_rc_encoder *rc,
				uint8_t **ppos, uint8_t *oend)
{
	if (rc->low >> 24 != UINT8_MAX) {
		const uint32_t carrybit = rc->low >> 32;

		DBG_BUGON(carrybit > 1);

		/* first or interrupted byte */
		if (unlikely(*ppos >= oend))
			return true;
		*(*ppos)++ = rc->firstbyte + carrybit;

		while (rc->extended_bytes) {
			--rc->extended_bytes;
			if (unlikely(*ppos >= oend)) {
				rc->firstbyte = UINT8_MAX;
				return true;
			}
			*(*ppos)++ = carrybit - 1;
		}
		rc->firstbyte = rc->low >> 24;
	} else {
		++rc->extended_bytes;
	}
	rc->low = (rc->low & 0x00FFFFFF) << RC_SHIFT_BITS;
	return false;
}

static inline bool rc_encode(struct lzma_rc_encoder *rc,
			     uint8_t **ppos, uint8_t *oend)
{
	DBG_BUGON(rc->count > RC_SYMBOLS_MAX);

	while (rc->pos < rc->count) {
		/* Normalize */
		if (rc->range < RC_TOP_VALUE) {
			if (rc_shift_low(rc, ppos, oend))
				return true;

			rc->range <<= RC_SHIFT_BITS;
		}

		/* Encode a bit */
		switch (rc->symbols[rc->pos]) {
		case RC_BIT_0: {
			probability prob = *rc->probs[rc->pos];

			rc->range = rc_bound(rc->range, prob);
			prob += (RC_BIT_MODEL_TOTAL - prob) >> RC_MOVE_BITS;
			*rc->probs[rc->pos] = prob;
			break;
		}

		case RC_BIT_1: {
			probability prob = *rc->probs[rc->pos];
			const uint32_t bound = rc_bound(rc->range, prob);

			rc->low += bound;
			rc->range -= bound;
			prob -= prob >> RC_MOVE_BITS;
			*rc->probs[rc->pos] = prob;
			break;
		}

		case RC_DIRECT_0:
			rc->range >>= 1;
			break;

		case RC_DIRECT_1:
			rc->range >>= 1;
			rc->low += rc->range;
			break;

		case RC_FLUSH:
			/* Prevent further normalizations */
			rc->range = UINT32_MAX;

			/* Flush the last five bytes (see rc_flush()) */
			do {
				if (rc_shift_low(rc, ppos, oend))
					return true;
			} while (++rc->pos < rc->count);

			/*
			 * Reset the range encoder so we are ready to continue
			 * encoding if we weren't finishing the stream.
			 */
			rc_reset(rc);
			return false;

		default:
			DBG_BUGON(1);
			break;
		}
		++rc->pos;
	}

	rc->count = 0;
	rc->pos = 0;
	return false;
}


static inline uint64_t rc_pending(const struct lzma_rc_encoder *rc)
{
	return rc->extended_bytes + 5;
}

#endif

