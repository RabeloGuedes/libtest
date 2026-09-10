#include <stdio.h>
#include <string.h>
#include <libtest.h>

/* ---------------------------------------------------------------------
** Internal tests: they are not registered in the runner. They are
** executed by the real testers via lt_run_one, that inspect the result.
** ------------------------------------------------------------------- */

static int	g_reached;
static int	g_line;

static void	inner_true(void)
{
	LT_ASSERT(1);
}

static void	inner_false(void)
{
	LT_ASSERT(0);
}

static void	inner_eq_one(void)
{
	int	x;

	x = 7;
	g_line = __LINE__ + 1;
	LT_ASSERT(x == 1);
}

static void	inner_stops_after_failure(void)
{
	LT_ASSERT(0);
	g_reached = 1;
}

static void	inner_first_failure_wins(void)
{
	g_line = __LINE__ + 1;
	LT_ASSERT(1 == 2);
	LT_ASSERT(3 == 4);
}

static void	inner_truthy_scalars(void)
{
	const char	*str;
	double		ratio;

	str = "abc";
	ratio = 0.5;
	LT_ASSERT(str);
	LT_ASSERT(ratio);
	LT_ASSERT(-1);
}

static void	inner_null_is_false(void)
{
	LT_ASSERT((void *)0);
}

static void	inner_runs_failing_test(void)
{
	const t_lt_test	t = LT_TEST(inner_false);

	lt_run_one(&t);
	LT_ASSERT(1);
}

/* ---------------------------------------------------------------------
** Framework's tests
** ------------------------------------------------------------------- */

static t_lt_result	run(t_lt_func func)
{
	const t_lt_test	t = {"inner", func};

	return (lt_run_one(&t));
}

static void	test_true_assertion_passes(void)
{
	LT_ASSERT(!run(inner_true).failed);
}

static void	test_false_assertion_fails(void)
{
	LT_ASSERT(run(inner_false).failed);
}

/* Regression: (!!cond) instead of !!(cond), which made it pass. */
static void	test_macro_respects_precedence(void)
{
	LT_ASSERT(run(inner_eq_one).failed);
}

static void	test_failure_records_location(void)
{
	t_lt_result	r;

	r = run(inner_eq_one);
	LT_ASSERT(strcmp(r.failure.expr, "x == 1") == 0);
	LT_ASSERT(strcmp(r.failure.file, __FILE__) == 0);
	LT_ASSERT(r.failure.line == g_line);
}

static void	test_failure_stops_the_test(void)
{
	g_reached = 0;
	run(inner_stops_after_failure);
	LT_ASSERT(g_reached == 0);
}

static void	test_first_failure_is_reported(void)
{
	t_lt_result	r;

	r = run(inner_first_failure_wins);
	LT_ASSERT(strcmp(r.failure.expr, "1 == 2") == 0);
	LT_ASSERT(r.failure.line == g_line);
}

static void	test_non_int_scalars_are_truthy(void)
{
	LT_ASSERT(!run(inner_truthy_scalars).failed);
	LT_ASSERT(run(inner_null_is_false).failed);
}

/*
** If lt_run_one did not restore the state, the internal test failure
** would leak to this one, and the runner would set as FAIL.
*/
static void	test_nested_run_does_not_leak(void)
{
	LT_ASSERT(!run(inner_runs_failing_test).failed);
}

/* ---------------------------------------------------------------------
** Sanity check without using LT_ASSERT. In case the framework is
** broken so that every assertion would pass, the above tests would
** pass without proving anything. This check is the base of trust.
** ------------------------------------------------------------------- */

static int	sanity_check(void)
{
	if (run(inner_true).failed || !run(inner_false).failed)
	{
		printf("sanity check failed: LT_ASSERT is broken\n");
		return (0);
	}
	return (1);
}

int	main(void)
{
	const t_lt_test	tests[] = {
		LT_TEST(test_true_assertion_passes),
		LT_TEST(test_false_assertion_fails),
		LT_TEST(test_macro_respects_precedence),
		LT_TEST(test_failure_records_location),
		LT_TEST(test_failure_stops_the_test),
		LT_TEST(test_first_failure_is_reported),
		LT_TEST(test_non_int_scalars_are_truthy),
		LT_TEST(test_nested_run_does_not_leak),
	};

	if (!sanity_check())
		return (1);
	return (LT_RUN(tests));
}
