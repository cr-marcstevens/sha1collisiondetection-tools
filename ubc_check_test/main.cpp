/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include <boost/cstdint.hpp>

using namespace std;
using boost::uint32_t;

#include "ubc_check_test.h"
#include "test_simd.h"

extern "C" {
#include "../../lib/ubc_check_verify.c"
}

char*	usage_str = "%s usage:\n"
					"\t--all      - Run all tests.\n"
#ifdef INCLUDE_BASIC_TEST
					"\t--basic    - Run unavoidable bit condition check tests (default true.)\n"
#endif
#ifdef INCLUDE_MMX64_TEST
					"\t--mmx64    - Run unavoidable bit condition check tests with sse128 improvements.\n"
#endif
#ifdef INCLUDE_SSE128_TEST
					"\t--sse128   - Run unavoidable bit condition check tests with sse128 improvements.\n"
#endif
#ifdef INCLUDE_AVX256_TEST
					"\t--avx256   - Run UBC tests with avx256 improvements.\n"
#endif
#ifdef INCLUDE_NEON128_TEST
					"\t--neon128  - Run unavoidable bit condition check tests with neon128 improvements.\n"
#endif
					"-p,--nocheck - Supress correctness checks.\n"
					"-c,--noperf  - Supress performance tests.\n"
					"\t-h,--help  - Print this help message\n"
					"\n";

bool run_all_test = false;
#define TEST_ALL_ARG_STR	"--all"

typedef int(*TestUBCFunction)();

typedef struct {
	TestUBCFunction fn_test_ubc_check;
	bool run_test;
	char* arg_str;
} TestConfigEntry;

TestConfigEntry testConfig[] =
{
#ifdef INCLUDE_BASIC_TEST
	{
		test_ubc_check,
		true,
		"--basic"
	},
#endif
#ifdef INCLUDE_MMX64_TEST
	{
		test_ubc_check_mmx64,
		false,
		"--mmx64"
	},
#endif
#ifdef INCLUDE_SSE128_TEST
	{
		test_ubc_check_sse128,
		false,
		"--sse128"
	},
#endif
#ifdef INCLUDE_AVX256_TEST
	{
		test_ubc_check_avx256,
		false,
		"--avx256"
	},
#endif
#ifdef INCLUDE_NEON128_TEST
	{
		test_ubc_check_neon128,
		false,
		"--neon128"
	}
#endif
};

const size_t cntTestConfig = sizeof(testConfig) / sizeof(TestConfigEntry);

bool run_correctness_checks = true;
bool run_perf_tests = true;

void usage(char* program_name)
{
	printf(usage_str, program_name);
}


int main(int argc, char** argv)
{
	for (size_t i = 1; i < argc; i++)
	{
		bool fTestArgStrFound = false;
		// loop through and set the config to run based on 
		for (size_t j = 0; (j < cntTestConfig) && !fTestArgStrFound; j++)
		{
			if (0 == strcmp(argv[i], testConfig[j].arg_str))
			{
				testConfig[j].run_test = true;
				fTestArgStrFound = true;
			}
		}

		if (0 == strcmp(argv[i], TEST_ALL_ARG_STR))
		{
			run_all_test = true;
		}
		else if ((0 == strcmp(argv[i], "-p")) ||
			     (0 == strcmp(argv[i], "--nocheck")))
		{
			run_correctness_checks = false;
		}
		else if ((0 == strcmp(argv[i], "-c")) ||
			     (0 == strcmp(argv[i], "--noperf")))
		{
			run_perf_tests = false;
		}
		else if ((0 == strcmp(argv[i], "-h")) || 
				 (0 == strcmp(argv[i], "--help")))
		{
			usage(argv[0]);
			exit(0);
		}
		else if (fTestArgStrFound)
		{
		}
		else
		{
			printf("Unknown argument: %s\n", argv[i]);
			usage(argv[0]);
			exit(-1);
		}
	}

	for (size_t j = 0; j < cntTestConfig; j++)
	{
		if (run_all_test || testConfig[j].run_test)
		{
			cout << "=====================================================================" << endl;
			testConfig[j].fn_test_ubc_check();
			cout << "=====================================================================" << endl << endl << endl;
		}
	}
}

