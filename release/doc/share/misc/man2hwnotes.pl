#!/usr/bin/perl -w
# Emacs should use -*- cperl -*- mode
#
# Copyright (c) 2003-2004 Simon L. Nielsen <simon@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

# Parse the list of supported hardware out of section 4 manual pages
# and output it on stdout as SGML/DocBook entities.

# The script will look for the following line in the manual page:
# .Sh HARDWARE
# and make an entity of the content until the line containing:
# .Sh
#
# For Lists only the first line will be printed.  If there are
# arguments to the .It command, only the argument will be printed.

# Usage:
# mdoc2sgml [-l] [-d 0-6] [-a <archlist file>] [-o <outputfile>] <manualpage> [<manualpage> ...]

use strict;
use Getopt::Std;
use Digest::MD5 qw(md5_hex);

# Section from manual page to extract
my $hwlist_sect = "HARDWARE";

# Override default archtecture list for some devices:
my $archlist_file = "dev.archlist.txt";
my %archlist;

# Globals
my $debuglevel = 0;
my $only_list_out = 0; # Should only lists be generated in the output?
my @out_lines; # Single lines
my @out_dev;   # Device entities

# Getopt
my %options = ();
if (!getopts("a:d:lo:",\%options)) {
    die("$!: Invalid command line arguments in ", __LINE__, "\n");
}

if (defined($options{d})) {
    $debuglevel = $options{d};
}
if (defined($options{a})) {
    $archlist_file = $options{a};
}
if (defined($options{l})) {
    $only_list_out = 1;
}

my $outputfile = $options{o};

if ($debuglevel > 0) {
    # Don't do output buffering in debug mode.
    $| = 1;
}

load_archlist($archlist_file);

if (defined($outputfile)) {
    open(OLDOUT, ">&STDOUT") || die("$!: Could not open STDOUT in ", __LINE__, ".\n");
    open(STDOUT, ">$outputfile") || die("$!: Could not open $outputfile in ", __LINE__, ".\n");
}

print <<EOT;
<!--
 These are automatically generated device lists for FreeBSD hardware notes.
-->
EOT

if ($only_list_out) {
    # Print the default device preamble entities
    print "<!ENTITY hwlist.preamble.pre 'The'>\n";
    print "<!ENTITY hwlist.preamble.post 'driver supports:'>\n";
}

foreach my $page (@ARGV) {
    if ($page !~ m/\.4$/) {
        dlog(2, "Skipped $page (not *.4)");
        next;
    }
    dlog(2, "Parsing $page");
    parse($page);

    if (@out_lines) {
        print join("\n", @out_lines), "\n";
    }
    if (@out_dev) {
        print join("\n", @out_dev), "\n";
    }

    @out_lines = ();
    @out_dev = ();
}

if (defined($outputfile)) {
    open(STDOUT, ">&OLDOUT") || die("$!: Could not open STDOUT in ", __LINE__, ".\n");
    close(OLDOUT) || die("$!: Could not close OLDOUT in ", __LINE__, ".\n");
}

sub normalize (@) {
    my @lines = @_;

    foreach my $l (@lines) {
        $l =~ s/\\&//g;
        $l =~ s:([\x21-\x2f\x5b-\x60\x7b-\x7f]):sprintf("&\#\%d;", ord($1)):eg;
    }
    return (wantarray) ? @lines : join "", @lines;
}

