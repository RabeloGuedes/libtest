# libtest

A small unit/integration test framework for C, built incrementally. The guiding
principle is **start small, consolidate the base, only then expand**. Every
module depends on the core; the core never depends on a module.

The owner works on **macOS with clang**. Everything must build there with the
flags below. GCC on Linux must also stay green.

## Commands

```sh
make                      # builds libtest.a
make test                 # builds and runs the framework's own test suite
make example              # runs example/, which fails ON PURPOSE (4/6 passed, make exits 1)
make test ARGS="--filter=str_ --no-fork"   # ARGS is passed to the test binary
make print-OBJS           # prints any Makefile variable, for debugging the build
```

`make test` must end with `N/N passed`. `make example` must show exactly two
failures (`test_add_fails_on_purpose`, `test_greeting_fails_on_purpose`) with
values printed. Run **both** before considering any change done.

## Layout

```
inc/libtest.h          public API: types, functions, assertion macros
inc/lt_internal.h      private: only the library's own .c files include it
src/runner.c           current-result state, lt_run_in, reporting, lt_run_suites, lt_main
src/assert.c           lt_check* functions, value formatting, string escaping
src/isolate.c          lt_run_forked_in: fork + pipe + waitpid + alarm timeout
src/select.c           --filter matching against "suite/test" names
src/options.c          t_lt_options, defaults, lt_parse_args
src/signal_name.c      signal number -> "SIGSEGV" etc. (hand-rolled, not strsignal)
tests/test_libtest.c   the framework testing itself (54 tests)
example/test_example.c usage demo: two suites, two intentional failures
```

## Current state (done and tested)

**Assertions.** All fatal: on failure they `return` from the test function, so
they can only be used directly inside a `t_lt_func`, never in helpers.

- `LT_ASSERT(cond)`
- `LT_ASSERT_INT_EQ(a, b)` (intmax_t), `LT_ASSERT_UINT_EQ(a, b)` (uintmax_t, use for size_t)
- `LT_ASSERT_STR_EQ(a, b)` (strcmp, NULL only equals NULL, values escaped: `\n`, `\t`, `\xHH`; truncated with `"...` outside the quotes)

Failures report `file:line: expr` plus `left:`/`right:` for typed assertions.

**Registration and running.** Explicit array, no auto-registration:

```c
int	main(int argc, char **argv)
{
	const t_lt_test	tests[] = {
		LT_TEST(test_something),
	};

	return (LT_MAIN(argc, argv, tests));
}
```

`LT_RUN(tests)` also exists (no argv). For fixtures see below. Exit codes: 0 all passed, 1 some failed,
2 usage error **or a filter that selects nothing** (a green 0/0 would hide typos in CI).

**Isolation.** `lt_run` runs every test through `lt_run_forked`. Crash ->
`died with SIGSEGV`, `exit()` inside a test -> `exited with status N`,
timeout (default 5 s, `alarm`) -> `timed out`. Test side effects stay in the child.
`lt_run_one` stays public and in-process: the self-tests need it to observe side effects.

**Options** (`--filter=SUBSTRING`, `--timeout=SECONDS` (0 disables, max 3600),
`--no-fork`, `--color`, `--no-color`, `--list`). Also settable in
code through `lt_options()`. `--no-fork` makes crashes fatal again by design; it
is meant for `gdb --args ./tests/run_tests --filter=X --no-fork`.

**Fixtures (setup / teardown).** Tests are grouped in suites; `LT_MAIN` keeps
working and wraps a bare array in an unnamed suite with no fixture:

```c
const t_lt_suite	suites[] = {
	LT_SUITE("buffer", buffer_setup, buffer_teardown, buffer_tests),
	LT_SUITE("math", NULL, NULL, math_tests),
};

return (LT_SUITES_MAIN(argc, argv, suites));
```

