#!/usr/bin/perl -w
# Emacs should use -*- cperl -*- mode
#
# Copyright (c) 2003-2006 Simon L. Nielsen <simon@FreeBSD.org>
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
# man2hwnotes.pl [-cl] [-d 0-6] [-a <archlist file>] [-o <outputfile>]
#                <manualpage> [<manualpage> ...]

use strict;
use Getopt::Std;
use Digest::MD5 qw(md5_hex);

# Section from manual page to extract
my $hwlist_sect = "HARDWARE";

# Override default archtecture list for some devices:
my $archlist_file = "dev.archlist.txt";
my %archlist;

# Globals
my $compat_mode = 0; # Enable compat for old Hardware Notes style
my $debuglevel = 0;
my $only_list_out = 0; # Should only lists be generated in the output?
my @out_lines; # Single lines
my @out_dev;   # Device entities

# Getopt
my %options = ();
if (!getopts("a:cd:lo:",\%options)) {
    die("$!: Invalid command line arguments in ", __LINE__, "\n");
}

if (defined($options{c})) {
    $compat_mode = 1;
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
        # Make sure ampersand is encoded as &amp; since jade seems to
        # be confused when it is encoded as &#38; inside an entity.
        $l =~ s/&#38;/&amp;/g;
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
    $mdocvars{first_para} = 1;
    $mdocvars{parabuf} = "";
    $mdocvars{listtype} = "";
    $mdocvars{it_nr} = 0;

    open(MANPAGE, "$manpage") || die("$!: Could not open $manpage in ", __LINE__, ".\n");
    while(<MANPAGE>) {
	chomp;
	my $line = $_;

	dlog(5, "Read '$line'");

	# Find commands
	if (s/^\.(.*)$/$1/) {
	    my $cmd = $1;

	    # Detect, and ignore, comment lines
	    if (s/^\\"(.*)$/$1/) {
		next;
	    }

	    $cmd =~ s/^([^ ]+).*$/$1/;

	    if (/^Nm "?(\w+)"?/ && !defined($mdocvars{Nm})) {
		dlog(3, "Setting Nm to $1");
		$mdocvars{Nm} = $1;
		# "_" cannot be used for an entity name.
		$mdocvars{EntNm} = $1;
		$mdocvars{EntNm} =~ s,_,.,g;

	    } elsif (/^Nm$/) {
		if (defined($mdocvars{Nm}) && $mdocvars{Nm} ne "") {
		    parabuf_addline(\%mdocvars, "&man.".$mdocvars{EntNm}.".$cur_mansection;");
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
				    "&man.".$mdocvars{EntNm}.".$cur_mansection; " .
				    "&hwlist.preamble.post;</para>");
		    }
		} elsif ($mdocvars{isin_hwlist}) {
		    dlog(2, "Found a HWLIST STOP key!");
		    add_sgmltag(\%mdocvars, "'>");
		    $mdocvars{isin_hwlist} = 0;
		}
		if ($mdocvars{isin_list}) {
		    dlog(1, "Warning: Still in list, but just entered new " .
			 "section.  This is probably due to missing .El; " .
			 "check manual page for errors.");
		    # If we try to recover from this we will probably
		    # just end with bad SGML output and it really
		    # should be fixed in the manual page so we don't
		    # even try to "fix" this.
		}


	    } elsif (/^Dt ([^ ]+) ([^ ]+)/) {
		dlog(4, "Setting mansection to $2");
		$mdocvars{cur_manname} = lc($1);
		$cur_mansection = $2;

		# "_" cannot be used for an entity name.
		$mdocvars{cur_manname} =~ s,_,.,g;

	    } elsif (/^It ?(.*)$/) {
		my $txt = $1;

		$mdocvars{it_nr}++;

		# Flush last item
		if ($mdocvars{parabuf} ne "") {
		    add_listitem(\%mdocvars);
		}

		# Remove quotes, if any.
		$txt =~ s/"(.*)"/$1/;

		if ($mdocvars{listtype} eq "column") {
		    # Ignore first item when it is likely to be a
		    # header.
		    if ($mdocvars{it_nr} == 1 && $txt =~ m/^(Em|Sy) /) {
			dlog(2, "Skipping header line in column list");
			next;
		    }
		    # Only extract the first column.
		    $txt =~ s/ Ta /\t/g;
		    $txt =~ s/([^\t]+)\t.*/$1/;
		}

		# Remove Li commands
		$txt =~ s/^Li //g;

		parabuf_addline(\%mdocvars, normalize($txt));
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
		} elsif (/-column/) {
		    $mdocvars{listtype} = "column";
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
		my ($txt, $punct_str) = split_punct_chars($1);

		parabuf_addline(\%mdocvars, normalize($txt . $punct_str));
	    } elsif (/^Xr ([^ ]+) (.+)$/) {
		my ($xr_sect, $punct_str) = split_punct_chars($2);
		my $txt;

		# We need to check if the manual page exist to avoid
		# breaking the doc build just because of a broken
		# reference.
		#$txt = "&man.$1.$xr_sect;$punct_str";
		$txt = "$1($xr_sect)$punct_str";
		parabuf_addline(\%mdocvars, normalize($txt));
	    } elsif (/^Dq (.+)$/) {
		my ($txt, $punct_str) = split_punct_chars($1);

		parabuf_addline(\%mdocvars,
				normalize("<quote>$txt</quote>$punct_str"));
	    } elsif (/^Sx (.+)$/) {
		if ($mdocvars{isin_hwlist}) {
		    dlog(1, "Warning: Reference to another section in the " .
			 "$hwlist_sect section in " . $mdocvars{Nm} .
			 "(${cur_mansection})");
		}
		parabuf_addline(\%mdocvars, normalize($1));
	    } elsif (/^Pa (.+)$/) {
		my ($txt, $punct_str) = split_punct_chars($1);

		$txt = make_ulink($txt) . $punct_str;
		parabuf_addline(\%mdocvars, normalize($txt));
	    } elsif (/^Pp/) {
		dlog(3, "Got Pp command - forcing new para");
		flush_out(\%mdocvars);
	    } elsif (/^Fx (.+)/) {
		dlog(3, "Got Fx command");
		parabuf_addline(\%mdocvars, "FreeBSD $1");
	    } elsif (/^Fx/) {
		dlog(3, "Got Fx command");
		parabuf_addline(\%mdocvars, "FreeBSD");
	    } else {
		# Ignore all other commands.
		dlog(3, "Ignoring unknown command $cmd");
	    }
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
	if ($compat_mode) {
	    $para_arch = ' arch="' . $archlist{${$mdocvars}{Nm}} . '"';
	} else {
	    $para_arch = '[' . $archlist{${$mdocvars}{Nm}} . '] ';
	}
    }
    if ($compat_mode) {
	$out = "<para".$para_arch.">&".$entity_name.";</para>";
    } else {
	if (${$mdocvars}{first_para}) {
	    $out = "<para>".$para_arch."&".$entity_name.";</para>";
	} else {
	    $out = "<para>&".$entity_name.";</para>";
	}
	${$mdocvars}{first_para} = 0;
    }

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

    if ($compat_mode) {
	if(defined($archlist{${$mdocvars}{Nm}})) {
	    $para_arch = ' arch="' . $archlist{${$mdocvars}{Nm}} . '"';
	}
    }
    $listitem = "<listitem><para".$para_arch.">&".$entity_name.";</para></listitem>";
    dlog(4, "Adding '$listitem' to out_dev");
    push(@out_dev, $listitem);

}

