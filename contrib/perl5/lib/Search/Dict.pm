package Search::Dict;
require 5.000;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(look);

=head1 NAME

Search::Dict, look - search for key in dictionary file

=head1 SYNOPSIS

    use Search::Dict;
    look *FILEHANDLE, $key, $dict, $fold;

=head1 DESCRIPTION

Sets file position in FILEHANDLE to be first line greater than or equal
(stringwise) to I<$key>.  Returns the new file position, or -1 if an error
occurs.

The flags specify dictionary order and case folding:

If I<$dict> is true, search by dictionary order (ignore anything but word
characters and whitespace).

If I<$fold> is true, ignore case.

=cut

sub look {
    local(*FH,$key,$dict,$fold) = @_;
    local($_);
    my(@stat) = stat(FH)
	or return -1;
    my($size, $blksize) = @stat[7,11];
    $blksize ||= 8192;
    $key =~ s/[^\w\s]//g if $dict;
    $key = lc $key if $fold;
    my($min, $max, $mid) = (0, int($size / $blksize));
    while ($max - $min > 1) {
	$mid = int(($max + $min) / 2);
	seek(FH, $mid * $blksize, 0)
	    or return -1;
	<FH> if $mid;			# probably a partial line
	$_ = <FH>;
	chop;
	s/[^\w\s]//g if $dict;
	$_ = lc $_ if $fold;
	if (defined($_) && $_ lt $key) {
	    $min = $mid;
	}
	else {
	    $max = $mid;
	}
    }
    $min *= $blksize;
    seek(FH,$min,0)
	or return -1;
    <FH> if $min;
    for (;;) {
	$min = tell(FH);
	defined($_ = <FH>)
	    or last;
	chop;
	s/[^\w\s]//g if $dict;
	$_ = lc $_ if $fold;
	last if $_ ge $key;
    }
    seek(FH,$min,0);
    $min;
}

1;
