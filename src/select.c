#include <string.h>
#include <lt_internal.h>

/*
** The full name of a test is "suite/test", or just "test" outside a
** suite. It is matched without building that string, so there is no
** buffer and no limit on name length. A match lies inside the suite,
** inside the test, or crosses the '/' that joins them.
*/

/* Does text end with the first len characters of part? */
static int	lt_ends_with(const char *text, const char *part, size_t len)
{
	size_t	text_len;

	text_len = strlen(text);
	if (len > text_len)
		return (0);
	return (strncmp(text + text_len - len, part, len) == 0);
}

/*
** Tries every '/' of the filter as the joint: what comes before it must
** end the suite name, and what comes after it must start the test name.
*/
static int	lt_crosses_joint(const char *filter, const char *suite,
				const char *test)
{
	const char	*slash;

	slash = strchr(filter, '/');
	while (slash)
	{
		if (lt_ends_with(suite, filter, (size_t)(slash - filter))
			&& strncmp(test, slash + 1, strlen(slash + 1)) == 0)
			return (1);
		slash = strchr(slash + 1, '/');
	}
	return (0);
}

int	lt_name_matches(const char *filter, const char *suite, const char *test)
{
	if (!filter || strstr(test, filter) != NULL)
		return (1);
	if (!suite)
		return (0);
	return (strstr(suite, filter) != NULL
		|| lt_crosses_joint(filter, suite, test));
}

int	lt_selected(const t_lt_suite *suite, const t_lt_test *test)
{
	return (lt_name_matches(lt_options()->filter, suite->name, test->name));
}

size_t	lt_count_selected(const t_lt_suite *suite)
{
	size_t	selected;
	size_t	i;

	selected = 0;
	i = 0;
	while (i < suite->count)
	{
		if (lt_selected(suite, &suite->tests[i]))
			selected++;
		i++;
	}
	return (selected);
}
