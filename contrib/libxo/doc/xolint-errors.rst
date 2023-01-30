'A percent sign appearing in text is a literal'
+++++++++++++++++++++++++++++++++++++++++++++++

The message "A percent sign appearing in text is a literal" can be caused by code like:

::

    xo_emit("cost: %d", cost);

This code should be replaced with code like:

::

    xo_emit("{L:cost}: {:cost/%d}", cost);

This can be a bit surprising and could be a field that was not
properly converted to a libxo-style format string.


'Unknown long name for role/modifier'
+++++++++++++++++++++++++++++++++++++

The message "Unknown long name for role/modifier" can be caused by code like:

::

    xo_emit("{,humanization:value}", value);

This code should be replaced with code like:

::

    xo_emit("{,humanize:value}", value);

The hn-* modifiers (hn-decimal, hn-space, hn-1000)
are only valid for fields with the {h:} modifier.


'Last character before field definition is a field type'
++++++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Last character before field definition is a field type" can be caused by code like:
A common typo:

::

    xo_emit("{T:Min} T{:Max}");

This code should be replaced with code like:

::

    xo_emit("{T:Min} {T:Max}");

Twiddling the "{" and the field role is a common typo.


'Encoding format uses different number of arguments'
++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Encoding format uses different number of arguments" can be caused by code like:

::

    xo_emit("{:name/%6.6s %%04d/%s}", name, number);

This code should be replaced with code like:

::

    xo_emit("{:name/%6.6s %04d/%s-%d}", name, number);

Both format should consume the same number of arguments off the stack


'Only one field role can be used'
+++++++++++++++++++++++++++++++++

The message "Only one field role can be used" can be caused by code like:

::

    xo_emit("{LT:Max}");

This code should be replaced with code like:

::

    xo_emit("{T:Max}");

'Potential missing slash after C, D, N, L, or T with format'
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Potential missing slash after C, D, N, L, or T with format" can be caused by code like:

::

    xo_emit("{T:%6.6s}\n", "Max");

This code should be replaced with code like:

::

    xo_emit("{T:/%6.6s}\n", "Max");

The "%6.6s" will be a literal, not a field format.  While
it's possibly valid, it's likely a missing "/".


'An encoding format cannot be given (roles: DNLT)'
++++++++++++++++++++++++++++++++++++++++++++++++++

The message "An encoding format cannot be given (roles: DNLT)" can be caused by code like:

::

    xo_emit("{T:Max//%s}", "Max");

Fields with the C, D, N, L, and T roles are not emitted in
the 'encoding' style (JSON, XML), so an encoding format
would make no sense.


'Format cannot be given when content is present (roles: CDLN)'
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Format cannot be given when content is present (roles: CDLN)" can be caused by code like:

::

    xo_emit("{N:Max/%6.6s}", "Max");

Fields with the C, D, L, or N roles can't have both
static literal content ("{L:Label}") and a
format ("{L:/%s}").
This error will also occur when the content has a backslash
in it, like "{N:Type of I/O}"; backslashes should be escaped,
like "{N:Type of I\\/O}".  Note the double backslash, one for
handling 'C' strings, and one for libxo.


'Field has color without fg- or bg- (role: C)'
++++++++++++++++++++++++++++++++++++++++++++++

The message "Field has color without fg- or bg- (role: C)" can be caused by code like:

::

    xo_emit("{C:green}{:foo}{C:}", x);

This code should be replaced with code like:

::

    xo_emit("{C:fg-green}{:foo}{C:}", x);

Colors must be prefixed by either "fg-" or "bg-".


'Field has invalid color or effect (role: C)'
+++++++++++++++++++++++++++++++++++++++++++++

The message "Field has invalid color or effect (role: C)" can be caused by code like:

::

    xo_emit("{C:fg-purple,bold}{:foo}{C:gween}", x);

This code should be replaced with code like:

::

    xo_emit("{C:fg-red,bold}{:foo}{C:fg-green}", x);

The list of colors and effects are limited.  The
set of colors includes default, black, red, green,
yellow, blue, magenta, cyan, and white, which must
be prefixed by either "fg-" or "bg-".  Effects are
limited to bold, no-bold, underline, no-underline,
inverse, no-inverse, normal, and reset.  Values must
be separated by commas.


'Field has humanize modifier but no format string'
++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Field has humanize modifier but no format string" can be caused by code like:

::

    xo_emit("{h:value}", value);

This code should be replaced with code like:

::

    xo_emit("{h:value/%d}", value);

Humanization is only value for numbers, which are not
likely to use the default format ("%s").


'Field has hn-* modifier but not 'h' modifier'
++++++++++++++++++++++++++++++++++++++++++++++

The message "Field has hn-* modifier but not 'h' modifier" can be caused by code like:

::

    xo_emit("{,hn-1000:value}", value);

This code should be replaced with code like:

::

    xo_emit("{h,hn-1000:value}", value);

