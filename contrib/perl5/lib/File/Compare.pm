package File::Compare;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $Too_Big *FROM *TO);

require Exporter;
use Carp;

$VERSION = '1.1001';
@ISA = qw(Exporter);
@EXPORT = qw(compare);
@EXPORT_OK = qw(cmp);

$Too_Big = 1024 * 1024 * 2;

sub VERSION {
    # Version of File::Compare
    return $File::Compare::VERSION;
}

sub compare {
    croak("Usage: compare( file1, file2 [, buffersize]) ")
      unless(@_ == 2 || @_ == 3);

    my $from = shift;
    my $to = shift;
    my $closefrom=0;
    my $closeto=0;
    my ($size, $fromsize, $status, $fr, $tr, $fbuf, $tbuf);
    local(*FROM, *TO);
    local($\) = '';

    croak("from undefined") unless (defined $from);
    croak("to undefined") unless (defined $to);

    if (ref($from) && 
        (UNIVERSAL::isa($from,'GLOB') || UNIVERSAL::isa($from,'IO::Handle'))) {
	*FROM = *$from;
    } elsif (ref(\$from) eq 'GLOB') {
	*FROM = $from;
    } else {
	open(FROM,"<$from") or goto fail_open1;
	binmode FROM;
	$closefrom = 1;
	$fromsize = -s FROM;
    }

    if (ref($to) &&
        (UNIVERSAL::isa($to,'GLOB') || UNIVERSAL::isa($to,'IO::Handle'))) {
	*TO = *$to;
    } elsif (ref(\$to) eq 'GLOB') {
	*TO = $to;
    } else {
	open(TO,"<$to") or goto fail_open2;
	binmode TO;
	$closeto = 1;
    }

    if ($closefrom && $closeto) {
	# If both are opened files we know they differ if their size differ
	goto fail_inner if $fromsize != -s TO;
    }

    if (@_) {
	$size = shift(@_) + 0;
	croak("Bad buffer size for compare: $size\n") unless ($size > 0);
    } else {
	$size = $fromsize;
	$size = 1024 if ($size < 512);
	$size = $Too_Big if ($size > $Too_Big);
    }

    $fbuf = '';
    $tbuf = '';
    while(defined($fr = read(FROM,$fbuf,$size)) && $fr > 0) {
	unless (defined($tr = read(TO,$tbuf,$fr)) and $tbuf eq $fbuf) {
            goto fail_inner;
	}
    }
    goto fail_inner if (defined($tr = read(TO,$tbuf,$size)) && $tr > 0);

    close(TO) || goto fail_open2 if $closeto;
    close(FROM) || goto fail_open1 if $closefrom;

    return 0;
    
  # All of these contortions try to preserve error messages...
  fail_inner:
    close(TO) || goto fail_open2 if $closeto;
    close(FROM) || goto fail_open1 if $closefrom;

    return 1;

  fail_open2:
    if ($closefrom) {
	$status = $!;
	$! = 0;
	close FROM;
	$! = $status unless $!;
    }
  fail_open1:
    return -1;
}

*cmp = \&compare;

1;

__END__

=head1 NAME

File::Compare - Compare files or filehandles

=head1 SYNOPSIS

  	use File::Compare;

	if (compare("file1","file2") == 0) {
	    print "They're equal\n";
	}

=head1 DESCRIPTION

The File::Compare::compare function compares the contents of two
sources, each of which can be a file or a file handle.  It is exported
from File::Compare by default.

File::Compare::cmp is a synonym for File::Compare::compare.  It is
exported from File::Compare only by request.

=head1 RETURN

File::Compare::compare return 0 if the files are equal, 1 if the
files are unequal, or -1 if an error was encountered.

=head1 AUTHOR

File::Compare was written by Nick Ing-Simmons.
Its original documentation was written by Chip Salzenberg.

=cut

