<!---

SPDX-License-Identifier: BSD-2-Clause

Copyright (c) 2018-2020 Gavin D. Howard and contributors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

-->

# NAME

bc - arbitrary-precision decimal arithmetic language and calculator

# SYNOPSIS

**bc** [**-ghilPqsvVw**] [**--global-stacks**] [**--help**] [**--interactive**] [**--mathlib**] [**--no-prompt**] [**--quiet**] [**--standard**] [**--warn**] [**--version**] [**-e** *expr*] [**--expression**=*expr*...] [**-f** *file*...] [**-file**=*file*...]
[*file*...]

# DESCRIPTION

bc(1) is an interactive processor for a language first standardized in 1991 by
POSIX. (The current standard is [here][1].) The language provides unlimited
precision decimal arithmetic and is somewhat C-like, but there are differences.
Such differences will be noted in this document.

After parsing and handling options, this bc(1) reads any files given on the
command line and executes them before reading from **stdin**.

# OPTIONS

The following are the options that bc(1) accepts.

**-g**, **--global-stacks**

    Turns the globals **ibase**, **obase**, and **scale** into stacks.

    This has the effect that a copy of the current value of all three are pushed
    onto a stack for every function call, as well as popped when every function
    returns. This means that functions can assign to any and all of those
    globals without worrying that the change will affect other functions.
    Thus, a hypothetical function named **output(x,b)** that simply printed
    **x** in base **b** could be written like this:

        define void output(x, b) {
            obase=b
            x
        }

    instead of like this:

        define void output(x, b) {
            auto c
            c=obase
            obase=b
            x
            obase=c
        }

    This makes writing functions much easier.

    However, since using this flag means that functions cannot set **ibase**,
    **obase**, or **scale** globally, functions that are made to do so cannot
    work anymore. There are two possible use cases for that, and each has a
    solution.

    First, if a function is called on startup to turn bc(1) into a number
    converter, it is possible to replace that capability with various shell
    aliases. Examples:

        alias d2o="bc -e ibase=A -e obase=8"
        alias h2b="bc -e ibase=G -e obase=2"

    Second, if the purpose of a function is to set **ibase**, **obase**, or
    **scale** globally for any other purpose, it could be split into one to
    three functions (based on how many globals it sets) and each of those
    functions could return the desired value for a global.

    If the behavior of this option is desired for every run of bc(1), then users
    could make sure to define **BC_ENV_ARGS** and include this option (see the
    **ENVIRONMENT VARIABLES** section for more details).

    If **-s**, **-w**, or any equivalents are used, this option is ignored.

    This is a **non-portable extension**.

**-h**, **--help**

:   Prints a usage message and quits.

**-i**, **--interactive**

:   Forces interactive mode. (See the **INTERACTIVE MODE** section.)

    This is a **non-portable extension**.

**-l**, **--mathlib**

:   Sets **scale** (see the **SYNTAX** section) to **20** and loads the included
    math library before running any code, including any expressions or files
    specified on the command line.

    To learn what is in the library, see the **LIBRARY** section.

**-P**, **--no-prompt**

:   Disables the prompt in TTY mode. (The prompt is only enabled in TTY mode.
    See the **TTY MODE** section) This is mostly for those users that do not
    want a prompt or are not used to having them in bc(1). Most of those users
    would want to put this option in **BC_ENV_ARGS** (see the
    **ENVIRONMENT VARIABLES** section).

    This is a **non-portable extension**.

**-q**, **--quiet**

:   This option is for compatibility with the [GNU bc(1)][2]; it is a no-op.
    Without this option, GNU bc(1) prints a copyright header. This bc(1) only
    prints the copyright header if one or more of the **-v**, **-V**, or
    **--version** options are given.

    This is a **non-portable extension**.

**-s**, **--standard**

:   Process exactly the language defined by the [standard][1] and error if any
    extensions are used.

    This is a **non-portable extension**.

**-v**, **-V**, **--version**

:   Print the version information (copyright header) and exit.

    This is a **non-portable extension**.

**-w**, **--warn**

