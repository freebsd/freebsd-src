#! xPERL_PATHx
# -*-Perl-*-
#
# $Id: cln_hist.pl,v 1.2 1995/07/10 02:01:26 kfogel Exp $
# Contributed by David G. Grubbs <dgg@ksr.com>
#
# Clean up the history file.  10 Record types: MAR OFT WUCG
#
# WUCG records are thrown out.
# MAR records are retained.
# T records: retain only last tag with same combined tag/module.
#
# Two passes:  Walk through the first time and remember the
#	1. Last Tag record with same "tag" and "module" names.
#	2. Last O record with unique user/module/directory, unless followed
#	   by a matching F record.
#

$r = $ENV{"CVSROOT"};
$c = "$r/CVSROOT";
$h = "$c/history";

eval "print STDERR \$die='Unknown parameter $1\n' if !defined \$$1; \$$1=\$';"
    while ($ARGV[0] =~ /^(\w+)=/ && shift(@ARGV));
exit 255 if $die;               # process any variable=value switches

%tags = ();
%outs = ();

#
# Move history file to safe place and re-initialize a new one.
#
rename($h, "$h.bak");
open(XX, ">$h");
close(XX);

#
# Pass1 -- remember last tag and checkout.
#
open(HIST, "$h.bak");
while (<HIST>) {
    next if /^[MARWUCG]/;

    # Save whole line keyed by tag|module
    if (/^T/) {
	@tmp = split(/\|/, $_);
	$tags{$tmp[4] . '|' . $tmp[5]} = $_;
    }
    # Save whole line
    if (/^[OF]/) {
	@tmp = split(/\|/, $_);
	$outs{$tmp[1] . '|' . $tmp[2] . '|' . $tmp[5]} = $_;
    }
}

#
# Pass2 -- print out what we want to save.
#
open(SAVE, ">$h.work");
open(HIST, "$h.bak");
while (<HIST>) {
    next if /^[FWUCG]/;

    # If whole line matches saved (i.e. "last") one, print it.
    if (/^T/) {
	@tmp = split(/\|/, $_);
	next if $tags{$tmp[4] . '|' . $tmp[5]} ne $_;
    }
    # Save whole line
    if (/^O/) {
	@tmp = split(/\|/, $_);
	next if $outs{$tmp[1] . '|' . $tmp[2] . '|' . $tmp[5]} ne $_;
    }

    print SAVE $_;
}

#
# Put back the saved stuff
#
system "cat $h >> $h.work";

if (-s $h) {
    rename ($h, "$h.interim");
    print "history.interim has non-zero size.\n";
} else {
    unlink($h);
}

rename ("$h.work", $h);

exit(0);
