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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Fletcher Checksums
 * ------------------
 *
 * ZFS's 2nd and 4th order Fletcher checksums are defined by the following
 * recurrence relations:
 *
 *	a  = a    + f
 *	 i    i-1    i-1
 *
 *	b  = b    + a
 *	 i    i-1    i
 *
 *	c  = c    + b		(fletcher-4 only)
 *	 i    i-1    i
 *
 *	d  = d    + c		(fletcher-4 only)
 *	 i    i-1    i
 *
 * Where
 *	a_0 = b_0 = c_0 = d_0 = 0
 * and
 *	f_0 .. f_(n-1) are the input data.
 *
 * Using standard techniques, these translate into the following series:
 *
 *	     __n_			     __n_
 *	     \   |			     \   |
 *	a  =  >     f			b  =  >     i * f
 *	 n   /___|   n - i		 n   /___|	 n - i
 *	     i = 1			     i = 1
 *
 *
 *	     __n_			     __n_
 *	     \   |  i*(i+1)		     \   |  i*(i+1)*(i+2)
 *	c  =  >     ------- f		d  =  >     ------------- f
 *	 n   /___|     2     n - i	 n   /___|	  6	   n - i
 *	     i = 1			     i = 1
 *
 * For fletcher-2, the f_is are 64-bit, and [ab]_i are 64-bit accumulators.
 * Since the additions are done mod (2^64), errors in the high bits may not
 * be noticed.  For this reason, fletcher-2 is deprecated.
 *
 * For fletcher-4, the f_is are 32-bit, and [abcd]_i are 64-bit accumulators.
 * A conservative estimate of how big the buffer can get before we overflow
 * can be estimated using f_i = 0xffffffff for all i:
 *
 * % bc
 *  f=2^32-1;d=0; for (i = 1; d<2^64; i++) { d += f*i*(i+1)*(i+2)/6 }; (i-1)*4
 * 2264
 *  quit
 * %
 *
 * So blocks of up to 2k will not overflow.  Our largest block size is
 * 128k, which has 32k 4-byte words, so we can compute the largest possible
 * accumulators, then divide by 2^64 to figure the max amount of overflow:
 *
 * % bc
 *  a=b=c=d=0; f=2^32-1; for (i=1; i<=32*1024; i++) { a+=f; b+=a; c+=b; d+=c }
 *  a/2^64;b/2^64;c/2^64;d/2^64
 * 0
 * 0
 * 1365
 * 11186858
 *  quit
 * %
 *
 * So a and b cannot overflow.  To make sure each bit of input has some
 * effect on the contents of c and d, we can look at what the factors of
 * the coefficients in the equations for c_n and d_n are.  The number of 2s
 * in the factors determines the lowest set bit in the multiplier.  Running
 * through the cases for n*(n+1)/2 reveals that the highest power of 2 is
 * 2^14, and for n*(n+1)*(n+2)/6 it is 2^15.  So while some data may overflow
 * the 64-bit accumulators, every bit of every f_i effects every accumulator,
 * even for 128k blocks.
 *
 * If we wanted to make a stronger version of fletcher4 (fletcher4c?),
 * we could do our calculations mod (2^32 - 1) by adding in the carries
 * periodically, and store the number of carries in the top 32-bits.
 *
 * --------------------
 * Checksum Performance
 * --------------------
 *
 * There are two interesting components to checksum performance: cached and
 * uncached performance.  With cached data, fletcher-2 is about four times
 * faster than fletcher-4.  With uncached data, the performance difference is
 * negligible, since the cost of a cache fill dominates the processing time.
 * Even though fletcher-4 is slower than fletcher-2, it is still a pretty
 * efficient pass over the data.
 *
 * In normal operation, the data which is being checksummed is in a buffer
 * which has been filled either by:
 *
 *	1. a compression step, which will be mostly cached, or
 *	2. a bcopy() or copyin(), which will be uncached (because the
 *	   copy is cache-bypassing).
 *
 * For both cached and uncached data, both fletcher checksums are much faster
 * than sha-256, and slower than 'off', which doesn't touch the data at all.
 */

#include "stdafx.h"
#include "Hash.h"

