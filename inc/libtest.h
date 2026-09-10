#ifndef LIBTEST_H
# define LIBTEST_H

# include <stddef.h>

typedef void (*t_lt_func)(void);
typedef struct s_lt_test
{
	const char	*name;
	t_lt_func	func;
}	t_lt_test;

int	lt_run(const t_lt_test *tests, size_t count);

/*
** Internal: it's only public so the macro can expand it in the callers code.
** Do not call it directly.
*/
int	lt_check(int ok, const char *expr, const char *file, int line);

/*
** Fatal: at fail, register where it fails and returns from the test function.
** This is the reason why it should only be used directly from t_lt_func.
*/

# define LT_ASSERT(cond) \
	do { \
		if (!lt_check(!!(cond), #cond, __FILE__, __LINE__)) \
			return ; \
	} while (0)

# define LT_TEST(func) { #func, func }

/* Only works with arrays, not with pointers. Remember, arrays passed to a function decays into a pointer. */
# define LT_RUN(tests) lt_run(tests, sizeof(tests) / sizeof((tests)[0]))

#endif
