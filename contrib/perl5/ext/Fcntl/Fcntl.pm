package Fcntl;

=head1 NAME

Fcntl - load the C Fcntl.h defines

=head1 SYNOPSIS

    use Fcntl;
    use Fcntl qw(:DEFAULT :flock);

=head1 DESCRIPTION

This module is just a translation of the C F<fnctl.h> file.
Unlike the old mechanism of requiring a translated F<fnctl.ph>
file, this uses the B<h2xs> program (see the Perl source distribution)
and your native C compiler.  This means that it has a 
far more likely chance of getting the numbers right.

=head1 NOTE

Only C<#define> symbols get translated; you must still correctly
pack up your own arguments to pass as args for locking functions, etc.

=head1 EXPORTED SYMBOLS

By default your system's F_* and O_* constants (eg, F_DUPFD and
O_CREAT) and the FD_CLOEXEC constant are exported into your namespace.

You can request that the flock() constants (LOCK_SH, LOCK_EX, LOCK_NB
and LOCK_UN) be provided by using the tag C<:flock>.  See L<Exporter>.

You can request that the old constants (FAPPEND, FASYNC, FCREAT,
FDEFER, FEXCL, FNDELAY, FNONBLOCK, FSYNC, FTRUNC) be provided for
compatibility reasons by using the tag C<:Fcompat>.  For new
applications the newer versions of these constants are suggested
(O_APPEND, O_ASYNC, O_CREAT, O_DEFER, O_EXCL, O_NDELAY, O_NONBLOCK,
O_SYNC, O_TRUNC).

Please refer to your native fcntl() and open() documentation to see
what constants are implemented in your system.

=cut

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
$VERSION = "1.03";
# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)
@EXPORT =
  qw(
	FD_CLOEXEC
	F_DUPFD
	F_EXLCK
	F_GETFD
	F_GETFL
	F_GETLK
	F_GETOWN
	F_POSIX
	F_RDLCK
	F_SETFD
	F_SETFL
	F_SETLK
	F_SETLKW
	F_SETOWN
	F_SHLCK
	F_UNLCK
	F_WRLCK
	O_ACCMODE
	O_APPEND
	O_ASYNC
	O_BINARY
	O_CREAT
	O_DEFER
	O_DSYNC
	O_EXCL
	O_EXLOCK
	O_NDELAY
	O_NOCTTY
	O_NONBLOCK
	O_RDONLY
	O_RDWR
	O_RSYNC
	O_SHLOCK
	O_SYNC
	O_TEXT
	O_TRUNC
	O_WRONLY
     );

# Other items we are prepared to export if requested
@EXPORT_OK = qw(
	FAPPEND
	FASYNC
	FCREAT
	FDEFER
	FEXCL
	FNDELAY
	FNONBLOCK
	FSYNC
	FTRUNC
	LOCK_EX
	LOCK_NB
	LOCK_SH
	LOCK_UN
);
# Named groups of exports
%EXPORT_TAGS = (
    'flock'   => [qw(LOCK_SH LOCK_EX LOCK_NB LOCK_UN)],
    'Fcompat' => [qw(FAPPEND FASYNC FCREAT FDEFER FEXCL
	             FNDELAY FNONBLOCK FSYNC FTRUNC)],
);

sub AUTOLOAD {
    (my $constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    my ($pack,$file,$line) = caller;
	    die "Your vendor has not defined Fcntl macro $constname, used at $file line $line.
";
	}
    }
    *$AUTOLOAD = sub { $val };
    goto &$AUTOLOAD;
}

bootstrap Fcntl $VERSION;

1;
