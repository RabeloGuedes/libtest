#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <lt_internal.h>

#define LT_GREEN "\x1b[32m"
#define LT_RED "\x1b[91m"
#define LT_RESET "\x1b[0m"

/* Result of the test being run. This is the only global state. */
static t_lt_result	*lt_current(void)
{
	static t_lt_result	current;

	return (&current);
}

t_lt_failure	*lt_fail(t_lt_loc loc)
{
	t_lt_result	*current;

	current = lt_current();
	current->failed = 1;
	current->signum = 0;
	current->exit_status = 0;
	current->failure.expr = loc.expr;
	current->failure.file = loc.file;
	current->failure.line = loc.line;
	current->failure.has_values = 0;
	return (&current->failure);
}

const t_lt_suite	*lt_no_suite(void)
{
	static const t_lt_suite	none;

	return (&none);
}

/* Runs one function of the test's life. Returns 1 if it did not fail. */
static int	lt_stage(t_lt_func func, int phase)
{
	lt_current()->phase = phase;
	func();
	return (!lt_current()->failed);
}

/*
** Teardown runs on the state the test left behind, and its own failure
** only counts if the test did not already fail: the earliest one wins.
*/
static t_lt_result	lt_teardown(t_lt_func teardown, t_lt_result before)
{
	if (!teardown)
		return (before);
	lt_stage(teardown, LT_PHASE_TEARDOWN);
	if (before.failed)
		return (before);
	return (*lt_current());
}

/*
** Every test starts from a clean result. The caller's state is saved
** and restored at the end, so a test can run another test without
** contaminating its own result. A failed setup leaves nothing to tear
** down, so it ends the run right there.
*/
t_lt_result	lt_run_in(const t_lt_suite *suite, const t_lt_test *test)
{
	static const t_lt_result	clean;
	t_lt_result					saved;
	t_lt_result					result;

	saved = *lt_current();
	*lt_current() = clean;
	if (suite->setup && !lt_stage(suite->setup, LT_PHASE_SETUP))
		result = *lt_current();
	else
	{
		lt_stage(test->func, LT_PHASE_TEST);
		result = lt_teardown(suite->teardown, *lt_current());
	}
	*lt_current() = saved;
	return (result);
}

t_lt_result	lt_run_one(const t_lt_test *test)
{
	return (lt_run_in(lt_no_suite(), test));
}

static const char	*lt_paint(const char *code, int color)
{
	if (color)
		return (code);
	return ("");
}

/* A test that died has no location to report, only how it died. */
static void	lt_report_death(const t_lt_result *result)
{
	if (result->signum == SIGALRM)
		printf("      timed out\n");
	else if (result->signum)
		printf("      died with %s\n", lt_signal_name(result->signum));
	else
		printf("      exited with status %d\n", result->exit_status);
}

/* A failure in the test itself needs no note: it is the default. */
static void	lt_report_phase(int phase)
{
	if (phase == LT_PHASE_SETUP)
		printf("      setup failed, the test did not run\n");
	else if (phase == LT_PHASE_TEARDOWN)
		printf("      teardown failed\n");
}

static void	lt_report_failure(const t_lt_result *result)
{
	const t_lt_failure	*f;

	f = &result->failure;
	if (result->signum || result->exit_status)
	{
		lt_report_death(result);
		return ;
	}
	lt_report_phase(result->phase);
	printf("      %s:%d: %s\n", f->file, f->line, f->expr);
	if (f->has_values)
		printf("      left:  %s\n      right: %s\n", f->left, f->right);
}

/* User output is prefixed, so it cannot be read as the runner's own. */
static void	lt_print_indented(const char *text, size_t size)
{
	size_t	i;

	printf("      | ");
	i = 0;
	while (i < size)
	{
		if (text[i] != '\n')
			putchar(text[i]);
		else if (i + 1 < size)
			printf("\n      | ");
		i++;
	}
	printf("\n");
	return ;
}

/* Only a failing test shows what it printed: the rest is noise. */
static void	lt_report_output(void)
{
	const t_lt_stream	*capture;

	capture = lt_captured();
	if (capture->size == 0)
		return ;
	printf("      output:\n");
	lt_print_indented(capture->text, capture->size);
	if (capture->truncated)
		printf("      | ... (truncated)\n");
	return ;
}

static void	lt_report(const t_lt_test *test, const t_lt_result *result,
				int color)
{
	if (!result->failed)
		printf("%sPASS%s  %s\n", lt_paint(LT_GREEN, color),
			lt_paint(LT_RESET, color), test->name);
	else
	{
		printf("%sFAIL%s  %s\n", lt_paint(LT_RED, color),
			lt_paint(LT_RESET, color), test->name);
		lt_report_failure(result);
		lt_report_output();
	}
	fflush(stdout);
}

static int	lt_resolve_color(void)
{
	if (lt_options()->color == LT_COLOR_AUTO)
		return (isatty(STDOUT_FILENO));
	return (lt_options()->color);
}

typedef struct s_lt_tally
{
	size_t	selected;
	size_t	passed;
}	t_lt_tally;

