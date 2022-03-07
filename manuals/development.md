# Development

Updated: 06 Oct 2021

This document is meant for the day when I (Gavin D. Howard) get [hit by a
bus][1]. In other words, it's meant to make the [bus factor][1] a non-issue.

This document is supposed to contain all of the knowledge necessary to develop
`bc` and `dc`.

In addition, this document is meant to add to the [oral tradition of software
engineering][118], as described by Bryan Cantrill.

This document will reference other parts of the repository. That is so a lot of
the documentation can be closest to the part of the repo where it is actually
necessary.

## What Is It?

This repository contains an implementation of both [POSIX `bc`][2] and [Unix
`dc`][3].

POSIX `bc` is a standard utility required for POSIX systems. `dc` is a
historical utility that was included in early Unix and even predates both Unix
and C. They both are arbitrary-precision command-line calculators with their own
programming languages. `bc`'s language looks similar to C, with infix notation
and including functions, while `dc` uses [Reverse Polish Notation][4] and allows
the user to execute strings as though they were functions.

In addition, it is also possible to build the arbitrary-precision math as a
library, named [`bcl`][156].

**Note**: for ease, I will refer to both programs as `bc` in this document.
However, if I say "just `bc`," I am referring to just `bc`, and if I say `dc`, I
am referring to just `dc`.

### History

This project started in January 2018 when a certain individual on IRC, hearing
that I knew how to write parsers, asked me to write a `bc` parser for his math
library. I did so. I thought about writing my own math library, but he
disparaged my programming skills and made me think that I couldn't do it.

However, he took so long to do it that I eventually decided to give it a try and
had a working math portion in two weeks. It taught me that I should not listen
to such people.

From that point, I decided to make it an extreme learning experience about how
to write quality software.

That individual's main goal had been to get his `bc` into [toybox][16], and I
managed to get my own `bc` in. I also got it in [busybox][17].

Eventually, in late 2018, I also decided to try my hand at implementing
[Karatsuba multiplication][18], an algorithm that that unnamed individual
claimed I could never implement. It took me a bit, but I did it.

This project became a passion project for me, and I continued. In mid-2019,
Stefan Eßer suggested I improve performance by putting more than 1 digit in each
section of the numbers. After I showed immaturity because of some burnout, I
implemented his suggestion, and the results were incredible.

Since that time, I have gradually been improving the `bc` as I have learned more
about things like fuzzing, [`scan-build`][19], [valgrind][20],
[AddressSanitizer][21] (and the other sanitizers), and many other things.

One of my happiest moments was when my `bc` was made the default in FreeBSD.

But since I believe in [finishing the software I write][22], I have done less
work on `bc` over time, though there are still times when I put a lot of effort
in, such as now (17 June 2021), when I am attempting to convince OpenBSD to use
my `bc`.

And that is why I am writing this document: someday, someone else is going to
want to change my code, and this document is my attempt to make it as simple as
possible.

### Values

[According to Bryan Cantrill][10], all software has values. I think he's
correct, though I [added one value for programming languages in particular][11].

However, for `bc`, his original list will do:

* Approachability
* Availability
* Compatibility
* Composability
* Debuggability
* Expressiveness
* Extensibility
* Interoperability
* Integrity
* Maintainability
* Measurability
* Operability
* Performance
* Portability
* Resiliency
* Rigor
* Robustness
* Safety
* Security
* Simplicity
* Stability
* Thoroughness
* Transparency
* Velocity

There are several values that don't apply. The reason they don't apply is
because `bc` and `dc` are existing utilities; this is just another
reimplementation. The designs of `bc` and `dc` are set in stone; there is
nothing we can do to change them, so let's get rid of those values that would
apply to their design:

* Compatibility
* Integrity
* Maintainability
* Measurability
* Performance
* Portability
* Resiliency
* Rigor
* Robustness
* Safety
* Security
* Simplicity
* Stability
* Thoroughness
* Transparency

Furthermore, some of the remaining ones don't matter to me, so let me get rid of
those and order the rest according to my *actual* values for this project:

* Robustness
* Stability
* Portability
* Compatibility
* Performance
* Security
* Simplicity

First is **robustness**. This `bc` and `dc` should be robust, accepting any
input, never crashing, and instead, returning an error.

Closely related to that is **stability**. The execution of `bc` and `dc` should
be deterministic and never change for the same inputs, including the
pseudo-random number generator (for the same seed).

Third is **portability**. These programs should run everywhere that POSIX
exists, as well as Windows. This means that just about every person on the
planet will have access to these programs.

Next is **compatibility**. These programs should, as much as possible, be
compatible with other existing implementations and standards.

Then we come to **performance**. A calculator is only usable if it's fast, so
these programs should run as fast as possible.

After that is **security**. These programs should *never* be the reason a user's
computer is compromised.

And finally, **simplicity**. Where possible, the code should be simple, while
deferring to the above values.

Keep these values in mind for the rest of this document, and for exploring any
other part of this repo.

#### Portability

But before I go on, I want to talk about portability in particular.

Most of these principles just require good attention and care, but portability
is different. Sometimes, it requires pulling in code from other places and
adapting it. In other words, sometimes I need to duplicate and adapt code.

This happened in a few cases:

* Option parsing (see [`include/opt.h`][35]).
* History (see [`include/history.h`][36]).
* Pseudo-Random Number Generator (see [`include/rand.h`][37]).

This was done because I decided to ensure that `bc`'s dependencies were
basically zero. In particular, either users have a normal install of Windows or
they have a POSIX system.

A POSIX system limited me to C99, `sh`, and zero external dependencies. That
last item is why I pull code into `bc`: if I pull it in, it's not an external
dependency.

That's why `bc` has duplicated code. Remove it, and you risk `bc` not being
portable to some platforms.

## Suggested Course

I do have a suggested course for programmers to follow when trying to understand
this codebase. The order is this:

1.	`bc` Spec.
2.	Manpages.
3.	Test suite.
4.	Understand the build.
5.	Algorithms manual.
6.	Code concepts.
7.	Repo structure.
8.	Headers.
9.	Source code.

This order roughly follows this order:

1. High-level requirements
2. Low-level requirements
3. High-level implementation
4. Low-level implementation

In other words, first understand what the code is *supposed* to do, then
understand the code itself.

## Useful External Tools

I have a few tools external to `bc` that are useful:

* A [Vim plugin with syntax files made specifically for my `bc` and `dc`][132].
* A [repo of `bc` and `dc` scripts][133].
* A set of `bash` aliases (see below).
* A `.bcrc` file with items useful for my `bash` setup (see below).

My `bash` aliases are these:

```sh
alias makej='make -j16'
alias mcmake='make clean && make'
alias mcmakej='make clean && make -j16'
alias bcdebug='CPPFLAGS="-DBC_DEBUG_CODE=1" CFLAGS="-Weverything -Wno-padded \
    -Wno-switch-enum -Wno-format-nonliteral -Wno-cast-align \
    -Wno-unreachable-code-return -Wno-missing-noreturn \
    -Wno-disabled-macro-expansion -Wno-unreachable-code -Wall -Wextra \
    -pedantic -std=c99" ./configure.sh'
alias bcconfig='CFLAGS="-Weverything -Wno-padded -Wno-switch-enum \
    -Wno-format-nonliteral -Wno-cast-align -Wno-unreachable-code-return \
    -Wno-missing-noreturn -Wno-disabled-macro-expansion -Wno-unreachable-code \
    -Wall -Wextra -pedantic -std=c99" ./configure.sh'
alias bcnoassert='CPPFLAGS="-DNDEBUG" CFLAGS="-Weverything -Wno-padded \
    -Wno-switch-enum -Wno-format-nonliteral -Wno-cast-align \
    -Wno-unreachable-code-return -Wno-missing-noreturn \
    -Wno-disabled-macro-expansion -Wno-unreachable-code -Wall -Wextra \
    -pedantic -std=c99" ./configure.sh'
alias bcdebugnoassert='CPPFLAGS="-DNDEBUG -DBC_DEBUG_CODE=1" \
    CFLAGS="-Weverything -Wno-padded -Wno-switch-enum -Wno-format-nonliteral \
    -Wno-cast-align -Wno-unreachable-code-return -Wno-missing-noreturn \
    -Wno-disabled-macro-expansion -Wno-unreachable-code -Wall -Wextra \
    -pedantic -std=c99" ./configure.sh'
alias bcunset='unset BC_LINE_LENGTH && unset BC_ENV_ARGS'
```

`makej` runs `make` with all of my cores.

`mcmake` runs `make clean` before running `make`. It will take a target on the
command-line.

`mcmakej` is a combination of `makej` and `mcmake`.

`bcdebug` configures `bc` for a full debug build, including `BC_DEBUG_CODE` (see
[Debugging][134] below).

`bcconfig` configures `bc` with Clang (Clang is my personal default compiler)
using full warnings, with a few really loud and useless warnings turned off.

`bcnoassert` configures `bc` to not have asserts built in.

`bcdebugnoassert` is like `bcnoassert`, except it also configures `bc` for debug
mode.

`bcunset` unsets my personal `bc` environment variables, which are set to:

```sh
export BC_ENV_ARGS="-l $HOME/.bcrc"
export BC_LINE_LENGTH="74"
```

Unsetting these environment variables are necessary for running
[`scripts/release.sh`][83] because otherwise, it will error when attempting to
run `bc -s` on my `$HOME/.bcrc`.

Speaking of which, the contents of that file are:

```bc
define void print_time_unit(t){
	if(t<10)print "0"
	if(t<1&&t)print "0"
	print t,":"
}
define void sec2time(t){
	auto s,m,h,d,r
	r=scale
	scale=0
	t=abs(t)
	s=t%60
	t-=s
	m=t/60%60
	t-=m
	h=t/3600%24
	t-=h
	d=t/86400
	if(d)print_time_unit(d)
	if(h)print_time_unit(h)
	print_time_unit(m)
	if(s<10)print "0"
	if(s<1&&s)print "0"
	s
	scale=r
}
define minutes(secs){
	return secs/60;
}
define hours(secs){
	return secs/3600;
}
define days(secs){
	return secs/3600/24;
}
define years(secs){
	return secs/3600/24/365.25;
}
define fbrand(b,p){
	auto l,s,t
	b=abs(b)$
	if(b<2)b=2
	s=scale
	t=b^abs(p)$
	l=ceil(l2(t),0)
	if(l>scale)scale=l
	t=irand(t)/t
	scale=s
	return t
}
define ifbrand(i,b,p){return irand(abs(i)$)+fbrand(b,p)}
```

This allows me to use `bc` as part of my `bash` prompt.

## Code Style

The code style for `bc` is...weird, and that comes from historical accident.

In [History][23], I mentioned how I got my `bc` in [toybox][16]. Well, in order
to do that, my `bc` originally had toybox style. Eventually, I changed to using
tabs, and assuming they were 4 spaces wide, but other than that, I basically
kept the same style, with some exceptions that are more or less dependent on my
taste.

The code style is as follows:

* Tabs are 4 spaces.
* Tabs are used at the beginning of lines for indent.
* Spaces are used for alignment.
* Lines are limited to 80 characters, period.
* Pointer asterisk (`*`) goes with the variable (on the right), not the type,
  unless it is for a pointer type returned from a function.
* The opening brace is put on the same line as the header for the function,
  loop, or `if` statement.
* Unless the header is more than one line, in which case the opening brace is
  put on its own line.
* If the opening brace is put on its own line, there is no blank line after it.
* If the opening brace is *not* put on its own line, there *is* a blank line
  after it, *unless* the block is only one or two lines long.
* Code lines are grouped into what I call "paragraphs." Basically, lines that
  seem like they should go together are grouped together. This one comes down
  to judgment.
* Bodies of `if` statements, `else` statements, and loops that are one line
  long are put on the same line as the statement, unless the header is more than
  one line long, and/or, the header and body cannot fit into 80 characters with
  a space inbetween them.
* If single-line bodies are on a separate line from their headers, and the
  headers are only a single line, then no braces are used.
* However, braces are *always* used if they contain another `if` statement or
  loop.
* Loops with empty bodies are ended with a semicolon.
* Expressions that return a boolean value are surrounded by paretheses.
* Macro backslashes are aligned as far to the left as possible.
* Binary operators have spaces on both sides.
* If a line with binary operators overflows 80 characters, a newline is inserted
  *after* binary operators.
* Function modifiers and return types are on the same line as the function name.
* With one exception, `goto`'s are only used to jump to the end of a function
  for cleanup.
* All structs, enums, and unions are `typedef`'ed.
* All constant data is in one file: [`src/data.c`][131], but the corresponding
  `extern` declarations are in the appropriate header file.
* All local variables are declared at the beginning of the scope where they
  appear. They may be initialized at that point, if it does not invoke UB or
  otherwise cause bugs.
* All precondition `assert()`'s (see [Asserts][135]) come *after* local variable
  declarations.
* Besides short `if` statements and loops, there should *never* be more than one
  statement per line.

### ClangFormat

I attempted three times to use [ClangFormat][24] to impose a standard,
machine-useful style on `bc`. All three failed. Otherwise, the style in this
repo would be more consistent.

## Repo Structure

Functions are documented with Doxygen-style doc comments. Functions that appear
in headers are documented in the headers, while static functions are documented
where they are defined.

### `configure`

A symlink to [`configure.sh`][69].

### `configure.sh`

This is the script to configure `bc` and [`bcl`][156] for building.

This `bc` has a custom build system. The reason for this is because of
[*portability*][136].

If `bc` used an outside build system, that build system would be an external
dependency. Thus, I had to write a build system for `bc` that used nothing but
C99 and POSIX utilities.

One of those utilities is POSIX `sh`, which technically implements a
Turing-complete programming language. It's a terrible one, but it works.

A user that wants to build `bc` on a POSIX system (not Windows) first runs
`configure.sh` with the options he wants. `configure.sh` uses those options and
the `Makefile` template ([`Makefile.in`][70]) to generate an actual valid
`Makefile`. Then `make` can do the rest.

For more information about the build process, see the [Build System][142]
section and the [build manual][14].

For more information about shell scripts, see [POSIX Shell Scripts][76].

`configure.sh` does the following:

1.	It processes command-line arguments and figure out what the user wants to
	build.
2.	It reads in [`Makefile.in`][70].
3.	One-by-one, it replaces placeholders (in [`Makefile.in`][70]) of the form
	`%%<placeholder_name>%%` based on the [build type][81].
4.	It appends a list of file targets based on the [build type][81].
5.	It appends the correct test targets.
6.	It copies the correct manpage and markdown manual for `bc` and `dc` into a
	location from which they can be copied for install.
7.	It does a `make clean` to reset the build state.

### `.gitattributes`

A `.gitattributes` file. This is needed to preserve the `crlf` line endings in
the Visual Studio files.

### `.gitignore`

The `.gitignore`

### `LICENSE.md`

This is the `LICENSE` file, including the licenses of various software that I
have borrowed.

### `Makefile.in`

This is the `Makefile` template for [`configure.sh`][69] to use for generating a
`Makefile`.

For more information, see [`configure.sh`][69], the [Build System][142] section,
and the [build manual][14].

Because of [portability][136], the generated `Makefile.in` should be a pure
[POSIX `make`][74]-compatible `Makefile` (minus the placeholders). Here are a
few snares for the unwary programmer in this file:

1.	No extensions allowed, including and especially GNU extensions.
2.	If new headers are added, they must also be added to `Makefile.in`.
3.	Don't delete the `.POSIX:` empty target at the top; that's what tells `make`
	implementations that pure [POSIX `make`][74] is needed.

In particular, there is no way to set up variables other than the `=` operator.
There are no conditionals, so all of the conditional stuff must be in
[`configure.sh`][69]. This is, in fact, why [`configure.sh`][69] exists in the
first place: [POSIX `make`][74] is barebones and only does a build with no
configuration.

### `NEWS.md`

A running changelog with an entry for each version. This should be updated at
the same time that [`include/version.h`][75] is.

### `NOTICE.md`

The `NOTICE` file with proper attributions.

### `README.md`

The `README`. Read it.

### `benchmarks/`

The folder containing files to generate benchmarks.

Each of these files was made, at one time or another, to benchmark some
experimental feature, so if it seems there is no rhyme or reason to these
benchmarks, it is because there is none, besides historical accident.

#### `bc/`

The folder containing `bc` scripts to generate `bc` benchmarks.

##### `add.bc`

The file to generate the benchmark to benchmark addition in `bc`.

##### `arrays_and_constants.bc`

The file to generate the benchmark to benchmark `bc` using lots of array names
and constants.

##### `arrays.bc`

The file to generate the benchmark to benchmark `bc` using lots of array names.

##### `constants.bc`

The file to generate the benchmark to benchmark `bc` using lots of constants.

##### `divide.bc`

The file to generate the benchmark to benchmark division in `bc`.

##### `functions.bc`

The file to generate the benchmark to benchmark `bc` using lots of functions.

##### `irand_long.bc`

The file to generate the benchmark to benchmark `bc` using lots of calls to
`irand()` with large bounds.

##### `irand_short.bc`

The file to generate the benchmark to benchmark `bc` using lots of calls to
`irand()` with small bounds.

##### `lib.bc`

The file to generate the benchmark to benchmark `bc` using lots of calls to
heavy functions in `lib.bc`.

##### `multiply.bc`

The file to generate the benchmark to benchmark multiplication in `bc`.

##### `postfix_incdec.bc`

The file to generate the benchmark to benchmark `bc` using postfix increment and
decrement operators.

##### `power.bc`

The file to generate the benchmark to benchmark power (exponentiation) in `bc`.

##### `subtract.bc`

The file to generate the benchmark to benchmark subtraction in `bc`.

##### `strings.bc`

The file to generate the benchmark to benchmark `bc` using lots of strings.

#### `dc/`

The folder containing `dc` scripts to generate `dc` benchmarks.

##### `modexp.dc`

The file to generate the benchmark to benchmark modular exponentiation in `dc`.

### `gen/`

A folder containing the files necessary to generate C strings that will be
embedded in the executable.

All of the files in this folder have license headers, but the program and script
that can generate strings from them include code to strip the license header out
before strings are generated.

#### `bc_help.txt`

A text file containing the text displayed for `bc -h` or `bc --help`.

This text just contains the command-line options and a short summary of the
differences from GNU and BSD `bc`'s. It also directs users to the manpage.

The reason for this is because otherwise, the help would be far too long to be
useful.

**Warning**: The text has some `printf()` format specifiers. You need to make
sure the format specifiers match the arguments given to `bc_file_printf()`.

#### `dc_help.txt`

A text file containing the text displayed for `dc -h` or `dc --help`.

This text just contains the command-line options and a short summary of the
differences from GNU and BSD `dc`'s. It also directs users to the manpage.

The reason for this is because otherwise, the help would be far too long to be
useful.

**Warning**: The text has some `printf()` format specifiers. You need to make
sure the format specifiers match the arguments given to `bc_file_printf()`.

#### `lib.bc`

A `bc` script containing the [standard math library][5] required by POSIX. See
the [POSIX standard][2] for what is required.

This file does not have any extraneous whitespace, except for tabs at the
beginning of lines. That is because this data goes directly into the binary,
and whitespace is extra bytes in the binary. Thus, not having any extra
whitespace shrinks the resulting binary.

However, tabs at the beginning of lines are kept for two reasons:

1.	Readability. (This file is still code.)
2.	The program and script that generate strings from this file can remove
	tabs at the beginning of lines.

For more details about the algorithms used, see the [algorithms manual][25].

However, there are a few snares for unwary programmers.

First, all constants must be one digit. This is because otherwise, multi-digit
constants could be interpreted wrongly if the user uses a different `ibase`.
This does not happen with single-digit numbers because they are guaranteed to be
interpreted what number they would be if the `ibase` was as high as possible.

This is why `A` is used in the library instead of `10`, and things like `2*9*A`
for `180` in [`lib2.bc`][26].

As an alternative, you can set `ibase` in the function, but if you do, make sure
to set it with a single-digit number and beware the snare below...

Second, `scale`, `ibase`, and `obase` must be safely restored before returning
from any function in the library. This is because without the `-g` option,
functions are allowed to change any of the globals.

Third, all local variables in a function must be declared in an `auto` statement
before doing anything else. This includes arrays. However, function parameters
are considered predeclared.

Fourth, and this is only a snare for `lib.bc`, not [`lib2.bc`][26], the code
must not use *any* extensions. It has to work when users use the `-s` or `-w`
flags.