:   Like **-s** and **--standard**, except that warnings (and not errors) are
    printed for non-standard extensions and execution continues normally.

    This is a **non-portable extension**.

**-e** *expr*, **--expression**=*expr*

:   Evaluates *expr*. If multiple expressions are given, they are evaluated in
    order. If files are given as well (see below), the expressions and files are
    evaluated in the order given. This means that if a file is given before an
    expression, the file is read in and evaluated first.

    After processing all expressions and files, bc(1) will exit, unless **-**
    (**stdin**) was given as an argument at least once to **-f** or **--file**.
    However, if any other **-e**, **--expression**, **-f**, or **--file**
    arguments are given after that, bc(1) will give a fatal error and exit.

    This is a **non-portable extension**.

**-f** *file*, **--file**=*file*

:   Reads in *file* and evaluates it, line by line, as though it were read
    through **stdin**. If expressions are also given (see above), the
    expressions are evaluated in the order given.

    After processing all expressions and files, bc(1) will exit, unless **-**
    (**stdin**) was given as an argument at least once to **-f** or **--file**.

    This is a **non-portable extension**.

All long options are **non-portable extensions**.

# STDOUT

Any non-error output is written to **stdout**.

**Note**: Unlike other bc(1) implementations, this bc(1) will issue a fatal
error (see the **EXIT STATUS** section) if it cannot write to **stdout**, so if
**stdout** is closed, as in **bc <file> >&-**, it will quit with an error. This
is done so that bc(1) can report problems when **stdout** is redirected to a
file.

If there are scripts that depend on the behavior of other bc(1) implementations,
it is recommended that those scripts be changed to redirect **stdout** to
**/dev/null**.

# STDERR

Any error output is written to **stderr**.

**Note**: Unlike other bc(1) implementations, this bc(1) will issue a fatal
error (see the **EXIT STATUS** section) if it cannot write to **stderr**, so if
**stderr** is closed, as in **bc <file> 2>&-**, it will quit with an error. This
is done so that bc(1) can exit with an error code when **stderr** is redirected
to a file.

If there are scripts that depend on the behavior of other bc(1) implementations,
it is recommended that those scripts be changed to redirect **stderr** to
**/dev/null**.

# SYNTAX

The syntax for bc(1) programs is mostly C-like, with some differences. This
bc(1) follows the [POSIX standard][1], which is a much more thorough resource
for the language this bc(1) accepts. This section is meant to be a summary and a
listing of all the extensions to the standard.

In the sections below, **E** means expression, **S** means statement, and **I**
means identifier.

Identifiers (**I**) start with a lowercase letter and can be followed by any
number (up to **BC_NAME_MAX-1**) of lowercase letters (**a-z**), digits
(**0-9**), and underscores (**\_**). The regex is **\[a-z\]\[a-z0-9\_\]\***.
Identifiers with more than one character (letter) are a
**non-portable extension**.

**ibase** is a global variable determining how to interpret constant numbers. It
is the "input" base, or the number base used for interpreting input numbers.
**ibase** is initially **10**. If the **-s** (**--standard**) and **-w**
(**--warn**) flags were not given on the command line, the max allowable value
for **ibase** is **36**. Otherwise, it is **16**. The min allowable value for
**ibase** is **2**. The max allowable value for **ibase** can be queried in
bc(1) programs with the **maxibase()** built-in function.

**obase** is a global variable determining how to output results. It is the
"output" base, or the number base used for outputting numbers. **obase** is
initially **10**. The max allowable value for **obase** is **BC_BASE_MAX** and
can be queried in bc(1) programs with the **maxobase()** built-in function. The
min allowable value for **obase** is **2**. Values are output in the specified
base.

The *scale* of an expression is the number of digits in the result of the
expression right of the decimal point, and **scale** is a global variable that
sets the precision of any operations, with exceptions. **scale** is initially
**0**. **scale** cannot be negative. The max allowable value for **scale** is
**BC_SCALE_MAX** and can be queried in bc(1) programs with the **maxscale()**
built-in function.

