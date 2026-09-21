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

/*
** Tests that share a fixture. setup runs before each test and teardown
** after it, both in the same process as the test. They are plain
** t_lt_func, so setup reports a failure with an assertion like any test.
** Either may be NULL. name is NULL for the implicit suite of LT_MAIN.
*/
typedef struct s_lt_suite
{
	const char		*name;
	t_lt_func		setup;
	t_lt_func		teardown;
	const t_lt_test	*tests;
	size_t			count;
}	t_lt_suite;

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

/* Where in the life of a test a failure happened. */
# define LT_PHASE_TEST 0
# define LT_PHASE_SETUP 1
# define LT_PHASE_TEARDOWN 2

/*
** phase says where the failure happened, and only means something when
** failed is set. signum and exit_status are only set when the test ran
** in its own process and did not finish normally. In that case failure
** holds no location, because the test died before reporting one, and
** phase is not known either.
*/
typedef struct s_lt_result
{
	int				failed;
	int				phase;
	int				signum;
	int				exit_status;
	t_lt_failure	failure;
}	t_lt_result;

/* Runs a test in this process. A crash here takes the runner down. */
t_lt_result	lt_run_one(const t_lt_test *test);

/*
** Same, inside a suite: setup, then the test, then teardown. A failing
** setup skips both the test and the teardown. Teardown runs even when
** the test failed. When several phases fail, the earliest one is kept.
*/
t_lt_result	lt_run_in(const t_lt_suite *suite, const t_lt_test *test);

/*
** Runs a test in a child process and brings the result back. A crash
** or a timeout becomes a failed result instead of killing the runner.
** Side effects of the test are confined to the child.
*/
t_lt_result	lt_run_forked(const t_lt_test *test);

/*
** Same, inside a suite. The fixture runs in the child too, so a crash
** skips the teardown: whatever lives outside the process leaks.
*/
t_lt_result	lt_run_forked_in(const t_lt_suite *suite, const t_lt_test *test);

/*
** How the runner behaves. Defaults come from lt_default_options.
** color is 1, 0, or LT_COLOR_AUTO to decide from the output stream.
*/
typedef struct s_lt_options
{
	const char		*filter;
	unsigned int	timeout;
	int				fork;
	int				color;
	int				list;
}	t_lt_options;

# define LT_COLOR_AUTO (-1)

t_lt_options	lt_default_options(void);

/* The options in use. Writable, for setting them without a command line. */
t_lt_options	*lt_options(void);

/*
** Parses argv into options. Returns 0 on success, or the index of the
** first bad argument. options is left untouched when that happens.
*/
int			lt_parse_args(int argc, char **argv, t_lt_options *options);

/*
** Does filter select the test? It is matched as a substring of
** "suite/test", or of "test" alone when suite is NULL. A NULL filter
** selects everything. Pure, so it can be tested without running.
*/
int			lt_name_matches(const char *filter, const char *suite,
				const char *test);

/* Runs every test, prints the report, returns 0 if all passed. */
int			lt_run(const t_lt_test *tests, size_t count);
int			lt_run_suites(const t_lt_suite *suites, size_t count);

/*
** Parses argv, applies the options and runs. Returns 0 if all tests
** passed, 1 if any failed, 2 if the command line was wrong or nothing
** was selected.
*/
int			lt_main(int argc, char **argv, const t_lt_test *tests,
				size_t count);
int			lt_main_suites(int argc, char **argv, const t_lt_suite *suites,
				size_t count);

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

/* Both work on arrays only, not on pointers. */
# define LT_COUNT_(tests) (sizeof(tests) / sizeof((tests)[0]))

# define LT_TEST(func) { #func, func }

/* name is a string literal. setup and teardown may be NULL. */
# define LT_SUITE(name, setup, teardown, tests) \
	{ (name), (setup), (teardown), (tests), LT_COUNT_(tests) }

# define LT_RUN(tests) lt_run(tests, LT_COUNT_(tests))
# define LT_MAIN(argc, argv, tests) \
	lt_main(argc, argv, tests, LT_COUNT_(tests))

# define LT_SUITES_RUN(suites) lt_run_suites(suites, LT_COUNT_(suites))
# define LT_SUITES_MAIN(argc, argv, suites) \
	lt_main_suites(argc, argv, suites, LT_COUNT_(suites))

#endif