#### `lib2.bc`

A `bc` script containing the [extended math library][7].

Like [`lib.bc`][8], and for the same reasons, this file should have no
extraneous whitespace, except for tabs at the beginning of lines.

For more details about the algorithms used, see the [algorithms manual][25].

Also, be sure to check [`lib.bc`][8] for the snares that can trip up unwary
programmers when writing code for `lib2.bc`.

#### `strgen.c`

Code for the program to generate C strings from text files. This is the original
program, although [`strgen.sh`][9] was added later.

The reason I used C here is because even though I knew `sh` would be available
(it must be available to run `configure.sh`), I didn't know how to do what I
needed to do with POSIX utilities and `sh`.

Later, [`strgen.sh`][9] was contributed by Stefan Eßer of FreeBSD, showing that
it *could* be done with `sh` and POSIX utilities.

However, `strgen.c` exists *still* exists because the versions generated by
[`strgen.sh`][9] may technically hit an environmental limit. (See the [draft C99
standard][12], page 21.) This is because [`strgen.sh`][9] generates string
literals, and in C99, string literals can be limited to 4095 characters, and
`gen/lib2.bc` is above that.

Fortunately, the limit for "objects," which include `char` arrays, is much
bigger: 65535 bytes, so that's what `strgen.c` generates.

However, the existence of `strgen.c` does come with a cost: the build needs C99
compiler that targets the host machine. For more information, see the ["Cross
Compiling" section][13] of the [build manual][14].

Read the comments in `strgen.c` for more detail about it, the arguments it
takes, and how it works.

#### `strgen.sh`

An `sh` script that will generate C strings that uses only POSIX utilities. This
exists for those situations where a host C99 compiler is not available, and the
environment limits mentioned above in [`strgen.c`][15] don't matter.

`strgen.sh` takes the same arguments as [`strgen.c`][15], and the arguments mean
the exact same things, so see the comments in [`strgen.c`][15] for more detail
about that, and see the comments in `strgen.sh` for more details about it and
how it works.

For more information about shell scripts, see [POSIX Shell Scripts][76].

### `include/`

A folder containing the headers.

The headers are not included among the source code because I like it better that
way. Also there were folders within `src/` at one point, and I did not want to
see `#include "../some_header.h"` or things like that.

So all headers are here, even though only one ([`bcl.h`][30]) is meant for end
users (to be installed in `INCLUDEDIR`).

#### `args.h`

This file is the API for processing command-line arguments.

#### `bc.h`

This header is the API for `bc`-only items. This includes the `bc_main()`
function and the `bc`-specific lexing and parsing items.

The `bc` parser is perhaps the most sensitive part of the entire codebase. See
the documentation in `bc.h` for more information.

The code associated with this header is in [`src/bc.c`][40],
[`src/bc_lex.c`][41], and [`src/bc_parse.c`][42].

#### `bcl.h`

This header is the API for the [`bcl`][156] library.

This header is meant for distribution to end users and contains the API that end
users of [`bcl`][156] can use in their own software.

This header, because it's the public header, is also the root header. That means
that it has platform-specific fixes for Windows. (If the fixes were not in this
header, the build would fail on Windows.)

The code associated with this header is in [`src/library.c`][43].

#### `dc.h`

This header is the API for `dc`-only items. This includes the `dc_main()`
function and the `dc`-specific lexing and parsing items.

The code associated with this header is in [`src/dc.c`][44],
[`src/dc_lex.c`][45], and [`src/dc_parse.c`][46].

#### `file.h`

This header is for `bc`'s internal buffered I/O API.

For more information about `bc`'s error handling and custom buffered I/O, see
[Error Handling][97] and [Custom I/O][114], along with [`status.h`][176] and the
notes about version [`3.0.0`][32] in the [`NEWS`][32].

The code associated with this header is in [`src/file.c`][47].

#### `history.h`

This header is for `bc`'s implementation of command-line editing/history, which
is based on a [UTF-8-aware fork][28] of [`linenoise`][29].

For more information, see the [Command-Line History][189] section.

The code associated with this header is in [`src/history.c`][48].

#### `lang.h`

This header defines the data structures and bytecode used for actual execution
of `bc` and `dc` code.

Yes, it's misnamed; that's an accident of history where the first things I put
into it all seemed related to the `bc` language.

The code associated with this header is in [`src/lang.c`][49].

#### `lex.h`

This header defines the common items that both programs need for lexing.

The code associated with this header is in [`src/lex.c`][50],
[`src/bc_lex.c`][41], and [`src/dc_lex.c`][45].

#### `library.h`

This header defines the things needed for [`bcl`][156] that users should *not*
have access to. In other words, [`bcl.h`][30] is the *public* header for the
library, and this header is the *private* header for the library.

The code associated with this header is in [`src/library.c`][43].

#### `num.h`

This header is the API for numbers and math.

The code associated with this header is in [`src/num.c`][39].

#### `opt.h`

This header is the API for parsing command-line arguments.

It's different from [`args.h`][31] in that [`args.h`][31] is for the main code
to process the command-line arguments into global data *after* they have already
been parsed by `opt.h` into proper tokens. In other words, `opt.h` actually
parses the command-line arguments, and [`args.h`][31] turns that parsed data
into flags (bits), strings, and expressions that will be used later.

Why are they separate? Because originally, `bc` used `getopt_long()` for
parsing, so [`args.h`][31] was the only one that existed. After it was
discovered that `getopt_long()` has different behavior on different platforms, I
adapted a [public-domain option parsing library][34] to do the job instead. And
in doing so, I gave it its own header.

They could probably be combined, but I don't really care enough at this point.

The code associated with this header is in [`src/opt.c`][51].

#### `parse.h`

This header defines the common items that both programs need for parsing.

Note that the parsers don't produce abstract syntax trees (AST's) or any
intermediate representations. They produce bytecode directly. In other words,
they don't have special data structures except what they need to do their job.

The code associated with this header is in [`src/parse.c`][50],
[`src/bc_lex.c`][42], and [`src/dc_lex.c`][46].

#### `program.h`

This header defines the items needed to manage the data structures in
[`lang.h`][38] as well as any helper functions needed to generate bytecode or
execute it.

The code associated with this header is in [`src/program.c`][53].

#### `rand.h`

This header defines the API for the [pseudo-random number generator
(PRNG)][179].

The PRNG only generates fixed-size integers. The magic of generating random
numbers of arbitrary size is actually given to the code that does math
([`src/num.c`][39]).

The code associated with this header is in [`src/rand.c`][54].

#### `read.h`

This header defines the API for reading from files and `stdin`.

Thus, [`file.h`][55] is really for buffered *output*, while this file is for
*input*. There is no buffering needed for `bc`'s inputs.

The code associated with this header is in [`src/read.c`][56].

#### `status.h`

This header has several things:

* A list of possible errors that internal `bc` code can use.
* Compiler-specific fixes.
* Platform-specific fixes.
* Macros for `bc`'s [error handling][97].

There is no code associated with this header.

#### `vector.h`

This header defines the API for the vectors (resizable arrays) that are used for
data structures.

Vectors are what do the heavy lifting in almost all of `bc`'s data structures.
Even the maps of identifiers and arrays use vectors.

#### `version.h`

This header defines the version of `bc`.

There is no code associated with this header.

#### `vm.h`

This header defines the API for setting up and running `bc` and `dc`.

It is so named because I think of it as the "virtual machine" of `bc`, though
that is probably not true as [`program.h`][57] is probably the "virtual machine"
API. Thus, the name is more historical accident.

The code associated with this header is in [`src/vm.c`][58].

### `locales/`

This folder contains a bunch of `.msg` files and soft links to the real `.msg`
files. This is how locale support is implemented in `bc`.

The files are in the format required by the [`gencat`][59] POSIX utility. They
all have the same messages, in the same order, with the same numbering, under
the same groups. This is because the locale system expects those messages in
that order.

The softlinks exist because for many locales, they would contain the exact same
information. To prevent duplication, they are simply linked to a master copy.

The naming format for all files is:

```
<language_code>_<country_code>.<encoding>.msg
```

This naming format must be followed for all locale files.

### `manuals/`

This folder contains the documentation for `bc`, `dc`, and [`bcl`][156], along
with a few other manuals.

#### `algorithms.md`

This file explains the mathematical algorithms that are used.

The hope is that this file will guide people in understanding how the math code
works.

#### `bc.1.md.in`

This file is a template for the markdown version of the `bc` manual and
manpages.

For more information about how the manpages and markdown manuals are generated,
and for why, see [`scripts/manpage.sh`][60] and [Manuals][86].

#### `bcl.3`

This is the manpage for the [`bcl`][156] library. It is generated from
[`bcl.3.md`][61] using [`scripts/manpage.sh`][60].

For the reason why I check generated data into the repo, see
[`scripts/manpage.sh`][60] and [Manuals][86].

#### `bcl.3.md`

This is the markdown manual for the [`bcl`][156] library. It is the source for the
generated [`bcl.3`][62] file.

#### `benchmarks.md`

This is a document that compares this `bc` to GNU `bc` in various benchmarks. It
was last updated when version [`3.0.0`][32] was released.

It has very little documentation value, other than showing what compiler options
are useful for performance.

#### `build.md`

This is the [build manual][14].

This `bc` has a custom build system. The reason for this is because of
[*portability*][136].

If `bc` used an outside build system, that build system would be an external
dependency. Thus, I had to write a build system for `bc` that used nothing but
C99 and POSIX utilities, including barebones [POSIX `make`][74].

for more information about the build system, see the [build system][142]
section, the [build manual][14], [`configure.sh`][69], and [`Makefile.in`][70].

#### `dc.1.md.in`

This file is a template for the markdown version of the `dc` manual and
manpages.

For more information about how the manpages and markdown manuals are generated,
and for why, see [`scripts/manpage.sh`][60] and [Manuals][86].

#### `development.md`

The file you are reading right now.

#### `header_bcl.txt`

Used by [`scripts/manpage.sh`][60] to give the [`bcl.3`][62] manpage a proper
header.

For more information about generating manuals, see [`scripts/manpage.sh`][60]
and [Manuals][86].

#### `header_bc.txt`

Used by [`scripts/manpage.sh`][60] to give the [generated `bc` manpages][79] a
proper header.

For more information about generating manuals, see [`scripts/manpage.sh`][60]
and [Manuals][86].

#### `header_dc.txt`

Used by [`scripts/manpage.sh`][60] to give the [generated `dc` manpages][80] a
proper header.

For more information about generating manuals, see [`scripts/manpage.sh`][60]
and [Manuals][86].

#### `header.txt`

Used by [`scripts/manpage.sh`][60] to give all generated manpages a license
header.

For more information about generating manuals, see [`scripts/manpage.sh`][60]
and [Manuals][86].

#### `release.md`

A checklist that I try to somewhat follow when making a release.

#### `bc/`

A folder containing the `bc` manuals.

Each `bc` manual corresponds to a [build type][81]. See that link for more
details.

For each manual, there are two copies: the markdown version generated from the
template, and the manpage generated from the markdown version.

#### `dc/`

A folder containing the `dc` manuals.

Each `dc` manual corresponds to a [build type][81]. See that link for more
details.

For each manual, there are two copies: the markdown version generated from the
template, and the manpage generated from the markdown version.

### `scripts/`

This folder contains helper scripts. Most of them are written in pure [POSIX
`sh`][72], but one ([`karatsuba.py`][78]) is written in Python 3.

For more information about the shell scripts, see [POSIX Shell Scripts][76].

#### `afl.py`

This script is meant to be used as part of the fuzzing workflow.

It does one of two things: checks for valid crashes, or runs `bc` and or `dc`
under all of the paths found by [AFL++][125].

See [Fuzzing][82] for more information about fuzzing, including this script.

#### `alloc.sh`

This script is a quick and dirty script to test whether or not the garbage
collection mechanism of the [`BcNum` caching][96] works. It has been little-used
because it tests something that is not important to correctness.

#### `benchmark.sh`

A script making it easy to run benchmarks and to run the executable produced by
[`ministat.c`][223] on them.

For more information, see the [Benchmarks][144] section.

#### `bitfuncgen.c`

A source file for an executable to generate tests for `bc`'s bitwise functions
in [`gen/lib2.bc`][26]. The executable is `scripts/bitfuncgen`, and it is built
with `make bitfuncgen`. It produces the test on `stdout` and the expected
results on `stderr`. This means that to generat tests, use the following
invokation:

```
scripts/bitfuncgen > tests/bc/bitfuncs.txt 2> tests/bc/bitfuncs_results.txt
```

It calls `abort()` if it runs into an error.

#### `exec-install.sh`

This script is the magic behind making sure `dc` is installed properly if it's
a symlink to `bc`. It checks to see if it is a link, and if so, it just creates
a new symlink in the install directory. Of course, it also installs `bc` itself,
or `dc` when it's alone.

#### `functions.sh`

This file is a bunch of common functions for most of the POSIX shell scripts. It
is not supposed to be run; instead, it is *sourced* by other POSIX shell
scripts, like so:

```
. "$scriptdir/functions.sh"
```

or the equivalent, depending on where the sourcing script is.

For more information about the shell scripts, see [POSIX Shell Scripts][76].

#### `fuzz_prep.sh`

Fuzzing is a regular activity when I am preparing for a release.

This script handles all the options and such for building a fuzzable binary.
Instead of having to remember a bunch of options, I just put them in this script
and run the script when I want to fuzz.

For more information about fuzzing, see [Fuzzing][82].

#### `karatsuba.py`

This script has at least one of two major differences from most of the other
scripts:

* It's written in Python 3.
* It's meant for software packagers.

For example, [`scripts/afl.py`][94] and [`scripts/randmath.py`][95] are both in
Python 3, but they are not meant for the end user or software packagers and are
not included in source distributions. But this script is.

This script breaks my rule of only POSIX utilities necessary for package
maintainers, but there's a very good reason for that: it's only meant to be run
*once* when the package is created for the first time, and maybe not even then.

You see, this script does two things: it tests the Karatsuba implementation at
various settings for `KARATSUBA_LEN`, and it figures out what the optimal
`KARATSUBA_LEN` is for the machine that it is running on.

Package maintainers can use this script, when creating a package for this `bc`,
to figure out what is optimal for their users. Then they don't have to run it
ever again. So this script only has to run on the packagers machine.

I tried to write the script in `sh`, by the way, and I finally accepted the
tradeoff of using Python 3 when it became too hard.

However, I also mentioned that it's for testing Karatsuba with various settings
of `KARATSUBA_LEN`. Package maintainers will want to run the [test suite][124],
right?

Yes, but this script is not part of the [test suite][124]; it's used for testing
in the [`scripts/release.sh`][83] script, which is maintainer use only.

However, there is one snare with `karatsuba.py`: I didn't want the user to have
to install any Python libraries to run it. Keep that in mind if you change it.

#### `link.sh`

This script is the magic behind making `dc` a symlink of `bc` when both
calculators are built.

#### `locale_install.sh`

This script does what its name says: it installs locales.

It turns out that this is complicated.

There is a magic environment variable, `$NLSPATH`, that tells you how and where
you are supposed to install locales.

Yes, *how*. And where.

But now is not the place to rant about `$NLSPATH`. For more information on
locales and `$NLSPATH`, see [Locales][85].

#### `locale_uninstall.sh`

This script does what its name says: it uninstalls locales.

This is far less complicated than installing locales. I basically generate a
wildcard path and then list all paths that fit that wildcard. Then I delete each
one of those paths. Easy.

For more information on locales, see [Locales][85].

#### `manpage.sh`

This script is the one that generates markdown manuals from a template and a
manpage from a markdown manual.

For more information about generating manuals, see [Manuals][86].

#### `ministat.c`

This is a file copied [from FreeBSD][221] that calculates the standard
statistical numbers, such as mean, average, and median, based on numbers
obtained from a file.

For more information, see the [FreeBSD ministat(1) manpage][222].

This file allows `bc` to build the `scripts/ministat` executable using the
command `make ministat`, and this executable helps programmers evaluate the
results of [benchmarks][144] more accurately.

#### `package.sh`

This script is what helps `bc` maintainers cut a release. It does the following:

1.	Creates the appropriate `git` tag.
2.	Pushes the `git` tag.
3.	Copies the repo to a temp directory.
4.	Removes files that should not be included in source distributions.
5.	Creates the tarballs.
6.	Signs the tarballs.
7.	Zips and signs the Windows executables if they exist.
8.	Calculates and outputs SHA512 and SHA256 sums for all of the files,
	including the signatures.

This script is for `bc` maintainers to use when cutting a release. It is not
meant for outside use. This means that some non-POSIX utilities can be used,
such as `git` and `gpg`.

In addition, before using this script, it expects that the folders that Windows
generated when building `bc`, `dc`, and [`bcl`][156], are in the parent
directory of the repo, exactly as Windows generated them. If they are not there,
then it will not zip and sign, nor calculate sums of, the Windows executables.

Because this script creates a tag and pushes it, it should *only* be run *ONCE*
per release.

#### `radamsa.sh`

A script to test `bc`'s command-line expression parsing code, which, while
simple, strives to handle as much as possible.

What this script does is it uses the test cases in [`radamsa.txt`][98] an input
to the [Radamsa fuzzer][99].

For more information, see the [Radamsa][128] section.

#### `radamsa.txt`

Initial test cases for the [`radamsa.sh`][100] script.

#### `randmath.py`

This script generates random math problems and checks that `bc`'s and `dc`'s
output matches the GNU `bc` and `dc`. (For this reason, it is necessary to have
GNU `bc` and `dc` installed before using this script.)

One snare: be sure that this script is using the GNU `bc` and `dc`, not a
previously-installed version of this `bc` and `dc`.

If you want to check for memory issues or failing asserts, you can build the
`bc` using `./scripts/fuzz_prep.sh -a`, and then run it under this script. Any
errors or crashes should be caught by the script and given to the user as part
of the "checklist" (see below).

The basic idea behind this script is that it generates as many math problems as
it can, biasing towards situations that may be likely to have bugs, and testing
each math problem against GNU `bc` or `dc`.

If GNU `bc` or `dc` fails, it just continues. If this `bc` or `dc` fails, it
stores that problem. If the output mismatches, it also stores the problem.

Then, when the user sends a `SIGINT`, the script stops testing and goes into
report mode. One-by-one, it will go through the "checklist," the list of failed
problems, and present each problem to the user, as well as whether this `bc` or
`dc` crashed, and its output versus GNU. Then the user can decide to add them as
test cases, which it does automatically to the appropriate test file.

#### `release_settings.txt`

A text file of settings combinations that [`release.sh`][83] uses to ensure that
`bc` and `dc` build and work with various default settings. [`release.sh`][83]
simply reads it line by line and uses each line for one build.

#### `release.sh`

This script is for `bc` maintainers only. It runs `bc`, `dc`, and [`bcl`][156]
through a gauntlet that is mostly meant to be used in preparation for a release.

It does the following:

1.	Builds every [build type][81], with every setting combo in
	[`release_settings.txt`][93] with both calculators, `bc` alone, and `dc`
	alone.
2.	Builds every [build type][81], with every setting combo in
	[`release_settings.txt`][93] with both calculators, `bc` alone, and `dc`
	alone for 32-bit.
3.	Does #1 and #2 for Debug, Release, Release with Debug Info, and Min Size
	Release builds.
4.	Runs the [test suite][124] on every build, if desired.
5.	Runs the [test suite][124] under [ASan, UBSan, and MSan][21] for every build
	type/setting combo.
6.	Runs [`scripts/karatsuba.py`][78] in test mode.
7.	Runs the [test suite][124] for both calculators, `bc` alone, and `dc` alone
	under [valgrind][20] and errors if there are any memory bugs or memory
	leaks.

#### `safe-install.sh`

A script copied from [musl][101] to atomically install files.

#### `test_settings.sh`

A quick and dirty script to help automate rebuilding while manually testing the
various default settings.

This script uses [`test_settings.txt`][103] to generate the various settings
combos.

For more information about settings, see [Settings][102] in the [build
manual][14].

#### `test_settings.txt`

A list of the various settings combos to be used by [`test_settings.sh`][104].

### `src/`

This folder is, obviously, where the actual heart and soul of `bc`, the source
code, is.

All of the source files are in one folder; this simplifies the build system
immensely.

There are separate files for `bc` and `dc` specific code ([`bc.c`][40],
[`bc_lex.c`][41], [`bc_parse.c`][42], [`dc.c`][44], [`dc_lex.c`][45], and
[`dc_parse.c`][46]) where possible because it is cleaner to exclude an entire
source file from a build than to have `#if`/`#endif` preprocessor guards.

