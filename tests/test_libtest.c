/* tmpfile, dup2 and fileno are POSIX, not C99. */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libtest.h>
#include <lt_internal.h>

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
	const t_lt_test	t = {"inner", func, NULL};

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
	const t_lt_test	t = {"inner", func, NULL};

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
	t_lt_result		r;
	unsigned int	saved;

	saved = lt_options()->timeout;
	lt_options()->timeout = 1;
	r = run_forked(inner_hangs);
	lt_options()->timeout = saved;
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
** Option parsing tests
**
** lt_parse_args is pure: it reads argv and writes a struct. That makes
** it testable without running anything.
** ------------------------------------------------------------------- */

static int	parse(t_lt_options *options, char *a, char *b)
{
	char	*argv[4];
	int		argc;

	argv[0] = (char *)"run_tests";
	argc = 1;
	if (a)
		argv[argc++] = a;
	if (b)
		argv[argc++] = b;
	argv[argc] = NULL;
	*options = lt_default_options();
	return (lt_parse_args(argc, argv, options));
}

static void	test_defaults_are_sane(void)
{
	t_lt_options	o;

	o = lt_default_options();
	LT_ASSERT(o.filter == NULL);
	LT_ASSERT(o.fork == 1);
	LT_ASSERT(o.capture == 1);
	LT_ASSERT(o.list == 0);
	LT_ASSERT(o.version == 0);
	LT_ASSERT(o.color == LT_COLOR_AUTO);
	LT_ASSERT_UINT_EQ(o.timeout, 5);
}

static void	test_no_arguments_keeps_defaults(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, NULL, NULL), 0);
	LT_ASSERT(o.fork == 1);
}

/*
** Parsing edits the options it is handed, it does not reset them. That
** is what lets a program set its own defaults before parsing argv.
*/
static void	test_parsing_starts_from_current_options(void)
{
	t_lt_options	o;
	char			*argv[2];

	o = lt_default_options();
	o.timeout = 42;
	argv[0] = (char *)"run_tests";
	argv[1] = (char *)"--no-fork";
	LT_ASSERT_INT_EQ(lt_parse_args(2, argv, &o), 0);
	LT_ASSERT_UINT_EQ(o.timeout, 42);
	LT_ASSERT_INT_EQ(o.fork, 0);
}

static void	test_parses_each_option(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--filter=str_", NULL), 0);
	LT_ASSERT_STR_EQ(o.filter, "str_");
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=0", NULL), 0);
	LT_ASSERT_UINT_EQ(o.timeout, 0);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--no-fork", NULL), 0);
	LT_ASSERT_INT_EQ(o.fork, 0);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--no-capture", NULL), 0);
	LT_ASSERT_INT_EQ(o.capture, 0);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--no-color", NULL), 0);
	LT_ASSERT_INT_EQ(o.color, 0);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--list", NULL), 0);
	LT_ASSERT_INT_EQ(o.list, 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--version", NULL), 0);
	LT_ASSERT_INT_EQ(o.version, 1);
}

/* An empty filter is legal: it matches everything, like no filter. */
static void	test_empty_filter_is_allowed(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--filter=", NULL), 0);
	LT_ASSERT_STR_EQ(o.filter, "");
}

/* The index tells the caller which argument to name in the message. */
static void	test_bad_option_returns_its_index(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--nope", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--no-fork", (char *)"--nope"), 2);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--filter", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"-f", NULL), 1);
}

static void	test_bad_timeout_is_rejected(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=abc", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=5x", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=-1", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--timeout=99999", NULL), 1);
}

/* A half applied option set would be worse than none at all. */
static void	test_bad_argument_leaves_options_untouched(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--no-fork", (char *)"--nope"), 2);
	LT_ASSERT_INT_EQ(o.fork, 1);
}

/*
** A filter that matches nothing has to be an error: reporting a green
** 0/0 to CI would hide a typo in the filter forever. lt_main writes to
** the shared options, so they are saved and put back.
*/
static void	test_empty_selection_is_an_error(void)
{
	const t_lt_test	one[] = {LT_TEST(inner_true)};
	t_lt_options	saved;
	char			*argv[2];
	int			status;

	argv[0] = (char *)"run_tests";
	argv[1] = (char *)"--filter=no_such_test";
	saved = *lt_options();
	status = lt_main(2, argv, one, 1);
	*lt_options() = saved;
	LT_ASSERT_INT_EQ(status, 2);
}

/* ---------------------------------------------------------------------
** Fixture tests
**
** The inner functions leave a letter in g_trace, so a test can check
** not only that something ran but in which order: S setup, T test,
** D teardown, U a second test.
** ------------------------------------------------------------------- */

static char	g_trace[16];
static int	g_ready;

static void	trace(char c)
{
	size_t	len;

	len = strlen(g_trace);
	g_trace[len] = c;
	g_trace[len + 1] = '\0';
}

static void	inner_setup(void)
{
	trace('S');
}

static void	inner_teardown(void)
{
	trace('D');
}

static void	inner_traced(void)
{
	trace('T');
}

static void	inner_traced_second(void)
{
	trace('U');
}

/* Each failing phase has its own expression, to tell them apart. */
static void	inner_setup_fails(void)
{
	trace('S');
	LT_ASSERT(5 == 6);
}

static void	inner_test_fails(void)
{
	trace('T');
	LT_ASSERT(1 == 2);
}

static void	inner_teardown_fails(void)
{
	trace('D');
	LT_ASSERT(3 == 4);
}

static void	inner_setup_prepares(void)
{
	g_ready = 1;
}

static void	inner_needs_setup(void)
{
	LT_ASSERT(g_ready == 1);
}

static t_lt_result	run_in(t_lt_func setup, t_lt_func func, t_lt_func teardown)
{
	const t_lt_test		t = {"inner", func, NULL};
	const t_lt_suite	s = {"suite", setup, teardown, &t, 1, NULL};

	g_trace[0] = '\0';
	return (lt_run_in(&s, &t));
}