`setup`/`teardown` are plain `t_lt_func` (either may be NULL), run in the same
process as the test, around each test. Fixture state lives in `static`
variables of the test file. A failing setup (it uses `LT_ASSERT` like any test)
skips the test **and** the teardown. Teardown runs after a failed test. The
earliest failure is the one reported (`t_lt_result.phase` says which:
`LT_PHASE_TEST/SETUP/TEARDOWN`, only meaningful when `failed`). Report: a
`== name ==` header and a `name: N/M passed` line per named suite, then the
overall total; a suite with nothing selected prints nothing; the unnamed suite
prints exactly what it printed before suites existed. `--filter` and `--list`
use `suite/test` names (`--filter=buffer/` runs one suite).

## Design decisions (do not undo without discussing with the owner)

- **C99, not C11.** Typed macros per type instead of `_Generic`: a `char *` can be
  a string or a buffer pointer and only the macro name can say which; some types
  alias per platform (`size_t` vs `unsigned long`); a C99 library works in C11
  projects but not the reverse. A `_Generic` layer may come later as an optional
  thin wrapper over the typed macros.
- **Fatal assertions via `return`, no setjmp.** One failure per test means the
  result needs no allocation.
- **`t_lt_result` is a fixed-size, trivially copyable struct.** It crosses the
  pipe as raw bytes. No malloc'd data inside it. Its only pointers (`expr`,
  `file`) point to string literals, which stay valid after `fork` because the
  child runs the same binary image. Any new field must respect this.
- **`lt_run_one` starts each test from a clean result** and restores the caller's
  state afterwards, so a test can run another test (the self-tests rely on this).
- **`left`/`right`, not expected/actual.** Frameworks disagree on the order and
  people swap it; neutral names never lie.
- **Fixture state is `static` variables, not a `void *context`.** `t_lt_func`
  stays `void (*)(void)`; the fork already gives each test a fresh copy of the
  globals. Decided with the owner; revisit only if a context is really needed.
- **Fixtures run inside the child, in `lt_run_in`.** Setup/test/teardown share one
  process and one `alarm`, so the timeout covers all three. A crash skips the
  teardown and the result carries no phase (the parent only sees the death).
  With `--no-fork` globals persist across tests, so setup must reset its state.
- **`lt_run_one` / `lt_run_forked` are thin wrappers** over `lt_run_in` /
  `lt_run_forked_in` with an empty suite, so the old API and the self-tests did
  not change.
- **`lt_name_matches` is pure and allocation-free.** It matches a substring of
  `suite/test` by checking the suite, the test, and every `/` of the filter as
  the joint, instead of building the string (no buffer, no length limit).
- **`lt_parse_args` is pure.** It parses into a copy (a bad argument leaves options
  untouched), starts from the options it is given (not from defaults), and returns
  the index of the bad argument instead of printing. `lt_main` does the printing.

## Code conventions

- Flags: `-Wall -Wextra -Werror -std=c99 -pedantic`. Files that use POSIX calls
  start with `#define _POSIX_C_SOURCE 200809L` before any include. No XSI or GNU
  extensions.
- **Comments are always in English.**
- 42 Norm-like style: tabs; `t_` typedefs over `s_` structs; declarations at the
  top of the function, assignments after; `return (x);`; `while` instead of
  `for`; at most 25 lines per function and 4 parameters. The one accepted
  exception is function-like macros, which the framework cannot exist without
  (only a macro can capture `__FILE__`, `__LINE__` and the expression text).
- Public names use the `lt_` / `LT_` prefix. Internal macros end in `_`
  (`LT_LOC_`, `LT_FATAL_`, `LT_COUNT_`). Functions public only because macros
  expand in user code are marked "Internal" in the header. Everything else
  internal goes in `src/lt_internal.h`.
- Runner output goes to stdout; usage errors to stderr.

### Macro hygiene (each rule below was learned from a real bug here)

1. Parenthesize every parameter use: `!!(cond)`, never `(!!cond)`. The latter
   parses `!!x == 1` as `(!!x) == 1`, which passes for any nonzero `x` with no warning.
2. Evaluate each argument exactly once: macros only forward values to a function
   (`lt_check_*`), which compares and formats.
