/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#include "stdafx.h"
#include "Compress.h"
#include "../zlib/zlib.h"

#define NBBY 8
#define	MATCH_BITS	6
#define	MATCH_MIN	3
#define	MATCH_MAX	((1 << MATCH_BITS) + (MATCH_MIN - 1))
#define	OFFSET_MASK	((1 << (16 - MATCH_BITS)) - 1)
#define	LEMPEL_SIZE	1024

/*
 * We keep our own copy of this algorithm for 3 main reasons:
 *	1. If we didn't, anyone modifying common/os/compress.c would
 *         directly break our on disk format
 *	2. Our version of lzjb does not have a number of checks that the
 *         common/os version needs and uses
 *	3. We initialize the lempel to ensure deterministic results,
 *	   so that identical blocks can always be deduplicated.
 * In particular, we are adding the "feature" that compress() can
 * take a destination buffer size and returns the compressed length, or the
 * source length if compression would overflow the destination buffer.
 */

size_t lzjb_compress(void* s_start, void* d_start, size_t s_len, size_t d_len, int n)
{
	uint8_t* src = (uint8_t*)s_start;
	uint8_t* dst = (uint8_t*)d_start;
	uint8_t* cpy;
	uint8_t* copymap;
	int copymask = 1 << (NBBY - 1);
	int mlen, offset, hash;
	uint16_t* hp;
	uint16_t lempel[LEMPEL_SIZE] = { 0 };

	while(src < (uint8_t*)s_start + s_len)
	{
		if((copymask <<= 1) == (1 << NBBY))
		{
			if(dst >= (uint8_t *)d_start + d_len - 1 - 2 * NBBY)
			{
				return s_len;
			}

			copymask = 1;
			copymap = dst;
			*dst++ = 0;
		}

		if(src > (uint8_t*)s_start + s_len - MATCH_MAX)
		{
			*dst++ = *src++;

			continue;
		}

		hash = (src[0] << 16) + (src[1] << 8) + src[2];
		hash += hash >> 9;
		hash += hash >> 5;
		hp = &lempel[hash & (LEMPEL_SIZE - 1)];
		offset = (intptr_t)(src - *hp) & OFFSET_MASK;
		*hp = (uint16_t)(uintptr_t)src;
		cpy = src - offset;

		if(cpy >= (uint8_t *)s_start && cpy != src && src[0] == cpy[0] && src[1] == cpy[1] && src[2] == cpy[2])
		{
			*copymap |= copymask;

			for(mlen = MATCH_MIN; mlen < MATCH_MAX; mlen++)
			{
				if(src[mlen] != cpy[mlen])
				{
					break;
				}
			}

			*dst++ = ((mlen - MATCH_MIN) << (NBBY - MATCH_BITS)) | (offset >> NBBY);
			*dst++ = (uint8_t)offset;
			src += mlen;
		}
		else
		{
			*dst++ = *src++;
		}
	}

	return dst - (uint8_t*)d_start;
}

int lzjb_decompress(void* s_start, void* d_start, size_t s_len, size_t d_len)
{
	uint8_t* src = (uint8_t*)s_start;
	uint8_t* dst = (uint8_t*)d_start;
	uint8_t* d_end = (uint8_t*)d_start + d_len;
	uint8_t* cpy;
	uint8_t copymap;
	int copymask = 1 << (NBBY - 1);

	while(dst < d_end)
	{
		if((copymask <<= 1) == (1 << NBBY))
		{
			copymask = 1;
			copymap = *src++;
		}

		if(copymap & copymask)
		{
			int mlen = (src[0] >> (NBBY - MATCH_BITS)) + MATCH_MIN;
			int offset = ((src[0] << NBBY) | src[1]) & OFFSET_MASK;

			src += 2;
			
			if((cpy = dst - offset) < (uint8_t*)d_start)
			{
				return -1;
			}

			while(--mlen >= 0 && dst < d_end)
			{
				*dst++ = *cpy++;
			}
		}
		else
		{
			*dst++ = *src++;
		}
	}

	return 0;
}

size_t gzip_compress(void* s_start, void* d_start, size_t s_len, size_t d_len, int n)
{
	size_t dstlen = d_len;

	ASSERT(d_len <= s_len);

	uLongf len = dstlen;

	if(compress2((Bytef*)d_start, &len, (Bytef*)s_start, s_len, n) != Z_OK)
	{
		if(d_len != s_len)
		{
			return s_len;
		}

		memcpy(s_start, d_start, s_len);

		return s_len;
	}

	dstlen = len;

	return dstlen;
}

