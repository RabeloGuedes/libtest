#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libtest.h>

/*
** Rule for this file: each layer is verified with the layer below it.
** The typed assertions are checked with LT_ASSERT + strcmp, never with
** themselves. LT_ASSERT in turn is checked by sanity_check, in main.
*/

/* ---------------------------------------------------------------------
** Inner tests: these are not registered with the runner. The real
** tests run them through lt_run_one and inspect the result.
** ------------------------------------------------------------------- */

static int	g_reached;
static int	g_line;
static int	g_calls;
static char	g_long[1000];

static int	next_value(void)
{
	g_calls++;
	return (g_calls);
}

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

/*
** Variables, not literals: with LT_ASSERT(0.5) the compiler warns on
** its own. The case that matters is the runtime value, which it has
** no way to see.
*/
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

static void	inner_int_eq_pass(void)
{
	LT_ASSERT_INT_EQ(2 + 2, 4);
}

static void	inner_int_eq_fail(void)
{
	LT_ASSERT_INT_EQ(1 + 1, 3);
}

static void	inner_int_negative(void)
{
	LT_ASSERT_INT_EQ(-5, 5);
}

static void	inner_single_eval_pass(void)
{
	LT_ASSERT_INT_EQ(next_value(), 1);
}

static void	inner_single_eval_fail(void)
{
	LT_ASSERT_INT_EQ(next_value(), 99);
}

static void	inner_uint_max(void)
{
	LT_ASSERT_UINT_EQ(SIZE_MAX, 0);
}

/*
** Own buffer, not a literal: two identical "abc" literals may end up at
** the same address, which would let a pointer-comparing implementation
** pass this test.
*/
static void	inner_str_eq_pass(void)
{
	char	buf[4];

	strcpy(buf, "abc");
	LT_ASSERT_STR_EQ(buf, "abc");
}

static void	inner_str_escapes(void)
{
	LT_ASSERT_STR_EQ("a\n", "a\t");
}

static void	inner_str_null(void)
{
	LT_ASSERT_STR_EQ(NULL, "a");
}

static void	inner_str_both_null(void)
{
	LT_ASSERT_STR_EQ(NULL, NULL);
}

static void	inner_crashes(void)
{
	int	*ptr;

	ptr = NULL;
	LT_ASSERT(*ptr == 0);
}

static void	inner_exits(void)
{
	exit(3);
}

static void	inner_hangs(void)
{
	while (1)
		;
}

static void	inner_writes_global(void)
{
	g_reached = 1;
}

static void	inner_str_long(void)
{
	memset(g_long, 'x', sizeof(g_long) - 1);
	g_long[sizeof(g_long) - 1] = '\0';
	LT_ASSERT_STR_EQ(g_long, "y");
}

/* ---------------------------------------------------------------------
** Core tests
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

/* Regression: (!!cond) instead of !!(cond) used to let this pass. */
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
** If lt_run_one did not restore the state, the inner test's failure
** would leak into this one and the runner would mark it as FAIL.
*/
static void	test_nested_run_does_not_leak(void)
{
	LT_ASSERT(!run(inner_runs_failing_test).failed);
}

/* ---------------------------------------------------------------------
** Typed assertion tests
** ------------------------------------------------------------------- */

static void	test_int_eq_passes_on_equal(void)
{
	LT_ASSERT(!run(inner_int_eq_pass).failed);
}

static void	test_int_eq_reports_values(void)
{
	t_lt_result	r;

	r = run(inner_int_eq_fail);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.failure.has_values);
	LT_ASSERT(strcmp(r.failure.expr, "1 + 1 == 3") == 0);
	LT_ASSERT(strcmp(r.failure.left, "2") == 0);
	LT_ASSERT(strcmp(r.failure.right, "3") == 0);
}

static void	test_int_eq_handles_negatives(void)
{
	t_lt_result	r;

	r = run(inner_int_negative);
	LT_ASSERT(strcmp(r.failure.left, "-5") == 0);
}

/* Both paths: when the assertion passes and when it fails. */
static void	test_arguments_evaluated_once(void)
{
	t_lt_result	r;

	g_calls = 0;
	LT_ASSERT(!run(inner_single_eval_pass).failed);
	LT_ASSERT(g_calls == 1);
	g_calls = 0;
	r = run(inner_single_eval_fail);
	LT_ASSERT(strcmp(r.failure.left, "1") == 0);
	LT_ASSERT(g_calls == 1);
}