static t_lt_result	run_forked_in(t_lt_func setup, t_lt_func func,
						t_lt_func teardown)
{
	const t_lt_test		t = {"inner", func, NULL};
	const t_lt_suite	s = {"suite", setup, teardown, &t, 1, NULL};

	return (lt_run_forked_in(&s, &t));
}

static void	test_fixture_wraps_the_test(void)
{
	t_lt_result	r;

	r = run_in(inner_setup, inner_traced, inner_teardown);
	LT_ASSERT(!r.failed);
	LT_ASSERT(strcmp(g_trace, "STD") == 0);
}

static void	test_fixture_is_optional(void)
{
	LT_ASSERT(!run_in(NULL, inner_traced, NULL).failed);
	LT_ASSERT(strcmp(g_trace, "T") == 0);
	LT_ASSERT(!run_in(inner_setup, inner_traced, NULL).failed);
	LT_ASSERT(strcmp(g_trace, "ST") == 0);
	LT_ASSERT(!run_in(NULL, inner_traced, inner_teardown).failed);
	LT_ASSERT(strcmp(g_trace, "TD") == 0);
}

/* A fatal assertion only returns from the test, so cleanup still runs. */
static void	test_teardown_runs_after_a_failed_test(void)
{
	t_lt_result	r;

	r = run_in(inner_setup, inner_test_fails, inner_teardown);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_TEST);
	LT_ASSERT(strcmp(g_trace, "STD") == 0);
}

/* Nothing was set up, so nothing is torn down and nothing is tested. */
static void	test_failed_setup_skips_test_and_teardown(void)
{
	t_lt_result	r;

	r = run_in(inner_setup_fails, inner_traced, inner_teardown);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_SETUP);
	LT_ASSERT(strcmp(r.failure.expr, "5 == 6") == 0);
	LT_ASSERT(strcmp(g_trace, "S") == 0);
}

static void	test_failed_teardown_is_reported(void)
{
	t_lt_result	r;

	r = run_in(inner_setup, inner_traced, inner_teardown_fails);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_TEARDOWN);
	LT_ASSERT(strcmp(r.failure.expr, "3 == 4") == 0);
	LT_ASSERT(strcmp(g_trace, "STD") == 0);
}

/* Single failure design: the earliest failure is the one reported. */
static void	test_test_failure_wins_over_teardown_failure(void)
{
	t_lt_result	r;

	r = run_in(inner_setup, inner_test_fails, inner_teardown_fails);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_TEST);
	LT_ASSERT(strcmp(r.failure.expr, "1 == 2") == 0);
	LT_ASSERT(strcmp(g_trace, "STD") == 0);
}

/* Like the nested run test, but through every path of lt_run_in. */
static void	test_fixture_run_does_not_leak(void)
{
	run_in(inner_setup_fails, inner_traced, inner_teardown);
	run_in(inner_setup, inner_test_fails, inner_teardown_fails);
	run_in(inner_setup, inner_traced, inner_teardown_fails);
	LT_ASSERT(1);
}

static void	test_fixture_state_stays_in_the_child(void)
{
	g_ready = 0;
	LT_ASSERT(!run_forked_in(inner_setup_prepares, inner_needs_setup,
			NULL).failed);
	LT_ASSERT(g_ready == 0);
}

static void	test_phase_crosses_the_pipe(void)
{
	t_lt_result	r;

	r = run_forked_in(inner_setup_fails, inner_traced, NULL);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_SETUP);
	LT_ASSERT(strcmp(r.failure.expr, "5 == 6") == 0);
	r = run_forked_in(NULL, inner_traced, inner_teardown_fails);
	LT_ASSERT(r.failed);
	LT_ASSERT(r.phase == LT_PHASE_TEARDOWN);
}

static void	test_suite_macro_fills_the_struct(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_true), LT_TEST(inner_false)};
	const t_lt_suite	s = LT_SUITE("db", inner_setup, NULL, tests);

	LT_ASSERT_STR_EQ(s.name, "db");
	LT_ASSERT(s.setup == inner_setup);
	LT_ASSERT(s.teardown == NULL);
	LT_ASSERT(s.tests == tests);
	LT_ASSERT_UINT_EQ(s.count, 2);
}

/* ---------------------------------------------------------------------
** Suite runner tests
**
** The runner prints its report, so these tests read it back: stdout is
** pointed at a temp file, and what was written lands in g_out. That
** happens in a child (the inner tests run through run_forked), so the
** redirection and the option changes never reach the real run.
** ------------------------------------------------------------------- */

static char	g_out[8192];

static const t_lt_test	g_first[] = {
	LT_TEST(inner_traced), LT_TEST(inner_traced_second)};
static const t_lt_test	g_second[] = {LT_TEST(inner_traced)};
static const t_lt_suite	g_suites[] = {
	LT_SUITE("one", inner_setup, inner_teardown, g_first),
	LT_SUITE("two", NULL, NULL, g_second),
};

/* Sends stdout to a temp file and keeps the runner in this process. */
static FILE	*redirect_stdout(void)
{
	FILE	*tmp;

	tmp = tmpfile();
	if (!tmp)
		return (NULL);
	fflush(stdout);
	dup2(fileno(tmp), STDOUT_FILENO);
	*lt_options() = lt_default_options();
	lt_options()->fork = 0;
	lt_options()->color = 0;
	g_trace[0] = '\0';
	return (tmp);
}

static void	read_back(FILE *tmp)
{
	size_t	got;

	fflush(stdout);
	rewind(tmp);
	got = fread(g_out, 1, sizeof(g_out) - 1, tmp);
	g_out[got] = '\0';
	fclose(tmp);
}

