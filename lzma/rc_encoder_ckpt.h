/* SPDX-License-Identifier: Unlicense */
/*
 * lzma/rc_encoder_ckpt.h - Range code encoder checkpoint
 *
 * Copyright (C) 2020 Gao Xiang <hsiangkao@aol.com>
 */
#ifndef __EZ_LZMA_RC_ENCODER_CKPT_H
#define __EZ_LZMA_RC_ENCODER_CKPT_H

#include "rc_encoder.h"

struct lzma_rc_ckpt {
	uint64_t low;
	uint64_t extended_bytes;
	uint32_t range;
	uint8_t firstbyte;
};

void rc_write_checkpoint(struct lzma_rc_encoder *rc, struct lzma_rc_ckpt *cp)
{
	DBG_BUGON(rc->count > 0);

	*cp = (struct lzma_rc_ckpt) { .low = rc->low,
				      .extended_bytes = rc->extended_bytes,
				      .range = rc->range,
				      .firstbyte = rc->firstbyte
	};
}

void rc_restore_checkpoint(struct lzma_rc_encoder *rc, struct lzma_rc_ckpt *cp)
{
	rc->low = cp->low;
	rc->extended_bytes = cp->extended_bytes;
	rc->range = cp->range;
	rc->firstbyte = cp->firstbyte;

	rc->pos = 0;
	rc->count = 0;
}

#endif

