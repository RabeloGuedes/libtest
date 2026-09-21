#include <string.h>
#include <lt_internal.h>

/*
** Tags are matched whole, never as substrings, so that --tag=unit does
** not drag in a test tagged "unitary". The list is walked in place:
** no copy, no allocation, no limit on how many tags a test may carry.
*/

/* Is tag the element that starts here? len is its length. */
static int	lt_tag_here(const char *elem, const char *tag, size_t len)
{
	if (strncmp(elem, tag, len) != 0)
		return (0);
	return (elem[len] == '\0' || elem[len] == ',');
}

/*
** Past this element and its separator. An empty element ("a,,b") needs
** no special case: lt_tag_here fails on it and the caller steps again.
*/
static const char	*lt_next_tag(const char *list)
{
	while (*list && *list != ',')
		list++;
	if (*list == ',')
		list++;
	return (list);
}

int	lt_has_tag(const char *list, const char *tag)
{
	size_t	len;

	if (!list || !tag)
		return (0);
	len = strlen(tag);
	if (len == 0)
		return (0);
	while (*list)
	{
		if (lt_tag_here(list, tag, len))
			return (1);
		list = lt_next_tag(list);
	}
	return (0);
}

/* A test carries its own tags plus the ones of its suite. */
static int	lt_carries(const t_lt_suite *suite, const t_lt_test *test,
				const char *tag)
{
	return (lt_has_tag(test->tags, tag) || lt_has_tag(suite->tags, tag));
}

static int	lt_is_skipped(const t_lt_suite *suite, const t_lt_test *test)
{
	const t_lt_options	*options;
	size_t				i;

	options = lt_options();
	i = 0;
	while (i < options->skip_count)
	{
		if (lt_carries(suite, test, options->skip_tags[i]))
			return (1);
		i++;
	}
	return (0);
}

/*
** With no --tag every test qualifies, otherwise it must carry at least
** one of them. A --skip-tag always wins, so "--tag=unit --skip-tag=slow"
** means what it says even for a test tagged "unit,slow".
*/
int	lt_tags_allow(const t_lt_suite *suite, const t_lt_test *test)
{
	const t_lt_options	*options;
	size_t				i;

	if (lt_is_skipped(suite, test))
		return (0);
	options = lt_options();
	if (options->tag_count == 0)
		return (1);
	i = 0;
	while (i < options->tag_count)
	{
		if (lt_carries(suite, test, options->tags[i]))
			return (1);
		i++;
	}
	return (0);
}
