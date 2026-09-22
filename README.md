# libtest

A small unit and integration test framework for C99.

Every test runs in its own process, so a crash is a failed test and not the end
of the run. Tests are registered by hand in an array: there is no magic, no
allocation, and nothing to configure.

```c
#include <libtest.h>

static void	test_add(void)
{
	LT_ASSERT_INT_EQ(2 + 2, 4);
}

int	main(int argc, char **argv)
{
	const t_lt_test	tests[] = {
		LT_TEST(test_add),
	};

	return (LT_MAIN(argc, argv, tests));
}
```

```sh
cc -std=c99 -I /usr/local/include my_tests.c -L /usr/local/lib -ltest -o my_tests
./my_tests
```

```
PASS  test_add

1/1 passed
```

## Install

```sh
make
sudo make install            # PREFIX=/usr/local by default
make install PREFIX=~/.local # or somewhere of your own
```

This installs `libtest.a` and `libtest.h`, nothing else. `make uninstall`
removes them, and both honour `DESTDIR` for packaging.

## Assertions

| Macro | Compares |
| --- | --- |
| `LT_ASSERT(cond)` | any scalar, for truth |
| `LT_ASSERT_INT_EQ(a, b)` | signed integers (`intmax_t`) |
| `LT_ASSERT_UINT_EQ(a, b)` | unsigned integers, including `size_t` |
| `LT_ASSERT_STR_EQ(a, b)` | strings, by content; `NULL` only equals `NULL` |

There is one macro per type instead of a C11 `_Generic` layer, because only the
macro's name can say whether a `char *` is a string or a buffer.

A failure reports the file, the line and the expression, plus both values for
the typed macros:

```
FAIL  test_greeting
      test_example.c:36: greeting() == "hello"
      left:  "hello\n"
      right: "hello"
```

Values are escaped (`\n`, `\t`, `\xHH`) and long ones are cut with `"...`.

**Assertions are fatal and work by returning.** On failure the test function
returns, so a failed assertion stops that test and only that test. This is why
an assertion only works directly inside a test function, never in a helper it
calls: a helper would only return to its caller. A helper should return a value
the test asserts on.

## Suites and fixtures

A suite groups tests that share a setup and a teardown. Both are plain test
functions, and either may be `NULL`.

```c
static char	*g_buffer;

static void	buffer_setup(void)
{
	g_buffer = malloc(16);
	LT_ASSERT(g_buffer != NULL);
	strcpy(g_buffer, "abc");
}

static void	buffer_teardown(void)
{
	free(g_buffer);
}

int	main(int argc, char **argv)
{
	const t_lt_test		tests[] = {
		LT_TEST(test_buffer_is_ready),
	};
	const t_lt_suite	suites[] = {
		LT_SUITE("buffer", buffer_setup, buffer_teardown, tests),
	};

	return (LT_SUITES_MAIN(argc, argv, suites));
}
```

Setup runs before each test and teardown after it, in the same process as the
test. Since every test gets a fresh process, fixture state can live in ordinary
`static` variables: each test starts from its own copy.

- A failing setup is reported as such, and the test does not run.
- Teardown runs even when the test failed.
- When more than one of the three fails, the earliest failure is the one
  reported.
- Teardown does **not** run after a crash. Anything outside the process (files,
  sockets) leaks in that case.

## Tags

Tests and suites can carry tags, as a comma separated string. A test's tags are
its own plus the ones of its suite.

```c
LT_TEST_TAGGED(test_slow_path, "slow"),
LT_SUITE_TAGGED("db", setup, teardown, tests, "integration"),
```

```sh
./my_tests --tag=unit --tag=integration   # either one
./my_tests --skip-tag=slow                # everything but these
```

Tags are matched whole, so `--tag=unit` never selects a test tagged `unitary`.
A `--skip-tag` always wins over a `--tag`.

## Testing a program

`lt_exec` runs a binary and fills a struct; the assertions you already have read
it. There is nothing new to learn.

```c
static void	test_program_greets(void)
{
	t_lt_process	proc;

	LT_EXEC(&proc, "./my_program", "--greet");
	LT_ASSERT(proc.started);
	LT_ASSERT_INT_EQ(proc.exit_status, 0);
	LT_ASSERT_STR_EQ(proc.out.text, "hello\n");
}
```

`LT_EXEC_IN(&proc, "input\n", "./my_program")` feeds standard input, and
`lt_exec(&proc, argv, input)` takes an `argv` built at runtime. `argv[0]` is
looked up in `PATH`.

| Field | Meaning |
| --- | --- |
| `started` | the program ran at all; 0 means it was never there |
| `exit_status` | its exit status, when `signum` is 0 |
| `signum` | the signal that killed it, 0 otherwise |
| `timed_out` | that signal was the timeout |
| `out`, `err` | its two streams, as `text`, `size` and `truncated` |

`started` exists because a program that cannot be run must not look like one
that exited 127, which is exactly what a shell reports for it.

## Isolation

Every test runs in a child process, so the runner survives anything the test
does:

```
FAIL  test_crashes
      died with SIGSEGV
FAIL  test_calls_exit
      exited with status 3
FAIL  test_loops_forever
      timed out
```

A test's side effects stay in its child, which is also why a test cannot change
what the next one sees. `--no-fork` runs everything in one process instead,
which is what you want under a debugger:

```sh
gdb --args ./my_tests --filter=test_crashes --no-fork
```

Without the fork a crashing test takes the whole runner down again, which is
the point when you are in a debugger. It is also why `--no-fork` usually wants
a `--filter` next to it.

Whatever a test prints is captured and shown **only if it fails**, so a passing
run stays readable:

```
FAIL  test_connects
      test_db.c:20: connect() == 0
      left:  -1
      right: 0
      output:
      | trying 127.0.0.1:5432
      | connection refused
```

## Options

| Option | Effect |
| --- | --- |
| `--filter=SUBSTRING` | run tests whose `suite/test` name contains it |
| `--tag=NAME` | run tests carrying the tag; repeat for "either" |
| `--skip-tag=NAME` | never run tests carrying it |
| `--timeout=SECONDS` | per test, 0 disables it (default 5) |
| `--no-fork` | run in one process, for a debugger |
| `--no-capture` | let tests print as they run |
| `--color`, `--no-color` | override terminal detection |
| `--list` | print the selected test names and exit |
| `--version` | print the libtest version and exit |

They can also be set from code, through `lt_options()`, before calling
`lt_main`.

Exit status is **0** when everything passed, **1** when something failed, and
**2** for a bad command line **or a selection that matched nothing** — a filter
with a typo in it should fail your CI, not report a green `0/0`.

## Building this repository

```sh
make          # builds libtest.a
make test     # builds and runs the framework's own test suite
make example  # runs example/, which fails on purpose
make test ARGS="--filter=str_ --no-fork"
```

`make example` exits non-zero by design: it contains two failing tests so you
can see what a failure report looks like.

## Status

Version 1.0.0. Verified with clang on macOS and with GCC 14 on Linux, both
natively.

One test, `test_exec_leaks_no_descriptors`, asserts that a program run by
`lt_exec` inherits no descriptor of the framework's. That assumes the
environment itself leaks none, which is not true under an emulator: run
`--skip-tag=clean-fds` there.

`CLAUDE.md` in this repository records why the framework is built the way it
is, which is worth reading before changing any of it.
