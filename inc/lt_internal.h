#ifndef LT_INTERNAL_H
# define LT_INTERNAL_H

# include <libtest.h>

/*
** Marks the current test as failed, records the location and returns
** the failure so the check can fill in the values, if it has any.
*/
t_lt_failure	*lt_fail(t_lt_loc loc);

/* A suite with no fixture, for running a bare test. */
const t_lt_suite	*lt_no_suite(void);

/* Whether the current filter selects the test, and how many it selects. */
int				lt_selected(const t_lt_suite *suite, const t_lt_test *test);
size_t			lt_count_selected(const t_lt_suite *suite);

/* Whether --tag and --skip-tag let the test run. */
int				lt_tags_allow(const t_lt_suite *suite, const t_lt_test *test);

/* "SIGSEGV" for known signals, "signal N" otherwise. */
const char		*lt_signal_name(int signum);

#endif