/* Up to two extra arguments, NULL when unused. Returns the exit status. */
static int	main_output(const char *a, const char *b,
				const t_lt_suite *suites, size_t count)
{
	char	*argv[4];
	int		argc;
	FILE	*tmp;
	int		status;

	argv[0] = (char *)"run_tests";
	argc = 1;
	if (a)
		argv[argc++] = (char *)a;
	if (b)
		argv[argc++] = (char *)b;
	argv[argc] = NULL;
	tmp = redirect_stdout();
	if (!tmp)
		return (-1);
	status = lt_main_suites(argc, argv, suites, count);
	read_back(tmp);
	return (status);
}

static void	test_filter_matches_suite_and_test_names(void)
{
	LT_ASSERT(lt_name_matches(NULL, "db", "test_insert"));
	LT_ASSERT(lt_name_matches("", "db", "test_insert"));
	LT_ASSERT(lt_name_matches("insert", "db", "test_insert"));
	LT_ASSERT(lt_name_matches("db", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("net", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("update", "db", "test_insert"));
}

/* A filter may span the '/' that joins the suite and the test. */
static void	test_filter_crosses_the_joint(void)
{
	LT_ASSERT(lt_name_matches("db/test_insert", "db", "test_insert"));
	LT_ASSERT(lt_name_matches("db/", "db", "test_insert"));
	LT_ASSERT(lt_name_matches("/test", "db", "test_insert"));
	LT_ASSERT(lt_name_matches("b/test_i", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("db/update", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("net/test", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("xdb/test", "db", "test_insert"));
}

/* Without the '/' the two names are not one string. */
static void	test_filter_does_not_ignore_the_joint(void)
{
	LT_ASSERT(!lt_name_matches("dbtest", "db", "test_insert"));
	LT_ASSERT(!lt_name_matches("dbtest_insert", "db", "test_insert"));
}

static void	test_filter_without_suite_sees_only_the_test(void)
{
	LT_ASSERT(lt_name_matches("test_a", NULL, "test_a"));
	LT_ASSERT(!lt_name_matches("/test_a", NULL, "test_a"));
	LT_ASSERT(!lt_name_matches("db/test_a", NULL, "test_a"));
}

/* Names may hold a '/' themselves: every one is tried as the joint. */
static void	test_filter_tries_every_slash(void)
{
	LT_ASSERT(lt_name_matches("a/b/c", "a/b", "c"));
	LT_ASSERT(lt_name_matches("a/b/c", "a", "b/c"));
	LT_ASSERT(!lt_name_matches("a/b/d", "a/b", "c"));
}

static void	inner_report_of_two_suites(void)
{
	LT_ASSERT_INT_EQ(main_output(NULL, NULL, g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "STDSUDT");
	LT_ASSERT_STR_EQ(g_out,
		"== one ==\n"
		"PASS  inner_traced\n"
		"PASS  inner_traced_second\n"
		"one: 2/2 passed\n"
		"\n"
		"== two ==\n"
		"PASS  inner_traced\n"
		"two: 1/1 passed\n"
		"\n"
		"3/3 passed\n");
}

/* A suite with nothing selected prints nothing: no header, no blank line. */
static void	inner_report_skips_unselected_suites(void)
{
	LT_ASSERT_INT_EQ(main_output("--filter=two/", NULL, g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "T");
	LT_ASSERT_STR_EQ(g_out,
		"== two ==\n"
		"PASS  inner_traced\n"
		"two: 1/1 passed\n"
		"\n"
		"1/1 passed\n");
}

static void	inner_filter_picks_a_test(void)
{
	LT_ASSERT_INT_EQ(main_output("--filter=one/inner_traced_s", NULL,
			g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "SUD");
	LT_ASSERT_STR_EQ(g_out,
		"== one ==\n"
		"PASS  inner_traced_second\n"
		"one: 1/1 passed\n"
		"\n"
		"1/1 passed\n");
}

/* --version wins over everything and runs no test. */
static void	inner_version_prints_and_stops(void)
{
	LT_ASSERT_INT_EQ(main_output("--version", NULL, g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "");
	LT_ASSERT_STR_EQ(g_out, "libtest " LT_VERSION "\n");
}

static void	inner_list_prints_full_names(void)
{
	LT_ASSERT_INT_EQ(main_output("--list", NULL, g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "");
	LT_ASSERT_STR_EQ(g_out,
		"one/inner_traced\none/inner_traced_second\ntwo/inner_traced\n");
	LT_ASSERT_INT_EQ(main_output("--list", "--filter=/inner_traced_s",
			g_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_out, "one/inner_traced_second\n");
}

/* LT_MAIN has no suite name, so its output must stay what it always was. */
static void	inner_bare_tests_print_no_headers(void)
{
	char	*argv[3];
	FILE	*tmp;
	int		status;

	argv[0] = (char *)"run_tests";
	argv[1] = (char *)"--no-fork";
	argv[2] = NULL;
	tmp = redirect_stdout();
	LT_ASSERT(tmp != NULL);
	status = LT_MAIN(2, argv, g_first);
	read_back(tmp);
	LT_ASSERT_INT_EQ(status, 0);
	LT_ASSERT_STR_EQ(g_out,
		"PASS  inner_traced\nPASS  inner_traced_second\n\n2/2 passed\n");
}

static void	inner_report_names_a_failed_setup(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_traced)};
	const t_lt_suite	suites[] = {
		LT_SUITE("db", inner_setup_fails, inner_teardown, tests)};

	LT_ASSERT_INT_EQ(main_output(NULL, NULL, suites, 1), 1);
	LT_ASSERT_STR_EQ(g_trace, "S");
	LT_ASSERT(strstr(g_out, "FAIL  inner_traced\n"
			"      setup failed, the test did not run\n") != NULL);
	LT_ASSERT(strstr(g_out, ": 5 == 6\n") != NULL);
	LT_ASSERT(strstr(g_out, "db: 0/1 passed\n\n0/1 passed\n") != NULL);
}

static void	inner_report_names_a_failed_teardown(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_traced)};
	const t_lt_suite	suites[] = {
		LT_SUITE("db", NULL, inner_teardown_fails, tests)};

	LT_ASSERT_INT_EQ(main_output(NULL, NULL, suites, 1), 1);
	LT_ASSERT_STR_EQ(g_trace, "TD");
	LT_ASSERT(strstr(g_out, "FAIL  inner_traced\n"
			"      teardown failed\n") != NULL);
}

/* A failed test says nothing about phases: that is the default. */
static void	inner_report_of_a_failed_test_has_no_phase_note(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_test_fails)};
	const t_lt_suite	suites[] = {LT_SUITE("db", inner_setup, NULL, tests)};

	LT_ASSERT_INT_EQ(main_output(NULL, NULL, suites, 1), 1);
	LT_ASSERT(strstr(g_out, "FAIL  inner_test_fails\n      ") != NULL);
	LT_ASSERT(strstr(g_out, "failed,") == NULL);
	LT_ASSERT(strstr(g_out, "teardown failed") == NULL);
}

static void	inner_suites_run_macro(void)
{
	FILE	*tmp;
	int		status;

	tmp = redirect_stdout();
	LT_ASSERT(tmp != NULL);
	status = LT_SUITES_RUN(g_suites);
	read_back(tmp);
	LT_ASSERT_INT_EQ(status, 0);
	LT_ASSERT_STR_EQ(g_trace, "STDSUDT");
}

static void	test_report_groups_tests_by_suite(void)
{
	LT_ASSERT(!run_forked(inner_report_of_two_suites).failed);
	LT_ASSERT(!run_forked(inner_report_skips_unselected_suites).failed);
}

static void	test_report_names_the_failed_phase(void)
{
	LT_ASSERT(!run_forked(inner_report_names_a_failed_setup).failed);
	LT_ASSERT(!run_forked(inner_report_names_a_failed_teardown).failed);
	LT_ASSERT(!run_forked(inner_report_of_a_failed_test_has_no_phase_note)
		.failed);
}

static void	test_filter_selects_a_test_of_a_suite(void)
{
	LT_ASSERT(!run_forked(inner_filter_picks_a_test).failed);
}

static void	test_list_prints_full_names(void)
{
	LT_ASSERT(!run_forked(inner_list_prints_full_names).failed);
}

static void	test_version_is_printed(void)
{
	LT_ASSERT(!run_forked(inner_version_prints_and_stops).failed);
}

static void	test_bare_tests_print_no_headers(void)
{
	LT_ASSERT(!run_forked(inner_bare_tests_print_no_headers).failed);
}

static void	test_suites_run_macro_runs_everything(void)
{
	LT_ASSERT(!run_forked(inner_suites_run_macro).failed);
}

static void	test_empty_suite_selection_is_an_error(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_true)};
	const t_lt_suite	suites[] = {LT_SUITE("one", NULL, NULL, tests)};
	t_lt_options		saved;
	char				*argv[2];
	int					status;

	argv[0] = (char *)"run_tests";
	argv[1] = (char *)"--filter=two/inner_true";
	saved = *lt_options();
	status = LT_SUITES_MAIN(2, argv, suites);
	*lt_options() = saved;
	LT_ASSERT_INT_EQ(status, 2);
}

/* ---------------------------------------------------------------------
** Tag tests
**
** lt_has_tag is pure, so it is checked directly. Selection by tag goes
** through the runner, in a child, like the suite tests above.
** ------------------------------------------------------------------- */

static const t_lt_test	g_tagged[] = {
	LT_TEST_TAGGED(inner_traced, "unit"),
	LT_TEST_TAGGED(inner_traced_second, "unit,slow"),
};
static const t_lt_test	g_plain_tests[] = {LT_TEST(inner_traced)};
static const t_lt_suite	g_tag_suites[] = {
	LT_SUITE("tagged", NULL, NULL, g_tagged),
	LT_SUITE_TAGGED("e2e", NULL, NULL, g_plain_tests, "integration"),
};

static void	test_has_tag_matches_whole_elements(void)
{
	LT_ASSERT(lt_has_tag("unit", "unit"));
	LT_ASSERT(lt_has_tag("unit,slow", "unit"));
	LT_ASSERT(lt_has_tag("unit,slow", "slow"));
	LT_ASSERT(lt_has_tag("a,b,c", "b"));
	LT_ASSERT(!lt_has_tag("unit", "slow"));
}

/* The whole reason the match is exact: these must not select. */
static void	test_has_tag_is_not_a_substring_match(void)
{
	LT_ASSERT(!lt_has_tag("unitary", "unit"));
	LT_ASSERT(!lt_has_tag("unit", "unitary"));
	LT_ASSERT(!lt_has_tag("unit,slower", "slow"));
	LT_ASSERT(!lt_has_tag("preunit", "unit"));
}

static void	test_has_tag_handles_edges(void)
{
	LT_ASSERT(!lt_has_tag(NULL, "unit"));
	LT_ASSERT(!lt_has_tag("unit", NULL));
	LT_ASSERT(!lt_has_tag("unit", ""));
	LT_ASSERT(!lt_has_tag("", "unit"));
	LT_ASSERT(lt_has_tag("a,,b", "b"));
	LT_ASSERT(lt_has_tag(",a,", "a"));
	LT_ASSERT(!lt_has_tag(",,", "a"));
}

/* LT_TEST and LT_SUITE must keep leaving the new field NULL. */
static void	test_tag_macros_fill_the_field(void)
{
	const t_lt_test		plain = LT_TEST(inner_true);
	const t_lt_test		tagged = LT_TEST_TAGGED(inner_true, "unit,slow");
	const t_lt_suite	bare = LT_SUITE("s", NULL, NULL, g_plain_tests);
	const t_lt_suite	with = LT_SUITE_TAGGED("s", NULL, NULL,
			g_plain_tests, "e2e");

	LT_ASSERT(plain.tags == NULL);
	LT_ASSERT_STR_EQ(tagged.tags, "unit,slow");
	LT_ASSERT_STR_EQ(tagged.name, "inner_true");
	LT_ASSERT(bare.tags == NULL);
	LT_ASSERT_STR_EQ(with.tags, "e2e");
	LT_ASSERT_UINT_EQ(with.count, 1);
}

static void	inner_no_tag_runs_everything(void)
{
	LT_ASSERT_INT_EQ(main_output(NULL, NULL, g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "TUT");
}

static void	inner_tag_selects_its_tests(void)
{
	LT_ASSERT_INT_EQ(main_output("--tag=slow", NULL, g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "U");
}

/* The suite's tag reaches a test that carries none of its own. */
static void	inner_suite_tag_reaches_its_tests(void)
{
	LT_ASSERT_INT_EQ(main_output("--tag=integration", NULL,
			g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "T");
}

/* Two --tag are an OR, and they reach across suites. */
static void	inner_tags_are_ored(void)
{
	LT_ASSERT_INT_EQ(main_output("--tag=slow", "--tag=integration",
			g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "UT");
}

static void	inner_skip_tag_excludes(void)
{
	LT_ASSERT_INT_EQ(main_output("--skip-tag=slow", NULL, g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "TT");
}

/* inner_traced_second is both "unit" and "slow": the skip wins. */
static void	inner_skip_beats_tag(void)
{
	LT_ASSERT_INT_EQ(main_output("--tag=unit", "--skip-tag=slow",
			g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "T");
}

/* Name and tag are an AND: both have to agree. */
static void	inner_tag_combines_with_filter(void)
{
	LT_ASSERT_INT_EQ(main_output("--tag=unit",
			"--filter=tagged/inner_traced_s", g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "U");
}

static void	inner_list_respects_tags(void)
{
	LT_ASSERT_INT_EQ(main_output("--list", "--tag=slow", g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_out, "tagged/inner_traced_second\n");
}

/*
** The message names the tag. Printing the filter alone used to say
** "(null)" here, which told the reader nothing.
*/
static void	inner_no_match_names_the_tag(void)
{
	char	*argv[2];
	FILE	*tmp;
	size_t	got;
	int		status;

	tmp = tmpfile();
	LT_ASSERT(tmp != NULL);
	argv[0] = (char *)"run_tests";
	argv[1] = (char *)"--tag=nope";
	*lt_options() = lt_default_options();
	dup2(fileno(tmp), STDERR_FILENO);
	status = lt_main_suites(2, argv, g_tag_suites, 2);
	fflush(stderr);
	rewind(tmp);
	got = fread(g_out, 1, sizeof(g_out) - 1, tmp);
	g_out[got] = '\0';
	fclose(tmp);
	LT_ASSERT_INT_EQ(status, 2);
	LT_ASSERT_STR_EQ(g_out, "no test matches: --tag=nope\n");
}

static void	test_tags_select_tests(void)
{
	LT_ASSERT(!run_forked(inner_no_tag_runs_everything).failed);
	LT_ASSERT(!run_forked(inner_tag_selects_its_tests).failed);
	LT_ASSERT(!run_forked(inner_suite_tag_reaches_its_tests).failed);
	LT_ASSERT(!run_forked(inner_tags_are_ored).failed);
}

/* Every --skip-tag counts, not just the first. */
static void	inner_skip_tags_are_all_applied(void)
{
	LT_ASSERT_INT_EQ(main_output("--skip-tag=slow", "--skip-tag=integration",
			g_tag_suites, 2), 0);
	LT_ASSERT_STR_EQ(g_trace, "T");
}

static void	test_skip_tag_excludes_tests(void)
{
	LT_ASSERT(!run_forked(inner_skip_tag_excludes).failed);
	LT_ASSERT(!run_forked(inner_skip_beats_tag).failed);
	LT_ASSERT(!run_forked(inner_skip_tags_are_all_applied).failed);
}

static void	test_tags_combine_with_filter_and_list(void)
{
	LT_ASSERT(!run_forked(inner_tag_combines_with_filter).failed);
	LT_ASSERT(!run_forked(inner_list_respects_tags).failed);
}

static void	test_unmatched_tag_is_an_error(void)
{
	LT_ASSERT(!run_forked(inner_no_match_names_the_tag).failed);
}

static void	test_parses_tags(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--tag=unit", NULL), 0);
	LT_ASSERT_UINT_EQ(o.tag_count, 1);
	LT_ASSERT_STR_EQ(o.tags[0], "unit");
	LT_ASSERT_UINT_EQ(o.skip_count, 0);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--tag=unit", (char *)"--tag=slow"), 0);
	LT_ASSERT_UINT_EQ(o.tag_count, 2);
	LT_ASSERT_STR_EQ(o.tags[1], "slow");
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--skip-tag=slow", NULL), 0);
	LT_ASSERT_UINT_EQ(o.skip_count, 1);
	LT_ASSERT_STR_EQ(o.skip_tags[0], "slow");
	LT_ASSERT_UINT_EQ(o.tag_count, 0);
}

/* A comma is rejected, not split: --tag=a,b would never match. */
static void	test_bad_tag_is_rejected(void)
{
	t_lt_options	o;

	LT_ASSERT_INT_EQ(parse(&o, (char *)"--tag=", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--tag=a,b", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--skip-tag=", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--skip-tag=a,b", NULL), 1);
	LT_ASSERT_INT_EQ(parse(&o, (char *)"--tag", NULL), 1);
}

/* The tag past the last slot is a usage error, not a silent drop. */
static void	test_too_many_tags_are_rejected(void)
{
	t_lt_options	o;
	char			*argv[LT_MAX_TAGS + 2];
	int				i;

	argv[0] = (char *)"run_tests";
	i = 0;
	while (i < LT_MAX_TAGS + 1)
	{
		argv[i + 1] = (char *)"--tag=unit";
		i++;
	}
	o = lt_default_options();
	LT_ASSERT_INT_EQ(lt_parse_args(LT_MAX_TAGS + 2, argv, &o),
		LT_MAX_TAGS + 1);
	LT_ASSERT_UINT_EQ(o.tag_count, 0);
}

/* ---------------------------------------------------------------------
** Capture tests
**
** Capture only happens around a fork, so these drive the runner with
** forking on. Side effects then stay in the grandchild, which is why
** they read the report instead of g_trace.
** ------------------------------------------------------------------- */

static void	inner_prints_and_fails(void)
{
	printf("on stdout\n");
	fprintf(stderr, "on stderr\n");
	LT_ASSERT(1 == 2);
}

static void	inner_prints_and_passes(void)
{
	printf("nobody should see this\n");
	LT_ASSERT(1);
}

static void	inner_fails_quietly(void)
{
	LT_ASSERT(3 == 4);
}

static void	inner_prints_then_crashes(void)
{
	int	*ptr;

	printf("last words\n");
	ptr = NULL;
	LT_ASSERT(*ptr == 0);
}

static void	inner_prints_a_lot(void)
{
	int	i;

	i = 0;
	while (i < 500)
	{
		printf("0123456789");
		i++;
	}
	LT_ASSERT(1 == 2);
}

static int	capture_run(const t_lt_suite *suites, size_t count, int capture)
{
	FILE	*tmp;

	tmp = redirect_stdout();
	if (!tmp)
		return (0);
	lt_options()->fork = 1;
	lt_options()->capture = capture;
	lt_run_suites(suites, count);
	read_back(tmp);
	return (1);
}

static void	inner_failure_shows_its_output(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_and_fails)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};

	LT_ASSERT(capture_run(suites, 1, 1));
	LT_ASSERT(strstr(g_out, "      output:\n") != NULL);
	LT_ASSERT(strstr(g_out, "      | on stdout\n") != NULL);
	LT_ASSERT(strstr(g_out, "      | on stderr\n") != NULL);
	LT_ASSERT(strstr(g_out, "(truncated)") == NULL);
}

static void	inner_pass_hides_its_output(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_and_passes)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};

	LT_ASSERT(capture_run(suites, 1, 1));
	LT_ASSERT(strstr(g_out, "PASS  inner_prints_and_passes") != NULL);
	LT_ASSERT(strstr(g_out, "nobody should see this") == NULL);
	LT_ASSERT(strstr(g_out, "output:") == NULL);
}

/* The whole reason for a file: a pipe would have lost this. */
static void	inner_output_survives_a_crash(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_then_crashes)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};

	LT_ASSERT(capture_run(suites, 1, 1));
	LT_ASSERT(strstr(g_out, "died with SIGSEGV") != NULL);
	LT_ASSERT(strstr(g_out, "      | last words") != NULL);
}

/* The start is what is kept, and the reader is told it was cut. */
static void	inner_long_output_is_truncated(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_a_lot)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};

	LT_ASSERT(capture_run(suites, 1, 1));
	LT_ASSERT(strstr(g_out, "      | 0123456789") != NULL);
	LT_ASSERT(strstr(g_out, "... (truncated)") != NULL);
}

/* Without capture the test prints where it always did. */
static void	inner_no_capture_lets_output_through(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_and_fails)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};

	LT_ASSERT(capture_run(suites, 1, 0));
	LT_ASSERT(strstr(g_out, "on stdout") != NULL);
	LT_ASSERT(strstr(g_out, "output:") == NULL);
}

/* In process there is no child to capture, so nothing is captured. */
static void	inner_no_fork_means_no_capture(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_and_fails)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};
	FILE				*tmp;

	tmp = redirect_stdout();
	LT_ASSERT(tmp != NULL);
	lt_run_suites(suites, 1);
	read_back(tmp);
	LT_ASSERT(strstr(g_out, "on stdout") != NULL);
	LT_ASSERT(strstr(g_out, "output:") == NULL);
}

/* The buffer is per test: the quiet one must not inherit the noisy one. */
static void	inner_output_does_not_leak_between_tests(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_and_fails),
		LT_TEST(inner_fails_quietly)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};
	const char			*second;

	LT_ASSERT(capture_run(suites, 1, 1));
	second = strstr(g_out, "FAIL  inner_fails_quietly");
	LT_ASSERT(second != NULL);
	LT_ASSERT(strstr(second, "on stdout") == NULL);
	LT_ASSERT(strstr(second, "output:") == NULL);
}

