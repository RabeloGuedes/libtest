/* mkstemp, unlink, dup2 and lseek are POSIX, not C99. */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lt_internal.h>

static t_lt_stream	*lt_capture(void)
{
	static t_lt_stream	capture;

	return (&capture);
}

const t_lt_stream	*lt_captured(void)
{
	return (lt_capture());
}

void	lt_capture_reset(void)
{
	static const t_lt_stream	clean;

	*lt_capture() = clean;
	return ;
}

/*
** A file, not a pipe: a pipe fills at 64 KB and would deadlock a test
** that prints more than that. Unlinked at once, so it goes away with
** the last descriptor even if the test crashes.
*/
int	lt_temp_file(void)
{
	char	path[] = "/tmp/libtest_XXXXXX";
	int		fd;

	fd = mkstemp(path);
	if (fd < 0)
		return (-1);
	unlink(path);
	return (fd);
}

int	lt_capture_start(void)
{
	if (!lt_options()->capture)
		return (-1);
	return (lt_temp_file());
}

/*
** Unbuffered on purpose: what a test prints right before it crashes
** has to be in the file already, not waiting in a buffer that dies
** with the process. The streams are flushed first, so setvbuf sees
** them empty.
*/
void	lt_capture_child(int fd)
{
	if (fd < 0)
		return ;
	fflush(stdout);
	fflush(stderr);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);
	return ;
}

/* Short reads and EINTR are normal, so loop until the buffer is full. */
size_t	lt_read_some(int fd, char *buf, size_t size)
{
	ssize_t	got;
	size_t	total;

	total = 0;
	while (total < size)
	{
		got = read(fd, buf + total, size - total);
		if (got < 0 && errno == EINTR)
			continue ;
		if (got <= 0)
			break ;
		total += (size_t)got;
	}
	return (total);
}

/*
** The writer shared this descriptor, so the offset sits at the end of
** what it wrote: rewind before reading. The start is what is kept,
** because that is where a test says what it was doing.
*/
void	lt_stream_read(int fd, t_lt_stream *stream)
{
	if (fd < 0)
		return ;
	if (lseek(fd, 0, SEEK_SET) == 0)
		stream->size = lt_read_some(fd, stream->text, LT_OUTPUT_SIZE - 1);
	stream->text[stream->size] = '\0';
	stream->truncated = (lseek(fd, 0, SEEK_END) > (off_t)stream->size);
	close(fd);
	return ;
}

void	lt_capture_read(int fd)
{
	lt_stream_read(fd, lt_capture());
	return ;
}
