#include <stdlib.h>
#include <string.h>
#include <lt_internal.h>

#define LT_DEFAULT_TIMEOUT 5

t_lt_options	lt_default_options(void)
{
	t_lt_options	options;

	options.filter = NULL;
	options.timeout = LT_DEFAULT_TIMEOUT;
	options.fork = 1;
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

static int	lt_parse_one(const char *arg, t_lt_options *options)
{
	const char	*value;

	value = lt_value_of(arg, "--filter");
	if (value)
	{
		options->filter = value;
		return (1);
	}
	value = lt_value_of(arg, "--timeout");
	if (value)
		return (lt_to_uint(value, &options->timeout));
	if (strcmp(arg, "--no-fork") == 0)
		return (options->fork = 0, 1);
	if (strcmp(arg, "--color") == 0)
		return (options->color = 1, 1);
	if (strcmp(arg, "--no-color") == 0)
		return (options->color = 0, 1);
	if (strcmp(arg, "--list") == 0)
		return (options->list = 1, 1);
	return (0);
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
