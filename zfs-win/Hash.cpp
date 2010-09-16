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

/*
#define	Ch(x, y, z)	((z) ^ ((x) & ((y) ^ (z))))
#define	Maj(x, y, z)	(((x) & (y)) ^ ((z) & ((x) ^ (y))))
#define	Rot32(x, s)	(((x) >> s) | ((x) << (32 - s)))
#define	SIGMA0(x)	(Rot32(x, 2) ^ Rot32(x, 13) ^ Rot32(x, 22))
#define	SIGMA1(x)	(Rot32(x, 6) ^ Rot32(x, 11) ^ Rot32(x, 25))
#define	sigma0(x)	(Rot32(x, 7) ^ Rot32(x, 18) ^ ((x) >> 3))
#define	sigma1(x)	(Rot32(x, 17) ^ Rot32(x, 19) ^ ((x) >> 10))

static const uint32_t SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void SHA256Transform(uint32_t *H, const uint8_t *cp)
{
	uint32_t a, b, c, d, e, f, g, h, t, T1, T2, W[64];

	for (t = 0; t < 16; t++, cp += 4)
		W[t] = (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3];

	for (t = 16; t < 64; t++)
		W[t] = sigma1(W[t - 2]) + W[t - 7] +
		    sigma0(W[t - 15]) + W[t - 16];

	a = H[0]; b = H[1]; c = H[2]; d = H[3];
	e = H[4]; f = H[5]; g = H[6]; h = H[7];

	for (t = 0; t < 64; t++) {
		T1 = h + SIGMA1(e) + Ch(e, f, g) + SHA256_K[t] + W[t];
		T2 = SIGMA0(a) + Maj(a, b, c);
		h = g; g = f; f = e; e = d + T1;
		d = c; c = b; b = a; a = T1 + T2;
	}

	H[0] += a; H[1] += b; H[2] += c; H[3] += d;
	H[4] += e; H[5] += f; H[6] += g; H[7] += h;
}

static void sha256(const void *buf, uint64_t size, cksum_t *zcp)
{
	uint32_t H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	uint8_t pad[128];
	int padsize = size & 63;
	int i;

	for (i = 0; i < size - padsize; i += 64)
		SHA256Transform(H, (uint8_t *)buf + i);

	for (i = 0; i < padsize; i++)
		pad[i] = ((uint8_t *)buf)[i];

	for (pad[padsize++] = 0x80; (padsize & 63) != 56; padsize++)
		pad[padsize] = 0;

	for (i = 0; i < 8; i++)
		pad[padsize++] = (size << 3) >> (56 - 8 * i);

	for (i = 0; i < padsize; i += 64)
		SHA256Transform(H, pad + i);

	zcp->set(
		(uint64_t)H[0] << 32 | H[1],
	    (uint64_t)H[2] << 32 | H[3],
	    (uint64_t)H[4] << 32 | H[5],
	    (uint64_t)H[6] << 32 | H[7]);
}
*/
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
