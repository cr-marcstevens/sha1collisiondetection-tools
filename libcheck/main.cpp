/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iomanip>
#include <array>

#include <boost/nondet_random.hpp>
#include <boost/random.hpp>
#include <boost/timer.hpp>
#include <boost/progress.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/variance.hpp>

extern "C"
{
#include "sha1.h"
#include "ubc_check.h"

	void ubc_check_verify(const uint32_t W[80], uint32_t dvmask[DVMASKSIZE]);

	void nc_callback(uint64_t byteoffset, const uint32_t ihvin1[5], const uint32_t ihvin2[5], const uint32_t m1[80], const uint32_t m2[80])
	{
		unsigned i;
		printf("Detected near-collision block:\n");
		printf("IHVin1  = { 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x };\n", ihvin1[0], ihvin1[1], ihvin1[2], ihvin1[3], ihvin1[4]);
		printf("IHVin2  = { 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x };\n", ihvin2[0], ihvin2[1], ihvin2[2], ihvin2[3], ihvin2[4]);
		printf("MSGBLK1 = { 0x%08x", m1[0]);
		for (i = 1; i < 16; ++i)
			printf(", 0x%08x", m1[i]);
		printf(" };\n");
		printf("MSGBLK2 = { 0x%08x", m2[0]);
		for (i = 1; i < 16; ++i)
			printf(", 0x%08x", m2[i]);
		printf(" };\n");
	}

	// the SHA-1 context
	typedef struct {
		uint64_t total;
		uint32_t ihv[5];
		unsigned char buffer[64];
		int bigendian;
	} SHA1reg_CTX;

	#define LE32_TO_BE32(X) \
		{X = ((X << 8) & 0xFF00FF00) | ((X >> 8) & 0xFF00FF); X = (X << 16) | (X >> 16);}

	void swap_bytes(uint32_t val[16])
	{
		LE32_TO_BE32(val[0]);
        	LE32_TO_BE32(val[1]);
	        LE32_TO_BE32(val[2]);
        	LE32_TO_BE32(val[3]);
        	LE32_TO_BE32(val[4]);
        	LE32_TO_BE32(val[5]);
        	LE32_TO_BE32(val[6]);
        	LE32_TO_BE32(val[7]);
        	LE32_TO_BE32(val[8]);
        	LE32_TO_BE32(val[9]);
        	LE32_TO_BE32(val[10]);
        	LE32_TO_BE32(val[11]);
        	LE32_TO_BE32(val[12]);
        	LE32_TO_BE32(val[13]);
        	LE32_TO_BE32(val[14]);
        	LE32_TO_BE32(val[15]);
	}


#define Rotate_right(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define Rotate_left(x,n)  (((x)<<(n))|((x)>>(32-(n))))

#define sha1_f1(b,c,d) ((d)^((b)&((c)^(d))))
#define sha1_f2(b,c,d) ((b)^(c)^(d))
#define sha1_f3(b,c,d) (((b) & ((c)|(d))) | ((c)&(d)))
#define sha1_f4(b,c,d) ((b)^(c)^(d))

#define HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, m, t) \
	{ e += Rotate_left(a, 5) + sha1_f1(b,c,d) + 0x5A827999 + m[t]; b = Rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, m, t) \
	{ e += Rotate_left(a, 5) + sha1_f2(b,c,d) + 0x6ED9EBA1 + m[t]; b = Rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, m, t) \
	{ e += Rotate_left(a, 5) + sha1_f3(b,c,d) + 0x8F1BBCDC + m[t]; b = Rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, m, t) \
	{ e += Rotate_left(a, 5) + sha1_f4(b,c,d) + 0xCA62C1D6 + m[t]; b = Rotate_left(b, 30); }

void sha1_compression(uint32_t ihv[5], const uint32_t m[16])
{
	uint32_t W[80];
	uint32_t a,b,c,d,e;
	unsigned i;

	memcpy(W, m, 16 * 4);
	for (i = 16; i < 80; ++i)
		W[i] = Rotate_left(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);

	a = ihv[0]; b = ihv[1]; c = ihv[2]; d = ihv[3]; e = ihv[4];

	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 0);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 1);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 2);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 3);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 4);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 5);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 6);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 7);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 8);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 9);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 10);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 11);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 12);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 13);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 14);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 15);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 16);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 17);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 18);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 19);

	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 20);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 21);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 22);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 23);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 24);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 25);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 26);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 27);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 28);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 29);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 30);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 31);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 32);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 33);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 34);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 35);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 36);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 37);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 38);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 39);

	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 40);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 41);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 42);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 43);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 44);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 45);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 46);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 47);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 48);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 49);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 50);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 51);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 52);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 53);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 54);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 55);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 56);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 57);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 58);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 59);

	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 60);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 61);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 62);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 63);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 64);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 65);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 66);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 67);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 68);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 69);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 70);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 71);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 72);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 73);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 74);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 75);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 76);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 77);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 78);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 79);

	ihv[0] += a; ihv[1] += b; ihv[2] += c; ihv[3] += d; ihv[4] += e;
}