bc(1) has both *global* variables and *local* variables. All *local*
variables are local to the function; they are parameters or are introduced in
the **auto** list of a function (see the **FUNCTIONS** section). If a variable
is accessed which is not a parameter or in the **auto** list, it is assumed to
be *global*. If a parent function has a *local* variable version of a variable
that a child function considers *global*, the value of that *global* variable in
the child function is the value of the variable in the parent function, not the
value of the actual *global* variable.

All of the above applies to arrays as well.

The value of a statement that is an expression (i.e., any of the named
expressions or operands) is printed unless the lowest precedence operator is an
assignment operator *and* the expression is notsurrounded by parentheses.

The value that is printed is also assigned to the special variable **last**. A
single dot (**.**) may also be used as a synonym for **last**. These are
**non-portable extensions**.

Either semicolons or newlines may separate statements.

## Comments

There are two kinds of comments:

1.	Block comments are enclosed in **/\*** and **\*/**.
2.	Line comments go from **#** until, and not including, the next newline. This
	is a **non-portable extension**.

## Named Expressions

The following are named expressions in bc(1):

1.	Variables: **I**
2.	Array Elements: **I[E]**
3.	**ibase**
4.	**obase**
5.	**scale**
6.	**last** or a single dot (**.**)

Number 6 is a **non-portable extension**.

Variables and arrays do not interfere; users can have arrays named the same as
variables. This also applies to functions (see the **FUNCTIONS** section), so a
user can have a variable, array, and function that all have the same name, and
they will not shadow each other, whether inside of functions or not.

Named expressions are required as the operand of **increment**/**decrement**
operators  and as the left side of **assignment** operators (see the *Operators*
subsection).

## Operands

The following are valid operands in bc(1):

1.	Numbers (see the *Numbers* subsection below).
2.	Array indices (**I[E]**).
3.	**(E)**: The value of **E** (used to change precedence).
4.	**sqrt(E)**: The square root of **E**. **E** must be non-negative.
5.	**length(E)**: The number of significant decimal digits in **E**.
6.	**length(I[])**: The number of elements in the array **I**. This is a
	**non-portable extension**.
7.	**scale(E)**: The *scale* of **E**.
8.	**abs(E)**: The absolute value of **E**. This is a **non-portable
	extension**.
9.	**I()**, **I(E)**, **I(E, E)**, and so on, where **I** is an identifier for
	a non-**void** function (see the *Void Functions* subsection of the
	**FUNCTIONS** section). The **E** argument(s) may also be arrays of the form
	**I[]**, which will automatically be turned into array references (see the
	*Array References* subsection of the **FUNCTIONS** section) if the
	corresponding parameter in the function definition is an array reference.
10.	**read()**: Reads a line from **stdin** and uses that as an expression. The
	result of that expression is the result of the **read()** operand. This is a
	**non-portable extension**.
11.	**maxibase()**: The max allowable **ibase**. This is a **non-portable
	extension**.
12.	**maxobase()**: The max allowable **obase**. This is a **non-portable
	extension**.
13.	**maxscale()**: The max allowable **scale**. This is a **non-portable
	extension**.

## Numbers

Numbers are strings made up of digits, uppercase letters, and at most **1**
period for a radix. Numbers can have up to **BC_NUM_MAX** digits. Uppercase
letters are equal to **9** + their position in the alphabet (i.e., **A** equals
**10**, or **9+1**). If a digit or letter makes no sense with the current value
of **ibase**, they are set to the value of the highest valid digit in **ibase**.

Single-character numbers (i.e., **A** alone) take the value that they would have
if they were valid digits, regardless of the value of **ibase**. This means that
**A** alone always equals decimal **10** and **Z** alone always equals decimal
**35**.

## Operators

The following arithmetic and logical operators can be used. They are listed in
order of decreasing precedence. Operators in the same group have the same
precedence.

**++** **--**

:   Type: Prefix and Postfix

    Associativity: None

    Description: **increment**, **decrement**

**-** **!**

:   Type: Prefix

    Associativity: None

    Description: **negation**, **boolean not**

**\^**

:   Type: Binary

    Associativity: Right

    Description: **power**

