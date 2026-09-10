#include <stdio.h>
#include <unistd.h>
#include <libtest.h>

#define LT_GREEN "\x1b[32m"
#define LT_RED "\x1b[91m"
#define LT_RESET "\x1b[0m"


static t_lt_result	*lt_current(void)
{
	static t_lt_result	current;

	return (&current);
}

static const char	*lt_paint(const char *code, int color)
{
	if (color)
		return (code);
	return ("");
}

int	lt_check(int ok, const char *expr, const char *file, int line)
{
	t_lt_result	*current;

	if (ok)
		return (1);
	current = lt_current();
	current->failed = 1;
	current->failure.expr = expr;
	current->failure.file = file;
	current->failure.line = line;
	return (0);
}

/*
** Saves the state of the caller tests and restores it in the end. This way 
** one test can run another one without contaminate its own result.
*/
t_lt_result	lt_run_one(const t_lt_test *test)
{
	t_lt_result	saved;
	t_lt_result	result;

	saved = *lt_current();
	lt_current()->failed = 0;
	test->func();
	result = *lt_current();
	*lt_current() = saved;
	return (result);
}

static void	lt_report(const t_lt_test *test, const t_lt_result *result, int color)
{
	if (!result->failed)
		printf("%sPASS%s  %s\n", lt_paint(LT_GREEN, color),
			lt_paint(LT_RESET, color), test->name);
	else
		printf("%sFAIL%s  %s\n      %s:%d: %s\n", lt_paint(LT_RED, color),
			lt_paint(LT_RESET, color), test->name, result->failure.file,
			result->failure.line, result->failure.expr);
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
		result = lt_run_one(&tests[i]);
		lt_report(&tests[i], &result, color);
		if (!result.failed)
			passed++;
		i++;
	}
	printf("\n%zu/%zu passed\n", passed, count);
	return (passed != count);
}