void sha1_compression_W(uint32_t ihv[5], const uint32_t W[80])
{
	uint32_t a = ihv[0], b = ihv[1], c = ihv[2], d = ihv[3], e = ihv[4];

	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 0);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 1);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 2);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 3);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 4);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 5);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 6);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 7);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 8);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 9);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 10);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 11);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 12);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 13);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 14);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 15);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 16);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 17);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 18);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 19);

	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 20);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 21);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 22);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 23);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 24);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 25);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 26);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 27);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 28);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 29);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 30);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 31);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 32);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 33);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 34);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 35);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 36);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 37);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 38);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 39);

	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 40);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 41);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 42);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 43);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 44);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 45);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 46);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 47);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 48);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 49);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 50);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 51);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 52);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 53);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 54);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 55);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 56);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 57);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 58);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 59);

	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 60);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 61);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 62);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 63);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 64);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 65);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 66);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 67);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 68);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 69);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 70);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 71);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 72);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 73);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 74);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 75);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 76);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 77);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 78);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 79);

	ihv[0] += a; ihv[1] += b; ihv[2] += c; ihv[3] += d; ihv[4] += e;
}

	void SHA1reg_Init(SHA1reg_CTX* ctx)
	{
		static const union { unsigned char bytes[4]; uint32_t value; } endianness = { { 0, 1, 2, 3 } };
		static const uint32_t littleendian = 0x03020100;
		ctx->total = 0;
		ctx->ihv[0] = 0x67452301;
		ctx->ihv[1] = 0xEFCDAB89;
		ctx->ihv[2] = 0x98BADCFE;
		ctx->ihv[3] = 0x10325476;
		ctx->ihv[4] = 0xC3D2E1F0;
		ctx->bigendian = (endianness.value != littleendian);
	}

	void SHA1reg_Update(SHA1reg_CTX* ctx, const char* buf, unsigned len)
	{
		unsigned left, fill;
		if (len == 0)
			return;

		left = ctx->total & 63;
		fill = 64 - left;

		if (left && len >= fill)
		{
			ctx->total += fill;
			memcpy(ctx->buffer + left, buf, fill);
			if (!ctx->bigendian)
				swap_bytes((uint32_t*)(ctx->buffer));
			sha1_compression(ctx->ihv, (uint32_t*)(ctx->buffer));
			buf += fill;
			len -= fill;
			left = 0;
		}
		while (len >= 64)
		{
			ctx->total += 64;
			if (!ctx->bigendian)
			{
				memcpy(ctx->buffer, buf, 64);
				swap_bytes((uint32_t*)(ctx->buffer));
				sha1_compression(ctx->ihv, (uint32_t*)(ctx->buffer));
			}
			else
				sha1_compression(ctx->ihv, (uint32_t*)(ctx->buffer));
			buf += 64;
			len -= 64;
		}
		if (len > 0)
		{
			ctx->total += len;
			memcpy(ctx->buffer + left, buf, len);
		}
	}

	static const unsigned char sha1_padding[64] =
	{
		0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	void SHA1reg_Final(unsigned char output[20], SHA1reg_CTX *ctx)
	{
		uint32_t last = ctx->total & 63;
		uint32_t padn = (last < 56) ? (56 - last) : (120 - last);
		uint64_t total;
		SHA1reg_Update(ctx, (const char*)(sha1_padding), padn);

		total = ctx->total - padn;
		total <<= 3;
		ctx->buffer[56] = (unsigned char)(total >> 56);
		ctx->buffer[57] = (unsigned char)(total >> 48);
		ctx->buffer[58] = (unsigned char)(total >> 40);
		ctx->buffer[59] = (unsigned char)(total >> 32);
		ctx->buffer[60] = (unsigned char)(total >> 24);
		ctx->buffer[61] = (unsigned char)(total >> 16);
		ctx->buffer[62] = (unsigned char)(total >> 8);
		ctx->buffer[63] = (unsigned char)(total);
		if (!ctx->bigendian)
			swap_bytes((uint32_t*)(ctx->buffer));
		sha1_compression(ctx->ihv, (uint32_t*)(ctx->buffer));
		output[0] = (unsigned char)(ctx->ihv[0] >> 24);
		output[1] = (unsigned char)(ctx->ihv[0] >> 16);
		output[2] = (unsigned char)(ctx->ihv[0] >> 8);
		output[3] = (unsigned char)(ctx->ihv[0]);
		output[4] = (unsigned char)(ctx->ihv[1] >> 24);
		output[5] = (unsigned char)(ctx->ihv[1] >> 16);
		output[6] = (unsigned char)(ctx->ihv[1] >> 8);
		output[7] = (unsigned char)(ctx->ihv[1]);
		output[8] = (unsigned char)(ctx->ihv[2] >> 24);
		output[9] = (unsigned char)(ctx->ihv[2] >> 16);
		output[10] = (unsigned char)(ctx->ihv[2] >> 8);
		output[11] = (unsigned char)(ctx->ihv[2]);
		output[12] = (unsigned char)(ctx->ihv[3] >> 24);
		output[13] = (unsigned char)(ctx->ihv[3] >> 16);
		output[14] = (unsigned char)(ctx->ihv[3] >> 8);
		output[15] = (unsigned char)(ctx->ihv[3]);
		output[16] = (unsigned char)(ctx->ihv[4] >> 24);
		output[17] = (unsigned char)(ctx->ihv[4] >> 16);
		output[18] = (unsigned char)(ctx->ihv[4] >> 8);
		output[19] = (unsigned char)(ctx->ihv[4]);
	}

} // extern "C"

