divert(-1)				-*-Text-*-
` Copyright (c) 1991 Free Software Foundation, Inc.'
` This file defines and documents the M4 macros used '
`      to preprocess some GNU manuals'
` $Id: pretex.m4,v 1.1 1993/11/03 00:55:40 paul Exp $'

I. INTRODUCTION

This collection of M4 macros is meant to help in pre-processing texinfo
files to allow configuring them by hosts; for example, the reader of an
as manual who only has access to a 386 may not really want to see crud about 
VAXen. 

A preprocessor is used, rather than extending texinfo, because this
way we can hack the conditionals in only one place; otherwise we would
have to write TeX macros, update makeinfo, and update the Emacs
info-formatting functions.

II. COMPATIBILITY

These macros should work with GNU m4 and System V m4; they do not work
with Sun or Berkeley M4.

III. USAGE

A. M4 INVOCATION
Assume this file is called "pretex.m4".  Then, to preprocess a
document "mybook.texinfo" you might do something like the following:

	m4 pretex.m4 none.m4 PARTIC.m4 mybook.texinfo >mybook-PARTIC.texinfo

---where your path is set to find GNU or SysV "m4", and the other m4
files mentioned are as follows:

	none.m4: A file that defines, as 0, all the options you might
		want to turn on using the conditionals defined below.
		Unlike the C preprocessor, m4 does not default
		undefined macros to 0.  For example, here is a "none.m4"
		I have been using:
	    _divert__(-1)

	    _define__(<_ALL_ARCH__>,<0>)
	    _define__(<_INTERNALS__>,<0>)

	    _define__(<_AMD29K__>,<0>)
	    _define__(<_I80386__>,<0>)
	    _define__(<_I960__>,<0>)
	    _define__(<_M680X0__>,<0>)
	    _define__(<_SPARC__>,<0>)
	    _define__(<_VAX__>,<0>)

	    _divert__<>

	PARTIC.m4: A file that turns on whichever options you actually
		want the manual configured for, in this particular
		instance.  Its contents are similar to one or more of
		the lines in "none.m4", but of course the second
		argument to _define__ is <1> rather than <0>.

		This is also a convenient place to _define__ any macros
		that you want to expand to different text for
		different configurations---for example, the name of
		the program being described.

Naturally, these are just suggested conventions; you could put your macro
definitions in any files or combinations of files you like.

These macros use the characters < and > as m4 quotes; if you need
these characters in your text, you will also want to use the macros
_0__ and _1__ from this package---see the description of "Quote
Handling" in the "Implementation" section below.

B. WHAT GOES IN THE PRE-TEXINFO SOURCE

For the most part, the text of your book.  In addition, you can
have text that is included only conditionally, using the macros
_if__ and _fi__ defined below.  They BOTH take an argument!  This is
primarily meant for readability (so a human can more easily see what
conditional end matches what conditional beginning), but the argument
is actually used in the _fi__ as well as the _if__ implementation.
You should always give a _fi__ the same argument as its matching
_if__.  Other arguments may appear to work for a while, but are almost
certain to produce the wrong output for some configurations.

For example, here is an excerpt from the very beginning of the
documentation for GNU as, to name the info file appropriately for
different configurations:
    _if__(_ALL_ARCH__)
    @setfilename as.info
    _fi__(_ALL_ARCH__)
    _if__(_M680X0__ && !_ALL_ARCH__)
    @setfilename as-m680x0.info
    _fi__(_M680X0__ && !_ALL_ARCH__)
    _if__(_AMD29K__ && !_ALL_ARCH__)
    @setfilename as-29k.info
    _fi__(_AMD29K__ && !_ALL_ARCH__) 