int gzip_decompress(void* s_start, void* d_start, size_t s_len, size_t d_len)
{
	size_t dstlen = d_len;

	ASSERT(d_len >= s_len);

	uLongf len = dstlen;

	if(uncompress((Bytef*)d_start, &len, (Bytef*)s_start, s_len) != Z_OK)
	{
		return -1;
	}

	dstlen = len;

	return 0;
}

/*
 * Zero-length encoding.  This is a fast and simple algorithm to eliminate
 * runs of zeroes.  Each chunk of compressed data begins with a length byte, b.
 * If b < n (where n is the compression parameter) then the next b + 1 bytes
 * are literal values.  If b >= n then the next (256 - b + 1) bytes are zero.
 */

size_t zle_compress(void* s_start, void* d_start, size_t s_len, size_t d_len, int n)
{
	uint8_t* src = (uint8_t*)s_start;
	uint8_t* dst = (uint8_t*)d_start;
	uint8_t* s_end = src + s_len;
	uint8_t* d_end = dst + d_len;

	while(src < s_end && dst < d_end - 1)
	{
		uint8_t* first = src;
		uint8_t* len = dst++;

		if(src[0] == 0)
		{
			uint8_t* last = src + (256 - n);

			while(src < std::min<uint8_t*>(last, s_end) && src[0] == 0)
			{
				src++;
			}

			*len = src - first - 1 + n;
		}
		else
		{
			uint8_t* last = src + n;
			
			if(d_end - dst < n)
			{
				break;
			}

			while(src < std::min<uint8_t*>(last, s_end) - 1 && (src[0] | src[1]))
			{
				*dst++ = *src++;
			}

			if(src[0])
			{
				*dst++ = *src++;
			}

			*len = src - first - 1;
		}
	}

	return src == s_end ? dst - (uint8_t*)d_start : s_len;
}

int zle_decompress_64(void* s_start, void* d_start, size_t s_len, size_t d_len)
{
	const int n = 64;

	uint8_t* src = (uint8_t*)s_start;
	uint8_t* dst = (uint8_t*)d_start;
	uint8_t* s_end = src + s_len;
	uint8_t* d_end = dst + d_len;

	while(src < s_end && dst < d_end)
	{
		int len = 1 + *src++;

		if(len <= n)
		{
			while(len-- != 0)
			{
				*dst++ = *src++;
			}
		}
		else
		{
			len -= n;
		
			while(len-- != 0)
			{
				*dst++ = 0;
			}
		}
	}

	return dst == d_end ? 0 : -1;
}

int copy_decompress(void* s_start, void* d_start, size_t s_len, size_t d_len)
{
	ASSERT(s_len == d_len);

	size_t size = std::min<size_t>(s_len, d_len);

	memcpy(d_start, s_start, size);

	return size;
}

typedef int (*decompress_func_t)(void* s_start, void* d_start, size_t s_len, size_t d_len);

static decompress_func_t s_decompress_func[] = 
{
	NULL, // ZIO_COMPRESS_INHERIT
	lzjb_decompress, // ZIO_COMPRESS_ON
	copy_decompress, // ZIO_COMPRESS_OFF
	lzjb_decompress, // ZIO_COMPRESS_LZJB
	NULL, // ZIO_COMPRESS_EMPTY
	gzip_decompress, // ZIO_COMPRESS_GZIP_1
	gzip_decompress, // ZIO_COMPRESS_GZIP_2
	gzip_decompress, // ZIO_COMPRESS_GZIP_3
	gzip_decompress, // ZIO_COMPRESS_GZIP_4
	gzip_decompress, // ZIO_COMPRESS_GZIP_5
	gzip_decompress, // ZIO_COMPRESS_GZIP_6
	gzip_decompress, // ZIO_COMPRESS_GZIP_7
	gzip_decompress, // ZIO_COMPRESS_GZIP_8
	gzip_decompress, // ZIO_COMPRESS_GZIP_9
	zle_decompress_64, // ZIO_COMPRESS_ZLE
};

bool ZFS::decompress(void* src, void* dst, size_t psize, size_t lsize, uint8_t comp_type)
{
	if(comp_type < sizeof(s_decompress_func) / sizeof(s_decompress_func[0]))
	{
		decompress_func_t f = s_decompress_func[comp_type];

		if(f != NULL)
		{
			f(src, dst, psize, lsize);

			return true;
		}
	}

	return false;
}
