#! /usr/bin/env perl

use strict;
# runs groff in safe mode, that seems to be the default
# installation now. That means that I have to fix all nice
# features outside groff. Sigh.
# I do agree however that the previous way opened a whole bunch
# of security holes.

my $no_exec;
# check for -x and remove it
if (grep(/^-x$/, @ARGV)) {
	$no_exec++;
	@ARGV = grep(!/^-x$/, @ARGV);
}

# mmroff should always have -mm, but not twice
@ARGV = grep(!/^-mm$/, @ARGV);
my $check_macro = "groff -rRef=1 -z -mm @ARGV";
my $run_macro = "groff -mm @ARGV";

my (%cur, $rfilename, $max_height, $imacro, $max_width, @out, @indi);
open(MACRO, "$check_macro 2>&1 |") || die "run $check_macro:$!";
while(<MACRO>) {
	if (m#^\.\\" Rfilename: (\S+)#) {
		# remove all directories just to be more secure
		($rfilename = $1) =~ s#.*/##;
		next;
	}
	if (m#^\.\\" Imacro: (\S+)#) {
		# remove all directories just to be more secure
		($imacro = $1) =~ s#.*/##;
		next;
	}
	if (m#^\.\\" Index: (\S+)#) {
		# remove all directories just to be more secure
		my $f;
		($f = $1) =~ s#.*/##;
		&print_index($f, \@indi, $imacro);
		@indi = ();
		$imacro = '';
		next;
	}
	my $x;
	if (($x) = m#^\.\\" IND (.+)#) {
		$x =~ s#\\##g;
		my @x = split(/\t/, $x);
		grep(s/\s+$//, @x);
		push(@indi, join("\t", @x));
		next;
	}
	if (m#^\.\\" PIC id (\d+)#) {
		%cur = ('id', $1);
		next;
	}
	if (m#^\.\\" PIC file (\S+)#) {
		&psbb($1);
		&ps_calc($1);
		next;
	}
	if (m#^\.\\" PIC (\w+)\s+(\S+)#) {
		eval "\$cur{'$1'} = '$2'";
		next;
	}
	s#\\ \\ $##;
	push(@out, $_);
}
close(MACRO);


if ($rfilename) {
	push(@out, ".nr pict*max-height $max_height\n") if defined $max_height;
	push(@out, ".nr pict*max-width $max_width\n") if defined $max_width;

	open(OUT, ">$rfilename") || "create $rfilename:$!";
	print OUT '.\" references', "\n";
	my $i;
	for $i (@out) {
		print OUT $i;
	}
	close(OUT);
}

exit 0 if $no_exec;
exit system($run_macro);

sub print_index {
	my ($f, $ind, $macro) = @_;

	open(OUT, ">$f") || "create $f:$!";
	my $i;
	for $i (sort @$ind) {
		if ($macro) {
			$i = '.'.$macro.' "'.join('" "', split(/\t/, $i)).'"';
		}
		print OUT "$i\n";
	}
	close(OUT);
}

sub ps_calc {
	my ($f) = @_;

	my $w = abs($cur{'llx'}-$cur{'urx'});
	my $h = abs($cur{'lly'}-$cur{'ury'});
	$max_width = $w if $w > $max_width;
	$max_height = $h if $h > $max_height;

	my $id = $cur{'id'};
	push(@out, ".ds pict*file!$id $f\n");
	push(@out, ".ds pict*id!$f $id\n");
	push(@out, ".nr pict*llx!$id $cur{'llx'}\n");
	push(@out, ".nr pict*lly!$id $cur{'lly'}\n");
	push(@out, ".nr pict*urx!$id $cur{'urx'}\n");
	push(@out, ".nr pict*ury!$id $cur{'ury'}\n");
	push(@out, ".nr pict*w!$id $w\n");
	push(@out, ".nr pict*h!$id $h\n");
}
		

sub psbb {
	my ($f) = @_;

	unless (open(IN, $f)) {
		print STDERR "Warning: Postscript file $f:$!";
		next;
	}
	while(<IN>) {
		if (/^%%BoundingBox:\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/) {
			$cur{'llx'} = $1;
			$cur{'lly'} = $2;
			$cur{'urx'} = $3;
			$cur{'ury'} = $4;
		}
	}
	close(IN);
}
