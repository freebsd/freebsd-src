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

# Name

dc - arbitrary-precision reverse-Polish notation calculator

# SYNOPSIS

**dc** [**-hiPvVx**] [**--version**] [**--help**] [**--interactive**] [**--no-prompt**] [**--extended-register**] [**-e** *expr*] [**--expression**=*expr*...] [**-f** *file*...] [**-file**=*file*...] [*file*...]

# DESCRIPTION

dc(1) is an arbitrary-precision calculator. It uses a stack (reverse Polish
notation) to store numbers and results of computations. Arithmetic operations
pop arguments off of the stack and push the results.

If no files are given on the command-line as extra arguments (i.e., not as
**-f** or **--file** arguments), then dc(1) reads from **stdin**. Otherwise,
those files are processed, and dc(1) will then exit.

This is different from the dc(1) on OpenBSD and possibly other dc(1)
implementations, where **-e** (**--expression**) and **-f** (**--file**)
arguments cause dc(1) to execute them and exit. The reason for this is that this
dc(1) allows users to set arguments in the environment variable **DC_ENV_ARGS**
(see the **ENVIRONMENT VARIABLES** section). Any expressions given on the
command-line should be used to set up a standard environment. For example, if a
user wants the **scale** always set to **10**, they can set **DC_ENV_ARGS** to
**-e 10k**, and this dc(1) will always start with a **scale** of **10**.

If users want to have dc(1) exit after processing all input from **-e** and
**-f** arguments (and their equivalents), then they can just simply add **-e q**
as the last command-line argument or define the environment variable
**DC_EXPR_EXIT**.

# OPTIONS

The following are the options that dc(1) accepts.

**-h**, **--help**

:   Prints a usage message and quits.

**-v**, **-V**, **--version**

:   Print the version information (copyright header) and exit.

**-i**, **--interactive**

:   Forces interactive mode. (See the **INTERACTIVE MODE** section.)

    This is a **non-portable extension**.

**-P**, **--no-prompt**

:   Disables the prompt in TTY mode. (The prompt is only enabled in TTY mode.
    See the **TTY MODE** section) This is mostly for those users that do not
    want a prompt or are not used to having them in dc(1). Most of those users
    would want to put this option in **DC_ENV_ARGS**.

    This is a **non-portable extension**.

**-x** **--extended-register**

:   Enables extended register mode. See the *Extended Register Mode* subsection
    of the **REGISTERS** section for more information.

    This is a **non-portable extension**.

**-e** *expr*, **--expression**=*expr*

:   Evaluates *expr*. If multiple expressions are given, they are evaluated in
    order. If files are given as well (see below), the expressions and files are
    evaluated in the order given. This means that if a file is given before an
    expression, the file is read in and evaluated first.

    In other dc(1) implementations, this option causes the program to execute
    the expressions and then exit. This dc(1) does not, unless the
    **DC_EXPR_EXIT** is defined (see the **ENVIRONMENT VARIABLES** section).

    This is a **non-portable extension**.

**-f** *file*, **--file**=*file*

:   Reads in *file* and evaluates it, line by line, as though it were read
    through **stdin**. If expressions are also given (see above), the
    expressions are evaluated in the order given.

    In other dc(1) implementations, this option causes the program to execute
    the files and then exit. This dc(1) does not, unless the
    **DC_EXPR_EXIT** is defined (see the **ENVIRONMENT VARIABLES** section).

    This is a **non-portable extension**.

All long options are **non-portable extensions**.

# STDOUT

Any non-error output is written to **stdout**.

**Note**: Unlike other dc(1) implementations, this dc(1) will issue a fatal
error (see the **EXIT STATUS** section) if it cannot write to **stdout**, so if
**stdout** is closed, as in **dc <file> >&-**, it will quit with an error. This
is done so that dc(1) can report problems when **stdout** is redirected to a
file.

