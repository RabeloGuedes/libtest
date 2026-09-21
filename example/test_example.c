#include <stdlib.h>
#include <string.h>
#include <libtest.h>

static int	add(int a, int b)
{
	return (a + b);
}

static const char	*greeting(void)
{
	return ("hello\n");
}

static void	test_add_positive(void)
{
	LT_ASSERT_INT_EQ(add(2, 3), 5);
}

static void	test_add_negative(void)
{
	LT_ASSERT_INT_EQ(add(-2, -3), -5);
}

static void	test_add_fails_on_purpose(void)
{
	LT_ASSERT_UINT_EQ(strlen("abc"), 3);
	LT_ASSERT_INT_EQ(add(1, 1), 3);
}

static void	test_greeting_fails_on_purpose(void)
{
	LT_ASSERT_STR_EQ(greeting(), "hello");
}

/*
** Fixture state lives in a static variable. Every test runs in its own
** process, so each one starts from a fresh copy of it, and setup runs
** again before each test.
*/
static char	*g_buffer;

static void	buffer_setup(void)
{
	g_buffer = malloc(16);
	LT_ASSERT(g_buffer != NULL);
	strcpy(g_buffer, "abc");
}

static void	buffer_teardown(void)
{
	free(g_buffer);
	g_buffer = NULL;
}

static void	test_buffer_is_ready(void)
{
	LT_ASSERT_STR_EQ(g_buffer, "abc");
}

static void	test_buffer_can_be_changed(void)
{
	strcpy(g_buffer, "xyz");
	LT_ASSERT_STR_EQ(g_buffer, "xyz");
}

int	main(int argc, char **argv)
{
	const t_lt_test		math[] = {
		LT_TEST(test_add_positive),
		LT_TEST(test_add_negative),
		LT_TEST_TAGGED(test_add_fails_on_purpose, "slow"),
		LT_TEST(test_greeting_fails_on_purpose),
	};
	const t_lt_test		buffer[] = {
		LT_TEST(test_buffer_is_ready),
		LT_TEST(test_buffer_can_be_changed),
	};
	const t_lt_suite	suites[] = {
		LT_SUITE_TAGGED("math", NULL, NULL, math, "unit"),
		LT_SUITE_TAGGED("buffer", buffer_setup, buffer_teardown, buffer,
			"integration"),
	};

	return (LT_SUITES_MAIN(argc, argv, suites));
}