That said, it was easier in many cases to use preprocessor macros where both
calculators used much of the same code and data structures, so there is a
liberal sprinkling of them through the code.

#### `args.c`

Code for processing command-line arguments.

The header for this file is [`include/args.h`][31].

#### `bc.c`

The code for the `bc` main function `bc_main()`.

The header for this file is [`include/bc.h`][106].

#### `bc_lex.c`

The code for lexing that only `bc` needs.

The headers for this file are [`include/lex.h`][180] and [`include/bc.h`][106].

#### `bc_parse.c`

The code for parsing that only `bc` needs. This code is the most complex and
subtle in the entire codebase.

The headers for this file are [`include/parse.h`][181] and
[`include/bc.h`][106].

#### `data.c`

Due to [historical accident][23] because of a desire to get my `bc` into
[toybox][16], all of the constant data that `bc` needs is all in one file. This
is that file.

There is no code in this file, but a lot of the const data has a heavy influence
on code, including the order of data in arrays because that order has to
correspond to the order of other things elsewhere in the codebase. If you change
the order of something in this file, run `make test`, and get errors, you
changed something that depends on the order that you messed up.

Almost all headers have `extern` references to items in this file.

#### `dc.c`

The code for the `dc` main function `dc_main()`.

The header for this file is [`include/dc.h`][182].

#### `dc_lex.c`

The code for lexing that only `dc` needs.

The headers for this file are [`include/lex.h`][180] and [`include/dc.h`][182].

#### `dc_parse.c`

The code for parsing that only `dc` needs.

The headers for this file are [`include/parse.h`][181] and
[`include/bc.h`][182].

#### `file.c`

The code for `bc`'s implementation of buffered I/O. For more information about
why I implemented my own buffered I/O, see [`include/file.h`][55], [Error
Handling][97], and [Custom I/O][114], along with [`status.h`][176] and the notes
about version [`3.0.0`][32] in the [`NEWS`][32].

The header for this file is [`include/file.h`][55].

#### `history.c`

The code for `bc`'s implementation of command-line editing/history, which is
based on a [UTF-8-aware fork][28] of [`linenoise`][29].

For more information, see the [Command-Line History][189] section.

The header for this file is [`include/history.h`][36].

#### `lang.c`

The data structures used for actual execution of `bc` and `dc` code.

While execution is done in [`src/program.c`][53], this file defines functions
for initializing, copying, and freeing the data structures, which is somewhat
orthogonal to actual execution.

Yes, it's misnamed; that's an accident of history where the first things I put
into it all seemed related to the `bc` language.

The header for this file is [`include/lang.h`][38].

#### `lex.c`

The code for the common things that both programs need for lexing.

The header for this file is [`include/lex.h`][180].

#### `library.c`

The code to implement the public API of the `bcl` library.

The code in this file does a lot to ensure that clients do not have to worry
about internal `bc` details, especially error handling with `setjmp()` and
`longjmp()`. That and encapsulating the handling of numbers are the bulk of what
the code in this file actually does because most of the library is still
implemented in [`src/num.c`][39].

The headers for this file are [`include/bcl.h`][30] and
[`include/library.h`][183].

#### `main.c`

The entry point for both programs; this is the `main()` function.

This file has no headers associated with it.

#### `num.c`

The code for all of the arbitrary-precision [numbers][177] and [math][178] in
`bc`.

The header for this file is [`include/num.h`][184].

#### `opt.c`

The code for parsing command-line options.

The header for this file is [`include/opt.h`][35].

#### `parse.c`

The code for the common items that both programs need for parsing.

The header for this file is [`include/parse.h`][181].

#### `program.c`

The code for the actual execution engine for `bc` and `dc` code.

The header for this file is [`include/program.h`][57].

#### `rand.c`

The code for the [pseudo-random number generator (PRNG)][179] and the special
stack handling it needs.

The PRNG only generates fixed-size integers. The magic of generating random
numbers of arbitrary size is actually given to the code that does math
([`src/num.c`][39]).

The header for this file is [`include/rand.h`][37].

#### `read.c`

The code for reading from files and `stdin`.

The header for this file is [`include/read.h`][185].

#### `vector.c`

The code for [vectors][111], [maps][186], and [slab vectors][187], along with
slabs.

The header for this file is [`include/vector.h`][174].

#### `vm.c`

The code for setting up and running `bc` and `dc`.

It is so named because I think of it as the "virtual machine" of `bc`, though
that is probably not true as [`program.h`][57] is probably the "virtual machine"
code. Thus, the name is more historical accident.

The header for this file is [`include/vm.h`][27].

### `tests/`

This directory contains the entire [test suite][124] and its infrastructure.

#### `all.sh`

A convenience script for the `make run_all_tests` target (see the [Group
Tests][141] section for more information).

#### `all.txt`

The file with the names of the calculators. This is to make it easier for the
test scripts to know where the standard and other test directories are.

#### `bcl.c`

The test for the [`bcl`][156] API. For more information, see the [`bcl`
Test][157] section.

#### `error.sh`

The script to run the file-based error tests in `tests/<calculator>/errors/` for
each calculator. For more information, see the [Error Tests][151] section.

This is a separate script so that each error file can be run separately and in
parallel.

#### `errors.sh`

The script to run the line-based error tests in `tests/<calculator>/errors.txt`
for each calculator. For more information, see the [Error Tests][151] section.

#### `extra_required.txt`

The file with the list of tests which both calculators have that need the [Extra
Math build option][188]. This exists to make it easy for test scripts to skip
those tests when the [Extra Math build option][188] is disabled.

#### `history.py`

The file with all of the history tests. For more information, see the [History
Tests][155] section.

#### `history.sh`

The script to integrate [`history.py`][139] into the build system in a portable
way, and to skip it if necessary.

This script also re-runs the test three times if it fails. This is because
`pexpect` can be flaky at times.

#### `other.sh`

The script to run the "other" (miscellaneous) tests for each calculator. For
more information, see the [Other Tests][154] section.

#### `read.sh`

The script to run the read tests for each calculator. For more information, see
the [`read()` Tests][153] section.

#### `script.sed`

The `sed` script to edit the output of GNU `bc` when generating script tests.
For more information, see the [Script Tests][150] section.

#### `script.sh`

The script for running one script test. For more information, see the [Script
Tests][150] section.

#### `scripts.sh`

The script to help the `make run_all_tests` (see the [Group Tests][141] section)
run all of the script tests.

#### `stdin.sh`

The script to run the `stdin` tests for each calculator. For more information,
see the [`stdin` Tests][152] section.

#### `test.sh`

The script to run one standard test. For more information, see the [Standard
Tests][149] section.

#### `bc/`

The standard tests directory for `bc`. For more information, see the [Standard
Tests][149] section.

##### `all.txt`

The file to tell the build system and `make run_all_tests` (see the [Group
Tests][141] section) what standard tests to run for `bc`, as well as in what
order.

This file just lists the test names, one per line.

##### `errors.txt`

The initial error test file for `bc`. This file has one test per line. See the
[Error Tests][151] section for more information.

##### `posix_errors.txt`

The file of tests for POSIX compatibility for `bc`. This file has one test per
line. For more information, see the [Error Tests][151] section.

##### `timeconst.sh`

The script to run the `bc` tests that use the [Linux `timeconst.bc` script][6].
For more information, see the [Linux `timeconst.bc` Script][191]section.

##### `errors/`

The directory with error tests for `bc`, most discovered by AFL++ (see the
[Fuzzing][82] section). There is one test per file. For more information, see
the [Error Tests][151] section.

##### `scripts/`

The script tests directory for `bc`. For more information, see the [Script
Tests][150] section.

###### `all.txt`

A file to tell the build system and `make run_all_tests` (see the [Group
Tests][141] section) what script tests to run for `bc`, as well as in what
order.

This file just lists the test names, one per line.

#### `dc/`

The standard tests directory for `dc`. For more information, see the [Standard
Tests][149] section.

##### `all.txt`

The file to tell the build system and `make run_all_tests` (see the [Group
Tests][141] section) what standard tests to run for `dc`, as well as in what
order.

This file just lists the test names, one per line.

##### `errors.txt`

The initial error test file for `dc`. This file has one test per line. See the
[Error Tests][151] section for more information.

##### `read_errors.txt`

The file of tests errors with the `?` command (`read()` in `bc`). This file has
one test per line. See the [Error Tests][151] section for more information.

##### `errors/`

The directory with error tests for `dc`, most discovered by AFL++ (see the
[Fuzzing][82] section). There is one test per file. For more information, see
the [Error Tests][151] section.

##### `scripts/`

The script tests directory for `dc`. For more information, see the [Script
Tests][150] section.

###### `all.txt`

The file to tell the build system and `make run_all_tests` (see the [Group
Tests][141] section) what script tests to run for `dc`, as well as in what
order.

This file just lists the test names, one per line.

#### `fuzzing/`

The directory containing the fuzzing infrastructure. For more information, see
the [Fuzzing][82] section.

##### `bc_afl_continue.yaml`

The [`tmuxp`][123] config (for use with [`tmux`][122]) for easily restarting a
fuzz run. For more information, see the [Convenience][130] subsection of the
[Fuzzing][82] section.

##### `bc_afl.yaml`

The [`tmuxp`][123] config (for use with [`tmux`][122]) for easily starting a
fuzz run. For more information, see the [Convenience][130] subsection of the
[Fuzzing][82] section.

Be aware that this will delete all previous unsaved fuzzing tests in the output
directories.

##### `bc_inputs1/`

The fuzzing input directory for the first third of inputs for `bc`. For more
information, see the [Corpuses][192] subsection of the [Fuzzing][82] section.

##### `bc_inputs2/`

The fuzzing input directory for the second third of inputs for `bc`. For more
information, see the [Corpuses][192] subsection of the [Fuzzing][82] section.

##### `bc_inputs3/`

The fuzzing input directory for the third third of inputs for `bc`. For more
information, see the [Corpuses][192] subsection of the [Fuzzing][82] section.

##### `dc_inputs/`

The fuzzing input directory for the inputs for `dc`. For more information, see
the [Corpuses][192] subsection of the [Fuzzing][82] section.

### `vs/`

The directory containing all of the materials needed to build `bc`, `dc`, and
`bcl` on Windows.

#### `bcl.sln`

A Visual Studio solution file for [`bcl`][156]. This, along with
[`bcl.vcxproj`][63] and [`bcl.vcxproj.filters`][64] is what makes it possible to
build [`bcl`][156] on Windows.

#### `bcl.vcxproj`

A Visual Studio project file for [`bcl`][156]. This, along with [`bcl.sln`][65]
and [`bcl.vcxproj.filters`][64] is what makes it possible to build [`bcl`][156]
on Windows.

#### `bcl.vcxproj.filters`

A Visual Studio filters file for [`bcl`][156]. This, along with [`bcl.sln`][65]
and [`bcl.vcxproj`][63] is what makes it possible to build [`bcl`][156] on
Windows.

#### `bc.sln`

A Visual Studio solution file for `bc`. This, along with [`bc.vcxproj`][66]
and [`bc.vcxproj.filters`][67] is what makes it possible to build `bc` on
Windows.

#### `bc.vcxproj`

A Visual Studio project file for `bc`. This, along with [`bc.sln`][68] and
[`bc.vcxproj.filters`][67] is what makes it possible to build `bc` on Windows.

#### `bc.vcxproj.filters`

A Visual Studio filters file for `bc`. This, along with [`bc.sln`][68] and
[`bc.vcxproj`][66] is what makes it possible to build `bc` on Windows.

#### `tests/`

A directory of files to run tests on Windows.

##### `tests_bc.bat`

A file to run basic `bc` tests on Windows. It expects that it will be run from
the directory containing it, and it also expects a `bc.exe` in the same
directory.

##### `tests_dc.bat`

A file to run basic `dc` tests on Windows. It expects that it will be run from
the directory containing it, and it also expects a `bc.exe` in the same
directory.

## Build System

The build system is described in detail in the [build manual][14], so
maintainers should start there. This section, however, describes some parts of
the build system that only maintainers will care about.

### Clean Targets

`bc` has a default `make clean` target that cleans up the build files. However,
because `bc`'s build system can generate many different types of files, there
are other clean targets that may be useful:

* `make clean_gen` cleans the `gen/strgen` executable generated from
  [`gen/strgen.c`][15]. It has no prerequisites.
* `make clean` cleans object files, `*.cat` files (see the [Locales][85]
  section), executables, and files generated from text files in [`gen/`][145],
  including `gen/strgen` if it was built. So this has a prerequisite on
  `make clean_gen` in normal use.
* `make clean_benchmarks` cleans [benchmarks][144], including the `ministat`
  executable. It has no prerequisites.
* `make clean_config` cleans the generated `Makefile` and the manuals that
  [`configure.sh`][69] copied in preparation for install. It also depends on
  `make clean` and `make clean_benchmarks`, so it cleans those items too. This
  is the target that [`configure.sh`][69] uses before it does its work.
* `make clean_coverage` cleans the generated coverage files for the [test
  suite][124]'s [code coverage][146] capabilities. It has no prerequisites. This
  is useful if the code coverage tools are giving errors.
* `make clean_tests` cleans *everything*. It has prerequisites on all previous
  clean targets, but it also cleans all of the [generated tests][143].

When adding more generated files, you may need to add them to one of these
targets and/or add a target for them especially.

### Preprocessor Macros

`bc` and `dc` use *a lot* of preprocessor macros to ensure that each build type:

* builds,
* works under the [test suite][124], and
* excludes as much code as possible from all builds.

This section will explain the preprocessor style of `bc` and `dc`, as well as
provide an explanation of the macros used.

#### Style

The style of macro use in `bc` is pretty straightforward: I avoid depending on
macro definitions and instead, I set defaults if the macro is not defined and
then test the value if the macro with a plain `#if`.

(Some examples of setting defaults are in [`include/status.h`][176], just above
the definition of the `BcStatus` enum.)

In other words, I use `#if` instead of `#ifndef` or `#ifdef`, where possible.

There are a couple of cases where I went with standard stuff instead. For
example, to test whether I am in debug mode or not, I still use the standard
`#ifndef NDEBUG`.

#### Standard Macros

`BC_ENABLED`

:   This macro expands to `1` if `bc` is enabled, `0` if disabled.

`DC_ENABLED`

:   This macro expands to `1` if `dc` is enabled, `0` if disabled.

`BUILD_TYPE`

:   The macro expands to the build type, which is one of: `A`, `E`, `H`, `N`,
    `EH`, `EN`, `HN`, `EHN`. This build type is used in the help text to direct
    the user to the correct markdown manual in the `git.yzena.com` website.

`EXECPREFIX`

:   This macro expands to the prefix on the executable name. This is used to
    allow `bc` and `dc` to skip the prefix when finding out which calculator is
    executing.

`BC_NUM_KARATSUBA_LEN`

:   This macro expands to an integer, which is the length of numbers below which
    the Karatsuba multiplication algorithm switches to brute-force
    multiplication.

`BC_ENABLE_EXTRA_MATH`

:   This macro expands to `1` if the [Extra Math build option][188] is enabled,
    `0` if disabled.

`BC_ENABLE_HISTORY`

:   This macro expands to `1` if the [History build option][193] is enabled, `0`
    if disabled.

`BC_ENABLE_NLS`

:   This macro expands to `1` if the [NLS build option][193] (for locales) is
    enabled, `0` if disabled.

`BC_ENABLE_LIBRARY`

:   This macro expands to `1` if the [`bcl` library][156] is enabled, `0` if
    disabled. If this is enabled, building the calculators themselves is
    disabled, but both `BC_ENABLED` and `DC_ENABLED` must be non-zero.

`BC_ENABLE_MEMCHECK`

:   This macro expands to `1` if `bc` has been built for use with Valgrind's
    [Memcheck][194], `0` otherwise. This ensures that fatal errors still free
    all of their memory when exiting. `bc` does not do that normally because
    what's the point?

`BC_ENABLE_AFL`

:   This macro expands to `1` if `bc` has been built for fuzzing with
    [AFL++][125], `0` otherwise. See the [Fuzzing][82] section for more
    information.

`BC_DEFAULT_BANNER`

:   This macro expands to the default value for displaying the `bc` banner.

`BC_DEFAULT_SIGINT_RESET`

:   The macro expands to the default value for whether or not `bc` should reset
    on `SIGINT` or quit.

`BC_DEFAULT_TTY_MODE`

:   The macro expands to the default value for whether or not `bc` should use
    TTY mode when it available.

`BC_DEFAULT_PROMPT`

:   This macro expands to the default value for whether or not `bc` should use a
    prompt when TTY mode is available.

`DC_DEFAULT_SIGINT_RESET`

:   The macro expands to the default value for whether or not `dc` should reset
    on `SIGINT` or quit.

`DC_DEFAULT_TTY_MODE`

:   The macro expands to the default value for whether or not `dc` should use
    TTY mode when it available.

`DC_DEFAULT_PROMPT`

:   This macro expands to the default value for whether or not `dc` should use a
    prompt when TTY mode is available.

`BC_DEBUG_CODE`

:   If this macro expands to a non-zero integer, then `bc` is built with *a lot*
    of extra debugging code. This is never set by the build system and must be
    set by the programmer manually. This should never be set in builds given to
    end users. For more information, see the [Debugging][134] section.

## Test Suite

While the source code may be the heart and soul of `bc`, the test suite is the
arms and legs: it gives `bc` the power to do anything it needs to do.

The test suite is what allowed `bc` to climb to such high heights of quality.
This even goes for fuzzing because fuzzing depends on the test suite for its
input corpuses. (See the [Fuzzing][82] section.)

Understanding how the test suite works should be, I think, the first thing that
maintainers learn after learning what `bc` and `dc` should do. This is because
the test suite, properly used, gives confidence that changes have not caused
bugs or regressions.

That is why I spent the time to make the test suite as easy to use and as fast
as possible.

To use the test suite (assuming `bc` and/or `dc` are already built), run the
following command:

```
make test
```

That's it. That's all.

It will return an error code if the test suite failed. It will also print out
information about the failure.

If you want the test suite to go fast, then run the following command:

```
make -j<cores> test
```

Where `<cores>` is the number of cores that your computer has. Of course, this
requires a `make` implementation that supports that option, but most do. (And I
will use this convention throughout the rest of this section.)

I have even tried as much as possible, to put longer-running tests near the
beginning of the run so that the entire suite runs as fast as possible.

However, if you want to be sure which test is failing, then running a bare
`make test` is a great way to do that.

But enough about how you have no excuses to use the test suite as much as
possible; let's talk about how it works and what you *can* do with it.

### Standard Tests

The heavy lifting of testing the math in `bc`, as well as basic scripting, is
done by the "standard tests" for each calculator.

These tests use the files in the [`tests/bc/`][161] and [`tests/dc/`][162]
directories (except for [`tests/bc/all.txt`][163], [`tests/bc/errors.txt`][164],
[`tests/bc/posix_errors.txt`][165], [`tests/bc/timeconst.sh`][166],
[`tests/dc/all.txt`][167], [`tests/dc/errors.txt`][168], and
[`tests/dc/read_errors.txt`][175]), which are called the "standard test
directories."

For every test, there is the test file and the results file. The test files have
names of the form `<test>.txt`, where `<test>` is the name of the test, and the
results files have names of the form `<test>_results.txt`.

If the test file exists but the results file does not, the results for that test
are generated by a GNU-compatible `bc` or `dc`. See the [Generated Tests][143]
section.

The `all.txt` file in each standard tests directory is what tells the test suite
and [build system][142] what tests there are, and the tests are either run in
that order, or in the case of parallel `make`, that is the order that the
targets are listed as prerequisites of `make test`.

If the test exists in the `all.txt` file but does not *actually* exist, the test
and its results are generated by a GNU-compatible `bc` or `dc`. See the
[Generated Tests][143] section.

To add a non-generated standard test, do the following:

* Add the test file (`<test>.txt` in the standard tests directory).
* Add the results file (`<test>_results.txt` in the standard tests directory).
  You can skip this step if just the results file needs to be generated. See the
  [Generated Tests][147] section for more information.
* Add the name of the test to the `all.txt` file in the standard tests
  directory, putting it in the order it should be in. If possible, I would put
  longer tests near the beginning because they will start running earlier with
  parallel `make`. I always keep `decimal` first, though, as a smoke test.

