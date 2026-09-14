#include <signal.h>
#include <stdio.h>
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

/*
** Every test starts from a clean result. The caller's state is saved
** and restored at the end, so a test can run another test without
** contaminating its own result.
*/
t_lt_result	lt_run_one(const t_lt_test *test)
{
	static const t_lt_result	clean;
	t_lt_result					saved;
	t_lt_result					result;

	saved = *lt_current();
	*lt_current() = clean;
	test->func();
	result = *lt_current();
	*lt_current() = saved;
	return (result);
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

static void	lt_report_failure(const t_lt_result *result)
{
	const t_lt_failure	*f;

	f = &result->failure;
	if (result->signum || result->exit_status)
	{
		lt_report_death(result);
		return ;
	}
	printf("      %s:%d: %s\n", f->file, f->line, f->expr);
	if (f->has_values)
		printf("      left:  %s\n      right: %s\n", f->left, f->right);
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
	}
	fflush(stdout);
}

int	lt_run(const t_lt_test *tests, size_t count)
{
	t_lt_result	result;
	size_t		passed;
	size_t		i;
	int			color;

	color = isatty(STDOUT_FILENO);
	passed = 0;
	i = 0;
	while (i < count)
	{
		result = lt_run_forked(&tests[i]);
		lt_report(&tests[i], &result, color);
		if (!result.failed)
			passed++;
		i++;
	}
	printf("\n%zu/%zu passed\n", passed, count);
	return (passed != count);
}
