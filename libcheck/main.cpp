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

	void swap_bytes(uint32_t val[16]);

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