If there are scripts that depend on the behavior of other dc(1) implementations,
it is recommended that those scripts be changed to redirect **stdout** to
**/dev/null**.

# STDERR

Any error output is written to **stderr**.

**Note**: Unlike other dc(1) implementations, this dc(1) will issue a fatal
error (see the **EXIT STATUS** section) if it cannot write to **stderr**, so if
**stderr** is closed, as in **dc <file> 2>&-**, it will quit with an error. This
is done so that dc(1) can exit with an error code when **stderr** is redirected
to a file.

If there are scripts that depend on the behavior of other dc(1) implementations,
it is recommended that those scripts be changed to redirect **stderr** to
**/dev/null**.

# SYNTAX

Each item in the input source code, either a number (see the **NUMBERS**
section) or a command (see the **COMMANDS** section), is processed and executed,
in order. Input is processed immediately when entered.

**ibase** is a register (see the **REGISTERS** section) that determines how to
interpret constant numbers. It is the "input" base, or the number base used for
interpreting input numbers. **ibase** is initially **10**. The max allowable
value for **ibase** is **16**. The min allowable value for **ibase** is **2**.
The max allowable value for **ibase** can be queried in dc(1) programs with the
**T** command.

**obase** is a register (see the **REGISTERS** section) that determines how to
output results. It is the "output" base, or the number base used for outputting
numbers. **obase** is initially **10**. The max allowable value for **obase** is
**DC_BASE_MAX** and can be queried with the **U** command. The min allowable
value for **obase** is **2**. Values are output in the specified base.

The *scale* of an expression is the number of digits in the result of the
expression right of the decimal point, and **scale** is a register (see the
**REGISTERS** section) that sets the precision of any operations (with
exceptions). **scale** is initially **0**. **scale** cannot be negative. The max
allowable value for **scale** can be queried in dc(1) programs with the **V**
command.

## Comments

Comments go from **#** until, and not including, the next newline. This is a
**non-portable extension**.

# NUMBERS

Numbers are strings made up of digits, uppercase letters up to **F**, and at
most **1** period for a radix. Numbers can have up to **DC_NUM_MAX** digits.
Uppercase letters are equal to **9** + their position in the alphabet (i.e.,
**A** equals **10**, or **9+1**). If a digit or letter makes no sense with the
current value of **ibase**, they are set to the value of the highest valid digit
in **ibase**.

Single-character numbers (i.e., **A** alone) take the value that they would have
if they were valid digits, regardless of the value of **ibase**. This means that
**A** alone always equals decimal **10** and **F** alone always equals decimal
**15**.

# COMMANDS

The valid commands are listed below.

## Printing

These commands are used for printing.

**p**

:   Prints the value on top of the stack, whether number or string, and prints a
    newline after.

    This does not alter the stack.

**n**

:   Prints the value on top of the stack, whether number or string, and pops it
    off of the stack.

**P**

:   Pops a value off the stack.

    If the value is a number, it is truncated and the absolute value of the
    result is printed as though **obase** is **UCHAR_MAX+1** and each digit is
    interpreted as an ASCII character, making it a byte stream.

    If the value is a string, it is printed without a trailing newline.

    This is a **non-portable extension**.

**f**

:   Prints the entire contents of the stack, in order from newest to oldest,
    without altering anything.

    Users should use this command when they get lost.

## Arithmetic

These are the commands used for arithmetic.

**+**

:   The top two values are popped off the stack, added, and the result is pushed
    onto the stack. The *scale* of the result is equal to the max *scale* of
    both operands.

**-**

:   The top two values are popped off the stack, subtracted, and the result is
    pushed onto the stack. The *scale* of the result is equal to the max
    *scale* of both operands.

**\***

:   The top two values are popped off the stack, multiplied, and the result is
    pushed onto the stack. If **a** is the *scale* of the first expression and
    **b** is the *scale* of the second expression, the *scale* of the result
    is equal to **min(a+b,max(scale,a,b))** where **min()** and **max()** return
    the obvious values.

