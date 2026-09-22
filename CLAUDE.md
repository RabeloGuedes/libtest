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
make example              # runs example/, which fails ON PURPOSE (6/8 passed, make exits 1)
make test ARGS="--filter=str_ --no-fork"   # ARGS is passed to the test binary
make print-OBJS           # prints any Makefile variable, for debugging the build
make install PREFIX=~/.local   # libtest.a + libtest.h; make uninstall undoes it
make version              # the version number, also in LT_VERSION and --version
```

`make test` must end with `N/N passed`. `make example` must show exactly two
failures (`test_add_fails_on_purpose`, `test_greeting_fails_on_purpose`) with
values printed. Run **both** before considering any change done.

## Layout

```
README.md              the public documentation: usage, options, install
inc/libtest.h          public API: types, functions, assertion macros
inc/lt_internal.h      private: only the library's own .c files include it
src/runner.c           current-result state, lt_run_in, reporting, lt_run_suites, lt_main
src/assert.c           lt_check* functions, value formatting, string escaping
src/isolate.c          lt_run_forked_in: fork + pipe + waitpid + alarm timeout
src/select.c           --filter matching against "suite/test" names
src/tags.c             --tag / --skip-tag matching, lt_has_tag
src/capture.c          a test's stdout/stderr into an unlinked temp file,
                       plus the temp file and stream helpers exec.c reuses
src/exec.c             lt_exec: fork + execvp + stdin/stdout/stderr files
src/options.c          t_lt_options, defaults, lt_parse_args
src/signal_name.c      signal number -> "SIGSEGV" etc. (hand-rolled, not strsignal)
tests/test_libtest.c   the framework testing itself (86 tests)
example/test_example.c usage demo: three tagged suites, two intentional failures
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

**Options** (`--filter=SUBSTRING`, `--tag=NAME`, `--skip-tag=NAME`,
`--timeout=SECONDS` (0 disables, max 3600), `--no-fork`, `--no-capture`,
`--color`, `--no-color`, `--list`, `--version`). Also settable in
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

**Tags.** A test and a suite can each carry tags; a test's tags are its own plus
its suite's. The untagged macros are unchanged, so nothing had to be rewritten:

```c
LT_TEST_TAGGED(test_slow_path, "slow"),
LT_SUITE_TAGGED("db", setup, teardown, tests, "integration"),
```

`--tag=NAME` keeps only tests carrying it; repeating it is an OR
(`--tag=unit --tag=e2e`). `--skip-tag=NAME` drops them, and always wins over a
`--tag`, so `--tag=unit --skip-tag=slow` does what it says for a test tagged
`unit,slow`. Tag and name are an AND: `--filter` still applies. `--list` obeys
both. At most `LT_MAX_TAGS` (8) of each per command line; one more is a usage
error, as is `--tag=` and `--tag=a,b` (one flag carries one tag).

**Output capture.** A forked test's stdout and stderr go to a temp file, and
only a **failing** test has them printed, prefixed with `| ` under an `output:`
line. Truncated at `LT_OUTPUT_SIZE` (4 KB), keeping the start, with a
`... (truncated)` marker. `--no-capture` turns it off; `--no-fork` disables it
too, since there is no child to capture.

**Running a program (end to end).** `lt_exec` runs a binary and fills a
`t_lt_process`; there are no new assertion macros, the existing ones read it:

```c
t_lt_process	proc;

LT_EXEC(&proc, "./my_program", "--flag");
LT_ASSERT(proc.started);
LT_ASSERT_INT_EQ(proc.exit_status, 0);
LT_ASSERT_STR_EQ(proc.out.text, "done\n");
```

`LT_EXEC_IN(&proc, "input\n", ...)` feeds stdin; `lt_exec(&proc, argv, input)`
takes an argv built at runtime. `argv[0]` goes through PATH. `out` and `err`
are separate `t_lt_stream` (same 4 KB buffer and `truncated` flag as capture).
`started` is 0 when the program never ran, which is **not** the same as it
exiting 127. `signum` is the signal that killed it and `timed_out` says that
signal was the timeout.

