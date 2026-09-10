#include <string.h>
#include <libtest.h>

static int	add(int a, int b)
{
	return (a + b);
}

static void	test_add_positive(void)
{
	LT_ASSERT(add(2, 3) == 5);
}

static void	test_add_negative(void)
{
	LT_ASSERT(add(-2, -3) == -5);
}

static void	test_fails_on_purpose(void)
{
	LT_ASSERT(strlen("abc") == 3);
	LT_ASSERT(add(1, 1) == 3);
	LT_ASSERT(0 && "nunca chega aqui");
}

int	main(void)
{
	const t_lt_test	tests[] = {
		LT_TEST(test_add_positive),
		LT_TEST(test_add_negative),
		LT_TEST(test_fails_on_purpose),
	};

	return (LT_RUN(tests));
}