static void	test_uint_eq_prints_unsigned(void)
{
	t_lt_result	r;
	char		expected[LT_VALUE_SIZE];

	r = run(inner_uint_max);
	snprintf(expected, sizeof(expected), "%ju", (uintmax_t)SIZE_MAX);
	LT_ASSERT(r.failed);
	LT_ASSERT(strcmp(r.failure.left, expected) == 0);
}

static void	test_str_eq_compares_content(void)
{
	LT_ASSERT(!run(inner_str_eq_pass).failed);
}

static void	test_str_eq_escapes_invisible_chars(void)
{
	t_lt_result	r;

	r = run(inner_str_escapes);
	LT_ASSERT(strcmp(r.failure.left, "\"a\\n\"") == 0);
	LT_ASSERT(strcmp(r.failure.right, "\"a\\t\"") == 0);
}

static void	test_str_eq_handles_null(void)
{
	t_lt_result	r;

	r = run(inner_str_null);
	LT_ASSERT(r.failed);
	LT_ASSERT(strcmp(r.failure.left, "NULL") == 0);
	LT_ASSERT(!run(inner_str_both_null).failed);
}

static void	test_str_eq_truncates_long_values(void)
{
	t_lt_result	r;
	size_t		len;

	r = run(inner_str_long);
	len = strlen(r.failure.left);
	LT_ASSERT(len < LT_VALUE_SIZE);
	LT_ASSERT(strcmp(r.failure.left + len - 4, "\"...") == 0);
	LT_ASSERT(strcmp(r.failure.right, "\"y\"") == 0);
}

/* ---------------------------------------------------------------------
** Isolation tests
** ------------------------------------------------------------------- */

/* Same test in-process: proves the child is what confines the effect. */
static int	run_one_reaches_global(void)
{
	g_reached = 0;
	run(inner_writes_global);
	return (g_reached == 1);
}

static t_lt_result	run_forked(t_lt_func func)
{
	const t_lt_test	t = {"inner", func};

	return (lt_run_forked(&t));
}

/* The whole point: this process is still alive to assert afterwards. */
static void	test_crash_becomes_a_failure(void)
{
	t_lt_result	r;

	r = run_forked(inner_crashes);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.signum == SIGSEGV);
}

static void	test_early_exit_becomes_a_failure(void)
{
	t_lt_result	r;

	r = run_forked(inner_exits);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.signum == 0);
	LT_ASSERT(r.exit_status == 3);
}

static void	test_timeout_kills_the_test(void)
{
	t_lt_result	r;

	lt_set_timeout(1);
	r = run_forked(inner_hangs);
	lt_set_timeout(5);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.signum == SIGALRM);
}

/* The failure has to survive the trip through the pipe intact. */
static void	test_failure_crosses_the_pipe(void)
{
	t_lt_result	r;

	r = run_forked(inner_int_eq_fail);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.failure.has_values);
	LT_ASSERT(strcmp(r.failure.expr, "1 + 1 == 3") == 0);
	LT_ASSERT(strcmp(r.failure.left, "2") == 0);
	LT_ASSERT(r.failure.line > 0);
}

static void	test_forked_pass_is_a_pass(void)
{
	LT_ASSERT(!run_forked(inner_int_eq_pass).failed);
}

/* Isolation cuts both ways: the test's side effects stay in the child. */
static void	test_side_effects_stay_in_the_child(void)
{
	g_reached = 0;
	LT_ASSERT(!run_forked(inner_writes_global).failed);
	LT_ASSERT(g_reached == 0);
	LT_ASSERT(run_one_reaches_global());
}

/* ---------------------------------------------------------------------
** Sanity check written WITHOUT LT_ASSERT. If the framework were broken
** badly enough that every assertion passed, all the tests above would
** pass while proving nothing. This check is the root of trust.
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
		LT_TEST(test_int_eq_passes_on_equal),
		LT_TEST(test_int_eq_reports_values),
		LT_TEST(test_int_eq_handles_negatives),
		LT_TEST(test_arguments_evaluated_once),
		LT_TEST(test_uint_eq_prints_unsigned),
		LT_TEST(test_str_eq_compares_content),
		LT_TEST(test_str_eq_escapes_invisible_chars),
		LT_TEST(test_str_eq_handles_null),
		LT_TEST(test_str_eq_truncates_long_values),
		LT_TEST(test_crash_becomes_a_failure),
		LT_TEST(test_early_exit_becomes_a_failure),
		LT_TEST(test_timeout_kills_the_test),
		LT_TEST(test_failure_crosses_the_pipe),
		LT_TEST(test_forked_pass_is_a_pass),
		LT_TEST(test_side_effects_stay_in_the_child),
	};

	if (!sanity_check())
		return (1);
	return (LT_RUN(tests));
}
