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

/*
** What a test printed while it ran. It lives in the parent, never in
** t_lt_result: the parent owns the temp file and reads it once the
** child is gone, so the result stays small and an output printed right
** before a crash is still there.
*/
# define LT_OUTPUT_SIZE 4096

typedef struct s_lt_capture
{
	char	text[LT_OUTPUT_SIZE];
	size_t	size;
	int		truncated;
}	t_lt_capture;

const t_lt_capture	*lt_captured(void);

/* Empties the buffer. Always called, so no test shows another's output. */
void			lt_capture_reset(void);

/* Opens the temp file, or -1 when capture is off or it cannot. */
int				lt_capture_start(void);

/* In the child: sends stdout and stderr to fd. In the parent: reads. */
void			lt_capture_child(int fd);
void			lt_capture_read(int fd);

/* "SIGSEGV" for known signals, "signal N" otherwise. */
const char		*lt_signal_name(int signum);

#endif
