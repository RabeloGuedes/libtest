/* fork, execvp, dup2, alarm, fcntl and waitpid are POSIX, not C99. */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <lt_internal.h>

/*
** Everything the child needs. ready is a pipe the child only writes to
** when exec fails: its write end is close-on-exec, so a successful exec
** closes it and the parent reads end of file. That is the one way to
** tell "the program exited 127" from "there was no program".
*/
typedef struct s_lt_fds
{
	int	in;
	int	out;
	int	err;
	int	ready[2];
}	t_lt_fds;

static void	lt_exec_close(t_lt_fds *fds)
{
	if (fds->in >= 0)
		close(fds->in);
	if (fds->out >= 0)
		close(fds->out);
	if (fds->err >= 0)
		close(fds->err);
	if (fds->ready[0] >= 0)
		close(fds->ready[0]);
	if (fds->ready[1] >= 0)
		close(fds->ready[1]);
	return ;
}

/*
** stdin is always a file, even with no input: a program that reads
** then gets end of file at once instead of hanging on a terminal.
*/
static int	lt_input_file(const char *input)
{
	int	fd;

	fd = lt_temp_file();
	if (fd < 0)
		return (-1);
	if (input && !lt_write_all(fd, input, strlen(input)))
	{
		close(fd);
		return (-1);
	}
	if (lseek(fd, 0, SEEK_SET) != 0)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

static int	lt_exec_open(t_lt_fds *fds, const char *input)
{
	fds->ready[0] = -1;
	fds->ready[1] = -1;
	fds->in = lt_input_file(input);
	fds->out = lt_temp_file();
	fds->err = lt_temp_file();
	if (fds->in < 0 || fds->out < 0 || fds->err < 0)
		return (0);
	if (pipe(fds->ready) < 0)
	{
		fds->ready[0] = -1;
		fds->ready[1] = -1;
		return (0);
	}
	return (fcntl(fds->ready[1], F_SETFD, FD_CLOEXEC) >= 0);
}

/*
** Puts from where the program expects it and drops the original, so
** the program inherits no descriptor of ours. The guard matters when
** from already is the target: closing it would leave nothing there.
*/
static void	lt_move_fd(int from, int to)
{
	if (from == to)
		return ;
	dup2(from, to);
	close(from);
	return ;
}

/*
** alarm survives exec, so the program inherits the timeout. _exit, not
** exit: the child must not flush buffers the parent also owns.
*/
static void	lt_exec_child(char **argv, t_lt_fds *fds)
{
	char	failed;

	close(fds->ready[0]);
	lt_move_fd(fds->in, STDIN_FILENO);
	lt_move_fd(fds->out, STDOUT_FILENO);
	lt_move_fd(fds->err, STDERR_FILENO);
	alarm(lt_options()->timeout);
	execvp(argv[0], argv);
	failed = 1;
	lt_write_all(fds->ready[1], &failed, 1);
	_exit(127);
}

static void	lt_exec_status(t_lt_process *proc, int status)
{
	if (WIFSIGNALED(status))
	{
		proc->signum = WTERMSIG(status);
		proc->timed_out = (proc->signum == SIGALRM);
	}
	else
		proc->exit_status = WEXITSTATUS(status);
	return ;
}

/* Nothing was written to ready means exec happened and closed it. */
static void	lt_exec_wait(t_lt_process *proc, pid_t pid, t_lt_fds *fds)
{
	char	failed;
	int		status;

	close(fds->ready[1]);
	fds->ready[1] = -1;
	proc->started = (lt_read_some(fds->ready[0], &failed, 1) == 0);
	close(fds->ready[0]);
	fds->ready[0] = -1;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		continue ;
	if (proc->started)
		lt_exec_status(proc, status);
	lt_stream_read(fds->out, &proc->out);
	lt_stream_read(fds->err, &proc->err);
	close(fds->in);
	return ;
}

void	lt_exec(t_lt_process *proc, char **argv, const char *input)
{
	t_lt_fds	fds;
	pid_t		pid;

	memset(proc, 0, sizeof(*proc));
	if (!lt_exec_open(&fds, input))
	{
		lt_exec_close(&fds);
		return ;
	}
	fflush(NULL);
	pid = fork();
	if (pid < 0)
	{
		lt_exec_close(&fds);
		return ;
	}
	if (pid == 0)
		lt_exec_child(argv, &fds);
	lt_exec_wait(proc, pid, &fds);
	return ;
}
