package Time::Local;
require 5.000;
require Exporter;
use Carp;

@ISA		= qw( Exporter );
@EXPORT		= qw( timegm timelocal );
@EXPORT_OK	= qw( timegm_nocheck timelocal_nocheck );

# Set up constants
    $SEC  = 1;
    $MIN  = 60 * $SEC;
    $HR   = 60 * $MIN;
    $DAY  = 24 * $HR;
# Determine breakpoint for rolling century
    my $thisYear = (localtime())[5];
    $nextCentury = int($thisYear / 100) * 100;
    $breakpoint = ($thisYear + 50) % 100;
    $nextCentury += 100 if $breakpoint < 50;

my %options;

sub timegm {
    my (@date) = @_;
    if ($date[5] > 999) {
        $date[5] -= 1900;
    }
    elsif ($date[5] >= 0 && $date[5] < 100) {
        $date[5] -= 100 if $date[5] > $breakpoint;
        $date[5] += $nextCentury;
    }
    $ym = pack(C2, @date[5,4]);
    $cheat = $cheat{$ym} || &cheat(@date);
    $cheat
    + $date[0] * $SEC
    + $date[1] * $MIN
    + $date[2] * $HR
    + ($date[3]-1) * $DAY;
}

sub timegm_nocheck {
    local $options{no_range_check} = 1;
    &timegm;
}

sub timelocal {
    my $t = &timegm;
    my $tt = $t;

    my (@lt) = localtime($t);
    my (@gt) = gmtime($t);
    if ($t < $DAY and ($lt[5] >= 70 or $gt[5] >= 70 )) {
	# Wrap error, too early a date
	# Try a safer date
	$tt += $DAY;
	@lt = localtime($tt);
	@gt = gmtime($tt);
    }

    my $tzsec = ($gt[1] - $lt[1]) * $MIN + ($gt[2] - $lt[2]) * $HR;

    if($lt[5] > $gt[5]) {
	$tzsec -= $DAY;
    }
    elsif($gt[5] > $lt[5]) {
	$tzsec += $DAY;
    }
    else {
	$tzsec += ($gt[7] - $lt[7]) * $DAY;
    }

    $tzsec += $HR if($lt[8]);
    
    $time = $t + $tzsec;
    @test = localtime($time + ($tt - $t));
    $time -= $HR if $test[2] != $_[2];
    $time;
}

sub timelocal_nocheck {
    local $options{no_range_check} = 1;
    &timelocal;
}