**Release.** `VERSION` in the Makefile and `LT_VERSION` in the header have to
agree; `--version` prints the second one. `make install` copies only
`libtest.a` and `libtest.h` (never `lt_internal.h`) under `PREFIX`, defaulting
to `/usr/local` and honouring `DESTDIR`. `make uninstall` removes exactly those
two.

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
- **A failed exec is reported through a close-on-exec pipe, not exit 127.**
  A successful `execvp` closes the pipe and the parent reads end of file; a
  failed one writes a byte first. Using 127 would be indistinguishable from the
  program itself exiting 127, which a shell does all the time.
- **`lt_exec` gives the program files for all three streams, never pipes.** Same
  reason as capture, plus stdin: a program that reads gets end of file at once
  instead of hanging on the runner's terminal, and no combination of streams
  can deadlock.
- **The program inherits none of the framework's descriptors.** `lt_move_fd`
  drops each temp file once duplicated (guarding the case where it already sits
  on the target, which happens when the runner is started with a stream
  closed), and both the ready pipe and the result pipe of `isolate.c` are
  close-on-exec. The result pipe leaked into every program a test ran until
  `lt_exec` existed to show it.
- **`alarm` survives `exec`,** so the child sets it before the exec and the
  program inherits the same timeout the test uses. The test's own alarm started
  earlier, so it still bounds everything.
- **Capture uses a temp file (`mkstemp` + immediate `unlink`), not a pipe.** A
  pipe fills at 64 KB and would deadlock a chatty test; a file also keeps what
  was printed right before a crash. stdout/stderr are unbuffered in the child
  for the same reason (a buffer dies with the process).
- **The captured text lives in the parent, not in `t_lt_result`.** The parent
  creates the file before forking and reads it after `waitpid`, so the result
  stays small, nothing extra crosses the pipe, and a crashed child still leaves
  its output behind. It is the second piece of global state after `lt_current`.
- **Tags are matched whole, never as a substring.** `--tag=unit` must not drag
  in a test tagged `unitary`, so `lt_has_tag` compares each comma separated
  element and checks that it ends there. `--filter` stays a substring match:
  a name is prose, a tag is an identifier.
- **A tag list is one string (`"unit,slow"`), not an array.** One pointer per
  test, no array literal per test in the registration block, and no limit on how
  many tags a test carries. Spaces are not trimmed: `"unit, slow"` holds a tag
  named `" slow"` that nothing will match.
- **`--tag=a,b` is rejected instead of split.** Splitting would give two ways to
  say the same thing; silently keeping it as one tag would never match. A usage
  error is the only option that tells the reader.
- **A `--skip-tag` beats a `--tag`.** Exclusion is what tags mainly buy over
  `--filter`, and "run unit, never slow" has to mean it for a `unit,slow` test.
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
- A new field in a public struct must be spelled out in the macros that build
  it (`LT_TEST` fills `tags` with `NULL`): `-Wextra` turns a missing initializer
  into an error, and user code is built with the same flags as the library.
  Hand-written struct literals in the tests have to be updated by hand.
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
  An equivalent mutant (one no test can kill because the code it changes buys
  nothing) counts as dead code: simplify to the form the mutant produced. That
  is how `lt_next_tag` lost its comma-skipping loop. A mutant that survives
  because no test can produce the condition (`lt_read_some` looping on a short
  read, which a regular file does not do) is neither: keep the code and say so
  here rather than claim a clean sweep.
  An overflow inside a struct is invisible to AddressSanitizer, so assert the
  invariant instead (`size < LT_OUTPUT_SIZE`).
  When mutating the runner itself, judge by `FAIL` lines, not by the exit code:
  a mutant that inverts the exit code makes `make test` return 0 while tests
  fail. Run `make fclean` between mutants (a restored file can leave a stale
  object) and mutate so it still compiles (an unused variable is a `-Werror`).
- Do not bend the framework to silence a warning caused by artificial test code.
  Example: clang's `-Wliteral-conversion` fired on `LT_ASSERT(0.5)`; the fix was
  a `double` variable in the test, not a change to the macro.
