#!/usr/bin/perl
#
# spkrtest - Test script for the speaker driver
#
# v1.0 by Eric S. Raymond (Feb 1990)
# v1.1 rightstuff contributed by Eric S. Tiedemann (est@snark.thyrsus.com)
# v2.0 dialog+perl by Wolfram Schneider <wosch@freebsd.org>, May 1995
#
# NOTE for iso-* (latin1) fonts: use TERM=cons25-iso8859-1
#
# $Id: $

$title = qq{
reveille   -- Reveille
contact	   -- Contact theme from Close Encounters
dance	   -- Lord of the Dance (aka Simple Gifts)
loony	   -- Loony Toons theme
sinister   -- standard villain's entrance music
rightstuff -- a trope from "The Right Stuff" score by Bill Conti
toccata	   -- opening bars of Bach's Toccata and Fugue in D Minor
startrek   -- opening bars of the theme from Star Trek Classic
};

$music = qq{
reveille   -- t255l8c.f.afc~c.f.afc~c.f.afc.f.a..f.~c.f.afc~c.f.afc~c.f.afc~c.f..
contact	   -- <cd<a#~<a#>f
dance	   -- t240<cfcfgagaa#b#>dc<a#a.~fg.gaa#.agagegc.~cfcfgagaa#b#>dc<a#a.~fg.gga.agfgfgf.
loony	   -- t255cf8f8edc<a>~cf8f8edd#e~ce8cdce8cd.<a>c8c8c#def8af8
sinister   -- mst200o2ola.l8bc.~a.~>l2d#
rightstuff -- olcega.a8f>cd2bgc.c8dee2
toccata	   -- msl16oldcd4mll8pcb-agf+4.g4p4<msl16dcd4mll8pa.a+f+4p16g4
startrek   -- l2b.f+.p16a.c+.p l4mn<b.>e8a2mspg+e8c+f+8b2
};

@checklist = ('/usr/bin/dialog', '--title',  'Speaker test', '--checklist',
    'Please select the melodies you wish to play (space for select)',
    '-1', '-1', '10');

$speaker = '/dev/speaker';

sub Exit {
    unlink $tmp if $tmp; exit;
}

$SIG{'INT'} = $SIG{'HUP'} = $SIG{'TRAP'} = $SIG{'QUIT'} =
    $SIG{'TERM'} = '&Exit';


# make assoc array from variable 'var'
# 'name -- description' -> $var{$name} = $description
sub splitconfig {
    local(*var) = @_;
    local($t, $name, $description);

    foreach $t (split('\n', $var)) {
	($name, $description) = split('--', $t);

	$name =~ s/^\s+//; $name =~ s/\s+$//;
	$description =~ s/\s+//; $description =~ s/\s+$//;

	$var{$name} = $description if $name && $description;
    }
}

&splitconfig(*title);
&splitconfig(*music);

foreach (sort keys %title) {
    push(@checklist, ($_, $title{$_}, 'OFF'));
}

$tmp = ($ENV{'TMP'} || "/tmp") . "/_spkrtest$$";

if (!open(SPEAKER, "> $speaker")) {
    warn "You have no write access to $speaker or the speaker device is not " .
	"enabled\nin kernel. Cannot play any melody! See spkr(4).\a\n";
    sleep 2;
}

open(SAVEERR, ">&STDERR") || die "open >&STDERR: $!\n";
open(STDERR, "> $tmp") || do { die "open > $tmp: $!\n"; };
system @checklist;		# start dialog
open(STDERR, ">&SAVEERR") || die "open >&SAVEERR: $!\n";
$return = $? >> 8;

# die if speaker device not avaiable
if (fileno(SPEAKER) eq "") {
    print "\nSorry, cannot play any melody!!!\n"; &Exit;
}


if (!$return) {			# not cancel
    select(SPEAKER); $| = 1;
    select(STDOUT);  $| = 1;

    if (! -z $tmp) {		# select melod(y/ies)
	print STDOUT "\n";
	open(STDIN, $tmp) || do { die "open $tmp: $!\n"; };
	foreach $melody (split($", <STDIN>)) {
	    $melody =~ s/^"//; $melody =~ s/"$//;
	    print STDOUT "$title{$melody}\n";
	    print SPEAKER "$music{$melody}";
	    sleep 1;
	}
    } else {			# use default melody
	$melody = (sort keys %title)[0];
	print STDOUT "Use default melody: $title{$melody}\n";
	print SPEAKER "$music{$melody}";
    }
    close SPEAKER;
}

&Exit;
