/* fork, pipe, waitpid and alarm are POSIX, not C99. */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <lt_internal.h>

/* Short writes and EINTR are normal on pipes, so loop until done. */
static int	lt_write_all(int fd, const char *buf, size_t size)
{
	ssize_t	written;

	while (size > 0)
	{
		written = write(fd, buf, size);
		if (written < 0 && errno == EINTR)
			continue ;
		if (written <= 0)
			return (0);
		buf += written;
		size -= (size_t)written;
	}
	return (1);
}

/* Returns 0 if the child died before sending a whole result. */
static int	lt_read_all(int fd, char *buf, size_t size)
{
	ssize_t	got;

	while (size > 0)
	{
		got = read(fd, buf, size);
		if (got < 0 && errno == EINTR)
			continue ;
		if (got <= 0)
			return (0);
		buf += got;
		size -= (size_t)got;
	}
	return (1);
}

/*
** _exit, not exit: the child must not run atexit handlers or flush
** buffers the parent also owns. stdout is flushed by hand first.
*/
static void	lt_child(const t_lt_suite *suite, const t_lt_test *test,
				int write_fd)
{
	t_lt_result	result;

	alarm(lt_options()->timeout);
	result = lt_run_in(suite, test);
	lt_write_all(write_fd, (const char *)&result, sizeof(result));
	fflush(stdout);
	_exit(0);
}

static t_lt_result	lt_died(int signum, int exit_status)
{
	t_lt_result	result;

	memset(&result, 0, sizeof(result));
	result.failed = 1;
	result.signum = signum;
	result.exit_status = exit_status;
	return (result);
}

static t_lt_result	lt_parent(pid_t pid, int read_fd)
{
	t_lt_result	result;
	int			status;
	int			complete;

	memset(&result, 0, sizeof(result));
	complete = lt_read_all(read_fd, (char *)&result, sizeof(result));
	close(read_fd);
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		continue ;
	if (WIFSIGNALED(status))
		return (lt_died(WTERMSIG(status), 0));
	if (!complete)
		return (lt_died(0, WEXITSTATUS(status)));
	return (result);
}

/*
** With --no-fork the test runs in this process instead, so a debugger
** can follow it without stepping through a fork.
*/
t_lt_result	lt_run_forked_in(const t_lt_suite *suite, const t_lt_test *test)
{
	int		fds[2];
	pid_t	pid;

	if (!lt_options()->fork || pipe(fds) < 0)
		return (lt_run_in(suite, test));
	fflush(NULL);
	pid = fork();
	if (pid < 0)
	{
		close(fds[0]);
		close(fds[1]);
		return (lt_run_in(suite, test));
	}
	if (pid == 0)
	{
		close(fds[0]);
		lt_child(suite, test, fds[1]);
	}
	close(fds[1]);
	return (lt_parent(pid, fds[0]));
}

t_lt_result	lt_run_forked(const t_lt_test *test)
{
	return (lt_run_forked_in(lt_no_suite(), test));
}