/* Runs the selected tests of a suite and returns how many passed. */
static size_t	lt_run_tests(const t_lt_suite *suite)
{
	t_lt_result	result;
	size_t		passed;
	size_t		i;

	passed = 0;
	i = 0;
	while (i < suite->count)
	{
		if (lt_selected(suite, &suite->tests[i]))
		{
			result = lt_run_forked_in(suite, &suite->tests[i]);
			lt_report(&suite->tests[i], &result, lt_resolve_color());
			passed += !result.failed;
		}
		i++;
	}
	return (passed);
}

/*
** A suite with a name gets a header and its own total. The implicit
** suite of LT_MAIN has none, so its output is just the tests. A suite
** with nothing selected prints nothing at all.
*/
static void	lt_run_suite(const t_lt_suite *suite, t_lt_tally *total)
{
	t_lt_tally	mine;

	mine.selected = lt_count_selected(suite);
	if (mine.selected == 0)
		return ;
	if (suite->name && total->selected > 0)
		printf("\n");
	if (suite->name)
		printf("== %s ==\n", suite->name);
	mine.passed = lt_run_tests(suite);
	if (suite->name)
		printf("%s: %zu/%zu passed\n", suite->name, mine.passed,
			mine.selected);
	total->selected += mine.selected;
	total->passed += mine.passed;
}

int	lt_run_suites(const t_lt_suite *suites, size_t count)
{
	t_lt_tally	total;
	size_t		i;

	total.selected = 0;
	total.passed = 0;
	i = 0;
	while (i < count)
	{
		lt_run_suite(&suites[i], &total);
		i++;
	}
	printf("\n%zu/%zu passed\n", total.passed, total.selected);
	return (total.passed != total.selected);
}

int	lt_run(const t_lt_test *tests, size_t count)
{
	t_lt_suite	implicit;

	implicit = *lt_no_suite();
	implicit.tests = tests;
	implicit.count = count;
	return (lt_run_suites(&implicit, 1));
}

/* Lists the full names, the same ones --filter matches against. */
static void	lt_list_suite(const t_lt_suite *suite)
{
	size_t	i;

	i = 0;
	while (i < suite->count)
	{
		if (lt_selected(suite, &suite->tests[i]))
		{
			if (suite->name)
				printf("%s/", suite->name);
			printf("%s\n", suite->tests[i].name);
		}
		i++;
	}
}

static size_t	lt_count_all_selected(const t_lt_suite *suites, size_t count)
{
	size_t	selected;
	size_t	i;

	selected = 0;
	i = 0;
	while (i < count)
	{
		selected += lt_count_selected(&suites[i]);
		i++;
	}
	return (selected);
}

static void	lt_list(const t_lt_suite *suites, size_t count)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		lt_list_suite(&suites[i]);
		i++;
	}
}

static void	lt_usage(const char *program)
{
	fprintf(stderr, "usage: %s [options]\n", program);
	fprintf(stderr, "  --filter=SUBSTRING  run tests whose name contains it\n");
	fprintf(stderr, "                      (matched against suite/test)\n");
	fprintf(stderr, "  --tag=NAME          run tests carrying the tag\n");
	fprintf(stderr, "  --skip-tag=NAME     never run tests carrying it\n");
	fprintf(stderr, "  --timeout=SECONDS   0 disables the timeout\n");
	fprintf(stderr, "  --no-fork           run in this process, for gdb\n");
	fprintf(stderr, "  --no-capture        let the tests print as they run\n");
	fprintf(stderr, "  --color, --no-color override terminal detection\n");
	fprintf(stderr, "  --list              print the test names and exit\n");
	fprintf(stderr, "  --version           print the libtest version and exit\n");
	return ;
}

/*
** Names what was selected on, so the typo is visible in the message.
** Printing the filter alone would say "(null)" for a bad --tag.
*/
static void	lt_report_no_match(void)
{
	const t_lt_options	*options;
	size_t				i;

	options = lt_options();
	fprintf(stderr, "no test matches:");
	if (options->filter)
		fprintf(stderr, " --filter=%s", options->filter);
	i = 0;
	while (i < options->tag_count)
		fprintf(stderr, " --tag=%s", options->tags[i++]);
	i = 0;
	while (i < options->skip_count)
		fprintf(stderr, " --skip-tag=%s", options->skip_tags[i++]);
	fprintf(stderr, "\n");
	return ;
}

/*
** An empty selection is an error, not a pass: a filter that matches
** nothing would otherwise report a green 0/0 to CI.
*/
int	lt_main_suites(int argc, char **argv, const t_lt_suite *suites,
		size_t count)
{
	int	bad;

	bad = lt_parse_args(argc, argv, lt_options());
	if (bad)
	{
		fprintf(stderr, "unknown or invalid option: %s\n", argv[bad]);
		lt_usage(argv[0]);
		return (2);
	}
	if (lt_options()->version)
	{
		printf("libtest %s\n", LT_VERSION);
		return (0);
	}
	if (lt_options()->list)
	{
		lt_list(suites, count);
		return (0);
	}
	if (lt_count_all_selected(suites, count) == 0)
	{
		lt_report_no_match();
		return (2);
	}
	if (lt_run_suites(suites, count) != 0)
		return (1);
	return (0);
}

int	lt_main(int argc, char **argv, const t_lt_test *tests, size_t count)
{
	t_lt_suite	implicit;

	implicit = *lt_no_suite();
	implicit.tests = tests;
	implicit.count = count;
	return (lt_main_suites(argc, argv, &implicit, 1));
}
