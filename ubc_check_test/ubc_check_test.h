/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/


#ifndef UBC_CHECK_TEST_HPP
#define UBC_CHECK_TEST_HPP

#define INCLUDE_BASIC_TEST

#ifdef HAVE_MMX
#define INCLUDE_MMX64_TEST
#endif
#ifdef HAVE_SSE
#define INCLUDE_SSE128_TEST
#endif
#ifdef HAVE_AVX
#define INCLUDE_AVX256_TEST
#endif
#ifdef HAVE_NEON
#define INCLUDE_NEON128_TEST
#endif 

#endif // UBC_CHECK_TEST_HPP