- `tests/test_libtest.c` includes `lt_internal.h` for exactly two things: the
  unlink check (`fstat` on the descriptor) and the capture buffer invariant.
  Everything else goes through the public API.
- To test the runner's printed output, redirect stdout inside a forked inner test
  (`tmpfile` + `dup2`) and compare the text read back. The redirection and any
  option change die with the child. Such a test must reset the options to the
  defaults first (`*lt_options() = lt_default_options()`), or it inherits the
  outer command line and breaks under `make test ARGS="--filter=x"`.
- Use real runtime values in tests: identical string literals may share an
  address (use a buffer to test `STR_EQ`); GCC folds `1 / 0` at `-O0` (use
  `raise(SIGFPE)`).

## Roadmap to 1.0

Work one step at a time, and stop for the owner's review after each step.

### 1. Fixtures (setup / teardown) — done, awaiting the owner's review

See "Fixtures" above and the design decisions. Not done on purpose: `--list`
prints no suite headers; a crash in setup/teardown cannot say which phase.

### 2. Tags — done, awaiting the owner's review

See "Tags" above and the design decisions. Not done on purpose: `--list` does
not show the tags, and there is no way to list the tags a binary knows.

### 3. Output capture — done, awaiting the owner's review

See "Output capture" above. It was listed as done in this file long before it
existed; it exists now.

### 4. End-to-end module — done, awaiting the owner's review

See "Running a program" above. Not done on purpose: no per-call timeout (the
option's is used), no environment control, no working directory, and no way to
compare an output larger than `LT_OUTPUT_SIZE`.

### 5. Release basics — done, awaiting the owner's review

`README.md`, `make install` / `make uninstall`, and a version number in three
places that must stay in step: `VERSION` in the Makefile, `LT_VERSION` in the
header, and the Status section of the README.

All five steps are done, so the roadmap to 1.0 is finished.

### After 1.0 (only if needed)

More assertions (`NE`, `LT`/`GT`, `MEM_EQ` with a hex diff, `NULL`/`NOT_NULL`);
non-fatal `EXPECT_*` (requires several failures per test, which breaks the
single-failure design); optional C11 `LT_ASSERT_EQ` over the typed macros;
machine-readable output (TAP or JUnit XML) for CI; parallel execution;
auto-registration with `__attribute__((constructor))` (not portable, last).

## Known loose ends

- The suite does not run clean under `-fsanitize=address`:
  `test_crash_becomes_a_failure` and `test_output_survives_a_crash` fail because
  ASan turns the deliberate NULL dereference into `SIGABRT` instead of
  `SIGSEGV`. The tests are right; ASan changes what the crash looks like. Run
  the rest under ASan (`make test CFLAGS="... -fsanitize=address"`) knowing
  those two will fail. Note the owner's shell aliases `cc` with
  `-fsanitize=address`, which `make` does not pick up.
- `test_exec_leaks_no_descriptors` reads `/dev/fd`, which exists on macOS and
  on Linux, but would need rewriting on a system without it. It checks
  descriptors 3 to 12 by hand, so a leak on a higher one would go unseen. It
  also assumes the environment leaks none of its own, which an emulator breaks:
  under `qemu-user` a bare fork and exec in C already leaves six descriptors
  open, with no libtest involved. Hence the `clean-fds` tag, and
  `--skip-tag=clean-fds` for such a run. The framework has no skip status to
  express this more gracefully.
- `test_exec_times_out` costs about a second, like the runner's own timeout
  test. Both are tagged `slow`, so `--skip-tag=slow` halves the suite's time.
- `lt_read_some`'s loop survives mutation: a regular file never returns a short
  read, so no test can force a second iteration. Kept as defensive code.
- Verified on macOS/clang (arm64) and Linux/GCC 14 (aarch64, native): 86/86 in
  both. Linux x86_64 was only run emulated, where everything passes but the
  `clean-fds` test, for the environmental reason above; native x86_64 remains
  untested.
- `lt_signal_name` returns a static buffer for unknown signals: fine while the
  runner is single-threaded, revisit if parallel execution ever arrives.
- The timeout test takes about one second of the suite's runtime.