If you need to add a generated standard test, see the [Generated Tests][147]
section for how to do that.

Some standard tests need to be skipped in certain cases. That is handled by the
[build system][142]. See the [Integration with the Build System][147] section
for more details.

In addition to all of the above, the standard test directory is not only the
directory for the standard tests of each calculator, it is also the parent
directory of all other test directories for each calculator.

#### `bc` Standard Tests

The list of current (17 July 2021) standard tests for `bc` is below:

decimal

:   Tests decimal parsing and printing.

print

:   Tests printing in every base from decimal. This is near the top for
    performance of parallel testing.

parse

:   Tests parsing in any base and outputting in decimal. This is near the top
    for performance of parallel testing.

lib2

:   Tests the extended math library. This is near the top for performance of
    parallel testing.

print2

:   Tests printing at the extreme values of `obase`.

length

:   Tests the `length()` builtin function.

scale

:   Tests the `scale()` builtin function.

shift

:   Tests the left (`<<`) and right (`>>`) shift operators.

add

:   Tests addition.

subtract

:   Tests subtraction.

multiply

:   Tests multiplication.

divide

:   Tests division.

modulus

:   Tests modulus.

power

:   Tests power (exponentiation).

sqrt

:   Tests the `sqrt()` (square root) builtin function.

trunc

:   Tests the truncation (`$`) operator.

places

:   Tests the places (`@`) operator.

vars

:   Tests some usage of variables. This one came from [AFL++][125] I think.

boolean

:   Tests boolean operators.

comp

:   Tests comparison operators.

abs

:   Tests the `abs()` builtin function.

assignments

:   Tests assignment operators, including increment/decrement operators.

functions

:   Tests functions, specifically function parameters being replaced before they
    themselves are used. See the comment in `bc_program_call()` about the last
    condition.

scientific

:   Tests scientific notation.

engineering

:   Tests engineering notation.

globals

:   Tests that assigning to globals affects callers.

strings

:   Tests strings.

strings2

:   Tests string allocation in slabs, to ensure slabs work.

letters

:   Tests single and double letter numbers to ensure they behave differently.
    Single-letter numbers always be set to the same value, regardless of
    `ibase`.

exponent

:   Tests the `e()` function in the math library.

log

:   Tests the `l()` function in the math library.

pi

:   Tests that `bc` produces the right value of pi for numbers with varying
    `scale` values.

arctangent

:   Tests the `a()` function in the math library.

sine

:   Tests the `s()` function in the math library.

cosine

:   Tests the `c()` function in the math library.

bessel

:   Tests the `j()` function in the math library.

arrays

:   Test arrays.

misc

:   Miscellaneous tests. I named it this because at the time, I struggled to
    classify them, but it's really testing multi-line numbers.

misc1

:   A miscellaneous test found by [AFL++][125].

misc2

:   A miscellaneous test found by [AFL++][125].

misc3

:   A miscellaneous test found by [AFL++][125].

misc4

:   A miscellaneous test found by [AFL++][125].

misc5

:   A miscellaneous test found by [AFL++][125].

misc6

:   A miscellaneous test found by [AFL++][125].

misc7

:   A miscellaneous test found by [AFL++][125].

void

:   Tests void functions.

rand

:   Tests the pseudo-random number generator and its special stack handling.

recursive_arrays

:   Tested the slab vector undo ability in used in `bc_parse_name()` when it
    existed. Now used as a stress test.

divmod

:   Tests divmod.

modexp

:   Tests modular exponentiation.

bitfuncs

:   Tests the bitwise functions, `band()`, `bor()`, `bxor()`, `blshift()` and
    `brshift()` in [`gen/lib2.bc`][26].

leadingzero

:   Tests the leading zero functionality and the `plz*()` and `pnlz*()`
    functions in [`gen/lib2.bc`][26].

#### `dc` Standard Tests

The list of current (17 July 2021) standard tests for `dc` is below:

decimal

:   Tests decimal parsing and printing.

length

:   Tests the `length()` builtin function, including for strings and arrays.

stack_len

:   Tests taking the length of the results stack.

stack_len

:   Tests taking the length of the execution stack.

add

:   Tests addition.

subtract

:   Tests subtraction.

multiply

:   Tests multiplication.

divide

:   Tests division.

modulus

:   Tests modulus.

divmod

:   Tests divmod.

power

:   Tests power (exponentiation).

sqrt

:   Tests the `sqrt()` (square root) builtin function.

modexp

:   Tests modular exponentiation.

boolean

:   Tests boolean operators.

negate

:   Tests negation as a command and as part of numbers.

trunc

:   Tests the truncation (`$`) operator.

places

:   Tests the places (`@`) operator.

shift

:   Tests the left (`<<`) and right (`>>`) shift operators.

abs

:   Tests the `abs()` builtin function.

scientific

:   Tests scientific notation.

engineering

:   Tests engineering notation.

vars

:   Tests some usage of variables. This one came from [AFL++][125] I think.

misc

:   Miscellaneous tests. I named it this because at the time, I struggled to
    classify them.

strings

:   Tests strings.

rand

:   Tests the pseudo-random number generator and its special stack handling.

exec_stack

:   Tests the execution stack depth command.

### Script Tests

The heavy lifting of testing the scripting of `bc` is done by the "script tests"
for each calculator.

These tests use the files in the [`tests/bc/scripts/`][169] and
[`tests/dc/scripts/`][170] directories (except for
[`tests/bc/scripts/all.txt`][171] and [`tests/dc/scripts/all.txt`][172]), which
are called the "script test directories."

To add a script test, do the following:

* Add the test file (`<test>.bc` or `<test>.dc` in the script tests directory).
* Add the results file (`<test>.txt` in the script tests directory). You can
  skip this step if just the results file needs to be generated. See the
  [Generated Tests][147] section for more information.
* Add the name of the test to the `all.txt` file in the script tests directory,
  putting it in the order it should be in. If possible, I would put longer tests
  near the beginning because they will start running earlier with parallel
  `make`.

Some script tests need to be skipped in certain cases. That is handled by the
[build system][142]. See the [Integration with the Build System][147] section
for more details.

Another unique thing about the script tests, at least for `bc`: they test the
`-g` and `--global-stacks` flags. This means that all of the script tests for
`bc` are written assuming the `-g` flag was given on the command-line

There is one extra piece of script tests: [`tests/script.sed`][190]. This `sed`
script is used to remove an incompatibility with GNU `bc`.

If there is only one more character to print at the end of `BC_LINE_LENGTH`, GNU
`bc` still prints a backslash+newline+digit combo. OpenBSD doesn't, which is
correct according to my reading of the `bc` spec, so my `bc` doesn't as well.

The `sed` script edits numbers that end with just one digit on a line by itself
to put it on the same line as others.

#### `bc` Script Tests

The list of current (17 July 2021) script tests for `bc` is below:

print.bc

:   Tests printing even harder than the print standard test.

multiply.bc

:   Tests multiplication even harder than the multiply standard test.

divide.bc

:   Tests division even harder than the divide standard test.

subtract.bc

:   Tests subtraction even harder than the subtract standard test.

add.bc

:   Tests addition even harder than the add standard test.

parse.bc

:   Tests parsing even harder than the parse standard test.

array.bc

:   Tests arrays even harder than the arrays standard test.

atan.bc

:   Tests arctangent even harder than the arctangent standard test.

bessel.bc

:   Tests bessel even harder than the bessel standard test.

functions.bc

:   Tests functions even harder than the functions standard test.

globals.bc

:   Tests global stacks directly.

len.bc

:   Tests the `length()` builtin on arrays.

rand.bc

:   Tests the random number generator in the presence of global stacks.

references.bc

:   Tests functions with array reference parameters.

screen.bc

:   A random script provided by an early user that he used to calculate the size
    of computer screens

strings2.bc

:   Tests escaping in strings.

ifs.bc

:   Tests proper ending of `if` statements without `else` statements.

ifs2.bc

:   More tests proper ending of `if` statements without `else` statements.

#### `dc` Script Tests

The list of current (17 July 2021) script tests for `dc` is below:

prime.dc

:   Tests scripting by generating the first 100,000 primes.

asciify.dc

:   Tests the asciify command.

stream.dc

:   Tests the stream command.

array.dc

:   Tests arrays.

else.dc

:   Tests else clauses on conditional execution commands.

factorial.dc

:   Tests scripting with factorial.

loop.dc

:   Tests scripting by implementing loops.

quit.dc

:   Tests the quit command in the presence of tail calls.

weird.dc

:   A miscellaneous test.

### Error Tests

One of the most useful parts of the `bc` test suite, in my opinion, is the heavy
testing of error conditions.

Just about every error condition I can think of is tested, along with many
machine-generated (by [AFL++][125]) ones.

However, because the error tests will often return error codes, they require
different infrastructure from the rest of the test suite, which assumes that
the calculator under test will return successfully. A lot of that infrastructure
is in the [`scripts/functions.sh`][105] script, but it basically allows the
calculator to exit with an error code and then tests that there *was* an error
code.

Besides returning error codes, error tests also ensure that there is output from
`stderr`. This is to make sure that an error message is always printed.

The error tests for each calculator are spread through two directories, due to
historical accident. These two directories are the standard test directory (see
the [Standard Tests][149] section) and the `errors/` directory directly
underneath the standard tests directory.

This split is convenient, however, because the tests in each directory are
treated differently.

The error tests in the standard test directory, which include `errors.txt` for
both calculators, `posix_errors.txt` for `bc`, and `read_errors.txt` for `dc`,
are run by [`tests/errors.sh`][226]. It reads them line-by-line and shoves the
data through `stdin`. Each line is considered a separate test. For this reason,
there can't be any blank lines in the error files in the standard tests
directory because a blank line causes a successful exit.

On the other hand, the tests in the `errors/` directory below the standard tests
directory are run by [`tests/error.sh`][227] and are considered to be one test
per file. As such, they are used differently. They are shoved into the
calculator through `stdin`, but they are also executed by passing them on the
command-line.

To add an error test, first figure out which kind you want.

Is it a simple one-liner, and you don't care if it's tested through a file?

Then put it in one of the error files in the standard test directory. I would
only put POSIX errors in the `posix_errors.txt` file for `bc`, and only `read()`
errors in the `read_errors.txt` file for `dc`; all others I would put in the
respective `errors.txt` file.

On the other hand, if you care if the error is run as a file on the
command-line, or the error requires multiple lines to reproduce, then put the
test in the respective `errors/` directory and run the [`configure.sh`][69]
script again.

After that, you are done; the test suite will automatically pick up the new
test, and you don't have to tell the test suite the expected results.

### `stdin` Tests

The `stdin` tests specifically test the lexing and parsing of multi-line
comments and strings. This is important because when reading from `stdin`, the
calculators can only read one line at a time, so partial parses are possible.

To add `stdin` tests, just add the tests to the `stdin.txt` file in the
respective standard tests directory, and add the expected results in the
`stdin_results.txt` in the respective standard tests directory.

### `read()` Tests

The `read()` tests are meant to test the `read()` builtin function, to ensure
that the parsing and execution is correct.

Each line is one test, as that is the nature of using the `read()` function, so
to add a test, just add it as another line in the `read.txt` file in the
respective standard tests directory, and add its result to the
`read_results.txt` file in the respective standard tests directory.

### Other Tests

The "other" tests are just random tests that I could not easily classify under
other types of tests. They usually include things like command-line parsing and
environment variable testing.

To add an other test, it requires adding the programming for it to
[`tests/other.sh`][195] because all of the tests are written specifically in
that script. It would be best to use the infrastructure in
[`scripts/functions.sh`][105].

### Linux `timeconst.bc` Script

One special script that `bc`'s test suite will use is the [Linux `timeconst.bc`
script][6].

I made the test suite able to use this script because the reason the
[toybox][16] maintainer wanted my `bc` is because of this script, and I wanted
to be sure that it would run correctly on the script.

However, it is not part of the distribution, nor is it part of the repository.
The reason for this is because [`timeconst.bc`][6] is under the GPL, while this
repo is under a BSD license.

If you want `bc` to run tests on [`timeconst.bc`][6], download it and place it
at `tests/bc/scripts/timeconst.bc`. If it is there, the test suite will
automatically run its tests; otherwise, it will skip it.

### History Tests

There are automatic tests for history; however, they have dependencies: Python 3
and [`pexpect`][137].

As a result, because I need the [test suite to be portable][138], like the rest
of `bc`, the history tests are carefully guarded with things to ensure that they
are skipped, rather than failing if Python and [`pexpect`][137] are not
installed. For this reason, there is a `sh` script, [`tests/history.sh`][140]
that runs the actual script, [`tests/history.py`][139].

I have added as many tests as I could to cover as many lines and branches as
possible. I guess I could have done more, but doing so would have required a lot
of time.

I have tried to make it as easy as possible to run the history tests. They will
run automatically if you use the `make test_history` command, and they will also
use parallel execution with `make -j<cores> test_history`.

However, the history tests are meant only to be run by maintainers of `bc`; they
are *not* meant to be run by users and packagers. The reason for this is that
they only seem to work reliably on Linux; `pexpect` seems to have issues on
other platforms, especially timeout issues.

Thus, they are excluded from running with `make test` and [`tests/all.sh`][225].
However, they can be run from the [`scripts/release.sh`][83] script.

All of the tests are contained in [`tests/history.py`][139]. The reason for this
is because they are in Python, and I don't have an easy way of including Python
(or at the very least, I am not familiar enough with Python to do that). So they
are all in the same file to make it easier on me.

Each test is one function in the script. They all take the same number and type
of arguments:

1.	`exe`: the executable to run.
2.	`args`: the arguments to pass to the executable.
3.	`env`: the environment.

Each function creates a child process with `pexpect.spawn` and then tests with
that child. Then the function returns the child to the caller, who closes it
and checks its error code against its expected error code.

Yes, the error code is not a success all the time. This is because of the UTF-8
tests; `bc` gives a fatal error on any non-ASCII data because ASCII is all `bc`
is required to handle, per the [standard][2].

So in [`tests/history.py`][139], there are four main arrays:

* `bc` test functions,
* `bc` expected error codes.
* `dc` test functions.
* `dc` expected error codes.

[`tests/history.py`][139] takes an index as an argument; that index is what test
it should run. That index is used to index into the proper test and error code
array.

If you need to add more history tests, you need to do the following:

1.	Add the function for that test to [`tests/history.py`][139].
2.	Add the function to the proper array of tests.
3.	Add the expected error code to the proper array of error codes.
4.	Add a target for the test to [`Makefile.in`][70].
5.	Add that target as a prerequisite to either `test_bc_history` or
	`test_dc_history`.

You do not need to do anything to add the test to `history_all_tests` (see
[Group Tests][141] below) because the scripts will automatically run all of the
tests properly.

### Generated Tests

Some tests are *large*, and as such, it is impractical to check them into `git`.
Instead, the tests depend on the existence of a GNU-compatible `bc` in the
`PATH`, which is then used to generate the tests.

If [`configure.sh`][69] was run with the `-G` argument, which disables generated
tests, then `make test` and friends will automatically skip generated tests.
This is useful to do on platforms that are not guaranteed to have a
GNU-compatible `bc` installed.

However, adding a generated test is a complicated because you have to figure out
*where* you want to put the file to generate the test.

For example, `bc`'s test suite will automatically use a GNU-compatible `bc` to
generate a `<test>_results.txt` file in the [standard tests][149] directory
(either `tests/bc/` or `tests/dc/`) if none exists for the `<test>` test. If no
`<test>.txt` file exists in the [standard tests][149] directory, then `bc`'s
test suite will look for a `<test>.bc` or `<test>.dc` file in the [script
tests][150] directory (either `tests/bc/scripts` or `tests/dc/scripts`), and if
that exists, it will use that script to generate the `<test>.txt` file in the
[standard tests][149] directory after which it will generate the
`<test>_results.txt` file in the [standard tests][149] directory.

So you can choose to either:

* Have a test in the [standard tests][149] directory without a corresponding
  `*_results.txt` file, or
* Have a script in the [script tests][150] directory to generate the
  corresponding file in the standard test directory before generating the
  corresponding `*_results.txt` file.

Adding a script has a double benefit: the script itself can be used as a test.
However, script test results can also be generated.

If `bc` is asked to run a script test, then if the script does not exist, `bc`'s
test suite returns an error. If it *does* exist, but no corresponding
`<test>.txt` file exists in the [script tests][150] directory, then a
GNU-compatible `bc` is used to generate the `<test>.txt` results file.

If generated tests are disabled through [`configure.sh`][69], then these tests
are not generated if they do not exist. However, if they *do* exist, then they
are run. This can happen if a `make clean_tests` was not run between a build
that generated tests and a build that will not.

### Group Tests

While the test suite has a lot of targets in order to get parallel execution,
there are five targets that allow you to run each section, or all, of the test
suite as one unit:

* `bc_all_tests` (`bc` tests)
* `timeconst_all_tests` ([Linux `timeconst.bc` script][6] tests)
* `dc_all_tests` (`dc` tests)
* `history_all_tests` (history tests)
* `run_all_tests` (combination of the previous four)

In addition, there are more fine-grained targets available:

* `test_bc` runs all `bc` tests (except history tests).
* `test_dc` runs all `dc` tests (except history tests).
* `test_bc_tests` runs all `bc` [standard tests][149].
* `test_dc_tests` runs all `dc` [standard tests][149].
* `test_bc_scripts` runs all `bc` [script tests][150].
* `test_dc_scripts` runs all `dc` [script tests][150].
* `test_bc_stdin` runs the `bc` [`stdin` tests][152].
* `test_dc_stdin` runs the `dc` [`stdin` tests][152].
* `test_bc_read` runs the `bc` [`read()` tests][153].
* `test_dc_read` runs the `dc` [`read()` tests][153].
* `test_bc_errors` runs the `bc` [error tests][151].
* `test_dc_errors` runs the `dc` [error tests][151].
* `test_bc_other` runs the `bc` [other tests][151].
* `test_dc_other` runs the `dc` [other tests][151].
* `timeconst` runs the tests for the [Linux `timeconst.bc` script][6].
* `test_history` runs all history tests.
* `test_bc_history` runs all `bc` history tests.
* `test_dc_history` runs all `dc` history tests.

All of the above tests are parallelizable.

### Individual Tests

In addition to all of the above, individual test targets are available. These
are mostly useful for attempting to fix a singular test failure.

These tests are:

* `test_bc_<test>`, where `<test>` is the name of a `bc` [standard test][149].
  The name is the name of the test file without the `.txt` extension. It is the
  name printed by the test suite when running the test.
* `test_dc_<test>`, where `<test>` is the name of a `dc` [standard test][149].
  The name is the name of the test file without the `.txt` extension. It is the
  name printed by the test suite when running the test.
* `test_bc_script_<test>`, where `<test>` is the name of a `bc` [script
  test][150]. The name of the test is the name of the script without the `.bc`
  extension.
* `test_dc_script_<test>`, where `<test>` is the name of a `dc` [script
  test][150]. The name of the test is the name of the script without the `.dc`
  extension.
* `test_bc_history<idx>` runs the `bc` history test with index `<idx>`.
* `test_dc_history<idx>` runs the `dc` history test with index `<idx>`.

### [`bcl`][156] Test

When [`bcl`][156] is built, the [build system][142] automatically ensures that
`make test` runs the [`bcl`][156] test instead of the `bc` and `dc` tests.

There is only one test, and it is built from [`tests/bcl.c`][158].

The reason the test is in C is because [`bcl`][156] is a C library; I did not
want to have to write C code *and* POSIX `sh` scripts to run it.

The reason there is only one test is because most of the code for the library is
tested by virtue of testing `bc` and `dc`; the test needs to only ensure that
the library bindings and plumbing do not interfere with the underlying code.

However, just because there is only one test does not mean that it doesn't test
more than one thing. The code actually handles a series of tests, along with
error checking to ensure that nothing went wrong.

To add a [`bcl`][156] test, just figure out what test you want, figure out where
in the [`tests/bcl.c`][158] would be best to put it, and put it there. Do as
much error checking as possible, and use the `err(BclError)` function. Ensure
that all memory is freed because that test is run through [Valgrind][159] and
[AddressSanitizer][160].

### Integration with the Build System

If it was not obvious by now, the test suite is heavily integrated into the
[build system][142], but the integration goes further than just making the test
suite easy to run from `make` and generating individual and group tests.

The big problem the test suite has is that some `bc` code, stuff that is
important to test, is only in *some* builds. This includes all of the extra math
extensions, for example.

