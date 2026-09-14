#include <stdio.h>
#include <string.h>
#include <lt_internal.h>

int	lt_check(int ok, t_lt_loc loc)
{
	if (!ok)
		lt_fail(loc);
	return (ok);
}

int	lt_check_int(intmax_t left, intmax_t right, t_lt_loc loc)
{
	t_lt_failure	*failure;

	if (left == right)
		return (1);
	failure = lt_fail(loc);
	failure->has_values = 1;
	snprintf(failure->left, LT_VALUE_SIZE, "%jd", left);
	snprintf(failure->right, LT_VALUE_SIZE, "%jd", right);
	return (0);
}

int	lt_check_uint(uintmax_t left, uintmax_t right, t_lt_loc loc)
{
	t_lt_failure	*failure;

	if (left == right)
		return (1);
	failure = lt_fail(loc);
	failure->has_values = 1;
	snprintf(failure->left, LT_VALUE_SIZE, "%ju", left);
	snprintf(failure->right, LT_VALUE_SIZE, "%ju", right);
	return (0);
}

/* Writes the output and its meta characters in a visible format. Returns the size. */
static size_t	lt_escape(char *out, unsigned char c)
{
	const char	*from = "\n\t\r\"\\";
	const char	*to = "ntr\"\\";
	const char	*hex = "0123456789abcdef";
	const char	*found;

	found = strchr(from, c);
	if (found)
	{
		out[0] = '\\';
		out[1] = to[found - from];
		return (2);
	}
	if (c >= 32 && c < 127)
	{
		out[0] = (char)c;
		return (1);
	}
	out[0] = '\\';
	out[1] = 'x';
	out[2] = hex[c >> 4];
	out[3] = hex[c & 15];
	return (4);
}

/*
** Copies src quoted and escaped. If it does not fit, truncate and ends with
** "... (ellipsis not quoted: do not belong to the value.)
*/
static void	lt_quote(char *dst, size_t size, const char *src)
{
	char	esc[4];
	size_t	len;
	size_t	n;

	if (!src)
	{
		snprintf(dst, size, "NULL");
		return ;
	}
	len = 0;
	dst[len++] = '"';
	while (*src)
	{
		n = lt_escape(esc, (unsigned char)*src++);
		if (len + n + 5 > size)
		{
			memcpy(dst + len, "\"...", 5);
			return ;
		}
		memcpy(dst + len, esc, n);
		len += n;
	}
	memcpy(dst + len, "\"", 2);
}

static int	lt_str_equal(const char *a, const char *b)
{
	if (!a || !b)
		return (a == b);
	return (strcmp(a, b) == 0);
}

int	lt_check_str(const char *left, const char *right, t_lt_loc loc)
{
	t_lt_failure	*failure;

	if (lt_str_equal(left, right))
		return (1);
	failure = lt_fail(loc);
	failure->has_values = 1;
	lt_quote(failure->left, LT_VALUE_SIZE, left);
	lt_quote(failure->right, LT_VALUE_SIZE, right);
	return (0);
}
