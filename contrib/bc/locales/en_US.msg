$ $
$ SPDX-License-Identifier: BSD-2-Clause
$ $
$ Copyright (c) 2018-2023 Gavin D. Howard and contributors.
$ $
$ Redistribution and use in source and binary forms, with or without
$ modification, are permitted provided that the following conditions are met:
$ $
$ * Redistributions of source code must retain the above copyright notice, this
$   list of conditions and the following disclaimer.
$ $
$ * Redistributions in binary form must reproduce the above copyright notice,
$   this list of conditions and the following disclaimer in the documentation
$   and/or other materials provided with the distribution.
$ $
$ THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
$ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
$ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
$ ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
$ LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
$ CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
$ SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
$ INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
$ CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
$ ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
$ POSSIBILITY OF SUCH DAMAGE.
$ $

$quote "

$ Error types.
$set 1

1 "Math error:"
2 "Parse error:"
3 "Runtime error:"
4 "Fatal error:"
5 "Warning:"

$ Math errors.
$set 2

1 "negative number"
2 "non-integer number"
3 "overflow: number does not fit into a hardware number"
4 "divide by 0"

$ Parse errors.
$set 3

1 "end of file"
2 "invalid character '%c'"
3 "string end cannot be found"
4 "comment end cannot be found"
5 "invalid token"
6 "invalid expression"
7 "empty expression"
8 "invalid print or stream statement"
9 "invalid function definition"
10 "invalid assignment: left side must be scale, ibase, obase, seed, last, var, or array element"
11 "no auto variable found"
12 "function parameter or auto \"%s%s\" already exists"
13 "block end cannot be found"
14 "cannot return a value from void function: %s()"
15 "var cannot be a reference: %s"
16 "POSIX does not allow names longer than 1 character: %s"
17 "POSIX does not allow '#' script comments"
18 "POSIX does not allow the following keyword: %s"
19 "POSIX does not allow a period ('.') as a shortcut for the last result"
20 "POSIX requires parentheses around return expressions"
21 "POSIX does not allow the following operator: %s"
22 "POSIX does not allow comparison operators outside if statements or loops"
23 "POSIX requires 0 or 1 comparison operators per condition"
24 "POSIX requires all 3 parts of a for loop to be non-empty"
25 "POSIX requires a newline between a semicolon and a function definition"
26 "POSIX does not allow exponential notation"
27 "POSIX does not allow array references as function parameters"
28 "POSIX does not allow void functions"
29 "POSIX requires the left brace be on the same line as the function header"
30 "POSIX does not allow strings to be assigned to variables or arrays"

$ Runtime errors.
$set 4

1 "invalid ibase: must be [%lu, %lu]"
2 "invalid obase: must be [%lu, %lu]"
3 "invalid scale: must be [%lu, %lu]"
4 "invalid read() expression"
5 "recursive read() call"
6 "variable or array element is the wrong type"
7 "stack has too few elements"
8 "stack for register \"%s\" has too few elements"
9 "wrong number of parameters; need %zu, have %zu"
10 "undefined function: %s()"
11 "cannot use a void value in an expression"

$ Fatal errors.
$set 5

1 "memory allocation failed"
2 "I/O error"
3 "cannot open file: %s"
4 "file is not text: %s"
5 "path is a directory: %s"
6 "invalid command-line option: \"%s\""
7 "option requires an argument: '%c' (\"%s\")"
8 "option takes no arguments: '%c' (\"%s\")"
9 "invalid command-line option argument: \"%s\""