So the test suite needs to have some way of turning off the tests that depend on
certain [build types][81] when those [build types][81] are not used.

This is the reason the is tightly integrated with the [build system][142]: the
[build system][142] knows what [build type][81] was used and can tell the test
suite to turn off the tests that do not apply.

It does this with arguments to the test scripts that are either a `1` or a `0`,
depending on whether tests of that type should be enabled or not. These
arguments are why I suggest, in the [Test Scripts][148] section, to always use a
`make` target to run the test suite or any individual test. I have added a lot
of targets to make this easy and as fast as possible.

In addition to all of that, the build system is responsible for selecting the
`bc`/`dc` tests or the [`bcl` test][157].

### Output Directories

During any run of the test suite, the test suite outputs the results of running
various tests to files. These files are usually output to `tests/bc_outputs/`
and `tests/dc_outputs/`.

However, in some cases, it may be necessary to output test results to a
different directory. If that is the case, set the environment variable
`BC_TEST_OUTPUT_DIR` to the name of the directory.

If that is done, then test results will be written to
`$BC_TEST_OUTPUT_DIR/bc_outputs/` and `$BC_TEST_OUTPUT_DIR/dc_outputs/`.

### Test Suite Portability

The test suite is meant to be run by users and packagers as part of their
install process.

This puts some constraints on the test suite, but the biggest is that the test
suite must be as [portable as `bc` itself][136].

This means that the test suite must be implemented in pure POSIX `make`, `sh`,
and C99.

#### Test Scripts

To accomplish the portability, the test suite is run by a bunch of `sh` scripts
that have the constraints laid out in [POSIX Shell Scripts][76].

However, that means they have some quirks, made worse by the fact that there are
[generated tests][143] and [tests that need to be skipped, but only
sometimes][147].

This means that a lot of the scripts take an awkward number and type of
arguments. Some arguments are strings, but most are integers, like
[`scripts/release.sh`][83].

It is for this reason that I do not suggest running the test scripts directly.
Instead, always use an appropriate `make` target, which already knows the
correct arguments for the test because of the [integration with the build
system][147].

### Test Coverage

In order to get test coverage information, you need `gcc`, `gcov`, and `gcovr`.

If you have them, run the following commands:

```
CC=gcc ./configure -gO3 -c
make -j<cores>
make coverage
```

Note that `make coverage` does not have a `-j<cores>` part; it cannot be run in
parallel. If you try, you will get errors. And note that `CC=gcc` is used.

After running those commands, you can open your web browser and open the
`index.html` file in the root directory of the repo. From there, you can explore
all of the coverage results.

If you see lines or branches that you think you could hit with a manual
execution, do such manual execution, and then run the following command:

```
make coverage_output
```

and the coverage output will be updated.

If you want to rerun `make coverage`, you must do a `make clean` and build
first, like this:

```
make clean
make -j<cores>
make coverage
```

Otherwise, you will get errors.

If you want to run tests in parallel, you can do this:

```
make -j<cores>
make -j<cores> test
make coverage_output
```

and that will generate coverage output correctly.

### [AddressSanitizer][21] and Friends

To run the test suite under [AddressSanitizer][21] or any of its friends, use
the following commands:

```
CFLAGS="-fsanitize=<sanitizer> ./configure -gO3 -m
make -j<cores>
make -j<cores> test
```

where `<sanitizer>` is the correct name of the desired sanitizer. There is one
exception to the above: `UndefinedBehaviorSanitizer` should be run on a build
that has zero optimization, so for `UBSan`, use the following commands:

```
CFLAGS="-fsanitize=undefined" ./configure -gO0 -m
make -j<cores>
make -j<cores> test
```

### [Valgrind][20]

To run the test suite under [Valgrind][20], run the following commands:

```
./configure -gO3 -v
make -j<cores>
make -j<cores> test
```

It really is that easy. I have directly added infrastructure to the build system
and the test suite to ensure that if [Valgrind][20] detects any memory errors or
any memory leaks at all, it will tell the test suite infrastructure to report an
error and exit accordingly.

## POSIX Shell Scripts

There is a lot of shell scripts in this repository, and every single one of them
is written in pure [POSIX `sh`][72].

The reason that they are written in [POSIX `sh`][72] is for *portability*: POSIX
systems are only guaranteed to have a barebones implementation of `sh`
available.

There are *many* snares for unwary programmers attempting to modify
[`configure.sh`][69], any of the scripts in this directory, [`strgen.sh`][9], or
any of the scripts in [`tests/`][77]. Here are some of them:

1.	No `bash`-isms.
2.	Only POSIX standard utilities are allowed.
3.	Only command-line options defined in the POSIX standard for POSIX utilities
	are allowed.
4.	Only the standardized behavior of POSIX utilities is allowed.
5.	Functions return data by *printing* it. Using `return` sets their exit code.

In other words, the script must only use what is standardized in the [`sh`][72]
and [Shell Command Language][73] standards in POSIX. This is *hard*. It precludes
things like `local` and the `[[ ]]` notation.

These are *enormous* restrictions and must be tested properly. I put out at
least one release with a change to `configure.sh` that wasn't portable. That was
an embarrassing mistake.

The lack of `local`, by the way, is why variables in functions are named with
the form:

```
_<function_name>_<var_name>
```

This is done to prevent any clashes of variable names with already existing
names. And this applies to *all* shell scripts. However, there are a few times
when that naming convention is *not* used; all of them are because those
functions are required to change variables in the global scope.

### Maintainer-Only Scripts

If a script is meant to be used for maintainers (of `bc`, not package
maintainers), then rules 2, 3, and 4 don't need to be followed as much because
it is assumed that maintainers will be able to install whatever tools are
necessary to do the job.

## Manuals

The manuals for `bc` and `dc` are all generated, and the manpages for `bc`,
`dc`, and `bcl` are also generated.

Why?

I don't like the format of manpages, and I am not confident in my ability to
write them. Also, they are not easy to read on the web.

So that explains why `bcl`'s manpage is generated from its markdown version. But
why are the markdown versions of the `bc` and `dc` generated?

Because the content of the manuals needs to change based on the [build
type][81]. For example, if `bc` was built with no history support, it should not
have the **COMMAND LINE HISTORY** section in its manual. If it did, that would
just confuse users.

So the markdown manuals for `bc` and `dc` are generated from templates
([`manuals/bc.1.md.in`][89] and [`manuals/dc.1.md.in`][90]). And from there,
the manpages are generated from the generated manuals.

The generated manpage for `bcl` ([`manuals/bcl.3`][62]) is checked into version
control, and the generated markdown manuals and manpages for `bc`
([`manuals/bc`][79]) and `dc` ([`manuals/dc`][80]) are as well.

This is because generating the manuals and manpages requires a heavy dependency
that only maintainers should care about: [Pandoc][92]. Because users [should not
have to install *any* dependencies][136], the files are generated, checked into
version control, and included in distribution tarballs.

If you run [`configure.sh`][69], you have an easy way of generating the markdown
manuals and manpages: just run `make manpages`. This target calls
[`scripts/manpage.sh`][60] appropriately for `bc`, `dc`, and `bcl`.

For more on how generating manuals and manpages works, see
[`scripts/manpage.sh`][60].

## Locales

The locale system of `bc` is enormously complex, but that's because
POSIX-compatible locales are terrible.

How are they terrible?

First, `gencat` does not work for generating cross-compilation. In other words,
it does not generate machine-portable files. There's nothing I can do about
this except for warn users.

Second, the format of `.msg` files is...interesting. Thank goodness it is text
because otherwise, it would be impossible to get them right.

Third, `.msg` files are not used. In other words, `gencat` exists. Why?

Fourth, `$NLSPATH` is an awful way to set where and *how* to install locales.

Yes, where and *how*.

Obviously, from it's name, it's a path, and that's the where. The *how* is more
complicated.

It's actually *not* a path, but a path template. It's a format string, and it
can have a few format specifiers. For more information on that, see [this
link][84]. But in essence, those format specifiers configure how each locale is
supposed to be installed.

With all those problems, why use POSIX locales? Portability, as always. I can't
assume that `gettext` will be available, but I *can* pretty well assume that
POSIX locales will be available.

The locale system of `bc` includes all files under [`locales/`][85],
[`scripts/locale_install.sh`][87], [`scripts/locale_uninstall.sh`][88],
[`scripts/functions.sh`][105], the `bc_err_*` constants in [`src/data.c`][131],
and the parts of the build system needed to activate it. There is also code in
[`src/vm.c`][58] (in `bc_vm_gettext()`) for loading the current locale.

If the order of error messages and/or categories are changed, the order of
errors must be changed in the enum, the default error messages and categories in
[`src/data.c`][131], and all of the messages and categories in the `.msg` files
under [`locales/`][85].

## Static Analysis

I do *some* static analysis on `bc`.

I used to use [Coverity][196], but I stopped using it when it started giving me
too many false positives and also because it had a vulnerability.

However, I still use the [Clang Static Analyzer][197] through
[`scan-build`][19]. I only use it in debug mode because I have to add some
special code to make it not complain about things that are definitely not a
problem.

The most frequent example of false positives is where a local is passed to a
function to be initialized. [`scan-build`][19] misses that fact, so I
pre-initialize such locals to prevent the warnings.

To run `scan-build`, do the following:

```
make clean
scan-build make
```

`scan-build` will print its warnings to `stdout`.

## Fuzzing

The quality of this `bc` is directly related to the amount of fuzzing I did. As
such, I spent a lot of work making the fuzzing convenient and fast, though I do
admit that it took me a long time to admit that it did need to be faster.

First, there were several things which make fuzzing fast:

* Using [AFL++][125]'s deferred initialization.
* Splitting `bc`'s corpuses.
* Parallel fuzzing.

Second, there are several things which make fuzzing convenient:

* Preprepared input corpuses.
* [`scripts/fuzz_prep.sh`][119].
* `tmux` and `tmuxp` configs.
* [`scripts/afl.py`][94].

### Fuzzing Performance

Fuzzing with [AFL++][125] can be ***SLOW***. Spending the time to make it as
fast as possible is well worth the time.

However, there is a caveat to the above: it is easy to make [AFL++][125] crash,
be unstable, or be unable to find "paths" (see [AFL++ Quickstart][129]) if the
performance enhancements are done poorly.

To stop [AFL++][125] from crashing on test cases, and to be stable, these are
the requirements:

* The state at startup must be *exactly* the same.
* The virtual memory setup at startup must be *exactly* the same.

The first isn't too hard; it's the second that is difficult.

`bc` allocates a lot of memory at start. ("A lot" is relative; it's far less
than most programs.) After going through an execution run, however, some of that
memory, while it could be cleared and reset, is in different places because of
vectors. Since vectors reallocate, their allocations are not guaranteed to be in
the same place.

So to make all three work, I had to set up the deferred initialization and
persistent mode *before* any memory was allocated (except for `vm.jmp_bufs`,
which is probably what caused the stability to drop below 100%). However, using
deferred alone let me put the [AFL++][125] initialization further back. This
works because [AFL++][125] sets up a `fork()` server that `fork()`'s `bc` right
at that call. Thus, every run has the exact same virtual memory setup, and each
run can skip all of the setup code.

I tested `bc` using [AFL++][125]'s deferred initialization, plus persistent
mode, plus shared memory fuzzing. In order to do it safely, with stability above
99%, all of that was actually *slower* than using just deferred initialization
with the initialization *right before* `stdin` was read. And as a bonus, the
stability in that situation is 100%.

As a result, my [AFL++][125] setup only uses deferred initialization. That's the
`__AFL_INIT()` call.

(Note: there is one more big item that must be done in order to have 100%
stability: the pseudo-random number generator *must* start with *exactly* the
same seed for every run. This is set up with the `tmux` and `tmuxp` configs that
I talk about below in [Convenience][130]. This seed is set before the
`__AFL_INIT()` call, so setting it has no runtime cost for each run, but without
it, stability would be abysmal.)

On top of that, while `dc` is plenty fast under fuzzing (because of a faster
parser and less test cases), `bc` can be slow. So I have split the `bc` input
corpus into three parts, and I set fuzzers to run on each individually. This
means that they will duplicate work, but they will also find more stuff.

On top of all of that, each input corpus (the three `bc` corpuses and the one
`dc` corpus) is set to run with 4 fuzzers. That works out perfectly for two
reasons: first, my machine has 16 cores, and second, the [AFL++][125] docs
recommend 4 parallel fuzzers, at least, to run different "power schedules."

### Convenience

The preprepared input corpuses are contained in the
`tests/fuzzing/bc_inputs{1,2,3}/`, and `tests/fuzzing/dc_inputs` directories.
There are three `bc` directories and only one `dc` directory because `bc`'s
input corpuses are about three times as large, and `bc` is a larger program;
it's going to need much more fuzzing.

(They do share code though, so fuzzing all of them still tests a lot of the same
math code.)

The next feature of convenience is the [`scripts/fuzz_prep.sh`][119] script. It
assumes the existence of `afl-clang-lto` in the `$PATH`, but if that exists, it
automatically configures and builds `bc` with a fuzz-ideal build.

A fuzz-ideal build has several things:

* `afl-clang-lto` as the compiler. (See [AFL++ Quickstart][129].)
* Debug mode, to crash as easily as possible.
* Full optimization (including [Link-Time Optimization][126]), for performance.
* [AFL++][125]'s deferred initialization (see [Fuzzing Performance][127] above).
* And `AFL_HARDEN=1` during the build to harden the build. See the [AFL++][125]
  documentation for more information.

There is one big thing that a fuzz-ideal build does *not* have: it does not use
[AFL++][125]'s `libdislocator.so`. This is because `libdislocator.so` crashes if
it fails to allocate memory. I do not want to consider those as crashes because
my `bc` does, in fact, handle them gracefully by exiting with a set error code.
So `libdislocator.so` is not an option.

However, to add to [`scripts/fuzz_prep.sh`][119] making a fuzz-ideal build, in
`tests/fuzzing/`, there are two `yaml` files: [`tests/fuzzing/bc_afl.yaml`][120]
and [`tests/fuzzing/bc_afl_continue.yaml`][121]. These files are meant to be
used with [`tmux`][122] and [`tmuxp`][123]. While other programmers will have to
adjust the `start_directory` item, once it is adjusted, then using this command:

```
tmuxp load tests/fuzzing/bc_afl.yaml
```

will start fuzzing.

In other words, to start fuzzing, the sequence is:

```
./scripts/fuzz_prep.sh
tmuxp load tests/fuzzing/bc_afl.yaml
```

Doing that will load, in `tmux`, 16 separate instances of [AFL++][125], 12 on
`bc` and 4 on `dc`. The outputs will be put into the
`tests/fuzzing/bc_outputs{1,2,3}/` and `tests/fuzzing/dc_outputs/` directories.

(Note that loading that config will also delete all unsaved [AFL++][125] output
from the output directories.)

Sometimes, [AFL++][125] will report crashes when there are none. When crashes
are reported, I always run the following command:

```
./scripts/afl.py <dir>
```