/*
** The temp file is unlinked the moment it exists, so it cannot outlive
** the run even if the test crashes: a file with no name has no links.
** This is the one test that reaches into lt_internal.h, because the
** effect is invisible from the public API.
*/
static void	test_capture_file_is_unlinked(void)
{
	struct stat	st;
	int			fd;

	lt_options()->capture = 1;
	fd = lt_capture_start();
	LT_ASSERT(fd >= 0);
	LT_ASSERT_INT_EQ(fstat(fd, &st), 0);
	LT_ASSERT_UINT_EQ(st.st_nlink, 0);
	close(fd);
}

/*
** The buffer always keeps its last byte for the terminator, so text is
** a valid C string however much a test prints.
*/
static void	inner_capture_keeps_room_for_a_terminator(void)
{
	const t_lt_test		tests[] = {LT_TEST(inner_prints_a_lot)};
	const t_lt_suite	suites[] = {LT_SUITE("cap", NULL, NULL, tests)};
	const t_lt_stream	*capture;

	LT_ASSERT(capture_run(suites, 1, 1));
	capture = lt_captured();
	LT_ASSERT(capture->size < LT_OUTPUT_SIZE);
	LT_ASSERT(capture->text[capture->size] == '\0');
	LT_ASSERT(capture->truncated);
}

