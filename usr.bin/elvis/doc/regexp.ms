.Go 4 "REGULAR EXPRESSIONS"

.PP
\*E uses regular expressions for searching and substututions.
A regular expression is a text string in which some characters have
special meanings.
This is much more powerful than simple text matching.
.SH
Syntax
.PP
\*E' regexp package treats the following one- or two-character
strings (called meta-characters) in special ways:
.IP "\e(\fIsubexpression\fP\e)" 0.8i
The \e( and \e) metacharacters are used to delimit subexpressions.
When the regular expression matches a particular chunk of text,
\*E will remember which portion of that chunk matched the \fIsubexpression\fP.
The :s/regexp/newtext/ command makes use of this feature.
.IP "^" 0.8i
The ^ metacharacter matches the beginning of a line.
If, for example, you wanted to find "foo" at the beginning of a line,
you would use a regular expression such as /^foo/.
Note that ^ is only a metacharacter if it occurs
at the beginning of a regular expression;
anyplace else, it is treated as a normal character.
.IP "$" 0.8i
The $ metacharacter matches the end of a line.
It is only a metacharacter when it occurs at the end of a regular expression;
elsewhere, it is treated as a normal character.
For example, the regular expression /$$/ will search for a dollar sign at
the end of a line.
.IP "\e<" 0.8i
The \e< metacharacter matches a zero-length string at the beginning of
a word.
A word is considered to be a string of 1 or more letters and digits.
A word can begin at the beginning of a line
or after 1 or more non-alphanumeric characters.
.IP "\e>" 0.8i
The \e> metacharacter matches a zero-length string at the end of a word.
A word can end at the end of the line
or before 1 or more non-alphanumeric characters.
For example, /\e<end\e>/ would find any instance of the word "end",
but would ignore any instances of e-n-d inside another word
such as "calendar".
.IP "\&." 0.8i
The . metacharacter matches any single character.
.IP "[\fIcharacter-list\fP]" 0.8i
This matches any single character from the \fIcharacter-list\fP.
Inside the \fIcharacter-list\fP, you can denote a span of characters
by writing only the first and last characters, with a hyphen between
them.
If the \fIcharacter-list\fP is preceded by a ^ character, then the
list is inverted -- it will match character that \fIisn't\fP mentioned
in the list.
For example, /[a-zA-Z]/ matches any letter, and /[^ ]/ matches anything
other than a blank.
.IP "\e{\fIn\fP\e}" 0.8i
This is a closure operator,
which means that it can only be placed after something that matches a
single character.
It controls the number of times that the single-character expression
should be repeated.
.IP "" 0.8i
The \e{\fIn\fP\e} operator, in particular, means that the preceding
expression should be repeated exactly \fIn\fP times.
For example, /^-\e{80\e}$/ matches a line of eighty hyphens, and
/\e<[a-zA-Z]\e{4\e}\e>/ matches any four-letter word.
.IP "\e{\fIn\fP,\fIm\fP\e}" 0.8i
This is a closure operator which means that the preceding single-character
expression should be repeated between \fIn\fP and \fIm\fP times, inclusive.
If the \fIm\fP is omitted (but the comma is present) then \fIm\fP is
taken to be inifinity.
For example, /"[^"]\e{3,5\e}"/ matches any pair of quotes which contains
three, four, or five non-quote characters.
.IP "*" 0.8i
The * metacharacter is a closure operator which means that the preceding
single-character expression can be repeated zero or more times.
It is equivelent to \e{0,\e}.
For example, /.*/ matches a whole line.
.IP "\e+" 0.8i
The \e+ metacharacter is a closure operator which means that the preceding
single-character expression can be repeated one or more times.
It is equivelent to \e{1,\e}.
For example, /.\e+/ matches a whole line, but only if the line contains
at least one character.
It doesn't match empty lines.
.IP "\e?" 0.8i
The \e? metacharacter is a closure operator which indicates that the
preceding single-character expression is optional -- that is, that it
can occur 0 or 1 times.
It is equivelent to \e{0,1\e}.
For example, /no[ -]\e?one/ matches "no one", "no-one", or "noone".
.PP
Anything else is treated as a normal character which must exactly match
a character from the scanned text.
The special strings may all be preceded by a backslash to
force them to be treated normally.
.SH
Substitutions
.PP
The :s command has at least two arguments: a regular expression,
and a substitution string.
The text that matched the regular expression is replaced by text
which is derived from the substitution string.
.br
.ne 15 \" so we don't mess up the table
.PP
Most characters in the substitution string are copied into the
text literally but a few have special meaning:
.LD
.ta 0.75i 1.3i
	&	Insert a copy of the original text
	~	Insert a copy of the previous replacement text
	\e1	Insert a copy of that portion of the original text which
		matched the first set of \e( \e) parentheses
	\e2-\e9	Do the same for the second (etc.) pair of \e( \e)
	\eU	Convert all chars of any later & or \e# to uppercase
	\eL	Convert all chars of any later & or \e# to lowercase
	\eE	End the effect of \eU or \eL
	\eu	Convert the first char of the next & or \e# to uppercase
	\el	Convert the first char of the next & or \e# to lowercase
.TA
.DE
.PP
These may be preceded by a backslash to force them to be treated normally.
If "nomagic" mode is in effect,
then & and ~ will be treated normally,
and you must write them as \e& and \e~ for them to have special meaning.
.SH
Options
.PP
\*E has two options which affect the way regular expressions are used.
These options may be examined or set via the :set command.
.PP
The first option is called "[no]magic".
This is a boolean option, and it is "magic" (TRUE) by default.
While in magic mode, all of the meta-characters behave as described above.
In nomagic mode, only ^ and $ retain their special meaning.
.PP
The second option is called "[no]ignorecase".
This is a boolean option, and it is "noignorecase" (FALSE) by default.
While in ignorecase mode, the searching mechanism will not distinguish between
an uppercase letter and its lowercase form.
In noignorecase mode, uppercase and lowercase are treated as being different.
.PP
Also, the "[no]wrapscan" option affects searches.
.SH
Examples
.PP
This example changes every occurence of "utilize" to "use":
.sp
.ti +1i
:%s/utilize/use/g
.PP
This example deletes all whitespace that occurs at the end of a line anywhere
in the file.
(The brackets contain a single space and a single tab.):
.sp
.ti +1i
:%s/[   ]\e+$//
.PP
This example converts the current line to uppercase:
.sp
.ti +1i
:s/.*/\eU&/
.PP
This example underlines each letter in the current line,
by changing it into an "underscore backspace letter" sequence.
(The ^H is entered as "control-V backspace".):
.sp
.ti +1i
:s/[a-zA-Z]/_^H&/g
.PP
This example locates the last colon in a line,
and swaps the text before the colon with the text after the colon.
The first \e( \e) pair is used to delimit the stuff before the colon,
and the second pair delimit the stuff after.
In the substitution text, \e1 and \e2 are given in reverse order
to perform the swap:
.sp
.ti +1i
:s/\e(.*\e):\e(.*\e)/\e2:\e1/