where `dir` is one of `bc1`, `bc2`, `bc3`, or `dc`, depending on which of the
16 instances reported the crash. If it was one of the first four (`bc11` through
`bc14`), I use `bc1`. If it was one of the second four (`bc21` through `bc24`, I
use `bc2`. If it was one of the third four (`bc31` through `bc34`, I use `bc3`.
And if it was `dc`, I use `dc`.

The [`scripts/afl.py`][94] script will report whether [AFL++][125] correctly
reported a crash or not. If so, it will copy the crashing test case to
`.test.txt` and tell you whether it was from running it as a file or through
`stdin`.

From there, I personally always investigate the crash and fix it. Then, when the
crash is fixed, I either move `.test.txt` to `tests/{bc,dc}/errors/<idx>.txt` as
an error test (if it produces an error) or I create a new
`tests/{bc,dc}/misc<idx>.txt` test for it and a corresponding results file. (See
[Test Suite][124] for more information about the test suite.) In either case,
`<idx>` is the next number for a file in that particular place. For example, if
the last file in `tests/{bc,dc}/errors/` is `tests/{bc,dc}/errors/18.txt`, I
move `.test.txt` to `tests/bc/error/19.txt`.

Then I immediately run [`scripts/afl.py`][94] again to find the next crash
because often, [AFL++][125] found multiple test cases that trigger the same
crash. If it finds another, I repeat the process until it is happy.

Once it *is* happy, I do the same `fuzz_prep.sh`, `tmuxp load` sequence and
restart fuzzing. Why do I restart instead of continuing? Because with the
changes, the test outputs could be stale and invalid.

However, there *is* a case where I continue: if [`scripts/afl.py`][94] finds
that every crash reported by [AFL++][125] is invalid. If that's the case, I can
just continue with the command:

```
tmuxp load tests/fuzzing/bc_afl_continue.yaml
```

(Note: I admit that I usually run [`scripts/afl.py`][94] while the fuzzer is
still running, so often, I don't find a need to continue since there was no
stop. However, the capability is there, if needed.)

In addition, my fuzzing setup, including the `tmux` and `tmuxp` configs,
automatically set up [AFL++][125] power schedules (see [Fuzzing
Performance][127] above). They also set up the parallel fuzzing such that there
is one fuzzer in each group of 4 that does deterministic fuzzing. It's always
the first one in each group.

For more information about deterministic fuzzing, see the [AFL++][125]
documentation.

### Corpuses

I occasionally add to the input corpuses. These files come from new files in the
[Test Suite][124]. In fact, I use soft links when the files are the same.

However, when I add new files to an input corpus, I sometimes reduce the size of
the file by removing some redundancies.

And then, when adding to the `bc` corpuses, I try to add them evenly so that
each corpus will take about the same amount of time to get to a finished state.

### [AFL++][125] Quickstart

The way [AFL++][125] works is complicated.

First, it is the one to invoke the compiler. It leverages the compiler to add
code to the binary to help it know when certain branches are taken.

Then, when fuzzing, it uses that branch information to generate information
about the "path" that was taken through the binary.

I don't know what AFL++ counts as a new path, but each new path is added to an
output corpus, and it is later used as a springboard to find new paths.

This is what makes AFL++ so effective: it's not just blindly thrashing a binary;
it adapts to the binary by leveraging information about paths.

### Fuzzing Runs

For doing a fuzzing run, I expect about a week or two where my computer is
basically unusable, except for text editing and light web browsing.

Yes, it can take two weeks for me to do a full fuzzing run, and that does *not*
include the time needed to find and fix crashes; it only counts the time on the
*last* run, the one that does not find any crashes. This means that the entire
process can take a month or more.

What I use as an indicator that the fuzzing run is good enough is when the
number of "Pending" paths (see [AFL++ Quickstart][129] above) for all fuzzer
instances, except maybe the deterministic instances, is below 50. And even then,
I try to let deterministic instances get that far as well.

You can see how many pending paths are left in the "path geometry" section of
the [AFL++][125] dashboard.

Also, to make [AFL++][125] quit, you need to send it a `SIGINT`, either with
`Ctrl+c` or some other method. It will not quit until you tell it to.

### Radamsa

I rarely use [Radamsa][99] instead of [AFL++][125]. In fact, it's only happened
once.

The reason I use [Radamsa][99] instead of [AFL++][125] is because it is easier
to use with varying command-line arguments, which was needed for testing `bc`'s
command-line expression parsing code, and [AFL++][125] is best when testing
input from `stdin`.

[`scripts/radamsa.sh`][100] does also do fuzzing on the [AFL++][125] inputs, but
it's not as effective at that, so I don't really use it for that either.

[`scripts/radamsa.sh`][100] and [Radamsa][99] were only really used once; I have
not had to touch the command-line expression parsing code since.

### [AddressSanitizer][21] with Fuzzing

One advantage of using [AFL++][125] is that it saves every test case that
generated a new path (see [AFL++ Quickstart][129] above), and it doesn't delete
them when the user makes it quit.

Keeping them around is not a good idea, for several reasons:

* They are frequently large.
* There are a lot of them.
* They go stale; after `bc` is changed, the generated paths may not be valid
  anymore.

However, before they are deleted, they can definitely be leveraged for even
*more* bug squashing by running *all* of the paths through a build of `bc` with
[AddressSanitizer][21].

This can easily be done with these four commands:

```
./scripts/fuzz_prep.sh -a
./scripts/afl.py --asan bc1
./scripts/afl.py --asan bc2
./scripts/afl.py --asan bc3
./scripts/afl.py --asan dc
```

(By the way, the last four commands could be run in separate terminals to do the
processing in parallel.)

These commands build an [ASan][21]-enabled build of `bc` and `dc` and then they
run `bc` and `dc` on all of the found crashes and path output corpuses. This is
to check that no path or crash has found any memory errors, including memory
leaks.

Because the output corpuses can contain test cases that generate infinite loops
in `bc` or `dc`, [`scripts/afl.py`][94] has a timeout of 8 seconds, which is far
greater than the timeout that [AFL++][125] uses and should be enough to catch
any crash.

If [AFL++][125] fails to find crashes *and* [ASan][21] fails to find memory
errors on the outputs of [AFL++][125], that is an excellent indicator of very
few bugs in `bc`, and a release can be made with confidence.

## Code Concepts

This section is about concepts that, if understood, will make it easier to
understand the code as it is written.

The concepts in this section are not found in a single source file, but they are
littered throughout the code. That's why I am writing them all down in a single
place.

### POSIX Mode

POSIX mode is `bc`-only.

In fact, POSIX mode is two different modes: Standard Mode and Warning Mode.
These modes are designed to help users write POSIX-compatible `bc` scripts.

#### Standard Mode

Standard Mode is activated with the `-s` or `--standard` flags.

In this mode, `bc` will error if any constructs are used that are not strictly
compatible with the [POSIX `bc` specification][2].

#### Warning Mode

Warning Mode is activated with the `-w` or `--warn` flags.

In this mode, `bc` will issue warnings, but continue, if any constructs are used
that are not strictly compatible with the [POSIX `bc` specification][2].

### Memory Management

The memory management in `bc` is simple: everything is owned by one thing.

If something is in a vector, it is owned by that vector.

If something is contained in a struct, it is owned by that struct with one
exception: structs can be given pointers to other things, but only if those
other things will outlast the struct itself.

As an example, the `BcParse` struct has a pointer to the one `BcProgram` in
`bc`. This is okay because the program is initialized first and deallocated
last.

In other words, it's simple: if a field in a struct is a pointer, then unless
that pointer is directly allocated by the struct (like the vector array or the
number limb array), that struct does not own the item at that pointer.
Otherwise, the struct *does* own the item.

### [Async-Signal-Safe][115] Signal Handling

`bc` is not the typical Unix utility. Most Unix utilities are I/O bound, but
`bc` is, by and large, CPU-bound. This has several consequences, but the biggest
is that there is no easy way to allow signals to interrupt it.

This consequence is not obvious, but it comes from the fact that a lot of I/O
operations can be interrupted and return [`EINTR`][198]. This makes such I/O
calls natural places for allowing signals to interrupt execution, even when the
signal comes during execution, and not interrupting I/O calls. The way this is
done is setting a flag in the signal handler, which is checked around the time
of the I/O call, when it is convenient.

Alternatively, I/O bound programs can use the [self-pipe trick][199].

Neither of these are possible in `bc` because the execution of math code can
take a long time. If a signal arrives during this long execution time, setting a
flag like an I/O bound application and waiting until the next I/O call could
take seconds, minutes, hours, or even days. (Last I checked, my `bc` takes a
week to calculate a million digits of pi, and it's not slow as far as `bc`
implementations go.)

Thus, using just the technique of setting the flag just will not work for an
interactive calculator.

Well, it can, but it requires a lot of code and massive inefficiencies. I know
this because that was the original implementation.

The original implementation set a flag and just exit the signal handler. Then,
on just about every loop header, I have a check for the signal flag. These
checks happened on every iteration of every loop. It was a massive waste because
it was polling, and [polling is evil][200].

So for version [3.0.0][32], I expended a lot of effort to change the
implementation.

In the new system, code *outside* the signal handler sets a flag (`vm.sig_lock`)
to tell the signal handler whether it can use `longjmp()` to stop the current
execution. If so, it does. If not, it sets a flag, which then is used by the
code outside the signal handler that set the `vm.sig_lock` flag. When that code
unsets `vm.sig_lock`, it checks to see if a signal happened, and if so, that
code executes the `longjmp()` and stops the current execution.

Other than that, the rest of the interrupt-based implementation is best
described in the [Error Handling][97].

However, there are rules for signal handlers that I must lay out.

First, signal handlers can only call [async-signal-safe][115] functions.

Second, any field set or read by both the signal handler and normal code must be
a `volatile sig_atomic_t`.

Third, when setting such fields, they must be set to constants and no math can
be done on them. This restriction and the above restriction exist in order to
ensure that the setting of the fields is always atomic with respect to signals.

These rules exist for *any* code using Unix signal handlers, not just `bc`.

#### Vectors and Numbers

Vectors and numbers needed special consideration with the interrupt-based signal
handling.

When vectors and numbers are about to allocate, or *reallocate* their arrays,
they need to lock signals to ensure that they do not call `malloc()` and friends
and get interrupted by a signal because, as you will see in the [Error
Handling][97] section, `longjmp()` cannot be used in a signal handler if it may
be able to interrupt a non-[async-signal-safe][115] function like `malloc()` and
friends.

### Asserts

If you asked me what procedure is used the most in `bc`, I would reply without
hesitation, "`assert()`."

I use `assert()` everywhere. In fact, it is what made [fuzzing][82] with
[AFL++][125] so effective. [AFL++][125] is incredibly good at finding crashes,
and a failing `assert()` counts as one.

So while a lot of bad bugs might have corrupted data and *not* caused crashes,
because I put in so many `assert()`'s, they were *turned into* crashing bugs,
and [AFL++][125] found them.

By far, the most bugs it found this way was in the `bc` parser. (See the [`bc`
Parsing][110] for more information.) And even though I was careful to put
`assert()`'s everywhere, most parser bugs manifested during execution of
bytecode because the virtual machine assumes the bytecode is valid.

Sidenote: one of those bugs caused an infinite recursion when running the sine
(`s()`) function in the math library, so yes, parser bugs can be *very* weird.

Anyway, the way I did `assert()`'s was like this: whenever I realized that I
had put assumptions into the code, I would put an `assert()` there to test it
**and** to *document* it.

Yes, documentation. In fact, by far the best documentation of the code in `bc`
is actually the `assert()`'s. The only time I would not put an `assert()` to
test an assumption is if that assumption was already tested by an `assert()`
earlier.

As an example, if a function calls another function and passes a pointer that
the caller previously `assert()`'ed was *not* `NULL`, then the callee does not
have to `assert()` it too, unless *also* called by another function that does
not `assert()` that.

At first glance, it may seem like putting asserts for pointers being non-`NULL`
everywhere would actually be good, but unfortunately, not for fuzzing. Each
`assert()` is a branch, and [AFL++][125] rates its own effectiveness based on
how many branches it covers. If there are too many `assert()`'s, it may think
that it is not being effective and that more fuzzing is needed.

This means that `assert()`'s show up most often in two places: function
preconditions and function postconditions.

Function preconditions are `assert()`'s that test conditions relating to the
arguments a function was given. They appear at the top of the function, usually
before anything else (except maybe initializing a local variable).

Function postconditions are `assert()`'s that test the return values or other
conditions when a function exits. These are at the bottom of a function or just
before a `return` statement.

The other `assert()`'s cover various miscellaneous assumptions.

If you change the code, I ***HIGHLY*** suggest that you use `assert()`'s to
document your assumptions. And don't remove them when [AFL++][125] gleefully
crashes `bc` and `dc` over and over again.

### Vectors

In `bc`, vectors mean resizable arrays, and they are the most fundamental piece
of code in the entire codebase.

I had previously written a [vector implementation][112], which I used to guide
my decisions, but I wrote a new one so that `bc` would not have a dependency. I
also didn't make it as sophisticated; the one in `bc` is very simple.

Vectors store some information about the type that they hold:

* The size (as returned by `sizeof`).
* An enum designating the destructor.

If the destructor is `BC_DTOR_NONE`, it is counted as the type not having a
destructor.

But by storing the size, the vector can then allocate `size * cap` bytes, where
`cap` is the capacity. Then, when growing the vector, the `cap` is doubled again
and again until it is bigger than the requested size.

But to store items, or to push items, or even to return items, the vector has to
figure out where they are, since to it, the array just looks like an array of
bytes.

It does this by calculating a pointer to the underlying type with
`v + (i * size)`, where `v` is the array of bytes, `i` is the index of the
desired element, and `size` is the size of the underlying type.

Doing that, vectors can avoid undefined behavior (because `char` pointers can
be cast to any other pointer type), while calculating the exact position of
every element.

Because it can do that, it can figure out where to push new elements by
calculating `v + (len * size)`, where `len` is the number of items actually in
the vector.

By the way, `len` is different from `cap`. While `cap` is the amount of storage
*available*, `len` is the number of actual elements in the vector at the present
point in time.

Growing the vector happens when `len` is equal to `cap` *before* pushing new
items, not after.

To add a destructor, you need to add an enum item to `BcDtorType` in
[`include/vector.h`][174] and add the actual destructor in the same place as the
enum item in the `bc_vec_dtors[]` array in [`src/data.c`][131].

#### Pointer Invalidation

There is one big danger with the vectors as currently implemented: pointer
invalidation.

If a piece of code receives a pointer from a vector, then adds an item to the
vector before they finish using the pointer, that code must then update the
pointer from the vector again.

This is because any pointer inside the vector is calculated based off of the
array in the vector, and when the vector grows, it can `realloc()` the array,
which may move it in memory. If that is done, any pointer returned by
`bc_vec_item()`, `bc_vec_top()` and `bc_vec_item_rev()` will be invalid.

This fact was the single most common cause of crashes in the early days of this
`bc`; wherever I have put a comment about pointers becoming invalidated and
updating them with another call to `bc_vec_item()` and friends, *do **NOT**
remove that code!*

#### Maps

Maps in `bc` are...not.

They are really a combination of two vectors. Those combinations are easily
recognized in the source because one vector is named `<name>s` (plural), and the
other is named `<name>_map`.

There are currently three, all in `BcProgram`:

* `fns` and `fn_map` (`bc` functions).
* `vars` and `var_map` (variables).
* `arrs` and `arr_map` (arrays).

They work like this: the `<name>_map` vector holds `BcId`'s, which just holds a
string and an index. The string is the name of the item, and the index is the
index of that item in the `<name>s` vector.

Obviously, I could have just done a linear search for items in the `<name>s`
vector, but that would be slow with a lot of functions/variables/arrays.
Instead, I ensure that whenever an item is inserted into the `<name>_map`
vector, the item is inserted in sorted order. This means that the `<name>_map`
is always sorted (by the names of the items).

So when looking up an item in the "map", what is really done is this:

1.	A binary search is carried out on the names in the `<name>_map` vector.
2.	When one is found, it returns the index in the `<name>_map` vector where the
	item was found.
3.	This index is then used to retrieve the `BcId`.
4.	The index from the `BcId` is then used to index into the `<name>s` vector,
	which returns the *actual* desired item.

Why were the `<name>s` and `<name>_map` vectors not combined for ease? The
answer is that sometime, when attempting to insert into the "map", code might
find that something is already there. For example, a function with that name may
already exist, or the variable might already exist.

If the insert fails, then the name already exists, and the inserting code can
forego creating a new item to put into the vector. However, if there is no item,
the inserting code must create a new item and insert it.

If the two vectors were combined together, it would not be possible to separate
the steps such that creating a new item could be avoided if it already exists.

#### Slabs and Slab Vectors

`bc` allocates *a lot* of small strings, and small allocations are the toughest
for general-purpose allocators to handle efficiently.

Because of that reason, I decided to create a system for allocating small
strings using something that I call a "slab vector" after [slab
allocators][201].

These vectors allocate what I call "slabs," which are just an allocation of a
single page with a length to tell the slab how much of the slab is used.

The vector itself holds slabs, and when the slab vector is asked to allocate a
string, it attempts to in the last slab. If that slab cannot do so, it allocates
a new slab and allocates from that.

There is one exception: if a string is going to be bigger than 128 bytes, then
the string is directly allocated, and a slab is created with that pointer and a
length of `SIZE_MAX`, which tells the slab vector that it is a direct
allocation. Then, the last slab is pushed into the next spot and the new special
slab is put into the vacated spot. This ensures that a non-special slab is
always last.

### Command-Line History

When I first wrote `bc`, I immediately started using it in order to eat my own
dog food.

It sucked, and the biggest reason why was because of the lack of command-line
history.

At first, I just dealt with it, not knowing how command-line history might be
implemented.

Eventually, I caved and attempted to adapt [`linenoise-mob`][28], which I had
known about for some time.

It turned out to be easier than I thought; the hardest part was the tedious
renaming of everything to fit the `bc` naming scheme.

Understanding command-line history in `bc` is really about understanding VT-100
escape codes, so I would start there.

Now, the history implementation of `bc` has been adapted far beyond that initial
adaptation to make the command-line history implementation perfect for `bc`
alone, including integrating it into `bc`'s [Custom I/O][114] and making sure
that it does not disturb output that did not end with a newline.

On top of that, at one point, I attempted to get history to work on Windows. It
barely worked after a lot of work and a lot of portability code, but even with
all of that, it does not have at least one feature: multi-line pasting from the
clipboard.

### Error Handling

The error handling on `bc` got an overhaul for version [`3.0.0`][32], and it
became one of the things that taught me the most about C in particular and
programming in general.

Before then, error handling was manual. Almost all functions returned a
`BcStatus` indicating if an error had occurred. This led to a proliferation of
lines like:

```
if (BC_ERR(s)) return s;
```

In fact, a quick and dirty count of such lines in version `2.7.2` (the last
version before [`3.0.0`][32]) turned up 252 occurrences of that sort of line.

And that didn't even guarantee that return values were checked *everywhere*.

But before I can continue, let me back up a bit.

From the beginning, I decided that I would not do what GNU `bc` does on errors;
it tries to find a point at which it can recover. Instead, I decided that I
would have `bc` reset to a clean slate, which I believed, would reduce the
number of bugs where an unclean state caused errors with continuing execution.

So from the beginning, errors would essentially unwind the stack until they got
to a safe place from which to clean the slate, reset, and ask for more input.

Well, if that weren't enough, `bc` also has to handle [POSIX signals][113]. As
such, it had a signal handler that set a flag. But it could not safely interrupt
execution, so that's all it could do.

In order to actually respond to the signal, I had to litter checks for the flag
*everywhere* in the code. And I mean *everywhere*. They had to be checked on
every iteration of *every* loop. They had to be checked going into and out of
certain functions.

It was a mess.

But fortunately for me, signals did the same thing that errors did: they unwound
the stack to the *same* place.

Do you see where I am going with this?

It turns out that what I needed was a [async-signal-safe][115] form of what
programmers call "exceptions" in other languages.

I knew that [`setjmp()`][116] and [`longjmp()`][117] are used in C to implement
exceptions, so I thought I would learn how to use them. How hard could it be?

Quite hard, it turns out, especially in the presence of signals. And that's
because there are a few big snares:

1.	The value of any local variables are not guaranteed to be preserved after a
	`longjmp()` back into a function.
2.	While `longjmp()` is required to be [async-signal-safe][115], if it is
	invoked by a signal handler that interrupted a non-[async-signal-safe][115]
	function, then the behavior is undefined.
3.	Any mutation that is not guaranteed to be atomic with respect to signals may
	be incomplete when a signal arrives.

Oh boy.

For number 1, the answer to this is to hide data that must stay changed behind
pointers. Only the *pointers* are considered local, so as long as I didn't do
any modifying pointer arithmetic, pointers and their data would be safe. For
cases where I have local data that must change and stay changed, I needed to
*undo* the `setjmp()`, do the change, and the *redo* the `setjmp()`.

For number 2 and number 3, `bc` needs some way to tell the signal handler that
it cannot do a `longjmp()`. This is done by "locking" signals with a `volatile
sig_atomic_t`. (For more information, see the [Async-Signal-Safe Signal
Handling][173] section.) For every function that calls a function that is not
async-signal-safe, they first need to use `BC_SIG_LOCK` to lock signals, and
afterward, use `BC_SIG_UNLOCK` to unlock signals.

Code also need to do this for all global, non-atomic mutation, which means that
modifying any part of the `BcVm` global struct.

`BC_SIG_UNLOCK` has another requirement: it must check for signals or errors and
jump if necessary.

On top of all of that, *all* functions with cleanup needed to be able to run
their cleanup. This meant that `longjmp()` could not just jump to the finish; it
had to start what I call a "jump series," using a stack of `jmp_buf`'s
(`jmp_bufs` in `BcVm`). Each `longjmp()` uses the top of the `jmp_bufs` stack to
execute its jump. Then, if the cleanup code was executed because of a jump, the
cleanup code was responsible for continuing the jump series by popping the
previous item off the stack and using the new top of the stack for a jump.

In this way, C++-style exceptions were implemented in pure C. Not fun, but it
works. However, the order of operations matters, especially in the macros that
help implement the error handling.

For example, in `BC_UNSETJMP`, signals are unlocked before checking for signals.
If a signal comes between, that's fine; it will still cause a jump to the right
place. However, disabling the lock after could mean that a signal could come
*after* checking for signals, but before signals were unlocked, causing the
handling of the signal to be delayed.

#### Custom I/O

Why did I implement my own buffered I/O for `bc`? Because I use `setjmp()` and
`longjmp()` for error handling (see the [Error Handling][97] section), and the
buffered I/O in `libc` does not interact well with the use of those procedures;
all of the buffered I/O API is basically non-[async-signal-safe][115].

Implementing custom buffered I/O had other benefits. First, it allowed me to
tightly integrate history with the I/O code. Second, it allowed me to make
changes to history in order to make it adapt to user prompts.

### Lexing

To simplify parsing, both calculators use lexers to turn the text into a more
easily-parsable form.

While some tokens are only one character long, others require many tokens, and
some of those need to store all of the text corresponding to the token for use
by the parsers. Tokens that need to store their corresponding text include, but
are not limited to:

* Strings.
* Numbers.
* Identifiers.

For this purpose, the lexer has a [vector][111] named `str` to store the data
for tokens. This data is overwritten if another token is lexed that needs to
store data, so the parsers need to copy the data before calling the lexer again.

Both lexers do some of the same things:

* Lex identifiers into tokens, storing the identifier in `str`.
* Lex number strings into tokens, storing the string in `str`.
* Lex whitespace.
* Lex comments.

Other than that, and some common plumbing, the lexers have separate code.

#### `dc` Lexing

The `dc` lexer is remarkably simple; in fact, besides [`src/main.c`][205],
[`src/bc.c`][40], and [`src/dc.c`][44], which just contain one function each,
the only file smaller than [`src/dc_lex.c`][45] is [`src/args.c`][206], which
just processes command-line arguments after they are parsed by
[`src/opt.c`][51].

For most characters, the `dc` lexer is able to convert directly from the
character to its corresponding token. This happens using `dc_lex_tokens[]` in
[`src/data.c`][131].

`dc`'s lexer also has to lex the register name after lexing tokens for commands
that need registers.

And finally, `dc`'s lexer needs to parse `dc` strings, which is the only part of
the `dc` lexer that is more complex than the `bc` lexer. This is because `dc`
strings need to have a balanced number of brackets.

#### `bc` Lexing

The `bc` lexer is fairly simple. It does the following things:

* Lexes `bc` strings.
* Lexes `bc` identifiers. This is necessary because this is how `bc` keywords
  are lexed. After ensuring that an identifier is not a keyword, the `bc` lexer
  allows the common identifier function to take over.
* Turns characters and groups of characters into `bc` operator tokens.

### Parsing

The difference between parsing `bc` and `dc` code is...vast. The `dc` parser is
simple, while the `bc` parser is the most complex piece of code in the entire
codebase.

However, they both do some of the same things.

First, the parsers do *not* use [abstract syntax trees][207]; instead, they
directly generate the bytecode that will be executed by the `BcProgram` code.
Even in the case of `bc`, this heavily simplifies the parsing because the
[Shunting-Yard Algorithm][109] is designed to generate [Reverse Polish
Notation][108], which is basically directly executable.

Second, any extra data that the `BcProgram` needs for execution is stored into
functions (see the [Functions][208] section). These include constants and
strings.

#### `dc` Parsing

The parser for `dc`, like its lexer, is remarkably simple. In fact, the easiness
of lexing and parsing [Reverse Polish notation][108] is probably why it was used
for `dc` when it was first created at Bell Labs.

For most tokens, the `dc` parser is able to convert directly from the token
to its corresponding instruction. This happens using `dc_parse_insts[]` in
[`src/data.c`][131].

`dc`'s parser also has to parse the register name for commands that need
registers. This is the most complex part of the `dc` parser; each different
register command needs to be parsed differently because most of them require two
or more instructions to execute properly.

For example, storing in a register requires a swap instruction and an assignment
instruction.

Another example are conditional execution instructions; they need to produce the
instruction for the condition, and then they must parse a possible "else" part,
which might not exist.

##### Existing Commands

`dc` is based on commands, which are usually one letter. The following table is
a table of which ASCII characters are already used:

| Characters | Used? | For...                                     |
|------------|-------|--------------------------------------------|
| Space      | x     | Separator                                  |
| `!`        | x     | Conditional Execution of Registers         |
| `"`        | x     | Bounded Rand Operator                      |
| `#`        | x     | Comments                                   |
| `$`        | x     | Truncation                                 |
| `%`        | x     | Modulus                                    |
| `&`        |       |                                            |
| `'`        | x     | Rand Operator                              |
| `(`        | x     | Greater Than Operator                      |
| `)`        | x     | Less Than Operator                         |
| `*`        | x     | Multiplication                             |
| `+`        | x     | Addition                                   |
| `,`        | x     | Depth of Execution Stack                   |
| `-`        | x     | Subtraction                                |
| `.`        | x     | Numbers                                    |
| `/`        | x     | Division                                   |
| `0-9`      | x     | Numbers                                    |
| `:`        | x     | Store into Array                           |
| `;`        | x     | Load from Array                            |
| `<`        | x     | Conditional Execution of Registers         |
| `=`        | x     | Conditional Execution of Registers         |
| `>`        | x     | Conditional Execution of Registers         |
| `?`        | x     | Ask for User Input                         |
| `@`        | x     | Places Operator                            |
| `A-F`      | x     | Numbers                                    |
| `G`        | x     | Equal Operator                             |
| `H`        | x     | Shift Left                                 |
| `I`        | x     | Push `ibase` onto Stack                    |
| `J`        | x     | Push `seed` onto Stack                     |
| `K`        | x     | Push `scale` onto Stack                    |
| `L`        | x     | Pop off of Register                        |
| `M`        | x     | Boolean And Operator                       |
| `N`        | x     | Boolean Not Operator                       |
| `O`        | x     | Push `obase` onto Stack                    |
| `P`        | x     | Byte Stream Printing                       |
| `Q`        | x     | Quit Some Number of Macros                 |
| `R`        | x     | Pop Top of Stack                           |
| `S`        | x     | Push onto Register                         |
| `T`        | x     | Push Max `ibase` onto Stack                |
| `U`        | x     | Push Max `obase` onto Stack                |
| `V`        | x     | Push Max `scale` onto Stack                |
| `W`        | x     | Push Max of `'` Operator                   |
| `X`        | x     | Scale of a Number                          |
| `Y`        | x     | Length of Array                            |
| `Z`        | x     | Number of Significant Digits               |
| `[`        | x     | Strings                                    |
| `\\`       | x     | Escaping Brackets in Strings               |
| `]`        | x     | Strings                                    |
| `^`        | x     | Power                                      |
| `_`        | x     | Negative Numbers and Negation              |
| Backtick   |       |                                            |
| `a`        | x     | Asciify                                    |
| `b`        | x     | Absolute Value                             |
| `c`        | x     | Clear Stack                                |
| `d`        | x     | Duplication of Top of Stack                |
| `e`        | x     | Else in Conditional Execution of Registers |
| `f`        | x     | Printing the Stack                         |
| `g`        | x     | Global Settings                            |
| `h`        | x     | Shift Right                                |
| `i`        | x     | Set `ibase`                                |
| `j`        | x     | Set `seed`                                 |
| `k`        | x     | Set `scale`                                |
| `l`        | x     | Load from Register                         |
| `m`        | x     | Boolean Or Operator                        |
| `n`        | x     | Print and Pop                              |
| `o`        | x     | Set `obase`                                |
| `p`        | x     | Print with Newline                         |
| `q`        | x     | Quit Two Macros                            |
| `r`        | x     | Swap Top Two Items                         |
| `s`        | x     | Store into Register                        |
| `t`        |       |                                            |
| `u`        |       |                                            |
| `v`        | x     | Square Root                                |
| `w`        |       |                                            |
| `x`        | x     | Execute String                             |
| `y`        | x     | Current Depth of a Register                |
| `z`        | x     | Current Depth of Stack                     |
| `{`        | x     | Greater Than or Equal Operator             |
| `\|`       | x     | Moduler Exponentiation                     |
| `}`        | x     | Less Than or Equal Operator                |
| `~`        | x     | Division and Modulus Combined              |

#### `bc` Parsing

`bc`'s parser is, by far, the most sensitive piece of code in this software, and
there is a very big reason for that: `bc`'s standard is awful and defined a very
poor language.

The standard says that either semicolons or newlines can end statements. Trying
to parse the end of a statement when it can either be a newline or a semicolon
is subtle. Doing it in the presence of control flow constructs that do not have
to use braces is even harder.

And then comes the biggest complication of all: `bc` has to assume that it is
*always* at a REPL (Read-Eval-Print Loop). `bc` is, first and foremost, an
*interactive* utility.

##### Flags

All of this means that `bc` has to be able to partially parse something, store
enough data to recreate that state later, and return, making sure to not
execute anything in the meantime.

*That* is what the flags in [`include/bc.h`][106] are: they are the state that
`bc` is saving for itself.

It saves them in a stack, by the way, because it's possible to nest
structures, just like any other programming language. Thus, not only does it
have to store state, it needs to do it arbitrarily, and still be able to
come back to it.

So `bc` stores its parser state with flags in a stack. Careful setting of these
flags, along with properly using them and maintaining the flag stack, are what
make `bc` parsing work, but it's complicated. In fact, as I mentioned, the `bc`
parser is the single most subtle, fickle, and sensitive piece of code in the
entire codebase. Only one thing came close once: square root, and that was only
sensitive because I wrote it wrong. This parser is pretty good, and it is
*still* sensitive. And flags are the reason why.

For more information about what individual flags there are, see the comments in
[`include/bc.h`][106].

##### Labels

`bc`'s language is Turing-complete. That means that code needs the ability to
jump around, specifically to implement control flow like `if` statements and
loops.

`bc` handles this while parsing with what I called "labels."

Labels are markers in the bytecode. They are stored in functions alongside the
bytecode, and they are just indices into the bytecode.

When the `bc` parser creates a label, it pushes an index onto the labels array,
and the index of the label in that array is the index that will be inserted into
the bytecode.

Then, when a jump happens, the index pulled out of the bytecode is used to index
the labels array, and the label (index) at the index is then used to set the
instruction pointer.

##### Cond Labels

"Cond" labels are so-called because they are used by conditionals.

The key to them is that they come *before* the code that uses them. In other
words, when jumping to a condition, code is jumping *backwards*.

This means that when a cond label is created, the value that should go there is
well-known. Cond labels are easy.

However, they are still stored on a stack so that the parser knows what cond
label to use.

##### Exit Labels

Exit labels are not so easy.

"Exit" labels are so-called because they are used by code "exiting" out of `if`
statements or loops.

The key to them is that they come *after* the code that uses them. In other
words, when jumping to an exit, code is jumping *forwards*.

But this means that when an exit label is created, the value that should go
there is *not* known. The code that needs it must be parsed and generated first.

That means that exit labels are created with the index of `SIZE_MAX`, which is
then specifically checked for with an assert in `bc_program_exec()` before using
those indices.

There should ***NEVER*** be a case when an exit label is not filled in properly
if the parser has no bugs. This is because every `if` statement, every loop,
must have an exit, so the exit must be set. If not, there is a bug.

Exit labels are also stored on a stack so that the parser knows what exit label
to use.

##### Expression Parsing

`bc` has expressions like you might expect in a typical programming language.
This means [infix notation][107].

One thing about infix notation is that you can't just generate code straight
from it like you can with [Reverse Polish notation][108]. It requires more work
to shape it into a form that works for execution on a stack machine.

That extra work is called the [Shunting-Yard algorithm][109], and the form it
translates infix notation into is...[Reverse Polish notation][108].

In order to understand the rest of this section, you must understand the
[Shunting-Yard algorithm][109]. Go do that before you read on.

###### Operator Stack

In `bc`, the [Shunting-Yard algorithm][109] is implemented with bytecode as the
output and an explicit operator stack (the `ops` field in `BcParse`) as the
operator stack. It stores tokens from `BcLex`.

However, there is one **HUGE** hangup: multiple expressions can stack. This
means that multiple expressions can be parsed at one time (think an array element
expression in the middle of a larger expression). Because of that, we need to
keep track of where the previous expression ended. That's what `start` parameter
to `bc_parse_operator()` is.

Parsing multiple expressions on one operator stack only works because
expressions can only *stack*; this means that, if an expression begins before
another ends, it must *also* end before that other expression ends. This
property ensures that operators will never interfere with each other on the
operator stack.

###### Recursion

Because expressions can stack, parsing expressions actually requires recursion.
Well, it doesn't *require* it, but the code is much more readable that way.

This recursion is indirect; the functions that `bc_parse_expr_err()` (the actual
expression parsing function) calls can, in turn, call it.

###### Expression Flags

There is one more big thing: not all expressions in `bc` are equal.

Some expressions have requirements that others don't have. For example, only
array arguments can be arrays (which are technically not expressions, but are
treated as such for parsing), and some operators (in POSIX) are not allowed in
certain places.

For this reason, functions that are part of the expression parsing
infrastructure in `bc`'s parser usually take a `flags` argument. This is meant
to be passed to children, and somewhere, they will be checked to ensure that the
resulting expression meets its requirements.

There are also places where the flags are changed. This is because the
requirements change.

Maintaining the integrity of the requirements flag set is an important part of
the `bc` parser. However, they do not have to be stored on a stack because their
stack is implicit from the recursion that expression parsing uses.

### Functions

Functions, in `bc`, are data structures that contain the bytecode and data
produced by the parsers. Functions are what the `BcProgram` program executes.

#### Main and Read Functions

There are two functions that always exist, which I call the "main" and "read"
functions.

The "main" function is the function in which any code and data outside other
functions is put. Basically, it is the function where the scripting code ends
up.

The "read" function is the function that is reset and parsed every time a call
to the `read()` builtin function happens.

#### `dc` Strings

In `dc`, strings can be executed, and since there are no actual "functions" in
`dc`, strings are handled as functions. In fact, they are effectively translated
into functions by parsing.

##### Tail Calls

Since strings in `dc` are functions, and the fact that `dc` has no native loops,
such loops are implemented in `dc` code using strings with conditional execution
commands at the end of strings.

When such conditional execution, or even unconditional execution, commands are
the very last commands in a string, then `dc` can perform a [tail call][202].

This is done by recording the fact that a tail call happened, done by
incrementing an integer on a stack. When a string is executed *without* a tail
call, a new entry is pushed onto the stack with the integer `1`.

When a string finally quits that followed tail calls, its stack entry is popped,
eliminating all of those tail calls.

Why perform tail calls? Because otherwise, `dc` would be subject to the same
thing that plagues [functional programming languages][203]: stack overflow. In
`dc`'s case, that would manifest itself as a growing [heap][204], because the
execution stack is stored on the heap, until a fatal allocation failure would
occur.

#### Execution

Execution is handled by an interpreter implemented using `BcProgram` and code
in [`src/program.c`][53].

The interpreter is a mix between a [stack machine][210] and a [register
machine][211]. It is a stack machine in that operations happen on a stack I call
the "results stack," but it is a register machine in that items on the stack can
be stored to and loaded from "registers" (`dc` terminology), variables (`bc`
terminology), and arrays.

##### Stacks

There are two stacks in the interpreter:

* The "results" stack (as mentioned above).
* The "execution" stack.

The results stack (the `results` field of the `BcProgram` struct) is the stack
where the results of computations are stored. It is what makes the interpreter
part [stack machine][210]. It is filled with `BcResult`'s.

The execution stack (the `stack` field of the `BcProgram` struct) is the stack
that tracks the current execution state of the interpreter. It is the presence
of this separate stack that allows the interpreter to implement the machine as a
loop, rather than recursively. It is filled with `BcInstPtr`'s, which are the
"instruction pointers."

These instruction pointers have three fields, all integers:

* `func`, the index of the function that is currently executing.
* `idx`, the index of the next bytecode instruction to execute in the function's
  bytecode array.
* `len`, which is the length of the results stack when the function started
  executing. This is not used by `dc`, but it used by `bc` because functions
  in `bc` should never affect the results stack of their callers.

With these three fields, and always executing using the instruction pointer at
the top of the execution stack, the interpreter can always keep track of its
execution.

When a function or a string starts executing, a new `BcInstPtr` is pushed onto
the execution stack for it. This includes if a function was called recursively.
And then, when the function or string returns, its `BcInstPtr` is popped off of
the execution stack.

##### Bytecode

Execution of functions are done through bytecode produced directly by the
parsers (see the [Parsing][209]). This bytecode is stored in the `code`
[vector][111] of the `BcFunc` struct.

This is a vector for two reasons:

* It makes it easier to add bytecode to the vector in the parsers.
* `bc` allows users to redefine functions.

The reason I can use bytecode is because there are less than 256 instructions,
so an `unsigned char` can store all the bytecodes.

###### Bytecode Indices

There is one other factor to bytecode: there are instructions that need to
reference strings, constants, variables, or arrays. Bytecode need some way to
reference those things.

Fortunately, all of those things can be referenced in the same way: with indices
because all of the items are in vectors.

So `bc` has a way of encoding an index into bytecode. It does this by, after
pushing the instruction that references anything, pushing a byte set to the
length of the index in bytes, then the bytes of the index are pushed in
little-endian order.

Then, when the interpreter encounters an instruction that needs one or more
items, it decodes the index or indices there and updates the `idx` field of the
current `BcInstPtr` to point to the byte after the index or indices.

One more thing: the encoder of the indices only pushes as many bytes as
necessary to encode the index. It stops pushing when the index has no more bytes
with any 1 bits.

##### Variables

In `bc`, the vector of variables, `vars` in `BcProgram`, is not a vector of
numbers; it is a vector of vector of numbers. The first vector is the vector of
variables, the second is the variable stack, and the last level is the actual
number.

This is because both `bc` and `dc` need variables to be stacks.

For `dc`, registers are *defined* to be stacks.

For `bc`, variables as stacks is how function arguments/parameters and function
`auto` variables are implemented.

When a function is called, and a value needs to be used as a function argument,
a copy of the value is pushed onto the stack corresponding to the variable with
the same name as the function's parameter. For `auto` variables, a new number
set to zero is pushed onto each stack corresponding to the `auto` variables.
(Zero is used because the [`bc` spec][2] requires that `auto` variables are set
to zero.)

It is in this way that the old value of the variable, which may not even be
related to the function parameter or `auto` variable, is preserved while the
variable is used as a function parameter or `auto` variable.

When the function returns, of course, the stacks of the variables for the
parameters and `auto`'s will have their top item popped, restoring the old value
as it was before the function call.

##### Arrays

Like variables, arrays are also implemented as stacks. However, because they are
arrays, there is yet another level; the `arrs` field in `BcProgram` is a vector
of vectors of vectors of numbers. The first of the two levels is the vector of
arrays, the second the stack of for each array, the third the actual array, and
last the numbers in the array.

`dc` has no need of this extra stack, but `bc` does because arrays can be
function parameters themselves.

When arrays are used for function arguments, they are copied with a deep copy;
each item of the source vector is copied. This is because in `bc`, according to
the [`bc` spec][2], all function arguments are passed by value.

However, array references are possible (see below).

When arrays are used as `auto`'s, a new vector is pushed with one element; if
more elements are needed, the array is grown automatically, and new elements are
given the value of zero.

In fact, if *any* array is accessed and does not have an element at that index,
the array is automaticall grown to that size, and all new elements are given the
value zero. This behavior is guaranteed by the [`bc` spec][2].

###### Array References

Array references had to be implemented as vectors themselves because they must
be pushed on the vectors stacks, which, as seen above, expect vectors
themselves.

So thus, references are implemented as vectors on the vector stacks. These
vectors are not vectors of vectors themselves; they are vectors of bytes; in
fact, the fact that they are byte vectors and not vector vectors is how a
reference vector is detected.

These reference vectors always have the same two things pushed: a byte encoding
(the same way bytecode indices are) of the referenced vector's index in the
`arrs` vector, and a byte encoding of the referenced vectors index in the vector
stack.

If an item in a referenced vector is needed, then the reference is dereferenced,
and the item is returned.

If a reference vector is passed to a function that does *not* expect a
reference, the vector is dereferenced and a deep copy is done, in the same way
as vectors are copied for normal array function parameters.

### Callbacks

There are many places in `bc` and `dc` where function pointers are used:

* To implement destructors in vectors. (See the [Vectors][111] section.)
* To select the correct lex and parse functions for `bc` and `dc`.
* To select the correct function to execute unary operators.
* To select the correct function to execute binary operators.
* To calculate the correct number size for binary operators.
* To print a "digit" of a number.
* To seed the pseudo-random number generator.

And there might be more.

In every case, they are used for reducing the amount of code. Instead of
`if`/`else` chains, such as:

```
if (BC_IS_BC) {
	bc_parse_parse(vm.parse);
}
else {
	dc_parse_parse(vm.parse);
}
```

The best example of this is `bc_num_binary()`. It is called by every binary
operator. It figures out if it needs to allocate space for a new `BcNum`. If so,
it allocates the space and then calls the function pointer to the *true*
operation.

Doing it like that shrunk the code *immensely*. First, instead of every single
binary operator duplicating the allocation code, it only exists in one place.
Second, `bc_num_binary()` itself does not have a massive `if`/`else` chain or a
`switch` statement.

But perhaps the most important use was for destructors in vectors.

Most of the data structures in `bc` are stored in vectors. If I hadn't made
destructors available for vectors, then ensuring that `bc` had no memory leaks
would have been nigh impossible. As it is, I check `bc` for memory leaks every
release when I change the code, and I have not released `bc` after version
`1.0.0` with any memory leaks, as far as I can remember anyway.

### Numbers

In order to do arbitrary-precision math, as `bc` must do, there must be some way
of representing arbitrary-precision numbers. `BcNum` in [`include/num.h`][184]
is `bc`'s way of doing that.

(Note: the word ["limb"][214] is used below; it has a specific meaning when
applied to arbitrary-precision numbers. It means one piece of the number. It can
have a single digit, which is what GNU `bc` does, or it can have multiple, which
is what this `bc` does.)

This struct needs to store several things:

* The array of limbs of the number. This is the `num` field.
* The location of the decimal point. This is the `rdx` (short for [radix][215])
  field.
* The number of limbs the number has. This is the `len` field.
* Whether the number is negative or not. This is the least significant bit of
  the `rdx` field. More on that later.

In addition, `bc`'s number stores the capacity of the limb array; this is the
`cap` field.

If the number needs to grow, and the capacity of the number is big enough, the
number is not reallocated; the number of limbs is just added to.

There is one additional wrinkle: to make the usual operations (binary operators)
fast, the decimal point is *not* allowed to be in the middle of a limb; it must
always be between limbs, after all limbs (integer), or before all limbs (real
between -1 and 1).

The reason for this is because addition, subtraction, multiplication, and
division expect digits to be lined up on the decimal point. By requiring that it
be between limbs, no extra alignment is needed, and those operations can proceed
without extra overhead.

This does make some operations, most notably extending, truncating, and
shifting, more expensive, but the overhead is constant, and these operations are
usually cheap compared to the binary operators anyway.

This also requires something else: `bc` numbers need to know *exactly* how many
decimal places they have after the decimal point. If the decimal point must be
inbetween limbs, the last decimal place could be in the middle of a limb. The
amount of decimal places in a number is carefully tracked and stored in the
`scale` field, and this number must always coincide with the `rdx` field by the
following formula:

```
scale + (BC_BASE_DIGS - 1) / BC_BASE_DIGS == rdx >> 1
```

(`BC_BASE_DIGS` is the number of decimal digits stored in one limb. It is 9 on
64-bit systems and 4 on other systems.)

Yes, `rdx` is shifted; that is because the negative bit is stored in the least
significant bit of the `rdx` field, and the actual radix (amount of limbs after
the decimal/radix point) is stored in the rest of the bits. This is safe because
`BC_BASE_DIGS` is always at least 4, which means `rdx` will always need at least
2 bits less than `scale`.

In addition to `rdx` always matching `scale`, another invariant is that `rdx`
must always be less than or equal to `len`. (Because `scale` may be greater than
`rdx`, `scale` does not have to be less than or equal to `len`.)

Another invariant is that `len` must always be less than or equal to `cap`, for
obvious reasons.

The last thing programmers need to know is that the limb array is stored in
little-endian order. This means that the last decimal places are in the limb
stored at index 0, and the most significant digits are stored at index `len-1`.

This is done to make the most important operations fast. Addition and
subtraction are done from least significant to most significant limbs, which
means they can speed through memory in the way most computers are best at.
Multiplication does the same, sort of, and with division, it matters less.
Comparison does need to go backwards, but that's after exhausting all other
alternatives, including for example, checking the length of the integer portion
of each limb array.

Finally, here are some possible special situations with numbers and what they
mean:

* `len == 0`: the number equals 0.
* `len == 0 && scale != 0`: the number equals 0, but it has a `scale` value.
  This is the only case where `scale` does not have to coincide with `rdx`
  This can happen with division, for example, that sets a specific `scale` for
  the result value but may produce 0.
* `(rdx >> 1) < len`: the number is greater than or equal to 1, or less than or
  equal to -1.
* `(rdx >> 1) == len`: the number is greater than -1 and less than 1, not
  including 0, although this will be true for 0 as well. However, 0 is always
  assumed to be represented by `len == 0`.
* `(rdx >> 1) == 0`: the number is an integer. In this case, `scale` must also
  equal 0.

#### Math Style

When I wrote the math for `bc`, I adopted a certain style that, if known, will
make it easier to understand the code. The style follows these rules:

* `BcNum` arguments always come before arguments of other types.
* Among the `BcNum` arguments, the operands always come first, and the `BcNum`
  where the result(s) will be stored come last.
* Error checking is placed first in the function.
* Easy cases are placed next.
* Preparation, such as allocating temporaries, comes next.
* The actual math.
* Cleanup and ensuring invariants.

While these rules are not hard and fast, using them as a guide will probably
help.

### Strings as Numbers

Strings can be assigned to variables. This is a problem because the vectors for
variable stacks expect `BcNum` structs only.

While I could have made a union, I decided that the complexity of adding an
entirely new type, with destructor and everything, was not worth it. Instead, I
took advantage of the fact that `free()`, when passed a `NULL` pointer, will do
nothing.

Using that, I made it so `BcNum`'s could store strings instead. This is marked
by the `BcNum` having a `NULL` limb array (`num`) and a `cap` of 0 (which should
*never* happen with a real number, though the other fields could be 0).

The `BcNum` stores the function that stores the string in the `rdx` field, and
it stores the index of the string in the `scale` field. This is used to actually
load the string if necessary.

Note that historically, string information was stored in the `loc` field of
the `d` union in a `BcResult`. This was changed recently to standardize; now,
all string information are stored in the `n` field of the `d` union regardless.
This means that all string information is stored in `BcNum`'s. This removes
extra cases.

Also, if a temp is made with a string, then the result type should still be
`BC_RESULT_STR`, not `BC_RESULT_TEMP`. This is to make it easier to do type
checks.

### Pseudo-Random Number Generator

In order to understand this section, I suggest you read the information in the
manpages about the pseudo-random number generator (PRNG) first; that will help
you understand the guarantees it has, which is important because this section
delves into implementation details.

First, the PRNG I use is seeded; this is because most OS's have an excellent
cryptographically secure PRNG available via command-line, usually
`/dev/urandom`, but the only *seeded* PRNG available is usually `bash`'s
`$RANDOM`, which is essentially a wrapper around C's `rand()`.

`rand()` is...bad. It is only guaranteed to return 15 bits of random data.
Obviously, getting good random data out of that would be hard with that alone,
but implementations also seem to be poor.

On top of that, `bc` is an arbitrary-precision calculator; if I made it able to
generate random numbers, I could make it generate random numbers of any size,
and since it would be seeded, results would be reproducible, when wanted.

So to get that, I needed a seeded PRNG with good characteristics. After scouring
the Internet, I decided on the [PCG PRNG][215], mostly because of [this blog
post][216]. Part of the reason was the behavior of the xoroshiro128+ author, who
hates on PCG and its author, but also because PCG seemed to do better when
tested by independent parties.

After that decision, I faced a challenge: PCG requires 255 bits of seed: 128 for
the actual seed, and 127 for the "increment." (Melissa O'Neill, the PCG author,
likens the increment to selecting a codebook.)

I could, of course, put the entire 255 bits into one massive arbitrary-precision
number; `bc` is good at that, after all. But that didn't sit right with me
because it would mean any seed selected by users would have the real portion
ignored, which is stupid in a program like `bc`.

Instead, I decided to make the integer portion the increment (clamped down to
size), and the real portion the seed.

In most cases, this would be a bad idea because you cannot, in general, know how
many decimal places you need to represent any number with `n` real digits in
base `b` in another base. However, there is an easy to how many decimal digits
after the decimal point it takes to represent reals of base 2 in base 10: the
power of two.

It turns out that, for base 2 represented in base 10, the power of 2 is
*exactly* how many digits are necessary to represent *any* number `n/2^p`, where
`p` is the power of 2. This is because at every halving, the number of decimal
places increases by 1:

```
0.5
0.25
0.125
0.0625
0.03125
0.015625
...
```

So the algorithm to convert all 255 bits of the seed is as follows:

1.	Convert the increment to a `BcNum`.
2.	Convert the seed to a `BcNum`.
3.	Divide the seed by `2^128` with a `scale` of 128. (For 32-bit systems,
	substitute 64 bits for 128.)
4.	Add the two numbers together.

Likewise, the algorithm to convert from a user-supplied number to a seed is:

1.	Truncate a copy of the number.
2.	Subtract the result from #1 from the original number. This gives the real
	portion of the number.
3.	Clamp the result of #1 to 127 (or 63) bits. This is the increment.
4.	Multiply the result of #2 by `2^128`.
5.	Truncate the result of #4. This is the seed.

#### Generating Arbitrary-Precision Numbers

I wrote a function (`bc_rand_bounded()`) that will return unbiased results with
any bound below the max that PCG can generate.

To generate an integer of arbitrary size using a bound, `bc` simply uses
`bc_rand_bounded()` to generate numbers with a bound `10^BC_BASE_DIGS` for as
many limbs as needed to satisfy the bigger bound.

To generate numbers with arbitrary precision after the decimal point, `bc`
merely generates an arbitrary precision integer with the bound `10^p`, where `p`
is the desired number of decimal places, then divides in by `10^p` with a
`scale` of `p`.

## Debug Code

Besides building `bc` in debug mode with the `-g` flag to [`configure.sh`][69],
programmers can also add `-DBC_DEBUG_CODE=1` to the `CFLAGS`. This will enable
the inclusion of *a lot* of extra code to assist with debugging.

For more information, see all of the code guarded by `#if BC_DEBUG_CODE` in the
[`include/`][212] directory and in the [`src/`][213] directory.

Yes, all of the code is guarded by `#if` preprocessor statements; this is
because the code should *never* be in a release build, and by making programmers
add this manually (not even an option to [`configure.sh`][69]), it is easier to
ensure that never happens.

However, that said, the extra debug code is useful; that was why I kept it in.

## Performance

While I have put in a lot of effort to make `bc` as fast as possible, there
might be some things you can do to speed it up without changing the code.

First, you can probably use [profile-guided optimization][217] to optimize even
better, using the test suite to profile.

Second, I included macros that might help branch placement and prediction:

* `BC_ERR(e)`
* `BC_UNLIKELY(e)`
* `BC_NO_ERR(e)`
* `BC_LIKELY(e)`

`BC_ERR` is the same as `BC_UNLIKELY`, and `BC_NO_ERR` is the same as
`BC_LIKELY`; I just added them to also document branches that lead to error
conditions or *away* from error conditions.

Anyway, if `BC_LIKELY` and `BC_UNLIKELY` are not defined during compilation,
they expand to nothing but the argument they were given.

They can, however, be defined to `__builtin_expect((e), 1)` and
`__builtin_expect((e), 0)`, respectively, on GCC and Clang for better branch
prediction and placement. (For more information about `__builtin_expect()` see
the [GCC documentation][218].)

There might be other compilers that can take advantage of that, but I don't know
anything about that.

Also, as stated in the [build manual][219], link-time optimization is excellent
at optimizing this `bc`. Use it.

### Benchmarks

To help programmers improve performance, I have built and assembled
infrastructure to make benchmarking easy.

First, in order to easily run benchmarks, I created
[`scripts/benchmark.sh`][220].

Second, I copied and adapted [`ministat.c`][223] [from FreeBSD][221], to make it
easier to judge whether the results are significant or not.

Third, I made the `make` clean target `make clean_benchmarks`, to clean
`scripts/ministat` and the generated benchmark files.

Fourth, I made it so [`scripts/benchmark.sh`][220] outputs the timing and memory
data in a format that is easy for `scripts/ministat` to digest.

To add a benchmark, add a script in the right directory to generate the
benchmark. Yes, generate.

All of the benchmarks are generated first, from `.bc` and `.dc` files in the
[`benchmarks/bc/`][91] and [`benchmarks/dc/`][224]. This is so that massive
amounts of data can be generated and then pushed through the calculators.

If you need to benchmark `bc` or `dc` with simple loops, have the generator
files simply print the loop code.

### Caching of Numbers

In order to provide some performance boost, `bc` tries to reuse old `BcNum`'s
that have the default capacity (`BC_NUM_DEF_SIZE`).

It does this by allowing `bc_num_free()` to put the limb array onto a
statically-allocated stack (it's just a global array with a set size). Then,
when a `BcNum` with the default capacity is needed, `bc_num_init()` asks if any
are available. If the answer is yes, the one on top of the stack is returned.
Otherwise, `NULL` is returned, and `bc_num_free()` knows it needs to `malloc()`
a new limb array.

When the stack is filled, any numbers that `bc` attempts to put on it are just
freed.

This setup saved a few percent in my testing for version [3.0.0][32], which is
when I added it.

## `bcl`

At the request of one of my biggest users, I spent the time to make a build mode
where the number and math code of `bc` could be wrapped into a library, which I
called `bcl`.

This mode is exclusive; `bc` and `dc` themselves are *not* built when building
`bcl`.

The only things in the `bc` math code that is not included is:

* Printing newlines (clients do not care about `bc`'s line lenth restriction).
* `dc`'s stream print.

Even the [pseudo-random number generator][179] is included, with extra support
for generating real numbers with it. (In `bc`, such support is in
[`lib2.bc`][26].)

### Signal Handling

Like signal handling in `bc` proper (see the [Async-Signal-Safe Signal
Handling][173] section), `bcl` has the infrastructure for signal handling.

This infrastructure is different, however, as `bcl` assumes that clients will
implement their own signal handling.

So instead of doing signal handling on its own, `bcl` provides the capability to
interrupt executions and return to the clients almost immediately. Like in `bc`,
this is done with `setjmp()` and `longjmp()`, although the jump series is
stopped before returning normally to client code.

### Contexts

Contexts were an idea by the same user that requested `bcl`. They are meant to
make it so multiple clients in one program can keep their data separate from
each other.

### Numbers

Numbers in `bcl` are literally indices into an encapsulated array of numbers,
hidden in the context. These indices are then passed to clients to refer to
numbers later.

### Operand Consumption

Most math functions in `bcl` "consume" their operand arguments; the arguments
are freed, whether or not an error is returned.

This is to make it easy to implement math code, like this:

```
n = bcl_add(bcl_mul(a, b), bcl_div(c, d));
```

If numbers need to be preserved, they can be with `bcl_dup()`:

```
n = bcl_add(bcl_mul(bcl_dup(a), bc_dup(b)), bcl_div(bcl_dup(c), bcl_dup(d)));
```

### Errors

Errors can be encoded in the indices representing numbers, and where necessary,
clients are responsible for checking those errors.

The encoding of errors is this: if an error happens, the value `0-error` is
returned. To decode, do the exact same thing. Thus, any index above
`0-num_errors` is an error.

If an index that represents an error is passed to a math function, that function
propagates the error to its result and does not perform the math operation.

All of this is to, once again, make it easy to implement the math code as above.

However, where possible, errors are returned directly.

[1]: https://en.wikipedia.org/wiki/Bus_factor
[2]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html#top
[3]: https://en.wikipedia.org/wiki/Dc_(Unix)
[4]: https://en.wikipedia.org/wiki/Reverse_Polish_notation
[5]: ./bc/A.1.md#standard-library
[6]: https://github.com/torvalds/linux/blob/master/kernel/time/timeconst.bc
[7]: ./bc/A.1.md#extended-library
[8]: #libbc-2
[9]: #strgensh
[10]: https://vimeo.com/230142234
[11]: https://gavinhoward.com/2019/12/values-for-yao/
[12]: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf
[13]: ./build.md#cross-compiling
[14]: ./build.md
[15]: #strgenc
[16]: http://landley.net/toybox/about.html
[17]: https://www.busybox.net/
[18]: https://en.wikipedia.org/wiki/Karatsuba_algorithm
[19]: https://clang-analyzer.llvm.org/scan-build.html
[20]: https://www.valgrind.org/
[21]: https://clang.llvm.org/docs/AddressSanitizer.html
[22]: https://gavinhoward.com/2019/11/finishing-software/
[23]: #history
[24]: https://clang.llvm.org/docs/ClangFormat.html
[25]: ./algorithms.md
[26]: #lib2bc
[27]: #vmh
[28]: https://github.com/rain-1/linenoise-mob
[29]: https://github.com/antirez/linenoise
[30]: #bclh
[31]: #argsh
[32]: ../NEWS.md#3-0-0
[33]: ../NEWS.md
[34]: https://github.com/skeeto/optparse
[35]: #opth
[36]: #historyh
[37]: #randh
[38]: #langh
[39]: #numc
[40]: #bcc
[41]: #bc_lexc
[42]: #bc_parsec
[43]: #libraryc
[44]: #dcc
[45]: #dc_lexc
[46]: #dc_parsec
[47]: #filec
[48]: #historyc
[49]: #langc
[50]: #lexc
[51]: #optc
[52]: #parsec
[53]: #programc
[54]: #randc
[55]: #fileh
[56]: #readc
[57]: #programh
[58]: #vmc
[59]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/gencat.html#top
[60]: #manpagesh
[61]: #bcl3md
[62]: #bcl3
[63]: #bclvcxproj
[64]: #bclvcxprojfilters
[65]: #bclsln
[66]: #bcvcxproj
[67]: #bcvcxprojfilters
[68]: #bcsln
[69]: #configuresh
[70]: #makefilein
[71]: #functionsh
[72]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/sh.html#top
[73]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18
[74]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/make.html#top
[75]: #versionh
[76]: ##posix-shell-scripts
[77]: #tests
[78]: #karatsubapy
[79]: #bc-1
[80]: #dc-1
[81]: ./build.md#build-type
[82]: #fuzzing-1
[83]: #releasesh
[84]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_02
[85]: #locales-1
[86]: #manuals-1
[87]: #locale_installsh
[88]: #locale_uninstallsh
[89]: #bc1mdin
[90]: #dc1mdin
[91]: #bc
[92]: https://pandoc.org/
[93]: #release_settingstxt
[94]: #aflpy
[95]: #randmathpy
[96]: #caching-of-numbers
[97]: #error-handling
[98]: #radamsatxt
[99]: https://gitlab.com/akihe/radamsa
[100]: #radamsash
[101]: https://musl.libc.org/
[102]: ./build.md#settings
[103]: #test_settingstxt
[104]: #test_settingssh
[105]: #functionssh
[106]: #bch
[107]: https://en.wikipedia.org/wiki/Infix_notation
[108]: https://en.wikipedia.org/wiki/Reverse_Polish_notation
[109]: https://en.wikipedia.org/wiki/Shunting-yard_algorithm
[110]: #bc-parsing
[111]: #vectors
[112]: https://git.yzena.com/Yzena/Yc/src/branch/master/include/yc/vector.h
[113]: https://en.wikipedia.org/wiki/Signal_(IPC)
[114]: #custom-io
[115]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04_03_03
[116]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/setjmp.html
[117]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/longjmp.html
[118]: https://www.youtube.com/watch?v=4PaWFYm0kEw
[119]: #fuzz_prepsh
[120]: #bc_aflyaml
[121]: #bc_afl_continueyaml
[122]: https://github.com/tmux/tmux
[123]: https://tmuxp.git-pull.com/
[124]: #test-suite
[125]: https://aflplus.plus/
[126]: #link-time-optimization
[127]: #fuzzing-performance
[128]: #radamsa
[129]: #afl-quickstart
[130]: #convenience
[131]: #datac
[132]: https://git.yzena.com/gavin/vim-bc
[133]: https://git.yzena.com/gavin/bc_libs
[134]: #debugging
[135]: #asserts
[136]: #portability
[137]: https://pexpect.readthedocs.io/en/stable/
[138]: #test-suite-portability
[139]: #historypy
[140]: #historysh
[141]: #group-tests
[142]: #build-system
[143]: #generated-tests
[144]: #benchmarks-1
[145]: #gen
[146]: #test-coverage
[147]: #integration-with-the-build-system
[148]: #test-scripts
[149]: #standard-tests
[150]: #script-tests
[151]: #error-tests
[152]: #stdin-tests
[153]: #read-tests
[154]: #other-tests
[155]: #history-tests
[156]: #bcl
[157]: #bcl-test
[158]: #bclc
[159]: #valgrind
[160]: #addresssanitizer-and-friends
[161]: #bc-2
[162]: #dc-2
[163]: #alltxt-1
[164]: #errorstxt
[165]: #posix_errorstxt
[166]: #timeconstsh
[167]: #alltxt-3
[168]: #errorstxt-1
[169]: #scripts-1
[170]: #scripts-2
[171]: #alltxt-2
[172]: #alltxt-4
[173]: #async-signal-safe-signal-handling
[174]: #vectorh
[175]: #read_errorstxt
[176]: #statush
[177]: #numbers
[178]: #math-style
[179]: #pseudo-random-number-generator
[180]: #lexh
[181]: #parseh
[182]: #dch
[183]: #libraryh
[184]: #numh
[185]: #readh
[186]: #maps
[187]: #slabs-and-slab-vectors
[188]: ./build.md#extra-math
[189]: #command-line-history
[190]: #scriptsed
[191]: #linux-timeconstbc-script
[192]: #corpuses
[193]: ./build.md#history
[194]: https://www.valgrind.org/docs/manual/mc-manual.html
[195]: #othersh
[196]: https://scan.coverity.com/
[197]: https://clang-analyzer.llvm.org/
[198]: https://unix.stackexchange.com/questions/253349/eintr-is-there-a-rationale-behind-it
[199]: https://cr.yp.to/docs/selfpipe.html
[200]: https://skarnet.org/cgi-bin/archive.cgi?2:mss:1607:201701:dfblejammjllfkggpcph
[201]: https://slembcke.github.io/2020/10/12/CustomAllocators.html#1-slab-allocator
[202]: https://en.wikipedia.org/wiki/Tail_call
[203]: https://en.wikipedia.org/wiki/Functional_programming_language
[204]: https://en.wikipedia.org/wiki/C_dynamic_memory_allocation
[205]: #mainc
[206]: #argc
[207]: https://en.wikipedia.org/wiki/Abstract_syntax_tree
[208]: #functions
[209]: #parsing
[210]: https://en.wikipedia.org/wiki/Stack_machine
[211]: https://en.wikipedia.org/wiki/Register_machine
[212]: #include
[213]: #src
[214]: https://gmplib.org/manual/Nomenclature-and-Types
[215]: https://en.wikipedia.org/wiki/Radix_point
[216]: #main-and-read-functions
[215]: https://www.pcg-random.org/
[216]: https://lemire.me/blog/2017/08/22/testing-non-cryptographic-random-number-generators-my-results/
[217]: https://en.wikipedia.org/wiki/Profile-guided_optimization
[218]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005fexpect
[219]: ./build.md#optimization
[220]: #benchmarksh
[221]: https://cgit.freebsd.org/src/tree/usr.bin/ministat/ministat.c
[222]: https://www.freebsd.org/cgi/man.cgi?query=ministat&apropos=0&sektion=0&manpath=FreeBSD+13.0-RELEASE+and+Ports&arch=default&format=html
[223]: #ministatc
[224]: #dc
[225]: #allsh
[226]: #errorssh
[227]: #errorsh