static void	test_failing_test_shows_its_output(void)
{
	LT_ASSERT(!run_forked(inner_failure_shows_its_output).failed);
	LT_ASSERT(!run_forked(inner_pass_hides_its_output).failed);
}

static void	test_output_survives_a_crash(void)
{
	LT_ASSERT(!run_forked(inner_output_survives_a_crash).failed);
}

static void	test_long_output_is_truncated(void)
{
	LT_ASSERT(!run_forked(inner_long_output_is_truncated).failed);
	LT_ASSERT(!run_forked(inner_capture_keeps_room_for_a_terminator).failed);
}

static void	test_capture_can_be_turned_off(void)
{
	LT_ASSERT(!run_forked(inner_no_capture_lets_output_through).failed);
	LT_ASSERT(!run_forked(inner_no_fork_means_no_capture).failed);
}

static void	test_output_does_not_leak_between_tests(void)
{
	LT_ASSERT(!run_forked(inner_output_does_not_leak_between_tests).failed);
}

/* ---------------------------------------------------------------------
** Exec tests
**
** These need no inner test: lt_exec is called straight from the test,
** which already runs in its own process. /bin/sh is the program under
** test because it can be told to do anything, on macOS and on Linux.
** ------------------------------------------------------------------- */

static void	test_exec_reports_the_exit_status(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c", "exit 3");
	LT_ASSERT(proc.started);
	LT_ASSERT_INT_EQ(proc.exit_status, 3);
	LT_ASSERT_INT_EQ(proc.signum, 0);
	LT_ASSERT_INT_EQ(proc.timed_out, 0);
	LT_EXEC(&proc, "/bin/sh", "-c", "exit 0");
	LT_ASSERT_INT_EQ(proc.exit_status, 0);
}

