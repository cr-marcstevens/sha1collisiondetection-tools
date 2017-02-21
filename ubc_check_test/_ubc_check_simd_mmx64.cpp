/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/


#include "ubc_check_test.h"

#ifdef INCLUDE_MMX64_TEST
#include "test_simd.h"
extern "C" {
#include "../../lib/ubc_check_simd_mmx64.c"
}
#endif