The hn-* modifiers (hn-decimal, hn-space, hn-1000)
are only valid for fields with the {h:} modifier.


'Value field must have a name (as content)")'
+++++++++++++++++++++++++++++++++++++++++++++

The message "Value field must have a name (as content)")" can be caused by code like:

::

    xo_emit("{:/%s}", "value");

This code should be replaced with code like:

::

    xo_emit("{:tag-name/%s}", "value");

The field name is used for XML and JSON encodings.  These
tags names are static and must appear directly in the
field descriptor.


'Use hyphens, not underscores, for value field name'
++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Use hyphens, not underscores, for value field name" can be caused by code like:

::

    xo_emit("{:no_under_scores}", "bad");

This code should be replaced with code like:

::

    xo_emit("{:no-under-scores}", "bad");

Use of hyphens is traditional in XML, and the XOF_UNDERSCORES
flag can be used to generate underscores in JSON, if desired.
But the raw field name should use hyphens.


'Value field name cannot start with digit'
++++++++++++++++++++++++++++++++++++++++++

The message "Value field name cannot start with digit" can be caused by code like:

::

    xo_emit("{:10-gig/}");

This code should be replaced with code like:

::

    xo_emit("{:ten-gig/}");

XML element names cannot start with a digit.


'Value field name should be lower case'
+++++++++++++++++++++++++++++++++++++++

The message "Value field name should be lower case" can be caused by code like:

::

    xo_emit("{:WHY-ARE-YOU-SHOUTING}", "NO REASON");

This code should be replaced with code like:

::

    xo_emit("{:why-are-you-shouting}", "no reason");

Lower case is more civilized.  Even TLAs should be lower case
to avoid scenarios where the differences between "XPath" and
"Xpath" drive your users crazy.  Lower case rules the seas.


'Value field name should be longer than two characters'
+++++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Value field name should be longer than two characters" can be caused by code like:

::

    xo_emit("{:x}", "mumble");

This code should be replaced with code like:

::

    xo_emit("{:something-meaningful}", "mumble");

Field names should be descriptive, and it's hard to
be descriptive in less than two characters.  Consider
your users and try to make something more useful.
Note that this error often occurs when the field type
is placed after the colon ("{:T/%20s}"), instead of before
it ("{T:/20s}").


'Value field name contains invalid character'
+++++++++++++++++++++++++++++++++++++++++++++

The message "Value field name contains invalid character" can be caused by code like:

::

    xo_emit("{:cost-in-$$/%u}", 15);

This code should be replaced with code like:

::

    xo_emit("{:cost-in-dollars/%u}", 15);

An invalid character is often a sign of a typo, like "{:]}"
instead of "{]:}".  Field names are restricted to lower-case
characters, digits, and hyphens.


'decoration field contains invalid character'
+++++++++++++++++++++++++++++++++++++++++++++

The message "decoration field contains invalid character" can be caused by code like:

::

    xo_emit("{D:not good}");

This code should be replaced with code like:

::

    xo_emit("{D:((}{:good}{D:))}", "yes");

This is minor, but fields should use proper roles.  Decoration
fields are meant to hold punctuation and other characters used
to decorate the content, typically to make it more readable
to human readers.


'Anchor content should be decimal width'
++++++++++++++++++++++++++++++++++++++++

The message "Anchor content should be decimal width" can be caused by code like:

::

    xo_emit("{[:mumble}");

This code should be replaced with code like:

::

    xo_emit("{[:32}");

Anchors need an integer value to specify the width of
the set of anchored fields.  The value can be positive
(for left padding/right justification) or negative (for
right padding/left justification) and can appear in
either the start or stop anchor field descriptor.


'Anchor format should be "%d"'
++++++++++++++++++++++++++++++

The message "Anchor format should be "%d"" can be caused by code like:

::

    xo_emit("{[:/%s}");

This code should be replaced with code like:

::

    xo_emit("{[:/%d}");

Anchors only grok integer values, and if the value is not static,
if must be in an 'int' argument, represented by the "%d" format.
Anything else is an error.


'Anchor cannot have both format and encoding format")'
++++++++++++++++++++++++++++++++++++++++++++++++++++++

The message "Anchor cannot have both format and encoding format")" can be caused by code like:

::

    xo_emit("{[:32/%d}");

This code should be replaced with code like:

::

    xo_emit("{[:32}");

Anchors can have a static value or argument for the width,
but cannot have both.


'Max width only valid for strings'
++++++++++++++++++++++++++++++++++

The message "Max width only valid for strings" can be caused by code like:

::

    xo_emit("{:tag/%2.4.6d}", 55);

This code should be replaced with code like:

::

    xo_emit("{:tag/%2.6d}", 55);

libxo allows a true 'max width' in addition to the traditional
printf-style 'max number of bytes to use for input'.  But this
is supported only for string values, since it makes no sense
for non-strings.  This error may occur from a typo,
like "{:tag/%6..6d}" where only one period should be used.