static void	test_exec_keeps_the_streams_apart(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c", "printf hello; printf oops >&2");
	LT_ASSERT_STR_EQ(proc.out.text, "hello");
	LT_ASSERT_STR_EQ(proc.err.text, "oops");
	LT_ASSERT_UINT_EQ(proc.out.size, 5);
	LT_ASSERT_UINT_EQ(proc.err.size, 4);
	LT_ASSERT_INT_EQ(proc.out.truncated, 0);
}

/* argv[0] goes through PATH, like execvp, not only absolute paths. */
static void	test_exec_searches_the_path(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "sh", "-c", "printf found");
	LT_ASSERT(proc.started);
	LT_ASSERT_STR_EQ(proc.out.text, "found");
}

static void	test_exec_feeds_stdin(void)
{
	t_lt_process	proc;

	LT_EXEC_IN(&proc, "ping\n", "/bin/cat");
	LT_ASSERT_STR_EQ(proc.out.text, "ping\n");
}

/*
** With no input the program must still get end of file at once. If
** stdin were left alone this would hang until the timeout.
*/
static void	test_exec_gives_empty_stdin_by_default(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/cat");
	LT_ASSERT(proc.started);
	LT_ASSERT_INT_EQ(proc.timed_out, 0);
	LT_ASSERT_UINT_EQ(proc.out.size, 0);
}

