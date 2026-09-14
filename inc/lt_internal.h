#ifndef LT_INTERNAL_H
# define LT_INTERNAL_H

# include <libtest.h>

/*
** Marks the current test as failed, records the location and returns
** the failure so the check can fill in the values, if it has any.
*/
t_lt_failure	*lt_fail(t_lt_loc loc);

/* "SIGSEGV" for known signals, "signal N" otherwise. */
const char		*lt_signal_name(int signum);

#endif