sub parse {
    my ($manpage) = @_;

    my $cur_mansection;
    my $found_hwlist = 0;
    my %mdocvars;
    $mdocvars{isin_hwlist} = 0;
    $mdocvars{isin_list} = 0;
    $mdocvars{parabuf} = "";
    $mdocvars{listtype} = "";

    open(MANPAGE, "$manpage") || die("$!: Could not open $manpage in ", __LINE__, ".\n");
    while(<MANPAGE>) {
	chomp;
	my $line = $_;

	dlog(5, "Read '$line'");

	# Find commands
	if (s/^\.(.*)$/$1/) {
	    # Detect, and ignore, comment lines
	    if (s/^\\"(.*)$/$1/) {
		next;
	    }

	    if (/^Nm "?(\w+)"?/ && !defined($mdocvars{Nm})) {
		dlog(3, "Setting Nm to $1");
		$mdocvars{Nm} = $1;

	    } elsif (/^Nm$/) {
		if (defined($mdocvars{Nm}) && $mdocvars{Nm} ne "") {
		    parabuf_addline(\%mdocvars, "&man.".$mdocvars{Nm}.".$cur_mansection;");
		} else {
		    dlog(2, "Warning: Bad Nm call in $manpage");
		}

	    } elsif (/^Sh (.+)$/) {
		dlog(4, "Setting section to $1");
		my $cur_section = $1;

		flush_out(\%mdocvars);

		if ($cur_section =~ /^${hwlist_sect}$/) {
		    dlog(2, "Found the device section ${hwlist_sect}");
		    $mdocvars{isin_hwlist} = 1;
		    $found_hwlist = 1;
		    add_sgmltag(\%mdocvars, "<!ENTITY hwlist.".$mdocvars{cur_manname}." '");
		    if ($only_list_out) {
			add_sgmltag("<para>&hwlist.preamble.pre; " .
				    "&man.".$mdocvars{Nm}.".$cur_mansection; " .
				    "&hwlist.preamble.post;</para>");
		    }
		} elsif ($mdocvars{isin_hwlist}) {
		    dlog(2, "Found a HWLIST STOP key!");
		    add_sgmltag(\%mdocvars, "'>");
		    $mdocvars{isin_hwlist} = 0;
		}

	    } elsif (/^Dt ([^ ]+) ([^ ]+)/) {
		dlog(4, "Setting mansection to $2");
		$mdocvars{cur_manname} = lc($1);
		$cur_mansection = $2;

	    } elsif (/^It ?(.*)$/) {
		# Flush last item
		if ($mdocvars{parabuf} ne "") {
		    add_listitem(\%mdocvars);
		}
		parabuf_addline(\%mdocvars, normalize($1));
	    } elsif (/^Bl/) {
		$mdocvars{isin_list} = 1;
		flush_out(\%mdocvars);
		add_sgmltag(\%mdocvars, "<itemizedlist>");

		if (/-tag/) {
		    $mdocvars{listtype} = "tag";
		    # YACK! Hack for ata(4)
		    if ($mdocvars{Nm} eq "ata") {
			$mdocvars{listtype} = "tagHACK";
		    }
		} elsif (/-bullet/) {
		    $mdocvars{listtype} = "bullet";
		} else {
		    $mdocvars{listtype} = "unknown";
		}
		dlog(2, "Listtype set to $mdocvars{listtype}");
	    } elsif (/^El/) {
		if ($mdocvars{parabuf} ne "") {
		    add_listitem(\%mdocvars);
		}

		add_sgmltag(\%mdocvars, "</itemizedlist>");
		$mdocvars{isin_list} = 0;
	    } elsif (/^Tn (.+)$/) {
		# For now we print TradeName text as regular text.
		my $txt = $1;
		$txt =~ s/^(.+) ,$/$1,/;

		parabuf_addline(\%mdocvars, normalize($txt));
	    } elsif (/^Xr (.+) (.+)/) {
		# We need to check if the manual page exist to avoid
		# breaking the doc build just because of a broken
		# reference.
		#parabuf_addline(\%mdocvars, "&man.$1.$2;");
		parabuf_addline(\%mdocvars, normalize("$1($2)"));
	    } elsif (/^Dq (.+)$/) {
		my (@stritems, $stritem, $punct_str);
		$punct_str = "";
		@stritems = split(/ /, $1);

		# Handle trailing punctuation characters specially.
		while (defined($stritem = $stritems[$#stritems]) &&
		       is_punct_char($stritem)) {
		    $punct_str = $stritem . $punct_str;
		    pop(@stritems);
		}
		my $txt = "<quote>".join(' ', @stritems)."</quote>$punct_str";

		parabuf_addline(\%mdocvars, normalize($txt));
	    } elsif (/^Sx (.+)$/) {
		if ($mdocvars{isin_hwlist}) {
		    dlog(1, "Warning: Reference to another section in the " .
			 "$hwlist_sect section in " . $mdocvars{Nm} .
			 "(${cur_mansection})");
		}
		parabuf_addline(\%mdocvars, normalize($1));
	    }
	    # Ignore all other commands
	} else {
	    # This is then regular text
	    parabuf_addline(\%mdocvars, normalize($_));
	}
    }
    close(MANPAGE) || die("$!: Could not close $manpage in ", __LINE__, ".\n");
    if (! $found_hwlist) {
	dlog(2, "Hardware list not found in $manpage");
    }
}

sub dlog {
    my ($level, $txt) = @_;

    if ($level <= $debuglevel) {
	print STDERR "$level: $txt\n";
    }
}

# Output a SGML tag.
sub add_sgmltag {
    my ($mdocvars, $txt) = (@_);

    # We only care about the HW list for now.
    if (${$mdocvars}{isin_hwlist}) {
	push(@out_dev, $txt);
    }
}

# Add a text entity, and return the used entity name.
sub add_txt_ent {
    my ($itemtxt) = (@_);
    my ($entity_name);

    # Convert mdoc(7) minus
    $itemtxt =~ s/\\-/-/g;

    $itemtxt =~ s/'/&lsquo;/g;

    $entity_name = "hwlist." . md5_hex($itemtxt);
    dlog(4, "Adding '$itemtxt' as entity $entity_name");
    push(@out_lines, "<!ENTITY $entity_name '$itemtxt'>");

    return ($entity_name);
}
sub flush_out {
    my ($mdocvars) = (@_);
    my ($entity_name, $out);
    my $para_arch = "";

    if (!${$mdocvars}{isin_hwlist} || ${$mdocvars}{parabuf} eq "") {
	return;
    }

    $entity_name = add_txt_ent(${$mdocvars}{parabuf});
    ${$mdocvars}{parabuf} = "";
    if(defined($archlist{${$mdocvars}{Nm}})) {
	$para_arch = ' arch="' . $archlist{${$mdocvars}{Nm}} . '"';
    }
    $out = "<para".$para_arch.">&".$entity_name.";</para>";

    dlog(4, "Flushing parabuf");
    add_sgmltag($mdocvars, $out);
}

# Add a new list item from the "parabuf".
sub add_listitem {
    my ($mdocvars) = (@_);
    my ($listitem, $entity_name);
    my $para_arch = "";

    $entity_name = add_txt_ent(${$mdocvars}{parabuf});
    ${$mdocvars}{parabuf} = "";

    if(defined($archlist{${$mdocvars}{Nm}})) {
	$para_arch = ' arch="' . $archlist{${$mdocvars}{Nm}} . '"';
    }
    $listitem = "<listitem><para".$para_arch.">&".$entity_name.";</para></listitem>";
    dlog(4, "Adding '$listitem' to out_dev");
    push(@out_dev, $listitem);

}

# Add a line to the "paragraph buffer"
sub parabuf_addline {
    my $mdocvars = shift;
    my ($txt) = (@_);

    dlog(5, "Now in parabuf_addline");

    # We only care about the HW list for now.
    if (!${$mdocvars}{isin_hwlist}) {
	return;
    }
    if ($txt eq "") {
	return;
    }

    if ($only_list_out && !${$mdocvars}{isin_list}) {
	return;
    }

    # We only add the first line for "tag" lists
    if (${$mdocvars}{parabuf} ne "" && ${$mdocvars}{isin_list} &&
	${$mdocvars}{listtype} eq "tag") {
	return;
    }

    if (${$mdocvars}{parabuf} ne "") {
	${$mdocvars}{parabuf} .= " ";
    }

    dlog(4, "Adding '$txt' to parabuf");

    ${$mdocvars}{parabuf} .= $txt;
}

sub load_archlist {
    my ($file) = (@_);

    my $lineno = 0;

    dlog(2, "Parsing archlist $file");

    open(FILE, "$file") || die("$!: Could not open archlist $file in ", __LINE__, ".\n");
    while(<FILE>) {
	chomp;
	$lineno++;

	if (/^#/ || $_ eq "") {
	    next;
	}

	if (/(\w+)\t([\w,]+)/) {
	    dlog(4, "For driver $1 setting arch to $2");
	    $archlist{$1} = $2;
	} else {
	    dlog(1, "Could not parse line $lineno");
	}
    }

    close(FILE);
}

# Check if a character is a mdoc(7) punctuation character.
sub is_punct_char {
    my ($str) = (@_);

    return ($str =~ /[\.,:;()\[\]\?!]/);
}