/*
** A program that is not there must not look like one that exited 127,
** which is what the shell would report. Everything else stays zero.
*/
static void	test_exec_reports_a_missing_program(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "./no_such_program_anywhere", "--flag");
	LT_ASSERT_INT_EQ(proc.started, 0);
	LT_ASSERT_INT_EQ(proc.exit_status, 0);
	LT_ASSERT_INT_EQ(proc.signum, 0);
	LT_ASSERT_UINT_EQ(proc.out.size, 0);
}

static void	test_exec_reports_a_signal(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c", "kill -SEGV $$");
	LT_ASSERT(proc.started);
	LT_ASSERT_INT_EQ(proc.signum, SIGSEGV);
	LT_ASSERT_INT_EQ(proc.timed_out, 0);
}

/* The alarm is set before the exec, so the program inherits it. */
static void	test_exec_times_out(void)
{
	t_lt_process	proc;

	lt_options()->timeout = 1;
	LT_EXEC(&proc, "/bin/sh", "-c", "sleep 30");
	LT_ASSERT(proc.started);
	LT_ASSERT_INT_EQ(proc.timed_out, 1);
	LT_ASSERT_INT_EQ(proc.signum, SIGALRM);
}

static void	test_exec_truncates_a_long_output(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c",
		"i=0; while [ $i -lt 500 ]; do printf 0123456789; i=$((i+1)); done");
	LT_ASSERT(proc.out.truncated);
	LT_ASSERT(proc.out.size < LT_OUTPUT_SIZE);
	LT_ASSERT(proc.out.text[proc.out.size] == '\0');
	LT_ASSERT_INT_EQ(strncmp(proc.out.text, "0123456789", 10), 0);
}

/* The struct is cleared first, so a reused one shows nothing stale. */
static void	test_exec_clears_the_struct(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c", "printf noisy; printf loud >&2; exit 4");
	LT_ASSERT_UINT_EQ(proc.out.size, 5);
	LT_EXEC(&proc, "/bin/sh", "-c", "exit 0");
	LT_ASSERT_UINT_EQ(proc.out.size, 0);
	LT_ASSERT_UINT_EQ(proc.err.size, 0);
	LT_ASSERT_INT_EQ(proc.exit_status, 0);
}

/* The macro only adds the NULL: an argv built by hand works the same. */
static void	test_exec_takes_a_plain_argv(void)
{
	t_lt_process	proc;
	char			*argv[4];

	argv[0] = (char *)"/bin/sh";
	argv[1] = (char *)"-c";
	argv[2] = (char *)"printf plain";
	argv[3] = NULL;
	lt_exec(&proc, argv, NULL);
	LT_ASSERT(proc.started);
	LT_ASSERT_STR_EQ(proc.out.text, "plain");
}

