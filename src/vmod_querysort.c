/*-
 * Copyright (c) 2022 Ori Livneh <ori.livneh@gmail.com>
 * Copyright (c) 2010-2014 Varnish Software AS
 * All rights reserved.
 *
 * Author: Naren Venkataraman of Vimeo Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "cache/cache.h"

#include "vcc_if.h"

#define LOG(...)                                                   \
	do {                                                       \
		VSL(SLT_Debug, 0, "vmod-querysort: " __VA_ARGS__); \
	} while (0)


/*
 * Is this the end of a query parameter key? End is either '=' or '['. The
 * latter may be percent-encoded as %5B.
 */
static inline int
is_key_stop(const char *p, const char *end)
{
	if (p[0] == '=' || p[0] == '[') {
		return (1);
	} else if (p[0] == '%' && (end - p) >= 3 && p[1] == '5' && (p[2] == 'B' || p[2] == 'b')) {
		return (1);
	}
	return (0);
}

static int
compa(const void *a, const void *b)
{
	const char * const *pa = a;
	const char * const *pb = b;
	const char *a1, *b1;

	for (a1 = pa[0], b1 = pb[0]; a1 < pa[1] && b1 < pb[1]; a1++, b1++) {
		if (is_key_stop(a1, pa[1])) {
			if (is_key_stop(b1, pb[1])) {
				break;
			} else {
				return -1;
			}
		} else if (is_key_stop(b1, pb[1])) {
			return 1;
		} else if (*a1 != *b1) {
			return (*a1 - *b1);
		}
	}
	/* Keys are identical, so compare by address to preserve order. */
	return pa[0] - pb[0];
}


VCL_STRING v_matchproto_(td_std_querysort)
vmod_querysort(VRT_CTX, VCL_STRING url)
{
	const char *cq, *cu;
	char *p, *r;
	const char **pp;
	const char **pe;
	unsigned u;
	int np;
	int i;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

	if (url == NULL)
		return (NULL);

	/* Split :query from :url */
	cu = strchr(url, '?');
	if (cu == NULL)
		return (url);

	/* Spot single-param queries */
	cq = strchr(cu, '&');
	if (cq == NULL)
		return (url);

	r = WS_Copy(ctx->ws, url, -1);
	if (r == NULL)
		return (url);

	u = WS_ReserveLumps(ctx->ws, sizeof(const char **));

#if VRT_MAJOR_VERSION < 12U
	pp = (void*)WS_Front(ctx->ws);
#else
	pp = WS_Reservation(ctx->ws);
#endif

	if (u < 4) {
		WS_Release(ctx->ws, 0);
		WS_MarkOverflow(ctx->ws);
		return (url);
	}
	pe = pp + u;

	/* Collect params as pointer pairs */
	np = 0;
	pp[np++] = 1 + cu;
	for (cq = 1 + cu; *cq != '\0'; cq++) {
		if (*cq == '&') {
			if (pp + np + 3 > pe) {
				WS_Release(ctx->ws, 0);
				WS_MarkOverflow(ctx->ws);
				return (url);
			}
			pp[np++] = cq;
			/* Skip trivially empty params */
			while (cq[1] == '&')
				cq++;
			pp[np++] = cq + 1;
		}
	}
	pp[np++] = cq;
	assert(!(np & 1));

	qsort(pp, np / 2, sizeof(*pp) * 2, compa);

	/* Emit sorted params */
	p = 1 + r + (cu - url);
	cq = "";
	for (i = 0; i < np; i += 2) {
		/* Ignore any edge-case zero length params */
		if (pp[i + 1] == pp[i])
			continue;
		assert(pp[i + 1] > pp[i]);
		if (*cq)
			*p++ = *cq;
		memcpy(p, pp[i], pp[i + 1] - pp[i]);
		p += pp[i + 1] - pp[i];
		cq = "&";
	}
	*p = '\0';

	WS_Release(ctx->ws, 0);
	return (r);
}