Note that you can use Boolean expressions in the arguments; the
expression language is that of the built-in m4 macro `eval', described
in the m4 manual.

IV. IMPLEMENTATION

A.PRIMITIVE RENAMING
First, we redefine m4's built-ins to avoid conflict with plain text.
The naming convention used is that our macros all begin with a single
underbar and end with two underbars.  The asymmetry is meant to avoid
conflict with some other conventions (which we may want to document) that
are intended to avoid conflict, like ANSI C predefined macros.

define(`_undefine__',defn(`undefine'))
define(`_define__',defn(`define'))
define(`_defn__',defn(`defn'))
define(`_ppf__',`_define__(`_$1__',_defn__(`$1'))_undefine__(`$1')')
_ppf__(`builtin')
_ppf__(`changecom')
_ppf__(`changequote')
_ppf__(`decr')
_ppf__(`define')
_ppf__(`defn')
_ppf__(`divert')
_ppf__(`divnum')
_ppf__(`dnl')
_ppf__(`dumpdef')
_ppf__(`errprint')
_ppf__(`esyscmd')
_ppf__(`eval')
_ppf__(`format')
_ppf__(`ifdef')
_ppf__(`ifelse')
_ppf__(`include')
_ppf__(`incr')
_ppf__(`index')
_ppf__(`len')
_ppf__(`m4exit')
_ppf__(`m4wrap')
_ppf__(`maketemp')
_ppf__(`patsubst')
_ppf__(`popdef')
_ppf__(`pushdef')
_ppf__(`regexp')
_ppf__(`shift')
_ppf__(`sinclude')
_ppf__(`substr')
_ppf__(`syscmd')
_ppf__(`sysval')
_ppf__(`traceoff')
_ppf__(`traceon')
_ppf__(`translit')
_ppf__(`undefine')
_ppf__(`undivert')
_ppf__(`unix')

B. QUOTE HANDLING.

The characters used as quotes by M4, by default, are unfortunately
quite likely to occur in ordinary text.  To avoid surprises, we will
use the characters <> ---which are just as suggestive (more so to
Francophones, perhaps) but a little less common in text (save for
those poor Francophones.  You win some, you lose some).  Still, we
expect also to have to set < and > occasionally in text; to do that,
we define a macro to turn off quote handling (_0__) and a macro to
turn it back on (_1__), according to our convention.  

	BEWARE: This seems to make < and > unusable as relational operations
		in calls to the builtin "eval".  So far I've gotten
		along without; but a better choice may be possible.

Note that we postponed this for a while, for convenience in discussing
the issue and in the primitive renaming---not to mention in defining
_0__ and _1__ themselves!  However, the quote redefinitions MUST
precede the _if__ / _fi__ definitions, because M4 will expand the text
as given---if we use the wrong quotes here, we will get the wrong
quotes when we use the conditionals.

_define__(_0__,`_changequote__(,)')_define__(_1__,`_changequote__(<,>)')
_1__

C. CONDITIONALS

We define two macros, _if__ and _fi__.  BOTH take arguments!  This is
meant both to help the human reader match up a _fi__ with its
corresponding _if__ and to aid in the implementation.  You may use the
full expression syntax supported by M4 (see docn of `eval' builtin in
the m4 manual).

The conditional macros are carefully defined to avoid introducing
extra whitespace (i.e., blank lines or blank characters).  One side
effect exists---

	BEWARE: text following an `_if__' on the same line is
		DISCARDED even if the condition is true; text
		following a `_fi__' on the same line is also 
		always discarded.

The recommended convention is to always place _if__ and _fi__ on a
line by themselves.  This will also aid the human reader.  TeX won't
care about the line breaks; as for info, you may want to insert calls
to `@refill' at the end of paragraphs containing conditionalized text,
where you don't want line breaks separating unconditional from
conditional text.  info formatting will then give you nice looking
paragraphs in the info file.

Nesting: conditionals are designed to nest, in the following way:
*nothing* is output between an outer pair of false conditionals, even
if there are true conditionals inside.  A false conditional "defeats"
all conditionals within it.  The counter _IF_FS__ is used to
implement this; kindly avoid redefining it directly.

_define__(<_IF_FS__>,<0>)

NOTE: The definitions for our "pushf" and "popf" macros use eval
rather than incr and decr, because GNU m4 (0.75) tries to call eval
for us when we say "incr" or "decr"---but doesn't notice we've changed
eval's name.

_define__(
	<_pushf__>,
	<_define__(<_IF_FS__>,
		_eval__((_IF_FS__)+1))>)
_define__(
	<_popf__>,
	<_ifelse__(0,_IF_FS__,
			<<>_dnl__<>>,
			<_define__(<_IF_FS__>,_eval__((_IF_FS__)-1))>)>)

_define__(
	<_if__>,
	<_ifelse__(1,_eval__( ($1) ),
			<<>_dnl__<>>,
			<_pushf__<>_divert__(-1)>)>)
_define__(
	<_fi__>,
	<_ifelse__(1,_eval__( ($1) ),
		<<>_dnl__<>>,
		<_popf__<>_ifelse__(0,_IF_FS__,
			<_divert__<>_dnl__<>>,<>)>)>)

D. CHAPTER/SECTION MACRO
In a parametrized manual, the heading level may need to be calculated;
for example, a manual that has a chapter on machine dependencies
should be conditionally structured as follows:
	- IF the manual is configured for a SINGLE machine type,  use
the chapter heading for that machine type, and run headings down
from there (top level for a particular machine is chapter, then within
that we have section, subsection etc);
	- ELSE, if MANY machine types are described in the chapter,
use a generic chapter heading such as "@chapter Machine Dependencies",
use "section" for the top level description of EACH machine, and run
headings down from there (top level for a particular machine is
section, then within that we have subsection, subsubsection etc).

The macro <_CHAPSEC__> is for this purpose: its argument is evaluated (so
you can construct expressions to express choices such as above), then
expands as follows:
   0: @chapter
   1: @section
   2: @subsection
   3: @subsubsection
 ...and so on.

_define__(<_CHAPSEC__>,<@_cs__(_eval__($1))>)
_define__(<_cs__>,<_ifelse__(
			0, $1, <chapter>,
			1, $1, <section>,
				<sub<>_cs__(_eval__($1 - 1))>)>)

_divert__<>_dnl__<>