static void fletcher_2(const void* buf, uint64_t size, cksum_t* zcp)
{
	const uint64_t* ip = (const uint64_t*)buf;
	const uint64_t* ipend = ip + (size / sizeof(uint64_t));

	uint64_t a0, b0, a1, b1;

	for(a0 = b0 = a1 = b1 = 0; ip < ipend; ip += 2)
	{
		a0 += ip[0];
		a1 += ip[1];
		b0 += a0;
		b1 += a1;
	}

	zcp->set(a0, a1, b0, b1);
}

static void fletcher_2_sse2(const void* buf, uint64_t size, cksum_t* zcp) // ~25% faster
{
	const __m128i* p = (__m128i*)buf;
	
	__m128i a = _mm_setzero_si128();
	__m128i b = _mm_setzero_si128();

	for(size_t i = 0, j = (size_t)(size >> 4); i < j; i++)
	{
		__m128i r = _mm_load_si128(&p[i]);

		a = _mm_add_epi64(a, r);
		b = _mm_add_epi64(b, a);
	}

	zcp->set(a.m128i_u64[0], a.m128i_u64[1], b.m128i_u64[0], b.m128i_u64[1]);
}

static void fletcher_4(const void* buf, uint64_t size, cksum_t* zcp)
{
	const uint32_t* ip = (const uint32_t*)buf;
	const uint32_t* ipend = ip + (size / sizeof(uint32_t));

	uint64_t a, b, c, d;

	for(a = b = c = d = 0; ip < ipend; ip++)
	{
		a += ip[0];
		b += a;
		c += b;
		d += c;
	}

	zcp->set(a, b, c, d);
}

static void sha256(const void* buf, uint64_t size, cksum_t* zcp)
{
	HCRYPTPROV hCryptProv; 
	HCRYPTHASH hHash; 

	if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) 
	{
		if(CryptCreateHash(hCryptProv, CALG_SHA_256, 0, 0, &hHash)) 
		{
			if(CryptHashData(hHash, (BYTE*)buf, (DWORD)size, 0))
			{
				DWORD dwHashLen = sizeof(cksum_t);

				if(CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)zcp, &dwHashLen, 0))
				{
					zcp->word[0] = BSWAP_64(zcp->word[0]);
					zcp->word[1] = BSWAP_64(zcp->word[1]);
					zcp->word[2] = BSWAP_64(zcp->word[2]);
					zcp->word[3] = BSWAP_64(zcp->word[3]);
				}
			}
		}
	}

	CryptDestroyHash(hHash); 
	CryptReleaseContext(hCryptProv, 0); 
}

typedef void (*cksum_func_t)(const void* buf, uint64_t size, cksum_t* zcp);

static struct cksum_func_struct
{
	cksum_func_t f[ZIO_CHECKSUM_FUNCTIONS];

	struct cksum_func_struct()
	{
		f[ZIO_CHECKSUM_INHERIT] = NULL;
		f[ZIO_CHECKSUM_ON] = fletcher_2;
		f[ZIO_CHECKSUM_OFF] = NULL;
		f[ZIO_CHECKSUM_LABEL] = sha256;
		f[ZIO_CHECKSUM_GANG_HEADER] = sha256;
		f[ZIO_CHECKSUM_ZILOG] = fletcher_2;
		f[ZIO_CHECKSUM_FLETCHER_2] = fletcher_2;
		f[ZIO_CHECKSUM_FLETCHER_4] = fletcher_4;
		f[ZIO_CHECKSUM_SHA256] = sha256;
		f[ZIO_CHECKSUM_ZILOG2] = fletcher_4;

		int buff[4];

		__cpuid(buff, 0x80000000);

		if((DWORD)buff[0] >= 1)
		{
			__cpuid(buff, 1);

			if(buff[3] & (1 << 26)) // SSE2
			{
				f[ZIO_CHECKSUM_ON] = fletcher_2_sse2;
				f[ZIO_CHECKSUM_ZILOG] = fletcher_2_sse2;
				f[ZIO_CHECKSUM_FLETCHER_2] = fletcher_2_sse2;
			}
		}
	}

} s_cksum_func;

void ZFS::hash(const void* buf, uint64_t size, cksum_t* zcp, uint8_t cksum_type)
{
	memset(zcp, 0, sizeof(*zcp));

	if(cksum_type < sizeof(s_cksum_func.f) / sizeof(s_cksum_func.f[0]))
	{
		cksum_func_t f = s_cksum_func.f[cksum_type];

		if(f != NULL)
		{
			f(buf, size, zcp);
		}
	}
}