**\*** **/** **%**

:   Type: Binary

    Associativity: Left

    Description: **multiply**, **divide**, **modulus**

**+** **-**

:   Type: Binary

    Associativity: Left

    Description: **add**, **subtract**

**=** **+=** **-=** **\*=** **/=** **%=** **\^=**

:   Type: Binary

    Associativity: Right

    Description: **assignment**

**==** **\<=** **\>=** **!=** **\<** **\>**

:   Type: Binary

    Associativity: Left

    Description: **relational**

**&&**

:   Type: Binary

    Associativity: Left

    Description: **boolean and**

**||**

:   Type: Binary

    Associativity: Left

    Description: **boolean or**

The operators will be described in more detail below.

**++** **--**

:   The prefix and postfix **increment** and **decrement** operators behave
    exactly like they would in C. They require a named expression (see the
    *Named Expressions* subsection) as an operand.

    The prefix versions of these operators are more efficient; use them where
    possible.

**-**

:   The **negation** operator returns **0** if a user attempts to negate any
    expression with the value **0**. Otherwise, a copy of the expression with
    its sign flipped is returned.

**!**

:   The **boolean not** operator returns **1** if the expression is **0**, or
    **0** otherwise.

    This is a **non-portable extension**.

**\^**

:   The **power** operator (not the **exclusive or** operator, as it would be in
    C) takes two expressions and raises the first to the power of the value of
    the second. The *scale* of the result is equal to **scale**.

    The second expression must be an integer (no *scale*), and if it is
    negative, the first value must be non-zero.

**\***

:   The **multiply** operator takes two expressions, multiplies them, and
    returns the product. If **a** is the *scale* of the first expression and
    **b** is the *scale* of the second expression, the *scale* of the result is
    equal to **min(a+b,max(scale,a,b))** where **min()** and **max()** return
    the obvious values.

**/**

:   The **divide** operator takes two expressions, divides them, and returns the
    quotient. The *scale* of the result shall be the value of **scale**.

    The second expression must be non-zero.

**%**

:   The **modulus** operator takes two expressions, **a** and **b**, and
    evaluates them by 1) Computing **a/b** to current **scale** and 2) Using the
    result of step 1 to calculate **a-(a/b)\*b** to *scale*
    **max(scale+scale(b),scale(a))**.

    The second expression must be non-zero.

**+**

:   The **add** operator takes two expressions, **a** and **b**, and returns the
    sum, with a *scale* equal to the max of the *scale*s of **a** and **b**.

**-**

:   The **subtract** operator takes two expressions, **a** and **b**, and
    returns the difference, with a *scale* equal to the max of the *scale*s of
    **a** and **b**.

**=** **+=** **-=** **\*=** **/=** **%=** **\^=**

:   The **assignment** operators take two expressions, **a** and **b** where
    **a** is a named expression (see the *Named Expressions* subsection).

    For **=**, **b** is copied and the result is assigned to **a**. For all
    others, **a** and **b** are applied as operands to the corresponding
    arithmetic operator and the result is assigned to **a**.

**==** **\<=** **\>=** **!=** **\<** **\>**

:   The **relational** operators compare two expressions, **a** and **b**, and
    if the relation holds, according to C language semantics, the result is
    **1**. Otherwise, it is **0**.

    Note that unlike in C, these operators have a lower precedence than the
    **assignment** operators, which means that **a=b\>c** is interpreted as
    **(a=b)\>c**.

    Also, unlike the [standard][1] requires, these operators can appear anywhere
    any other expressions can be used. This allowance is a
    **non-portable extension**.

**&&**

:   The **boolean and** operator takes two expressions and returns **1** if both
    expressions are non-zero, **0** otherwise.

    This is *not* a short-circuit operator.

    This is a **non-portable extension**.

**||**

:   The **boolean or** operator takes two expressions and returns **1** if one
    of the expressions is non-zero, **0** otherwise.

    This is *not* a short-circuit operator.

    This is a **non-portable extension**.

## Statements

The following items are statements:

1.	**E**
2.	**{** **S** **;** ... **;** **S** **}**
3.	**if** **(** **E** **)** **S**
4.	**if** **(** **E** **)** **S** **else** **S**
5.	**while** **(** **E** **)** **S**
6.	**for** **(** **E** **;** **E** **;** **E** **)** **S**
7.	An empty statement
8.	**break**
9.	**continue**
10.	**quit**
11.	**halt**
12.	**limits**
13.	A string of characters, enclosed in double quotes
14.	**print** **E** **,** ... **,** **E**
15.	**I()**, **I(E)**, **I(E, E)**, and so on, where **I** is an identifier for
	a **void** function (see the *Void Functions* subsection of the
	**FUNCTIONS** section). The **E** argument(s) may also be arrays of the form
	**I[]**, which will automatically be turned into array references (see the
	*Array References* subsection of the **FUNCTIONS** section) if the
	corresponding parameter in the function definition is an array reference.

Numbers 4, 9, 11, 12, 14, and 15 are **non-portable extensions**.

Also, as a **non-portable extension**, any or all of the expressions in the
header of a for loop may be omitted. If the condition (second expression) is
omitted, it is assumed to be a constant **1**.

The **break** statement causes a loop to stop iterating and resume execution
immediately following a loop. This is only allowed in loops.

The **continue** statement causes a loop iteration to stop early and returns to
the start of the loop, including testing the loop condition. This is only
allowed in loops.

The **if** **else** statement does the same thing as in C.

The **quit** statement causes bc(1) to quit, even if it is on a branch that will
not be executed (it is a compile-time command).

The **halt** statement causes bc(1) to quit, if it is executed. (Unlike **quit**
if it is on a branch of an **if** statement that is not executed, bc(1) does not
quit.)

The **limits** statement prints the limits that this bc(1) is subject to. This
is like the **quit** statement in that it is a compile-time command.

An expression by itself is evaluated and printed, followed by a newline.

## Print Statement

The "expressions" in a **print** statement may also be strings. If they are, there
are backslash escape sequences that are interpreted specially. What those
sequences are, and what they cause to be printed, are shown below:

-------- -------
**\\a**  **\\a**
**\\b**  **\\b**
**\\\\** **\\**
**\\e**  **\\**
**\\f**  **\\f**
**\\n**  **\\n**
**\\q**  **"**
**\\r**  **\\r**
**\\t**  **\\t**
-------- -------

Any other character following a backslash causes the backslash and character to
be printed as-is.

Any non-string expression in a print statement shall be assigned to **last**,
like any other expression that is printed.

## Order of Evaluation

All expressions in a statment are evaluated left to right, except as necessary
to maintain order of operations. This means, for example, assuming that **i** is
equal to **0**, in the expression

    a[i++] = i++

the first (or 0th) element of **a** is set to **1**, and **i** is equal to **2**
at the end of the expression.

This includes function arguments. Thus, assuming **i** is equal to **0**, this
means that in the expression

    x(i++, i++)

the first argument passed to **x()** is **0**, and the second argument is **1**,
while **i** is equal to **2** before the function starts executing.

# FUNCTIONS

Function definitions are as follows:

```
define I(I,...,I){
	auto I,...,I
	S;...;S
	return(E)
}
```

Any **I** in the parameter list or **auto** list may be replaced with **I[]** to
make a parameter or **auto** var an array, and any **I** in the parameter list
may be replaced with **\*I[]** to make a parameter an array reference. Callers
of functions that take array references should not put an asterisk in the call;
they must be called with just **I[]** like normal array parameters and will be
automatically converted into references.

As a **non-portable extension**, the opening brace of a **define** statement may
appear on the next line.

As a **non-portable extension**, the return statement may also be in one of the
following forms:

1.	**return**
2.	**return** **(** **)**
3.	**return** **E**

The first two, or not specifying a **return** statement, is equivalent to
**return (0)**, unless the function is a **void** function (see the *Void
Functions* subsection below).

## Void Functions

Functions can also be **void** functions, defined as follows:

```
define void I(I,...,I){
	auto I,...,I
	S;...;S
	return
}
```

