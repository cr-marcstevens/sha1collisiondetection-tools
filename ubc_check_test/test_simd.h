/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/

#ifdef HAVE_NEON
#include <arm_neon.h>
#endif

#if defined(HAVE_MMX) || defined(HAVE_SSE) || defined(HAVE_AVX)
#include <immintrin.h>
#endif

extern "C"
{
#include "../../lib/ubc_check.h"
	void ubc_check_verify(const uint32_t W[80], uint32_t dvmask[DVMASKSIZE]);
#ifdef INCLUDE_MMX64_TEST
	void ubc_check_mmx64(const __m64* W, __m64* dvmask);
#endif
#ifdef INCLUDE_SSE128_TEST
	void ubc_check_sse128(const __m128i* W, __m128i* dvmask);
#endif
#ifdef INCLUDE_AVX256_TEST
	void ubc_check_avx256(const __m256i* W, __m256i* dvmask);
#endif
#ifdef INCLUDE_NEON128_TEST
	void ubc_check_neon128(const int32x4_t* W, int32x4_t* dvmask);
#endif
}


#ifdef INCLUDE_BASIC_TEST
int test_ubc_check();
#endif

#ifdef INCLUDE_MMX64_TEST
int test_ubc_check_mmx64();
#endif

#ifdef INCLUDE_SSE128_TEST
int test_ubc_check_sse128();
#endif

#ifdef INCLUDE_AVX256_TEST
int test_ubc_check_avx256();
#endif

#ifdef INCLUDE_NEON128_TEST
int test_ubc_check_neon128();
#endif

template<typename SIMD_WORD, void(*ubc_check_simd)(const SIMD_WORD*, SIMD_WORD*)> inline int test_ubc_check_simd(const char* simd_name_str);