# Add a line to the "paragraph buffer"
sub parabuf_addline {
    my $mdocvars = shift;
    my ($txt) = (@_);

    dlog(5, "Now in parabuf_addline for '$txt'");

    # We only care about the HW list for now.
    if (!${$mdocvars}{isin_hwlist}) {
	dlog(6, "Exiting parabuf_addline due to: !\${\$mdocvars}{isin_hwlist}");
	return;
    }
    if ($txt eq "") {
	dlog(6, "Exiting parabuf_addline due to: \$txt eq \"\"");
	return;
    }

    if ($only_list_out && !${$mdocvars}{isin_list}) {
	dlog(6, "Exiting parabuf_addline due to: ".
	     "\$only_list_out && !\${\$mdocvars}{isin_list}");
	return;
    }

    # We only add the first line for "tag" lists
    if (${$mdocvars}{parabuf} ne "" && ${$mdocvars}{isin_list} &&
	${$mdocvars}{listtype} eq "tag") {
	dlog(6, "Exiting parabuf_addline due to: ".
	     "\${\$mdocvars}{parabuf} ne \"\" && \${\$mdocvars}{isin_list} && ".
	     "\${\$mdocvars}{listtype} eq \"tag\"");
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
	    dlog(1, "Warning: Could not parse archlist line $lineno");
	}
    }

    close(FILE);
}

# Check if a character is a mdoc(7) punctuation character.
sub is_punct_char {
    my ($str) = (@_);

    return (length($str) == 1 && $str =~ /[\.,:;()\[\]\?!]/);
}

# Split out the punctuation characters of a mdoc(7) line.
sub split_punct_chars {
    my ($str) = (@_);
    my (@stritems, $stritem, $punct_str);

    $punct_str = "";
    @stritems = split(/ /, $str);

    while (defined($stritem = $stritems[$#stritems]) &&
	   is_punct_char($stritem)) {
	$punct_str = $stritem . $punct_str;
	pop(@stritems);
    }

    return (join(' ', @stritems), $punct_str);
}

# Create a ulink, if the string contains an URL.
sub make_ulink {
    my ($str) = (@_);

    $str =~ s,(http://[^ ]+),<ulink url="$1"></ulink>,;

    return $str;
}
