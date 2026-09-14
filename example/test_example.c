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

int	main(void)
{
	const t_lt_test	tests[] = {
		LT_TEST(test_add_positive),
		LT_TEST(test_add_negative),
		LT_TEST(test_add_fails_on_purpose),
		LT_TEST(test_greeting_fails_on_purpose),
	};

	return (LT_RUN(tests));
}
