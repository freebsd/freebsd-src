# Term::ANSIColor -- Color screen output using ANSI escape sequences.
# $Id: ANSIColor.pm,v 1.1 1997/12/10 20:05:29 eagle Exp $
#
# Copyright 1996, 1997 by Russ Allbery <rra@stanford.edu>
#                     and Zenin <zenin@best.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

############################################################################
# Modules and declarations
############################################################################

package Term::ANSIColor;
require 5.001;

use strict;
use vars qw(@ISA @EXPORT %EXPORT_TAGS $VERSION $AUTOLOAD %attributes
            $AUTORESET $EACHLINE);

use Exporter ();
@ISA         = qw(Exporter);
@EXPORT      = qw(color colored);
%EXPORT_TAGS = (constants => [qw(CLEAR RESET BOLD UNDERLINE UNDERSCORE BLINK
                                 REVERSE CONCEALED BLACK RED GREEN YELLOW
                                 BLUE MAGENTA CYAN WHITE ON_BLACK ON_RED
                                 ON_GREEN ON_YELLOW ON_BLUE ON_MAGENTA
                                 ON_CYAN ON_WHITE)]);
Exporter::export_ok_tags ('constants');
    
($VERSION = (split (' ', q$Revision: 1.1 $ ))[1]) =~ s/\.(\d)$/.0$1/;


############################################################################
# Internal data structures
############################################################################

%attributes = ('clear'      => 0,
               'reset'      => 0,
               'bold'       => 1,
               'underline'  => 4,
               'underscore' => 4,
               'blink'      => 5,
               'reverse'    => 7,
               'concealed'  => 8,

               'black'      => 30,   'on_black'   => 40, 
               'red'        => 31,   'on_red'     => 41, 
               'green'      => 32,   'on_green'   => 42, 
               'yellow'     => 33,   'on_yellow'  => 43, 
               'blue'       => 34,   'on_blue'    => 44, 
               'magenta'    => 35,   'on_magenta' => 45, 
               'cyan'       => 36,   'on_cyan'    => 46, 
               'white'      => 37,   'on_white'   => 47);


############################################################################
# Implementation (constant form)
############################################################################

# Time to have fun!  We now want to define the constant subs, which are
# named the same as the attributes above but in all caps.  Each constant sub
# needs to act differently depending on whether $AUTORESET is set.  Without
# autoreset:
#
#   BLUE "text\n"  ==>  "\e[34mtext\n"
#
# If $AUTORESET is set, we should instead get:
#
#   BLUE "text\n"  ==>  "\e[34mtext\n\e[0m"
#
# The sub also needs to handle the case where it has no arguments correctly.
# Maintaining all of this as separate subs would be a major nightmare, as
# well as duplicate the %attributes hash, so instead we define an AUTOLOAD
# sub to define the constant subs on demand.  To do that, we check the name
# of the called sub against the list of attributes, and if it's an all-caps
# version of one of them, we define the sub on the fly and then run it.
sub AUTOLOAD {
    my $sub;
    ($sub = $AUTOLOAD) =~ s/^.*:://;
    my $attr = $attributes{lc $sub};
    if ($sub =~ /^[A-Z_]+$/ && defined $attr) {
        $attr = "\e[" . $attr . 'm';
        eval qq {
            sub $AUTOLOAD {
                if (\$AUTORESET && \@_) {
                    '$attr' . "\@_" . "\e[0m";
                } else {
                    ('$attr' . "\@_");
                }
            }
        };
        goto &$AUTOLOAD;
    } else {
        die "undefined subroutine &$AUTOLOAD called";
    }
}


############################################################################
# Implementation (attribute string form)
############################################################################

# Return the escape code for a given set of color attributes.
sub color {
    my @codes = map { split } @_;
    my $attribute = '';
    foreach (@codes) {
        $_ = lc $_;
        unless (defined $attributes{$_}) {
            require Carp;
            Carp::croak ("Invalid attribute name $_");
        }
        $attribute .= $attributes{$_} . ';';
    }
    chop $attribute;
    ($attribute ne '') ? "\e[${attribute}m" : undef;
}

# Given a string and a set of attributes, returns the string surrounded by
# escape codes to set those attributes and then clear them at the end of the
# string.  If $EACHLINE is set, insert a reset before each occurrence of the
# string $EACHLINE and the starting attribute code after the string
# $EACHLINE, so that no attribute crosses line delimiters (this is often
# desirable if the output is to be piped to a pager or some other program).
sub colored {
    my $string = shift;
    if (defined $EACHLINE) {
        my $attr = color (@_);
        join '', 
            map { $_ && $_ ne $EACHLINE ? $attr . $_ . "\e[0m" : $_ }
                split (/(\Q$EACHLINE\E)/, $string);
    } else {
        color (@_) . $string . "\e[0m";
    }
}


############################################################################
# Module return value and documentation
############################################################################

# Ensure we evaluate to true.
1;
__END__

=head1 NAME

Term::ANSIColor - Color screen output using ANSI escape sequences

