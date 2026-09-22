#include <stdlib.h>
#include <string.h>
#include <lt_internal.h>

#define LT_DEFAULT_TIMEOUT 5

t_lt_options	lt_default_options(void)
{
	t_lt_options	options;

	memset(&options, 0, sizeof(options));
	options.filter = NULL;
	options.timeout = LT_DEFAULT_TIMEOUT;
	options.fork = 1;
	options.capture = 1;
	options.color = LT_COLOR_AUTO;
	options.list = 0;
	return (options);
}

t_lt_options	*lt_options(void)
{
	static t_lt_options	options;
	static int			ready;

	if (!ready)
	{
		options = lt_default_options();
		ready = 1;
	}
	return (&options);
}

/* Returns what follows "--name=" in arg, or NULL if arg is not it. */
static const char	*lt_value_of(const char *arg, const char *name)
{
	size_t	len;

	len = strlen(name);
	if (strncmp(arg, name, len) != 0 || arg[len] != '=')
		return (NULL);
	return (arg + len + 1);
}

/* Rejects empty input, trailing junk and anything out of range. */
static int	lt_to_uint(const char *text, unsigned int *out)
{
	char			*end;
	unsigned long	value;

	if (*text < '0' || *text > '9')
		return (0);
	value = strtoul(text, &end, 10);
	if (*end != '\0' || value > 3600)
		return (0);
	*out = (unsigned int)value;
	return (1);
}

/*
** One --tag carries one tag. A comma is rejected rather than split, so
** that --tag=a,b is a usage error instead of a tag that never matches.
** An empty tag is rejected for the same reason.
*/
static int	lt_add_tag(const char *value, const char **list, size_t *count)
{
	if (*value == '\0' || strchr(value, ',') != NULL)
		return (0);
	if (*count >= LT_MAX_TAGS)
		return (0);
	list[*count] = value;
	(*count)++;
	return (1);
}

/* The --name=value options. Returns -1 when arg is not one of them. */
static int	lt_parse_value(const char *arg, t_lt_options *options)
{
	const char	*value;

	value = lt_value_of(arg, "--filter");
	if (value)
		return (options->filter = value, 1);
	value = lt_value_of(arg, "--timeout");
	if (value)
		return (lt_to_uint(value, &options->timeout));
	value = lt_value_of(arg, "--tag");
	if (value)
		return (lt_add_tag(value, options->tags, &options->tag_count));
	value = lt_value_of(arg, "--skip-tag");
	if (value)
		return (lt_add_tag(value, options->skip_tags, &options->skip_count));
	return (-1);
}

static int	lt_parse_flag(const char *arg, t_lt_options *options)
{
	if (strcmp(arg, "--no-fork") == 0)
		return (options->fork = 0, 1);
	if (strcmp(arg, "--no-capture") == 0)
		return (options->capture = 0, 1);
	if (strcmp(arg, "--color") == 0)
		return (options->color = 1, 1);
	if (strcmp(arg, "--no-color") == 0)
		return (options->color = 0, 1);
	if (strcmp(arg, "--list") == 0)
		return (options->list = 1, 1);
	if (strcmp(arg, "--version") == 0)
		return (options->version = 1, 1);
	return (0);
}

static int	lt_parse_one(const char *arg, t_lt_options *options)
{
	int	handled;

	handled = lt_parse_value(arg, options);
	if (handled >= 0)
		return (handled);
	return (lt_parse_flag(arg, options));
}

/*
** Parses into a copy, so a bad argument leaves the caller's options
** exactly as they were instead of half applied.
*/
int	lt_parse_args(int argc, char **argv, t_lt_options *options)
{
	t_lt_options	parsed;
	int				i;

	parsed = *options;
	i = 1;
	while (i < argc)
	{
		if (!lt_parse_one(argv[i], &parsed))
			return (i);
		i++;
	}
	*options = parsed;
	return (0);
}