**/**

:   The top two values are popped off the stack, divided, and the result is
    pushed onto the stack. The *scale* of the result is equal to **scale**.

    The first value popped off of the stack must be non-zero.

**%**

:   The top two values are popped off the stack, remaindered, and the result is
    pushed onto the stack.

    Remaindering is equivalent to 1) Computing **a/b** to current **scale**, and
    2) Using the result of step 1 to calculate **a-(a/b)\*b** to *scale*
    **max(scale+scale(b),scale(a))**.

    The first value popped off of the stack must be non-zero.

**~**

:   The top two values are popped off the stack, divided and remaindered, and
    the results (divided first, remainder second) are pushed onto the stack.
    This is equivalent to **x y / x y %** except that **x** and **y** are only
    evaluated once.

    The first value popped off of the stack must be non-zero.

    This is a **non-portable extension**.

**\^**

:   The top two values are popped off the stack, the second is raised to the
    power of the first, and the result is pushed onto the stack.

    The first value popped off of the stack must be an integer, and if that
    value is negative, the second value popped off of the stack must be
    non-zero.

**v**

:   The top value is popped off the stack, its square root is computed, and the
    result is pushed onto the stack. The *scale* of the result is equal to
    **scale**.

    The value popped off of the stack must be non-negative.

**\_**

:   If this command *immediately* precedes a number (i.e., no spaces or other
    commands), then that number is input as a negative number.

    Otherwise, the top value on the stack is popped and copied, and the copy is
    negated and pushed onto the stack. This behavior without a number is a
    **non-portable extension**.

**b**

:   The top value is popped off the stack, and if it is zero, it is pushed back
    onto the stack. Otherwise, its absolute value is pushed onto the stack.

    This is a **non-portable extension**.

**|**

:   The top three values are popped off the stack, a modular exponentiation is
    computed, and the result is pushed onto the stack.

    The first value popped is used as the reduction modulus and must be an
    integer and non-zero. The second value popped is used as the exponent and
    must be an integer and non-negative. The third value popped is the base and
    must be an integer.

    This is a **non-portable extension**.

**G**

:   The top two values are popped off of the stack, they are compared, and a
    **1** is pushed if they are equal, or **0** otherwise.

    This is a **non-portable extension**.

**N**

:   The top value is popped off of the stack, and if it a **0**, a **1** is
    pushed; otherwise, a **0** is pushed.

    This is a **non-portable extension**.

**(**

:   The top two values are popped off of the stack, they are compared, and a
    **1** is pushed if the first is less than the second, or **0** otherwise.

    This is a **non-portable extension**.

**{**

:   The top two values are popped off of the stack, they are compared, and a
    **1** is pushed if the first is less than or equal to the second, or **0**
    otherwise.

    This is a **non-portable extension**.

**)**

:   The top two values are popped off of the stack, they are compared, and a
    **1** is pushed if the first is greater than the second, or **0** otherwise.

    This is a **non-portable extension**.

**}**

:   The top two values are popped off of the stack, they are compared, and a
    **1** is pushed if the first is greater than or equal to the second, or
    **0** otherwise.

    This is a **non-portable extension**.

**M**

:   The top two values are popped off of the stack. If they are both non-zero, a
    **1** is pushed onto the stack. If either of them is zero, or both of them
    are, then a **0** is pushed onto the stack.

    This is like the **&&** operator in bc(1), and it is *not* a short-circuit
    operator.

    This is a **non-portable extension**.

**m**

:   The top two values are popped off of the stack. If at least one of them is
    non-zero, a **1** is pushed onto the stack. If both of them are zero, then a
    **0** is pushed onto the stack.

    This is like the **||** operator in bc(1), and it is *not* a short-circuit
    operator.

    This is a **non-portable extension**.

## Stack Control

These commands control the stack.

**c**

:   Removes all items from ("clears") the stack.

**d**

:   Copies the item on top of the stack ("duplicates") and pushes the copy onto
    the stack.

**r**

:   Swaps ("reverses") the two top items on the stack.

**R**

:   Pops ("removes") the top value from the stack.

## Register Control

These commands control registers (see the **REGISTERS** section).

**s***r*

:   Pops the value off the top of the stack and stores it into register *r*.

**l***r*

:   Copies the value in register *r* and pushes it onto the stack. This does not
    alter the contents of *r*.

**S***r*

:   Pops the value off the top of the (main) stack and pushes it onto the stack
    of register *r*. The previous value of the register becomes inaccessible.

**L***r*

:   Pops the value off the top of the stack for register *r* and push it onto
    the main stack. The previous value in the stack for register *r*, if any, is
    now accessible via the **l***r* command.

## Parameters

These commands control the values of **ibase**, **obase**, and **scale**. Also
see the **SYNTAX** section.

**i**

:   Pops the value off of the top of the stack and uses it to set **ibase**,
    which must be between **2** and **16**, inclusive.

    If the value on top of the stack has any *scale*, the *scale* is ignored.

**o**

:   Pops the value off of the top of the stack and uses it to set **obase**,
    which must be between **2** and **DC_BASE_MAX**, inclusive (see the
    **LIMITS** section).

    If the value on top of the stack has any *scale*, the *scale* is ignored.

**k**

:   Pops the value off of the top of the stack and uses it to set **scale**,
    which must be non-negative.

    If the value on top of the stack has any *scale*, the *scale* is ignored.

**I**

:   Pushes the current value of **ibase** onto the main stack.

**O**

:   Pushes the current value of **obase** onto the main stack.

**K**

:   Pushes the current value of **scale** onto the main stack.

**T**

:   Pushes the maximum allowable value of **ibase** onto the main stack.

    This is a **non-portable extension**.

**U**

:   Pushes the maximum allowable value of **obase** onto the main stack.

    This is a **non-portable extension**.

**V**

:   Pushes the maximum allowable value of **scale** onto the main stack.

    This is a **non-portable extension**.

## Strings

The following commands control strings.

dc(1) can work with both numbers and strings, and registers (see the
**REGISTERS** section) can hold both strings and numbers. dc(1) always knows
whether the contents of a register are a string or a number.

While arithmetic operations have to have numbers, and will print an error if
given a string, other commands accept strings.

Strings can also be executed as macros. For example, if the string **[1pR]** is
executed as a macro, then the code **1pR** is executed, meaning that the **1**
will be printed with a newline after and then popped from the stack.

**\[**_characters_**\]**

:   Makes a string containing *characters* and pushes it onto the stack.

    If there are brackets (**\[** and **\]**) in the string, then they must be
    balanced. Unbalanced brackets can be escaped using a backslash (**\\**)
    character.

    If there is a backslash character in the string, the character after it
    (even another backslash) is put into the string verbatim, but the (first)
    backslash is not.

**a**

:   The value on top of the stack is popped.

    If it is a number, it is truncated and its absolute value is taken. The
    result mod **UCHAR_MAX+1** is calculated. If that result is **0**, push an
    empty string; otherwise, push a one-character string where the character is
    the result of the mod interpreted as an ASCII character.

    If it is a string, then a new string is made. If the original string is
    empty, the new string is empty. If it is not, then the first character of
    the original string is used to create the new string as a one-character
    string. The new string is then pushed onto the stack.

    This is a **non-portable extension**.

**x**

:   Pops a value off of the top of the stack.

    If it is a number, it is pushed back onto the stack.

    If it is a string, it is executed as a macro.

    This behavior is the norm whenever a macro is executed, whether by this
    command or by the conditional execution commands below.

**\>***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is greater than the second, then the contents of register
    *r* are executed.

    For example, **0 1>a** will execute the contents of register **a**, and
    **1 0>a** will not.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**>***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**!\>***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is not greater than the second (less than or equal to), then
    the contents of register *r* are executed.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**!\>***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**\<***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is less than the second, then the contents of register *r*
    are executed.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**\<***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**!\<***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is not less than the second (greater than or equal to), then
    the contents of register *r* are executed.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**!\<***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**=***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is equal to the second, then the contents of register *r*
    are executed.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**=***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**!=***r*

:   Pops two values off of the stack that must be numbers and compares them. If
    the first value is not equal to the second, then the contents of register
    *r* are executed.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

**!=***r***e***s*

:   Like the above, but will execute register *s* if the comparison fails.

    If either or both of the values are not numbers, dc(1) will raise an error
    and reset (see the **RESET** section).

    This is a **non-portable extension**.

**?**

:   Reads a line from the **stdin** and executes it. This is to allow macros to
    request input from users.

**q**

:   During execution of a macro, this exits the execution of that macro and the
    execution of the macro that executed it. If there are no macros, or only one
    macro executing, dc(1) exits.

**Q**

:   Pops a value from the stack which must be non-negative and is used the
    number of macro executions to pop off of the execution stack. If the number
    of levels to pop is greater than the number of executing macros, dc(1)
    exits.

## Status

These commands query status of the stack or its top value.

**Z**

:   Pops a value off of the stack.

    If it is a number, calculates the number of significant decimal digits it
    has and pushes the result.

    If it is a string, pushes the number of characters the string has.

**X**

:   Pops a value off of the stack.

    If it is a number, pushes the *scale* of the value onto the stack.

    If it is a string, pushes **0**.

**z**

:   Pushes the current stack depth (before execution of this command).

## Arrays

These commands manipulate arrays.

**:***r*

:   Pops the top two values off of the stack. The second value will be stored in
    the array *r* (see the **REGISTERS** section), indexed by the first value.

**;***r*

:   Pops the value on top of the stack and uses it as an index into the array
    *r*. The selected value is then pushed onto the stack.

# REGISTERS

Registers are names that can store strings, numbers, and arrays. (Number/string
registers do not interfere with array registers.)

Each register is also its own stack, so the current register value is the top of
the stack for the register. All registers, when first referenced, have one value
(**0**) in their stack.

In non-extended register mode, a register name is just the single character that
follows any command that needs a register name. The only exception is a newline
(**'\\n'**); it is a parse error for a newline to be used as a register name.

## Extended Register Mode

Unlike most other dc(1) implentations, this dc(1) provides nearly unlimited
amounts of registers, if extended register mode is enabled.

If extended register mode is enabled (**-x** or **--extended-register**
command-line arguments are given), then normal single character registers are
used *unless* the character immediately following a command that needs a
register name is a space (according to **isspace()**) and not a newline
(**'\\n'**).

In that case, the register name is found according to the regex
**\[a-z\]\[a-z0-9\_\]\*** (like bc(1) identifiers), and it is a parse error if
the next non-space characters do not match that regex.

# RESET

When dc(1) encounters an error or a signal that it has a non-default handler
for, it resets. This means that several things happen.

First, any macros that are executing are stopped and popped off the stack.
The behavior is not unlike that of exceptions in programming languages. Then
the execution point is set so that any code waiting to execute (after all
macros returned) is skipped.

Thus, when dc(1) resets, it skips any remaining code waiting to be executed.
Then, if it is interactive mode, and the error was not a fatal error (see the
**EXIT STATUS** section), it asks for more input; otherwise, it exits with the
appropriate return code.

# PERFORMANCE

Most dc(1) implementations use **char** types to calculate the value of **1**
decimal digit at a time, but that can be slow. This dc(1) does something
different.

It uses large integers to calculate more than **1** decimal digit at a time. If
built in a environment where **DC_LONG_BIT** (see the **LIMITS** section) is
**64**, then each integer has **9** decimal digits. If built in an environment
where **DC_LONG_BIT** is **32** then each integer has **4** decimal digits. This
value (the number of decimal digits per large integer) is called
**DC_BASE_DIGS**.

In addition, this dc(1) uses an even larger integer for overflow checking. This
integer type depends on the value of **DC_LONG_BIT**, but is always at least
twice as large as the integer type used to store digits.

# LIMITS

The following are the limits on dc(1):

**DC_LONG_BIT**

:   The number of bits in the **long** type in the environment where dc(1) was
    built. This determines how many decimal digits can be stored in a single
    large integer (see the **PERFORMANCE** section).

**DC_BASE_DIGS**

:   The number of decimal digits per large integer (see the **PERFORMANCE**
    section). Depends on **DC_LONG_BIT**.

**DC_BASE_POW**

:   The max decimal number that each large integer can store (see
    **DC_BASE_DIGS**) plus **1**. Depends on **DC_BASE_DIGS**.

**DC_OVERFLOW_MAX**

:   The max number that the overflow type (see the **PERFORMANCE** section) can
    hold. Depends on **DC_LONG_BIT**.

**DC_BASE_MAX**

:   The maximum output base. Set at **DC_BASE_POW**.

**DC_DIM_MAX**

:   The maximum size of arrays. Set at **SIZE_MAX-1**.

**DC_SCALE_MAX**

:   The maximum **scale**. Set at **DC_OVERFLOW_MAX-1**.

**DC_STRING_MAX**

:   The maximum length of strings. Set at **DC_OVERFLOW_MAX-1**.

**DC_NAME_MAX**

:   The maximum length of identifiers. Set at **DC_OVERFLOW_MAX-1**.

**DC_NUM_MAX**

:   The maximum length of a number (in decimal digits), which includes digits
    after the decimal point. Set at **DC_OVERFLOW_MAX-1**.

Exponent

:   The maximum allowable exponent (positive or negative). Set at
    **DC_OVERFLOW_MAX**.

Number of vars

:   The maximum number of vars/arrays. Set at **SIZE_MAX-1**.

These limits are meant to be effectively non-existent; the limits are so large
(at least on 64-bit machines) that there should not be any point at which they
become a problem. In fact, memory should be exhausted before these limits should
be hit.

# ENVIRONMENT VARIABLES

dc(1) recognizes the following environment variables:

**DC_ENV_ARGS**

:   This is another way to give command-line arguments to dc(1). They should be
    in the same format as all other command-line arguments. These are always
    processed first, so any files given in **DC_ENV_ARGS** will be processed
    before arguments and files given on the command-line. This gives the user
    the ability to set up "standard" options and files to be used at every
    invocation. The most useful thing for such files to contain would be useful
    functions that the user might want every time dc(1) runs. Another use would
    be to use the **-e** option to set **scale** to a value other than **0**.

    The code that parses **DC_ENV_ARGS** will correctly handle quoted arguments,
    but it does not understand escape sequences. For example, the string
    **"/home/gavin/some dc file.dc"** will be correctly parsed, but the string
    **"/home/gavin/some \"dc\" file.dc"** will include the backslashes.

    The quote parsing will handle either kind of quotes, **'** or **"**. Thus,
    if you have a file with any number of single quotes in the name, you can use
    double quotes as the outside quotes, as in **"some 'bc' file.bc"**, and vice
    versa if you have a file with double quotes. However, handling a file with
    both kinds of quotes in **DC_ENV_ARGS** is not supported due to the
    complexity of the parsing, though such files are still supported on the
    command-line where the parsing is done by the shell.

**DC_LINE_LENGTH**

:   If this environment variable exists and contains an integer that is greater
    than **1** and is less than **UINT16_MAX** (**2\^16-1**), dc(1) will output
    lines to that length, including the backslash newline combo. The default
    line length is **70**.

**DC_EXPR_EXIT**

:   If this variable exists (no matter the contents), dc(1) will exit
    immediately after executing expressions and files given by the **-e** and/or
    **-f** command-line options (and any equivalents).

# EXIT STATUS

dc(1) returns the following exit statuses:

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
    (**\^**) operator.

**2**

:   A parse error occurred.

    Parse errors include unexpected **EOF**, using an invalid character, failing
    to find the end of a string or comment, and using a token where it is
    invalid.

**3**

:   A runtime error occurred.

    Runtime errors include assigning an invalid number to **ibase**, **obase**,
    or **scale**; give a bad expression to a **read()** call, calling **read()**
    inside of a **read()** call, type errors, and attempting an operation when
    the stack has too few elements.

**4**

:   A fatal error occurred.

    Fatal errors include memory allocation errors, I/O errors, failing to open
    files, attempting to use files that do not have only ASCII characters (dc(1)
    only accepts ASCII characters), attempting to open a directory as a file,
    and giving invalid command-line options.

The exit status **4** is special; when a fatal error occurs, dc(1) always exits
and returns **4**, no matter what mode dc(1) is in.

The other statuses will only be returned when dc(1) is not in interactive mode
(see the **INTERACTIVE MODE** section), since dc(1) resets its state (see the
**RESET** section) and accepts more input when one of those errors occurs in
interactive mode. This is also the case when interactive mode is forced by the
**-i** flag or **--interactive** option.

These exit statuses allow dc(1) to be used in shell scripting with error
checking, and its normal behavior can be forced by using the **-i** flag or
**--interactive** option.

# INTERACTIVE MODE

Like bc(1), dc(1) has an interactive mode and a non-interactive mode.
Interactive mode is turned on automatically when both **stdin** and **stdout**
are hooked to a terminal, but the **-i** flag and **--interactive** option can
turn it on in other cases.

In interactive mode, dc(1) attempts to recover from errors (see the **RESET**
section), and in normal execution, flushes **stdout** as soon as execution is
done for the current input.

# TTY MODE

If **stdin**, **stdout**, and **stderr** are all connected to a TTY, dc(1) turns
on "TTY mode."

The prompt is enabled in TTY mode.

TTY mode is different from interactive mode because interactive mode is required
in the [bc(1) specification][1], and interactive mode requires only **stdin**
and **stdout** to be connected to a terminal.

# SIGNAL HANDLING

Sending a **SIGINT** will cause dc(1) to stop execution of the current input. If
dc(1) is in TTY mode (see the **TTY MODE** section), it will reset (see the
**RESET** section). Otherwise, it will clean up and exit.

Note that "current input" can mean one of two things. If dc(1) is processing
input from **stdin** in TTY mode, it will ask for more input. If dc(1) is
processing input from a file in TTY mode, it will stop processing the file and
start processing the next file, if one exists, or ask for input from **stdin**
if no other file exists.

This means that if a **SIGINT** is sent to dc(1) as it is executing a file, it
can seem as though dc(1) did not respond to the signal since it will immediately
start executing the next file. This is by design; most files that users execute
when interacting with dc(1) have function definitions, which are quick to parse.
If a file takes a long time to execute, there may be a bug in that file. The
rest of the files could still be executed without problem, allowing the user to
continue.

**SIGTERM** and **SIGQUIT** cause dc(1) to clean up and exit, and it uses the
default handler for all other signals.

# LOCALES

This dc(1) ships with support for adding error messages for different locales
and thus, supports **LC_MESSAGS**.

# SEE ALSO

bc(1)

# STANDARDS

The dc(1) utility operators are compliant with the operators in the bc(1)
[IEEE Std 1003.1-2017 (“POSIX.1-2017”)][1] specification.

# BUGS

None are known. Report bugs at https://git.yzena.com/gavin/bc.

# AUTHOR

Gavin D. Howard <yzena.tech@gmail.com> and contributors.

[1]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html
