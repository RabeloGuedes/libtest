#ifndef LIBTEST_H
# define LIBTEST_H

# include <stddef.h>
# include <stdint.h>

# define LT_VALUE_SIZE 128

typedef void	(*t_lt_func)(void);

typedef struct s_lt_test
{
	const char	*name;
	t_lt_func	func;
}	t_lt_test;

/* Where an assertion sits in the source. Built by the macros. */
typedef struct s_lt_loc
{
	const char	*expr;
	const char	*file;
	int			line;
}	t_lt_loc;

typedef struct s_lt_failure
{
	const char	*expr;
	const char	*file;
	int			line;
	int			has_values;
	char		left[LT_VALUE_SIZE];
	char		right[LT_VALUE_SIZE];
}	t_lt_failure;

/*
** signum and exit_status are only set when the test ran in its own
** process and did not finish normally. In that case failure holds no
** location, because the test died before reporting one.
*/
typedef struct s_lt_result
{
	int				failed;
	int				signum;
	int				exit_status;
	t_lt_failure	failure;
}	t_lt_result;

/* Runs a test in this process. A crash here takes the runner down. */
t_lt_result	lt_run_one(const t_lt_test *test);

/*
** Runs a test in a child process and brings the result back. A crash
** or a timeout becomes a failed result instead of killing the runner.
** Side effects of the test are confined to the child.
*/
t_lt_result	lt_run_forked(const t_lt_test *test);

/* Seconds before a forked test is killed. 0 disables the timeout. */
void		lt_set_timeout(unsigned int seconds);

/* Runs every test, prints the report, returns 0 if all passed. */
int			lt_run(const t_lt_test *tests, size_t count);

/*
** Internal: public only because the macros expand in user code.
** They take already-evaluated values, so every macro argument is
** evaluated exactly once. Return 1 when the check passed.
*/
int			lt_check(int ok, t_lt_loc loc);
int			lt_check_int(intmax_t left, intmax_t right, t_lt_loc loc);
int			lt_check_uint(uintmax_t left, uintmax_t right, t_lt_loc loc);
int			lt_check_str(const char *left, const char *right, t_lt_loc loc);

/* The single stringification point: callers pass raw expressions. */
# define LT_LOC_(expr) ((t_lt_loc){#expr, __FILE__, __LINE__})

/*
** Fatal: on failure it returns from the test function. That is why the
** assertions can only be used directly inside a t_lt_func.
*/
# define LT_FATAL_(check) \
	do { \
		if (!(check)) \
			return ; \
	} while (0)

# define LT_ASSERT(cond) \
	LT_FATAL_(lt_check(!!(cond), LT_LOC_(cond)))

/* Signed integers. */
# define LT_ASSERT_INT_EQ(a, b) \
	LT_FATAL_(lt_check_int((a), (b), LT_LOC_(a == b)))

/* Unsigned integers: size_t, unsigned, and so on. */
# define LT_ASSERT_UINT_EQ(a, b) \
	LT_FATAL_(lt_check_uint((a), (b), LT_LOC_(a == b)))

/* Compares content (strcmp). NULL is only equal to NULL. */
# define LT_ASSERT_STR_EQ(a, b) \
	LT_FATAL_(lt_check_str((a), (b), LT_LOC_(a == b)))

# define LT_TEST(func) { #func, func }

/* Works on arrays only, not on pointers. */
# define LT_RUN(tests) lt_run(tests, sizeof(tests) / sizeof((tests)[0]))

#endif
