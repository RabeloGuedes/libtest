#include <stdio.h>
#include <unistd.h>
#include <libtest.h>

#define LT_GREEN "\x1b[32m"
#define LT_RED "\x1b[91m"
#define LT_RESET "\x1b[0m"

typedef struct s_lt_failure
{
	const char	*expr;
	const char	*file;
	int			line;
}	t_lt_failure;

typedef struct s_lt_state
{
	int				failed;
	int				color;
	t_lt_failure	failure;
}	t_lt_state;

static t_lt_state	*lt_state(void)
{
	static t_lt_state	state;

	return (&state);
}

static const char	*lt_paint(const char *code)
{
	if (lt_state()->color)
		return (code);
	return ("");
}

int	lt_check(int ok, const char *expr, const char *file, int line)
{
	t_lt_state	*state;

	if (ok)
		return (1);
	state = lt_state();
	state->failed = 1;
	state->failure.expr = expr;
	state->failure.file = file;
	state->failure.line = line;
	return (0);
}

static void	lt_report(const t_lt_test *test, const t_lt_state *state)
{
	if (!state->failed)
		printf("%sPASS%s  %s\n", lt_paint(LT_GREEN), lt_paint(LT_RESET),
			test->name);
	else
		printf("%sFAIL%s  %s\n      %s:%d: %s\n", lt_paint(LT_RED),
			lt_paint(LT_RESET), test->name, state->failure.file,
			state->failure.line, state->failure.expr);
	fflush(stdout);
}

int	lt_run(const t_lt_test *tests, size_t count)
{
	t_lt_state	*state;
	size_t		passed;
	size_t		i;

	state = lt_state();
	state->color = isatty(STDOUT_FILENO);
	passed = 0;
	i = 0;
	while (i < count)
	{
		state->failed = 0;
		tests[i].func();
		lt_report(&tests[i], state);
		if (!state->failed)
			passed++;
		i++;
	}
	printf("\n%zu/%zu passed\n", passed, count);
	return (passed != count);
}