sub cheat {
    $year = $_[5];
    $month = $_[4];
    unless ($options{no_range_check}) {
	croak "Month '$month' out of range 0..11" if $month > 11 || $month < 0;
	croak "Day '$_[3]' out of range 1..31"	  if $_[3] > 31 || $_[3] < 1;
	croak "Hour '$_[2]' out of range 0..23"	  if $_[2] > 23 || $_[2] < 0;
	croak "Minute '$_[1]' out of range 0..59" if $_[1] > 59 || $_[1] < 0;
	croak "Second '$_[0]' out of range 0..59" if $_[0] > 59 || $_[0] < 0;
    }
    $guess = $^T;
    @g = gmtime($guess);
    $lastguess = "";
    $counter = 0;
    while ($diff = $year - $g[5]) {
	croak "Can't handle date (".join(", ",@_).")" if ++$counter > 255;
	$guess += $diff * (363 * $DAY);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    croak "Can't handle date (".join(", ",@_).")";
	    #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    while ($diff = $month - $g[4]) {
	croak "Can't handle date (".join(", ",@_).")" if ++$counter > 255;
	$guess += $diff * (27 * $DAY);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    croak "Can't handle date (".join(", ",@_).")";
	    #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    @gfake = gmtime($guess-1); #still being sceptic
    if ("@gfake" eq $lastguess){
        croak "Can't handle date (".join(", ",@_).")";
        #date beyond this machine's integer limit
    }
    $g[3]--;
    $guess -= $g[0] * $SEC + $g[1] * $MIN + $g[2] * $HR + $g[3] * $DAY;
    $cheat{$ym} = $guess;
}

1;

__END__

=head1 NAME

Time::Local - efficiently compute time from local and GMT time

=head1 SYNOPSIS

    $time = timelocal($sec,$min,$hours,$mday,$mon,$year);
    $time = timegm($sec,$min,$hours,$mday,$mon,$year);

=head1 DESCRIPTION

These routines are the inverse of built-in perl fuctions localtime()
and gmtime().  They accept a date as a six-element array, and return
the corresponding time(2) value in seconds since the Epoch (Midnight,
January 1, 1970).  This value can be positive or negative.

It is worth drawing particular attention to the expected ranges for
the values provided.  While the day of the month is expected to be in
the range 1..31, the month should be in the range 0..11.  
This is consistent with the values returned from localtime() and gmtime().

The timelocal() and timegm() functions perform range checking on the
input $sec, $min, $hours, $mday, and $mon values by default.  If you'd
rather they didn't, you can explicitly import the timelocal_nocheck()
and timegm_nocheck() functions.

	use Time::Local 'timelocal_nocheck';

	{
	    # The 365th day of 1999
	    print scalar localtime timelocal_nocheck 0,0,0,365,0,99;

	    # The twenty thousandth day since 1970
	    print scalar localtime timelocal_nocheck 0,0,0,20000,0,70;

	    # And even the 10,000,000th second since 1999!
	    print scalar localtime timelocal_nocheck 10000000,0,0,1,0,99;
	}

Your mileage may vary when trying these with minutes and hours,
and it doesn't work at all for months.

Strictly speaking, the year should also be specified in a form consistent
with localtime(), i.e. the offset from 1900.
In order to make the interpretation of the year easier for humans,
however, who are more accustomed to seeing years as two-digit or four-digit
values, the following conventions are followed:

=over 4

=item *

Years greater than 999 are interpreted as being the actual year,
rather than the offset from 1900.  Thus, 1963 would indicate the year
Martin Luther King won the Nobel prize, not the year 2863.

=item *

Years in the range 100..999 are interpreted as offset from 1900, 
so that 112 indicates 2012.  This rule also applies to years less than zero
(but see note below regarding date range).

=item *

Years in the range 0..99 are interpreted as shorthand for years in the
rolling "current century," defined as 50 years on either side of the current
year.  Thus, today, in 1999, 0 would refer to 2000, and 45 to 2045,
but 55 would refer to 1955.  Twenty years from now, 55 would instead refer
to 2055.  This is messy, but matches the way people currently think about
two digit dates.  Whenever possible, use an absolute four digit year instead.

=back

The scheme above allows interpretation of a wide range of dates, particularly
if 4-digit years are used.  

Please note, however, that the range of dates that can be actually be handled
depends on the size of an integer (time_t) on a given platform.  
Currently, this is 32 bits for most systems, yielding an approximate range 
from Dec 1901 to Jan 2038.

Both timelocal() and timegm() croak if given dates outside the supported
range.

=head1 IMPLEMENTATION

These routines are quite efficient and yet are always guaranteed to agree
with localtime() and gmtime().  We manage this by caching the start times
of any months we've seen before.  If we know the start time of the month,
we can always calculate any time within the month.  The start times
themselves are guessed by successive approximation starting at the
current time, since most dates seen in practice are close to the
current date.  Unlike algorithms that do a binary search (calling gmtime
once for each bit of the time value, resulting in 32 calls), this algorithm
calls it at most 6 times, and usually only once or twice.  If you hit
the month cache, of course, it doesn't call it at all.

timelocal() is implemented using the same cache.  We just assume that we're
translating a GMT time, and then fudge it when we're done for the timezone
and daylight savings arguments.  Note that the timezone is evaluated for
each date because countries occasionally change their official timezones.
Assuming that localtime() corrects for these changes, this routine will
also be correct.  The daylight savings offset is currently assumed 
to be one hour.

=head1 BUGS

The whole scheme for interpreting two-digit years can be considered a bug.

Note that the cache currently handles only years from 1900 through 2155.

The proclivity to croak() is probably a bug.

=cut
