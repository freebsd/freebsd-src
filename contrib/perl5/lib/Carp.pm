package Carp;

=head1 NAME

carp    - warn of errors (from perspective of caller)

cluck   - warn of errors with stack backtrace
          (not exported by default)

croak   - die of errors (from perspective of caller)

confess - die of errors with stack backtrace

=head1 SYNOPSIS

    use Carp;
    croak "We're outta here!";

    use Carp qw(cluck);
    cluck "This is how we got here!";

=head1 DESCRIPTION

The Carp routines are useful in your own modules because
they act like die() or warn(), but report where the error
was in the code they were called from.  Thus if you have a 
routine Foo() that has a carp() in it, then the carp() 
will report the error as occurring where Foo() was called, 
not where carp() was called.

=head2 Forcing a Stack Trace

As a debugging aid, you can force Carp to treat a croak as a confess
and a carp as a cluck across I<all> modules. In other words, force a
detailed stack trace to be given.  This can be very helpful when trying
to understand why, or from where, a warning or error is being generated.

This feature is enabled by 'importing' the non-existent symbol
'verbose'. You would typically enable it by saying

    perl -MCarp=verbose script.pl

or by including the string C<MCarp=verbose> in the L<PERL5OPT>
environment variable.

=head1 BUGS

The Carp routines don't handle exception objects currently.
If called with a first argument that is a reference, they simply
call die() or warn(), as appropriate.

=cut

# This package is heavily used. Be small. Be fast. Be good.

# Comments added by Andy Wardley <abw@kfs.org> 09-Apr-98, based on an
# _almost_ complete understanding of the package.  Corrections and
# comments are welcome.

# The $CarpLevel variable can be set to "strip off" extra caller levels for
# those times when Carp calls are buried inside other functions.  The
# $Max(EvalLen|(Arg(Len|Nums)) variables are used to specify how the eval
# text and function arguments should be formatted when printed.

$CarpLevel = 0;		# How many extra package levels to skip on carp.
$MaxEvalLen = 0;	# How much eval '...text...' to show. 0 = all.
$MaxArgLen = 64;        # How much of each argument to print. 0 = all.
$MaxArgNums = 8;        # How many arguments to print. 0 = all.
$Verbose = 0;		# If true then make shortmess call longmess instead

require Exporter;
@ISA = ('Exporter');
@EXPORT = qw(confess croak carp);
@EXPORT_OK = qw(cluck verbose);
@EXPORT_FAIL = qw(verbose);	# hook to enable verbose mode


# if the caller specifies verbose usage ("perl -MCarp=verbose script.pl")
# then the following method will be called by the Exporter which knows
# to do this thanks to @EXPORT_FAIL, above.  $_[1] will contain the word
# 'verbose'.

sub export_fail {
    shift;
    $Verbose = shift if $_[0] eq 'verbose';
    return @_;
}


# longmess() crawls all the way up the stack reporting on all the function
# calls made.  The error string, $error, is originally constructed from the
# arguments passed into longmess() via confess(), cluck() or shortmess().
# This gets appended with the stack trace messages which are generated for
# each function call on the stack.

sub longmess {
    { local $@; require Carp::Heavy; }	# XXX fix require to not clear $@?
    goto &longmess_heavy;
}


# shortmess() is called by carp() and croak() to skip all the way up to
# the top-level caller's package and report the error from there.  confess()
# and cluck() generate a full stack trace so they call longmess() to
# generate that.  In verbose mode shortmess() calls longmess() so
# you always get a stack trace

sub shortmess {	# Short-circuit &longmess if called via multiple packages
    { local $@; require Carp::Heavy; }	# XXX fix require to not clear $@?
    goto &shortmess_heavy;
}


# the following four functions call longmess() or shortmess() depending on
# whether they should generate a full stack trace (confess() and cluck())
# or simply report the caller's package (croak() and carp()), respectively.
# confess() and croak() die, carp() and cluck() warn.

sub croak   { die  shortmess @_ }
sub confess { die  longmess  @_ }
sub carp    { warn shortmess @_ }
sub cluck   { warn longmess  @_ }

1;