/*
** The program must inherit none of our descriptors: the temp files are
** dropped once duplicated, and the pipe that reports a failed exec is
** close-on-exec. Anything above 2 open in the child is a leak.
**
** This only holds where the environment itself keeps descriptors tidy.
** Under an emulator (qemu-user, for one) a bare fork and exec already
** leaves several open, and then there is nothing to compare against:
** hence the tag, so such a run can skip it with --skip-tag=clean-fds.
*/
static void	test_exec_leaks_no_descriptors(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/sh", "-c",
		"for n in 3 4 5 6 7 8 9 10 11 12; do "
		"[ -e /dev/fd/$n ] && printf \"leak$n\"; done; printf clean");
	LT_ASSERT(proc.started);
	LT_ASSERT_STR_EQ(proc.out.text, "clean");
}

/*
** Run with stdin closed (./run_tests <&-) the temp file lands on
** descriptor 0 itself, and moving it must not close what it just put
** there. This test closes stdin in its own process to force that.
*/
static void	test_exec_survives_a_closed_stdin(void)
{
	t_lt_process	proc;

	close(STDIN_FILENO);
	LT_EXEC_IN(&proc, "ping\n", "/bin/cat");
	LT_ASSERT(proc.started);
	LT_ASSERT_STR_EQ(proc.out.text, "ping\n");
}

/* One argument or several: the macro closes the array either way. */
static void	test_exec_macro_handles_any_arity(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "/bin/echo");
	LT_ASSERT_STR_EQ(proc.out.text, "\n");
	LT_EXEC(&proc, "/bin/echo", "a", "b", "c");
	LT_ASSERT_STR_EQ(proc.out.text, "a b c\n");
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

int	main(int argc, char **argv)
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
		LT_TEST(test_defaults_are_sane),
		LT_TEST(test_no_arguments_keeps_defaults),
		LT_TEST(test_parsing_starts_from_current_options),
		LT_TEST(test_parses_each_option),
		LT_TEST(test_empty_filter_is_allowed),
		LT_TEST(test_bad_option_returns_its_index),
		LT_TEST(test_bad_timeout_is_rejected),
		LT_TEST(test_bad_argument_leaves_options_untouched),
		LT_TEST(test_empty_selection_is_an_error),
		LT_TEST(test_fixture_wraps_the_test),
		LT_TEST(test_fixture_is_optional),
		LT_TEST(test_teardown_runs_after_a_failed_test),
		LT_TEST(test_failed_setup_skips_test_and_teardown),
		LT_TEST(test_failed_teardown_is_reported),
		LT_TEST(test_test_failure_wins_over_teardown_failure),
		LT_TEST(test_fixture_run_does_not_leak),
		LT_TEST(test_fixture_state_stays_in_the_child),
		LT_TEST(test_phase_crosses_the_pipe),
		LT_TEST(test_suite_macro_fills_the_struct),
		LT_TEST(test_filter_matches_suite_and_test_names),
		LT_TEST(test_filter_crosses_the_joint),
		LT_TEST(test_filter_does_not_ignore_the_joint),
		LT_TEST(test_filter_without_suite_sees_only_the_test),
		LT_TEST(test_filter_tries_every_slash),
		LT_TEST(test_report_groups_tests_by_suite),
		LT_TEST(test_report_names_the_failed_phase),
		LT_TEST(test_filter_selects_a_test_of_a_suite),
		LT_TEST(test_list_prints_full_names),
		LT_TEST(test_version_is_printed),
		LT_TEST(test_bare_tests_print_no_headers),
		LT_TEST(test_suites_run_macro_runs_everything),
		LT_TEST(test_empty_suite_selection_is_an_error),
		LT_TEST(test_has_tag_matches_whole_elements),
		LT_TEST(test_has_tag_is_not_a_substring_match),
		LT_TEST(test_has_tag_handles_edges),
		LT_TEST(test_tag_macros_fill_the_field),
		LT_TEST(test_tags_select_tests),
		LT_TEST(test_skip_tag_excludes_tests),
		LT_TEST(test_tags_combine_with_filter_and_list),
		LT_TEST(test_unmatched_tag_is_an_error),
		LT_TEST(test_parses_tags),
		LT_TEST(test_bad_tag_is_rejected),
		LT_TEST(test_too_many_tags_are_rejected),
		LT_TEST(test_capture_file_is_unlinked),
		LT_TEST(test_failing_test_shows_its_output),
		LT_TEST(test_output_survives_a_crash),
		LT_TEST(test_long_output_is_truncated),
		LT_TEST(test_capture_can_be_turned_off),
		LT_TEST(test_output_does_not_leak_between_tests),
		LT_TEST(test_exec_reports_the_exit_status),
		LT_TEST(test_exec_keeps_the_streams_apart),
		LT_TEST(test_exec_searches_the_path),
		LT_TEST(test_exec_feeds_stdin),
		LT_TEST(test_exec_gives_empty_stdin_by_default),
		LT_TEST(test_exec_reports_a_missing_program),
		LT_TEST(test_exec_reports_a_signal),
		LT_TEST_TAGGED(test_exec_times_out, "slow"),
		LT_TEST(test_exec_truncates_a_long_output),
		LT_TEST(test_exec_clears_the_struct),
		LT_TEST(test_exec_takes_a_plain_argv),
		LT_TEST_TAGGED(test_exec_leaks_no_descriptors, "clean-fds"),
		LT_TEST(test_exec_survives_a_closed_stdin),
		LT_TEST(test_exec_macro_handles_any_arity),
	};

	if (!sanity_check())
		return (1);
	return (LT_MAIN(argc, argv, tests));
}