They can only be used as standalone expressions, where such an expression would
be printed alone, except in a print statement.

Void functions can only use the first two **return** statements listed above.
They can also omit the return statement entirely.

The word "void" is not treated as a keyword; it is still possible to have
variables, arrays, and functions named **void**. The word "void" is only
treated specially right after the **define** keyword.

This is a **non-portable extension**.

## Array References

For any array in the parameter list, if the array is declared in the form

```
*I[]
```

it is a **reference**. Any changes to the array in the function are reflected,
when the function returns, to the array that was passed in.

Other than this, all function arguments are passed by value.

This is a **non-portable extension**.

# LIBRARY

All of the functions below  are available when the **-l** or **--mathlib**
command-line flags are given.

## Standard Library

The [standard][1] defines the following functions for the math library:

**s(x)**

:   Returns the sine of **x**, which is assumed to be in radians.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

**c(x)**

:   Returns the cosine of **x**, which is assumed to be in radians.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

**a(x)**

:   Returns the arctangent of **x**, in radians.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

**l(x)**

:   Returns the natural logarithm of **x**.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

**e(x)**

:   Returns the mathematical constant **e** raised to the power of **x**.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

**j(x, n)**

:   Returns the bessel integer order **n** (truncated) of **x**.

    This is a transcendental function (see the *Transcendental Functions*
    subsection below).

## Transcendental Functions

All transcendental functions can return slightly inaccurate results (up to 1
[ULP][4]). This is unavoidable, and [this article][5] explains why it is
impossible and unnecessary to calculate exact results for the transcendental
functions.

Because of the possible inaccuracy, I recommend that users call those functions
with the precision (**scale**) set to at least 1 higher than is necessary. If
exact results are *absolutely* required, users can double the precision
(**scale**) and then truncate.

The transcendental functions in the standard math library are:

* **s(x)**
* **c(x)**
* **a(x)**
* **l(x)**
* **e(x)**
* **j(x, n)**

# RESET

When bc(1) encounters an error or a signal that it has a non-default handler
for, it resets. This means that several things happen.

First, any functions that are executing are stopped and popped off the stack.
The behavior is not unlike that of exceptions in programming languages. Then
the execution point is set so that any code waiting to execute (after all
functions returned) is skipped.

Thus, when bc(1) resets, it skips any remaining code waiting to be executed.
Then, if it is interactive mode, and the error was not a fatal error (see the
**EXIT STATUS** section), it asks for more input; otherwise, it exits with the
appropriate return code.

Note that this reset behavior is different from the GNU bc(1), which attempts to
start executing the statement right after the one that caused an error.

# PERFORMANCE

Most bc(1) implementations use **char** types to calculate the value of **1**
decimal digit at a time, but that can be slow. This bc(1) does something
different.

It uses large integers to calculate more than **1** decimal digit at a time. If
built in a environment where **BC_LONG_BIT** (see the **LIMITS** section) is
**64**, then each integer has **9** decimal digits. If built in an environment
where **BC_LONG_BIT** is **32** then each integer has **4** decimal digits. This
value (the number of decimal digits per large integer) is called
**BC_BASE_DIGS**.

The actual values of **BC_LONG_BIT** and **BC_BASE_DIGS** can be queried with
the **limits** statement.

In addition, this bc(1) uses an even larger integer for overflow checking. This
integer type depends on the value of **BC_LONG_BIT**, but is always at least
twice as large as the integer type used to store digits.

# LIMITS

The following are the limits on bc(1):

**BC_LONG_BIT**

:   The number of bits in the **long** type in the environment where bc(1) was
    built. This determines how many decimal digits can be stored in a single
    large integer (see the **PERFORMANCE** section).

**BC_BASE_DIGS**

:   The number of decimal digits per large integer (see the **PERFORMANCE**
    section). Depends on **BC_LONG_BIT**.

**BC_BASE_POW**

:   The max decimal number that each large integer can store (see
    **BC_BASE_DIGS**) plus **1**. Depends on **BC_BASE_DIGS**.

**BC_OVERFLOW_MAX**

