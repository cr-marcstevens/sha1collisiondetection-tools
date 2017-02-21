/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/


#include <iostream>
#include <iomanip>
#include <vector>

#include <boost/cstdint.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random.hpp>
#include <boost/progress.hpp>
#include <boost/timer.hpp>
#include <boost/array.hpp>

#include "ubc_check_test.h"
#include "test_simd.h"

using namespace std;
using boost::uint32_t;

// on 32 bit architectures, we need to allocate a smaller buffer for performance testing
#if defined(_M_IX86) || defined(__arm__)
#define VECTOR_COUNT (1 << 19)
#else
#define VECTOR_COUNT (1 << 20)
#endif 

inline uint32_t rotate_left(const uint32_t x, const unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

template<typename RNG, typename SIMD_WORD>
inline
void gen_W(RNG& rng, SIMD_WORD* W)
{
	const size_t SIMD_VECSIZE = sizeof(SIMD_WORD) / sizeof(uint32_t);

	for (unsigned i = 0; i < 16; ++i)
	{
		union {
			SIMD_WORD v;
			uint32_t w[SIMD_VECSIZE];
		} tmp;
		for (unsigned j = 0; j < SIMD_VECSIZE; ++j)
			tmp.w[j] = rng();
		W[i] = tmp.v;
	}

	for (unsigned i = 16; i < 80; ++i)
	{
		for (size_t j = 0; j < SIMD_VECSIZE; j++)
			((uint32_t*)&W[i])[j] = rotate_left((((uint32_t*)&W[i - 3])[j] ^ ((uint32_t*)&W[i - 8])[j] ^ ((uint32_t*)&W[i - 14])[j] ^ ((uint32_t*)&W[i - 16])[j]), 1);
	}
}

extern bool run_correctness_checks;
extern bool run_perf_tests;

template<typename SIMD_WORD, void(*ubc_check_simd)(const SIMD_WORD*, SIMD_WORD*)>
inline
int test_ubc_check_simd(const char* simd_name_str)
{
	boost::random::random_device seeder;
	boost::random::mt19937 rng(seeder);

	const size_t SIMD_VECSIZE = sizeof(SIMD_WORD) / sizeof(uint32_t);

	SIMD_WORD W[80];
	SIMD_WORD dvmask[DVMASKSIZE];
	uint32_t dvmask_test[DVMASKSIZE];
	union {
		SIMD_WORD v;
		uint32_t w[SIMD_VECSIZE];
	} tmp;

	if (run_correctness_checks)
	{
		cout << "Verifying ubc_check" << simd_name_str << "() against ubc_check_verify():" << endl;
		boost::progress_display pd(1 << 24);
		for (unsigned ll = 0; ll < (1 << 24); ++ll, ++pd)
		{
			gen_W(rng, W);

			for (unsigned i = 0; i < DVMASKSIZE; ++i)
			{
				for (size_t j = 0; j < SIMD_VECSIZE; j++)
					((uint32_t*)&dvmask[i])[j] = 0;
			}

			ubc_check_simd(W, dvmask);

			for (unsigned w = 0; w < SIMD_VECSIZE; ++w)
			{
				uint32_t W2[80];
				for (unsigned i = 0; i < 80; ++i)
				{
					tmp.v = W[i];
					W2[i] = tmp.w[w];
				}

				ubc_check_verify(W2, dvmask_test);

				for (unsigned i = 0; i < DVMASKSIZE; ++i)
				{
					tmp.v = dvmask[i];
					if (tmp.w[w] != dvmask_test[i])
					{
						cerr << "Found error:" << endl
							<< "dvmask [" << i << "] = 0x" << hex << std::setw(8) << std::setfill('0') << tmp.w[w] << dec << endl
							<< "dvmask2[" << i << "] = 0x" << hex << std::setw(8) << std::setfill('0') << dvmask_test[i] << dec << endl
							;
						return 1;
					}
				}
			}
		}
		cout << "Found no discrepancies between ubc_check" << simd_name_str << "() and ubc_check_verify()." << endl << endl;
	}

	if (run_perf_tests)
	{
		cout << "Measuring performance of ubc_check" << simd_name_str << "() " << VECTOR_COUNT << " iterations :" << endl;

		vector< boost::array<SIMD_WORD, 80> > vec_Ws(VECTOR_COUNT);
		typename vector< boost::array<SIMD_WORD, 80> >::iterator it;
		for (it = vec_Ws.begin(); it != vec_Ws.end(); ++it)
			gen_W(rng, &((*it)[0]));

		boost::timer timer;
		SIMD_WORD x;
		int count = 1;
		double sec = 0, perf = 0;

		for (size_t j = 0; j < SIMD_VECSIZE; j++)
			((uint32_t*)&x)[j] = 0;

		cout << "(";
		while (true)
		{
			timer.restart();

			for (int ll = 0; ll < count; ++ll)
			{
				for (it = vec_Ws.begin(); it != vec_Ws.end(); ++it)
				{
					ubc_check_simd(&((*it)[0]), dvmask);
					for (unsigned i = 0; i < DVMASKSIZE; ++i)
					{
						uint32_t* px = (uint32_t*)&x;
						for (size_t j = 0; j < SIMD_VECSIZE; j++)
							px[j] = px[j] + ((uint32_t*)&dvmask[i])[j];
					}
				}
			}

			sec = timer.elapsed();
			perf = double(vec_Ws.size())*double(count) / sec;
			cout << " " << sec << flush;
			if (sec >= 10)
				break;

			count *= 2;
		}

		cout << " ) [ " << hex;
		for (size_t j = 0; j < SIMD_VECSIZE; j++)
		{
			tmp.w[j] = ((uint32_t*)&x)[j];
			cout << tmp.w[j] << " ";
		}

		cout << "]" << dec << endl;

		cout << "Performance: " << SIMD_VECSIZE << " x 2^" << log(perf) / log(2.0) << " #/s" << endl;
	}

	return 0;
}
