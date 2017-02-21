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

#include "test_simd.cpp"

#include "../../lib/simd_mmx64.h"

#ifdef INCLUDE_MMX64_TEST
int test_ubc_check_mmx64()
{
	return test_ubc_check_simd<SIMD_WORD, ubc_check_mmx64>("_mmx64");
}
#endif