:   The max number that the overflow type (see the **PERFORMANCE** section) can
    hold. Depends on **BC_LONG_BIT**.

**BC_BASE_MAX**

:   The maximum output base. Set at **BC_BASE_POW**.

**BC_DIM_MAX**

:   The maximum size of arrays. Set at **SIZE_MAX-1**.

**BC_SCALE_MAX**

:   The maximum **scale**. Set at **BC_OVERFLOW_MAX-1**.

**BC_STRING_MAX**

:   The maximum length of strings. Set at **BC_OVERFLOW_MAX-1**.

**BC_NAME_MAX**

:   The maximum length of identifiers. Set at **BC_OVERFLOW_MAX-1**.

**BC_NUM_MAX**

:   The maximum length of a number (in decimal digits), which includes digits
    after the decimal point. Set at **BC_OVERFLOW_MAX-1**.

Exponent

:   The maximum allowable exponent (positive or negative). Set at
    **BC_OVERFLOW_MAX**.

Number of vars

:   The maximum number of vars/arrays. Set at **SIZE_MAX-1**.

The actual values can be queried with the **limits** statement.

These limits are meant to be effectively non-existent; the limits are so large
(at least on 64-bit machines) that there should not be any point at which they
become a problem. In fact, memory should be exhausted before these limits should
be hit.

# ENVIRONMENT VARIABLES

bc(1) recognizes the following environment variables:

**POSIXLY_CORRECT**

:   If this variable exists (no matter the contents), bc(1) behaves as if
    the **-s** option was given.

**BC_ENV_ARGS**

:   This is another way to give command-line arguments to bc(1). They should be
    in the same format as all other command-line arguments. These are always
    processed first, so any files given in **BC_ENV_ARGS** will be processed
    before arguments and files given on the command-line. This gives the user
    the ability to set up "standard" options and files to be used at every
    invocation. The most useful thing for such files to contain would be useful
    functions that the user might want every time bc(1) runs.

    The code that parses **BC_ENV_ARGS** will correctly handle quoted arguments,
    but it does not understand escape sequences. For example, the string
    **"/home/gavin/some bc file.bc"** will be correctly parsed, but the string
    **"/home/gavin/some \"bc\" file.bc"** will include the backslashes.

    The quote parsing will handle either kind of quotes, **'** or **"**. Thus,
    if you have a file with any number of single quotes in the name, you can use
    double quotes as the outside quotes, as in **"some 'bc' file.bc"**, and vice
    versa if you have a file with double quotes. However, handling a file with
    both kinds of quotes in **BC_ENV_ARGS** is not supported due to the
    complexity of the parsing, though such files are still supported on the
    command-line where the parsing is done by the shell.

**BC_LINE_LENGTH**

:   If this environment variable exists and contains an integer that is greater
    than **1** and is less than **UINT16_MAX** (**2\^16-1**), bc(1) will output
    lines to that length, including the backslash (**\\**). The default line
    length is **70**.

# EXIT STATUS

bc(1) returns the following exit statuses:

**0**

:   No error.

**1**

:   A math error occurred. This follows standard practice of using **1** for
    expected errors, since math errors will happen in the process of normal
    execution.

    Math errors include divide by **0**, taking the square root of a negative
    number, attempting to convert a negative number to a hardware integer,
    overflow when converting a number to a hardware integer, and attempting to
    use a non-integer where an integer is required.

    Converting to a hardware integer happens for the second operand of the power
    (**\^**) operator and the corresponding assignment operator.

**2**

:   A parse error occurred.

    Parse errors include unexpected **EOF**, using an invalid character, failing
    to find the end of a string or comment, using a token where it is invalid,
    giving an invalid expression, giving an invalid print statement, giving an
    invalid function definition, attempting to assign to an expression that is
    not a named expression (see the *Named Expressions* subsection of the
    **SYNTAX** section), giving an invalid **auto** list, having a duplicate
    **auto**/function parameter, failing to find the end of a code block,
    attempting to return a value from a **void** function, attempting to use a
    variable as a reference, and using any extensions when the option **-s** or
    any equivalents were given.