using namespace std;
using namespace boost::accumulators;

inline double LogBase2(double x)
{
	return (log(x) / log(2.0));
}

inline uint32_t rotate_left(const uint32_t x, const unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

template<typename RNG>
void gen_W(RNG& rng, uint32_t W[80])
{
	for (unsigned i = 0; i < 16; ++i)
		W[i] = rng();
	for (unsigned i = 16; i < 80; ++i)
		W[i] = rotate_left(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
}

int main(int argc, char** argv)
{
	boost::random::random_device seeder;
	boost::random::mt19937 rng(seeder);

	SHA1_CTX ctx;
	SHA1reg_CTX ctxreg;
	unsigned char hash[20];

	vector<char> buffer;

	boost::timer timer;



	uint32_t dvmask[DVMASKSIZE], dvmask_test[DVMASKSIZE];

	cout << "Verifying ubc_check() against ubc_check_verify():" << endl;
	boost::progress_display pd(1 << 24);
	for (unsigned ll = 0; ll < (1 << 24); ++ll, ++pd)
	{
		uint32_t W[80];

		gen_W(rng, W);

		for (unsigned i = 0; i < DVMASKSIZE; ++i)
		{
			dvmask[i] = 0;
			dvmask_test[i] = ~uint32_t(0);
		}

		ubc_check(W, dvmask);
		ubc_check_verify(W, dvmask_test);

		for (unsigned i = 0; i < DVMASKSIZE; ++i)
			if (dvmask[i] != dvmask_test[i])
			{
				cerr << "Found error:" << endl
					<< "dvmask [" << i << "] = 0x" << hex << std::setw(8) << std::setfill('0') << dvmask[i] << dec << endl
					<< "dvmask2[" << i << "] = 0x" << hex << std::setw(8) << std::setfill('0') << dvmask_test[i] << dec << endl
					;
				return 1;
			}
	}
	cout << "Found no discrepancies between ubc_check() and ubc_check_verify()." << endl << endl;

	const size_t testCnt = 17;
	size_t iterCnt = 1 << 24;

	accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_ubc;
	accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_sha;
	accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_shawnome;

	cout << "Measuring performance of ubc_check, SHA-1 Compress and SHA-1 Compress w/out message expansion." << endl;

	uint32_t x = 0; // variable that accumulates results from inner loops to prevent them from being optimized away

	boost::progress_display perf_pd(testCnt);

	for (size_t k = 0; k < testCnt; k++, ++perf_pd)
	{
		vector< vector<uint32_t> > Wlist;
		for (size_t i = 0; i < (1 << 20); ++i)
		{
			vector<uint32_t> W(80);
			for (unsigned j = 0; j < 80; ++j)
				W[j] = rng();
			Wlist.push_back(W);
		}

		timer.restart();
		for (size_t j = 0; j < (iterCnt >> 20); ++j)
			for (size_t i = 0; i < (1 << 20); ++i)
			{
				ubc_check(&Wlist[i][0], dvmask);
				x += dvmask[0];
			}
		double ubcchecktime = timer.elapsed();

		acc_ubc(iterCnt/ubcchecktime);

		uint32_t IHV[5], M[80];
		for (unsigned i = 0; i < 5; ++i)
			IHV[i] = rng();
		for (unsigned i = 0; i < 80; ++i)
			M[i] = rng();

		timer.restart();
		for (size_t i = 0; i < iterCnt; ++i)
			sha1_compression(IHV, M);
		double shatime = timer.elapsed();

		acc_sha(iterCnt/shatime);

		timer.restart();
		for (size_t i = 0; i < (iterCnt); ++i)
			sha1_compression_W(IHV, M);
		double shawometime = timer.elapsed();

		x += IHV[0] + IHV[1] + IHV[2] + IHV[3] + IHV[4]; // prevent l
		acc_shawnome(iterCnt/shawometime);
	}

	cout << "SHA-1 compress performance: ";
	cout << "median 2^" << LogBase2(median(acc_sha)) << " sha1 compress/s ";
	cout << "mean 2^" << LogBase2(mean(acc_sha)) << " sha1 compress/s ";
	cout << "variance " << variance(acc_sha) << endl;

	cout << "UBC Check performance: ";
	cout << "median 2^" << LogBase2(median(acc_ubc)) << " ubc_check/s (" << median(acc_ubc) / mean(acc_sha) << ") ";
	cout << "mean 2^" << LogBase2(mean(acc_ubc)) << " ubc_check/s (" << mean(acc_ubc) / mean(acc_sha) << ") ";
	cout << "variance " << variance(acc_ubc) << " DVMASK:[" << x << "]" << endl;

	cout << "SHA-1 compress w/o msgexp performance: ";
	cout << "median 2^" << LogBase2(median(acc_shawnome)) << " sha1 compress no ME/s (" << median(acc_shawnome) / mean(acc_sha) << ") ";
	cout << "mean 2^" << LogBase2(mean(acc_shawnome)) << " sha1 compress no ME/s (" << mean(acc_shawnome) / mean(acc_sha) << ") ";
	cout << "variance " << variance(acc_shawnome) << endl;

	size_t testcount = 128;
	size_t mindata = 1 << 20;

	for (size_t i = 11; i < 12; ++i)
	{
		buffer.resize(mindata);

		size_t minloop = mindata / (1<<i);
		if (minloop == 0)
			minloop = 1;

		double elap_noubc = 0;
		double elap_ubc = 0;
		double elap_reg = 0;

		accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_shafullcd;
		accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_shaubc;
		accumulator_set<double, stats<tag::mean, tag::variance, tag::median> > acc_shareg;

		for (size_t n = 0; n < testCnt; n++)
		{
			for (size_t r = 0; r < testcount; ++r)
			{
				for (size_t j = 0; j < buffer.size(); ++j)
					buffer[j] = rng();

				timer.restart();
				for (size_t k = 0; k < minloop; ++k)
				{
					SHA1DCInit(&ctx);
					SHA1DCSetCallback(&ctx, nc_callback);
					SHA1DCSetUseDetectColl(&ctx, 0);
					SHA1DCUpdate(&ctx, &buffer[k * (1<<i)], 1<<i);
					SHA1DCFinal(hash, &ctx);
					x += hash[0];
				}
				elap_reg += timer.elapsed();

				timer.restart();
				for (size_t k = 0; k < minloop; ++k)
				{
					SHA1DCInit(&ctx);
					SHA1DCSetCallback(&ctx, nc_callback);
					SHA1DCSetUseUBC(&ctx, 0);
					SHA1DCUpdate(&ctx, &buffer[k * (1<<i)], 1<<i);
					SHA1DCFinal(hash, &ctx);
					x += hash[0];
				}
				elap_noubc += timer.elapsed();

				timer.restart();
				for (size_t k = 0; k < minloop; ++k)
				{
					SHA1DCInit(&ctx);
					SHA1DCSetCallback(&ctx, nc_callback);
	//				SHA1DCSetUseUBC(&ctx, 1);
					SHA1DCUpdate(&ctx, &buffer[k*(1<<i)], 1<<i);
					SHA1DCFinal(hash, &ctx);
					x += hash[0];
				}
				elap_ubc += timer.elapsed();

			}

			elap_noubc /= double(minloop)*double(testcount);
			elap_ubc /= double(minloop)*double(testcount);
			elap_reg /= double(minloop)*double(testcount);

			acc_shafullcd(elap_noubc);
			acc_shaubc(elap_ubc);
			acc_shareg(elap_reg);

		}

		cout << (1 << i) << "\t : SHA Regular Median: " << median(acc_shareg) << "s \tMean: " << mean(acc_shareg) << "s \t Variance: " << variance(acc_shareg) << "s" << endl;
		cout << (1 << i) << "\t : SHA Collision Detection w/out UBC Median: " << median(acc_shafullcd) << "s " << median(acc_shafullcd) / median(acc_shareg) << "(no ubc/reg) \tMean: " << mean(acc_shafullcd) << "s " << mean(acc_shafullcd) / mean(acc_shareg) << "(no ubc/reg) \t Variance: " << variance(acc_shafullcd) << "s" << endl;
		cout << (1 << i) << "\t : SHA Collision Detection w/ UBC Median: " << median(acc_shaubc) << "s " << median(acc_shaubc) / median(acc_shareg) << "(no ubc/reg) \tMean: " << mean(acc_shaubc) << "s " << mean(acc_shaubc) / mean(acc_shareg) << "(no ubc/reg) \t Variance: " << variance(acc_shaubc) << "s" << endl;
	}
	// finally act on x to prevent this variable to be optimized away
	if (x)
		cout << " " << flush;
	else
		cout << " ";
	return 0;

	cout << "Performing endurance test..." << endl;
	uint64_t total = 0;
	SHA1DCInit(&ctx);
	SHA1DCSetCallback(&ctx, nc_callback);
	buffer.resize(1 << 30);
	timer.restart();
	while (true)
	{
		for (size_t i = 0; i < buffer.size(); i += 4)
			(*reinterpret_cast<uint32_t*>(&buffer[i])) = rng();
		SHA1DCUpdate(&ctx, &buffer[0], buffer.size());
		total += buffer.size();
		if (timer.elapsed() > 60)
		{
			cout << "Hashed " << (total >> 30) << " GB..." << endl;
			timer.restart();
		}
	}


}