=head1 SYNOPSIS

    use Term::ANSIColor;
    print color 'bold blue';
    print "This text is bold blue.\n";
    print color 'reset';
    print "This text is normal.\n";
    print colored ("Yellow on magenta.\n", 'yellow on_magenta');
    print "This text is normal.\n";

    use Term::ANSIColor qw(:constants);
    print BOLD, BLUE, "This text is in bold blue.\n", RESET;

    use Term::ANSIColor qw(:constants);
    $Term::ANSIColor::AUTORESET = 1;
    print BOLD BLUE "This text is in bold blue.\n";
    print "This text is normal.\n";

=head1 DESCRIPTION

This module has two interfaces, one through color() and colored() and the
other through constants.
    
color() takes any number of strings as arguments and considers them to be
space-separated lists of attributes.  It then forms and returns the escape
sequence to set those attributes.  It doesn't print it out, just returns
it, so you'll have to print it yourself if you want to (this is so that
you can save it as a string, pass it to something else, send it to a file
handle, or do anything else with it that you might care to).

The recognized attributes (all of which should be fairly intuitive) are
clear, reset, bold, underline, underscore, blink, reverse, concealed,
black, red, green, yellow, blue, magenta, on_black, on_red, on_green,
on_yellow, on_blue, on_magenta, on_cyan, and on_white.  Case is not
significant.  Underline and underscore are equivalent, as are clear and
reset, so use whichever is the most intuitive to you.  The color alone
sets the foreground color, and on_color sets the background color.

Note that attributes, once set, last until they are unset (by sending the
attribute "reset").  Be careful to do this, or otherwise your attribute will
last after your script is done running, and people get very annoyed at
having their prompt and typing changed to weird colors.

As an aid to help with this, colored() takes a scalar as the first
argument and any number of attribute strings as the second argument and
returns the scalar wrapped in escape codes so that the attributes will be
set as requested before the string and reset to normal after the string.
Normally, colored() just puts attribute codes at the beginning and end of
the string, but if you set $Term::ANSIColor::EACHLINE to some string,
that string will be considered the line delimiter and the attribute will
be set at the beginning of each line of the passed string and reset at the
end of each line.  This is often desirable if the output is being sent to
a program like a pager that can be confused by attributes that span lines.
Normally you'll want to set $Term::ANSIColor::EACHLINE to C<"\n"> to use
this feature.

Alternately, if you import C<:constants>, you can use the constants CLEAR,
RESET, BOLD, UNDERLINE, UNDERSCORE, BLINK, REVERSE, CONCEALED, BLACK, RED,
GREEN, YELLOW, BLUE, MAGENTA, ON_BLACK, ON_RED, ON_GREEN, ON_YELLOW,
ON_BLUE, ON_MAGENTA, ON_CYAN, and ON_WHITE directly.  These are the same
as color('attribute') and can be used if you prefer typing:

    print BOLD BLUE ON_WHITE "Text\n", RESET;

to

    print colored ("Text\n", 'bold blue on_white');

When using the constants, if you don't want to have to remember to add the
C<, RESET> at the end of each print line, you can set
$Term::ANSIColor::AUTORESET to a true value.  Then, the display mode will
automatically be reset if there is no comma after the constant.  In other
words, with that variable set:

    print BOLD BLUE "Text\n";

will reset the display mode afterwards, whereas:

    print BOLD, BLUE, "Text\n";

will not.

The subroutine interface has the advantage over the constants interface in
that only 2 soubrutines are exported into your namespace, verses 22 in the
constants interface.  On the flip side, the constants interface has the
advantage of better compile time error checking, since misspelled names of
colors or attributes in calls to color() and colored() won't be caught
until runtime whereas misspelled names of constants will be caught at
compile time.  So, polute your namespace with almost two dozen subrutines
that you may not even use that oftin, or risk a silly bug by mistyping an
attribute.  Your choice, TMTOWTDI after all.

=head1 DIAGNOSTICS

=over 4

=item Invalid attribute name %s

You passed an invalid attribute name to either color() or colored().

=item Identifier %s used only once: possible typo

You probably mistyped a constant color name such as:

    print FOOBAR "This text is color FOOBAR\n";

It's probably better to always use commas after constant names in order to
force the next error.

=item No comma allowed after filehandle

You probably mistyped a constant color name such as:

    print FOOBAR, "This text is color FOOBAR\n";

Generating this fatal compile error is one of the main advantages of using
the constants interface, since you'll immediately know if you mistype a
color name.

=item Bareword %s not allowed while "strict subs" in use

You probably mistyped a constant color name such as:

    $Foobar = FOOBAR . "This line should be blue\n";

or:

    @Foobar = FOOBAR, "This line should be blue\n";

This will only show up under use strict (another good reason to run under
use strict).

=back

=head1 RESTRICTIONS

It would be nice if one could leave off the commas around the constants
entirely and just say:

    print BOLD BLUE ON_WHITE "Text\n" RESET;

but the syntax of Perl doesn't allow this.  You need a comma after the
string.  (Of course, you may consider it a bug that commas between all the
constants aren't required, in which case you may feel free to insert
commas unless you're using $Term::ANSIColor::AUTORESET.)

For easier debuging, you may prefer to always use the commas when not
setting $Term::ANSIColor::AUTORESET so that you'll get a fatal compile
error rather than a warning.

=head1 AUTHORS

Original idea (using constants) by Zenin (zenin@best.com), reimplemented
using subs by Russ Allbery (rra@stanford.edu), and then combined with the
original idea by Russ with input from Zenin.

=cut