**3**

:   A runtime error occurred.

    Runtime errors include assigning an invalid number to **ibase**, **obase**,
    or **scale**; give a bad expression to a **read()** call, calling **read()**
    inside of a **read()** call, type errors, passing the wrong number of
    arguments to functions, attempting to call an undefined function, and
    attempting to use a **void** function call as a value in an expression.

**4**

:   A fatal error occurred.

    Fatal errors include memory allocation errors, I/O errors, failing to open
    files, attempting to use files that do not have only ASCII characters (bc(1)
    only accepts ASCII characters), attempting to open a directory as a file,
    and giving invalid command-line options.

The exit status **4** is special; when a fatal error occurs, bc(1) always exits
and returns **4**, no matter what mode bc(1) is in.

The other statuses will only be returned when bc(1) is not in interactive mode
(see the **INTERACTIVE MODE** section), since bc(1) resets its state (see the
**RESET** section) and accepts more input when one of those errors occurs in
interactive mode. This is also the case when interactive mode is forced by the
**-i** flag or **--interactive** option.

These exit statuses allow bc(1) to be used in shell scripting with error
checking, and its normal behavior can be forced by using the **-i** flag or
**--interactive** option.

# INTERACTIVE MODE

Per the [standard][1], bc(1) has an interactive mode and a non-interactive mode.
Interactive mode is turned on automatically when both **stdin** and **stdout**
are hooked to a terminal, but the **-i** flag and **--interactive** option can
turn it on in other cases.

In interactive mode, bc(1) attempts to recover from errors (see the **RESET**
section), and in normal execution, flushes **stdout** as soon as execution is
done for the current input.

# TTY MODE

If **stdin**, **stdout**, and **stderr** are all connected to a TTY, bc(1) turns
on "TTY mode."

The prompt is enabled in TTY mode.

TTY mode is different from interactive mode because interactive mode is required
in the [bc(1) specification][1], and interactive mode requires only **stdin**
and **stdout** to be connected to a terminal.

# SIGNAL HANDLING

Sending a **SIGINT** will cause bc(1) to stop execution of the current input. If
bc(1) is in TTY mode (see the **TTY MODE** section), it will reset (see the
**RESET** section). Otherwise, it will clean up and exit.

Note that "current input" can mean one of two things. If bc(1) is processing
input from **stdin** in TTY mode, it will ask for more input. If bc(1) is
processing input from a file in TTY mode, it will stop processing the file and
start processing the next file, if one exists, or ask for input from **stdin**
if no other file exists.

This means that if a **SIGINT** is sent to bc(1) as it is executing a file, it
can seem as though bc(1) did not respond to the signal since it will immediately
start executing the next file. This is by design; most files that users execute
when interacting with bc(1) have function definitions, which are quick to parse.
If a file takes a long time to execute, there may be a bug in that file. The
rest of the files could still be executed without problem, allowing the user to
continue.

**SIGTERM** and **SIGQUIT** cause bc(1) to clean up and exit, and it uses the
default handler for all other signals.

# SEE ALSO

dc(1)

# STANDARDS

bc(1) is compliant with the [IEEE Std 1003.1-2017 (“POSIX.1-2017”)][1]
specification. The flags **-efghiqsvVw**, all long options, and the extensions
noted above are extensions to that specification.

Note that the specification explicitly says that bc(1) only accepts numbers that
use a period (**.**) as a radix point, regardless of the value of
**LC_NUMERIC**.

# BUGS

None are known. Report bugs at https://git.yzena.com/gavin/bc.

# AUTHORS

Gavin D. Howard <gavin@yzena.com> and contributors.

[1]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html
[2]: https://www.gnu.org/software/bc/
[3]: https://en.wikipedia.org/wiki/Rounding#Round_half_away_from_zero
[4]: https://en.wikipedia.org/wiki/Unit_in_the_last_place
[5]: https://people.eecs.berkeley.edu/~wkahan/LOG10HAF.TXT
[6]: https://en.wikipedia.org/wiki/Rounding#Rounding_away_from_zero
