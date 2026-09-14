#include <signal.h>
#include <stdio.h>
#include <lt_internal.h>

/*
** Hand-rolled instead of strsignal: the names are the same everywhere
** and this keeps the library free of XSI-only functions.
*/
const char	*lt_signal_name(int signum)
{
	static char	buffer[32];

	if (signum == SIGSEGV)
		return ("SIGSEGV");
	if (signum == SIGBUS)
		return ("SIGBUS");
	if (signum == SIGABRT)
		return ("SIGABRT");
	if (signum == SIGFPE)
		return ("SIGFPE");
	if (signum == SIGILL)
		return ("SIGILL");
	if (signum == SIGPIPE)
		return ("SIGPIPE");
	if (signum == SIGKILL)
		return ("SIGKILL");
	if (signum == SIGTERM)
		return ("SIGTERM");
	snprintf(buffer, sizeof(buffer), "signal %d", signum);
	return (buffer);
}