3. **One stringification point:** `LT_LOC_(expr)` applies `#expr`. Callers pass raw
   tokens (`LT_LOC_(a == b)`), never `#a`, never `" == "` literals. Stringifying
   twice produced `"add(1, 1)" " == " "3"` and escaped quotes in reports.

## Testing method (non-negotiable)

- Every feature ships with self-tests in `tests/test_libtest.c`, using the
  `inner_*` pattern: an unregistered test function run through `run()`
  (in-process) or `run_forked()`, and a registered test that inspects the result.
- **Layering:** verify each layer with the one below it. Typed assertions are
  checked with `LT_ASSERT` + `strcmp`, never with themselves. `LT_ASSERT` itself is
  checked by `sanity_check()` in `main`, which uses plain `if` on purpose.
- **Mutation testing after every module:** break the new code deliberately (remove
  a check, invert a condition) and confirm a test fails. A surviving mutant is
  either a missing test (write it) or dead code (delete the code). Never keep a
  test that cannot fail. Wrap mutation runs in `timeout`, since removing `alarm`
  hangs the suite. When grepping make output, note that `rror` matches `-Werror`.
  When mutating the runner itself, judge by `FAIL` lines, not by the exit code:
  a mutant that inverts the exit code makes `make test` return 0 while tests
  fail. Run `make fclean` between mutants (a restored file can leave a stale
  object) and mutate so it still compiles (an unused variable is a `-Werror`).
- Do not bend the framework to silence a warning caused by artificial test code.
  Example: clang's `-Wliteral-conversion` fired on `LT_ASSERT(0.5)`; the fix was
  a `double` variable in the test, not a change to the macro.
- To test the runner's printed output, redirect stdout inside a forked inner test
  (`tmpfile` + `dup2`) and compare the text read back. The redirection and any
  option change die with the child.
- Use real runtime values in tests: identical string literals may share an
  address (use a buffer to test `STR_EQ`); GCC folds `1 / 0` at `-O0` (use
  `raise(SIGFPE)`).

## Roadmap to 1.0

Work one step at a time, and stop for the owner's review after each step.

### 1. Fixtures (setup / teardown) — done, awaiting the owner's review

See "Fixtures" above and the design decisions. Not done on purpose: `--list`
prints no suite headers; a crash in setup/teardown cannot say which phase.

### 2. Tags — next

The owner's original idea of "modules" (unit, integration, e2e) maps to tags,
not to separate code: the mechanics are identical, only the capabilities used
differ. Probably a `tags` field on `t_lt_test` (or on the suite) and a
`--tag=NAME` option. Keep `LT_TEST(func)` working unchanged.

### 3. End-to-end module

Run an external binary and assert on its behavior: argv, optional stdin,
captured stdout, stderr and exit status. Reuse `isolate.c`
(fork/exec, temp files, timeout); output capture does not exist yet (see loose ends). Open questions: the result type
(outputs can exceed `LT_OUTPUT_SIZE`, so they may need file-backed comparison),
assertions such as exit code and stdout equality, and how to show diffs.

### 4. Release basics

`README.md` with usage, an `install` target (header + `libtest.a`), and a version
number.

### After 1.0 (only if needed)

More assertions (`NE`, `LT`/`GT`, `MEM_EQ` with a hex diff, `NULL`/`NOT_NULL`);
non-fatal `EXPECT_*` (requires several failures per test, which breaks the
single-failure design); optional C11 `LT_ASSERT_EQ` over the typed macros;
machine-readable output (TAP or JUnit XML) for CI; parallel execution;
auto-registration with `__attribute__((constructor))` (not portable, last).

## Known loose ends

- Output capture (only failing tests print their output; stdout/stderr into an
  unlinked `mkstemp` file rather than a pipe, which would deadlock a chatty test
  at 64 KB) was designed but **never implemented**, although this file once
  described it as done. Build it as its own step if it is still wanted.
- GCC on Linux was not run for the fixtures step (only clang on macOS).
- `lt_signal_name` returns a static buffer for unknown signals: fine while the
  runner is single-threaded, revisit if parallel execution ever arrives.
- The timeout test takes about one second of the suite's runtime.