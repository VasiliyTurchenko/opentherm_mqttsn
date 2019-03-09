/** @file test_ascii_helpers.c
 *  @brief tests for ascii helpers functions
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 09-Mar-2019
 */
#include <string.h>

#include "testhelpers.h"
#include "G:\DIY_projects\OpenTherm\C\opentherm_mqttsn\Core\Inc\helpers\test_ascii_helpers.h"
#include "G:\DIY_projects\OpenTherm\C\opentherm_mqttsn\Core\Inc\helpers\ascii_helpers.h"

typedef struct test_case {
	char *in_str;
	bool resH;
	bool resD;
} test_case_t;

static test_case_t test_cases[] = {
	{ "", false, false},
	{ "0", true, true},
	{ "A", true, false},
	{ "g", false, false},
	{ "as", false, false},
	{ "deadbeEF", true, false},
	{ "00000000000000000000000000000000000000000000", true, true},
	{ "0000000000000000000000000000000000000000000fz", false, false},
	{ "1234567890", true, true},
	{ "l234567890", false, false},
	{ "\x2F", false, false},
	{ "\x30", true, true},

};

static size_t const n_cases = sizeof(test_cases) / sizeof(test_cases[0]);

/* test isHex */
static void TEST_isHex(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_cases; ++i) {
		bool res = isHex(test_cases[i].in_str, strlen(test_cases[i].in_str));
		if (res == test_cases[i].resH) {
			TEST_PASSED(i);
		} else {
			TEST_FAILED(i, (res) ? "true" : "false");
		}
	}

	TestFooter(test_name);
}

/* test isDec */
static void TEST_isDec(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_cases; ++i) {
		bool res = isDec(test_cases[i].in_str, strlen(test_cases[i].in_str));
		if (res == test_cases[i].resD) {
			TEST_PASSED(i);
		} else {
			TEST_FAILED(i, (res) ? "true" : "false");
		}
	}

	TestFooter(test_name);
}



typedef struct test_case1 {
	char		*in_str;
	size_t		conv_len;
	uint8_t		val;
} test_case1_t;

static test_case1_t h2b[] = {
	{"0", 0U, 0U},
	{"0", 1U, 0U},
	{"1", 0U, 0U},
	{"1", 1U, 1U},
	{"1s", 0U, 0U},
	{"1s", 0U, 0U},
	{"s1", 0U, 0U},
	{"s1", 1U, 0U},
	{"3s2", 2U, 0x30U},
	{"FF", 2U, 0xFFU},
	{"FF0", 2U, 0xFFU},
	{"FFZZ", 2U, 0xFFU},
	{"FFZZ", 3U, 0xF0U},
	{"ZFFZ", 3U, 0xFFU},
	{"ZFFZ", 1U, 0x0U},
	{"ZFFZ", 2U, 0x0FU},
	{"99", 2U, 0x99U},
	{"199", 3U, 0x99U},
	{"a9", 2U, 0xa9U},
	{"9", 1U, 0x9U},
};

static size_t const n_h2b = sizeof(h2b) / sizeof(h2b[0]);

/* test ahex2byte */
static void TEST_ahex2byte(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_h2b; ++i) {
		uint8_t res = ahex2byte(h2b[i].in_str, h2b[i].conv_len );
		if (res == h2b[i].val) {
			TEST_PASSED(i);
		} else {
			char t[] = "XXXXXXXXXXXXXXX\0";
			sprintf(&t, "%d", res);
			TEST_FAILED(i, t);
		}
	}



	TestFooter(test_name);
}

/* test ahex2byte */
static test_case1_t d2b[] = {

	{"2A5", 2U, 20U},

	{"0", 0U, 0U},
	{"0", 1U, 0U},
	{"1", 0U, 0U},
	{"1", 1U, 1U},
	{"1s", 0U, 0U},
	{"1s", 0U, 0U},
	{"s1", 0U, 0U},
	{"s1", 1U, 0U},

	{"11", 1U, 1U},
	{"11", 2U, 11U},
	{"111", 2U, 11U},
	{"255", 3U, 255U},

	{"2345", 3U, 234U},
	{"2345", 4U, 345-256},


	{"2A5", 1U, 2U},
	{"2A5", 2U, 20U},
	{"2A5", 3U, 205U},

	{"eee", 3U, 0U},


};

static size_t const n_d2b = sizeof(d2b) / sizeof(d2b[0]);

