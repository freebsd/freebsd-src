package Time::Local;
require 5.000;
require Exporter;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(timegm timelocal);

=head1 NAME

Time::Local - efficiently compute time from local and GMT time

=head1 SYNOPSIS

    $time = timelocal($sec,$min,$hours,$mday,$mon,$year);
    $time = timegm($sec,$min,$hours,$mday,$mon,$year);

=head1 DESCRIPTION

These routines are quite efficient and yet are always guaranteed to
agree with localtime() and gmtime(), the most notable points being
that year is year-1900 and month is 0..11.  We manage this by caching
the start times of any months we've seen before.  If we know the start
time of the month, we can always calculate any time within the month.
The start times themselves are guessed by successive approximation
starting at the current time, since most dates seen in practice are
close to the current date.  Unlike algorithms that do a binary search
(calling gmtime once for each bit of the time value, resulting in 32
calls), this algorithm calls it at most 6 times, and usually only once
or twice.  If you hit the month cache, of course, it doesn't call it
at all.

timelocal is implemented using the same cache.  We just assume that we're
translating a GMT time, and then fudge it when we're done for the timezone
and daylight savings arguments.  The timezone is determined by examining
the result of localtime(0) when the package is initialized.  The daylight
savings offset is currently assumed to be one hour.

Both routines return -1 if the integer limit is hit. I.e. for dates
after the 1st of January, 2038 on most machines.

=cut

BEGIN {
    $SEC  = 1;
    $MIN  = 60 * $SEC;
    $HR   = 60 * $MIN;
    $DAY  = 24 * $HR;
    $epoch = (localtime(2*$DAY))[5];	# Allow for bugs near localtime == 0.

    $YearFix = ((gmtime(946684800))[5] == 100) ? 100 : 0;

}

sub timegm {
    $ym = pack(C2, @_[5,4]);
    $cheat = $cheat{$ym} || &cheat;
    return -1 if $cheat<0 and $^O ne 'VMS';
    $cheat + $_[0] * $SEC + $_[1] * $MIN + $_[2] * $HR + ($_[3]-1) * $DAY;
}

sub timelocal {
    my $t = &timegm;
    my $tt = $t;

    my (@lt) = localtime($t);
    my (@gt) = gmtime($t);
    if ($t < $DAY and ($lt[5] >= 70 or $gt[5] >= 70 )) {
      # Wrap error, too early a date
      # Try a safer date
      $tt = $DAY;
      @lt = localtime($tt);
      @gt = gmtime($tt);
    }

    my $tzsec = ($gt[1] - $lt[1]) * $MIN + ($gt[2] - $lt[2]) * $HR;

    my($lday,$gday) = ($lt[7],$gt[7]);
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
    return -1 if $cheat<0 and $^O ne 'VMS';
    @test = localtime($time + ($tt - $t));
    $time -= $HR if $test[2] != $_[2];
    $time;
}

sub cheat {
    $year = $_[5];
    $year -= 1900
    	if $year > 1900;
    $month = $_[4];
    croak "Month '$month' out of range 0..11"	if $month > 11 || $month < 0;
    croak "Day '$_[3]' out of range 1..31"	if $_[3] > 31 || $_[3] < 1;
    croak "Hour '$_[2]' out of range 0..23"	if $_[2] > 23 || $_[2] < 0;
    croak "Minute '$_[1]' out of range 0..59"	if $_[1] > 59 || $_[1] < 0;
    croak "Second '$_[0]' out of range 0..59"	if $_[0] > 59 || $_[0] < 0;
    $guess = $^T;
    @g = gmtime($guess);
    $year += $YearFix if $year < $epoch;
    $lastguess = "";
    $counter = 0;
    while ($diff = $year - $g[5]) {
	croak "Can't handle date (".join(", ",@_).")" if ++$counter > 255;
	$guess += $diff * (363 * $DAY);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    return -1; #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    while ($diff = $month - $g[4]) {
	croak "Can't handle date (".join(", ",@_).")" if ++$counter > 255;
	$guess += $diff * (27 * $DAY);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    return -1; #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    @gfake = gmtime($guess-1); #still being sceptic
    if ("@gfake" eq $lastguess){
	return -1; #date beyond this machine's integer limit
    }
    $g[3]--;
    $guess -= $g[0] * $SEC + $g[1] * $MIN + $g[2] * $HR + $g[3] * $DAY;
    $cheat{$ym} = $guess;
}

1;
