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
const t_lt_stream	*lt_captured(void);

/* An unlinked temp file, or -1. The caller closes it. */
int				lt_temp_file(void);

/* Rewinds fd, fills stream, flags truncation and closes fd. */
void			lt_stream_read(int fd, t_lt_stream *stream);

/* Loop until the buffer is full, EOF or a real error. Returns how much. */
size_t			lt_read_some(int fd, char *buf, size_t size);

/* Loops over short writes and EINTR. Returns 0 if it could not finish. */
int				lt_write_all(int fd, const char *buf, size_t size);

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