static void TEST_adec2byte(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_d2b; ++i) {
		uint8_t res = adec2byte(d2b[i].in_str, d2b[i].conv_len );
		if (res == d2b[i].val) {
			TEST_PASSED(i);
		} else {
			char t[] = "XXXXXXXXXXXXXXX\0";
			sprintf(&t, "%d", res);
			TEST_FAILED(i, t);
		}
	}



	TestFooter(test_name);
}


/* test ahex2uint16 */

typedef struct test_case2 {
	char		*in_str;
	size_t		conv_len;
	uint16_t		val;
} test_case2_t;

static test_case2_t h2h[] =
{
	{"aa2", 3U, 0xaa2U},

	{"0", 0U, 0U},
	{"0", 1U, 0U},
	{"1", 0U, 0U},
	{"1", 1U, 1U},
	{"1s", 0U, 0U},
	{"1s", 0U, 0U},
	{"s1", 0U, 0U},
	{"s1", 1U, 0U},

	{"11", 1U, 1U},

/*9*/	{"11", 2U, 0x11U},
	{"111", 2U, 0x11U},
	{"255", 3U, 0x255U},

	{"2345", 3U, 0x234U},
	{"2345", 4U, 0x2345},


	{"2A5", 1U, 2U},
	{"2A5", 2U, 0x2AU},
	{"2a5", 3U, 0x2A5U},
	{"2A5", 3U, 0x2a5U},

	{"2aa", 2U, 0x2aU},
	{"2aa", 3U, 0x2aaU},

	{"aa2", 3U, 0xaa2U},

	{"DEAD", 4U, 0xDEADU},
	{"DEADBEEF", 8U, 0xBEEFU},

	{"DEOD", 4U, 0xDE0DU},
	{"DE!D", 4U, 0xDE0DU},
	{"@~!a", 4U, 0xAU},
	{"@~!a", 3U, 0x0U},


};

static size_t const n_h2h = sizeof(h2h) / sizeof(h2h[0]);

static void TEST_ahex2uint16(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_h2h; ++i) {
		uint16_t res = ahex2uint16(h2h[i].in_str, h2h[i].conv_len );
		if (res == h2h[i].val) {
			TEST_PASSED(i);
		} else {
			char t[] = "XXXXXXXXXXXXXXX\0";
			sprintf(&t, "%d", res);
			TEST_FAILED(i, t);
		}
	}
	TestFooter(test_name);
}


static test_case2_t d2h[] =
{
	{"0", 0U, 0U},
	{"0", 1U, 0U},
	{"1", 0U, 0U},
	{"1", 1U, 1U},
	{"1s", 0U, 0U},
	{"1s", 0U, 0U},
	{"s1", 0U, 0U},
	{"s1", 1U, 0U},

/*8 */	{"11", 1U, 1U},

/*9*/	{"11", 2U, 11U},
	{"111", 2U, 11U},
	{"255", 3U, 255U},

	{"2345", 3U, 234U},
	{"2345", 4U, 2345},


	{"2A5", 1U, 2U},

	{"2A5", 2U, 20U},
	{"2a5", 3U, 205U},

	{"2A5", 3U, 205U},

	{"65535", 3U, 655U},
	{"65534", 3U, 655U},

	{"65535", 5U, 65535U},
	{"65536", 5U, 0U},

	{"2aa", 2U, 20U},
	{"2aa", 3U, 200U},

	{"aa2", 3U, 2U},

	{"DEAD", 4U, 0U},
	{"DEADBEEF", 8U, 0U},

	{"DEOD", 4U, 0U},
	{"@~!a", 3U, 0x0U},
	{"6`535", 5U, 60535U},


};


static size_t const n_d2h = sizeof(d2h) / sizeof(d2h[0]);

static void TEST_adec2uint16(void)
{
	const char *test_name = __FUNCTION__;
	TestHeader(test_name);

	for (size_t i = 0U; i < n_d2h; ++i) {
		uint16_t res = adec2uint16(d2h[i].in_str, d2h[i].conv_len );
		if (res == d2h[i].val) {
			TEST_PASSED(i);
		} else {
			char t[] = "XXXXXXXXXXXXXXX\0";
			sprintf(&t, "%d", res);
			TEST_FAILED(i, t);
		}
	}
	TestFooter(test_name);
}



void TEST_ascii_helpers(void)
{
	TEST_isHex();
	TEST_isDec();
	TEST_ahex2byte();
	TEST_adec2byte();
	TEST_ahex2uint16();
	TEST_adec2uint16();
}
