#!/usr/bin/perl
# $FreeBSD$

use Text::Iconv;
use Encode;
use strict;
use utf8;

die "Usage: $0 filename.kbd CHARSET" unless ($ARGV[1]);
my $converter = Text::Iconv->new($ARGV[1], "UTF-8");

sub local_to_UCS_string
{
    my ($string) = @_;

    return $converter->convert($string);
}

sub prettyprint_token
{
    my ($code) = @_;

    return "'" . chr($code) . "'"
        if 32 <= $code and $code <= 126; # print as ASCII if possible
#    return sprintf "%d", $code; # <---- temporary decimal
    return sprintf "0x%02x", $code
        if $code <= 255;        # print as hex number, else
    return sprintf "0x%04x", $code;
}

sub local_to_UCS_code
{
    my ($char) = @_;

    return prettyprint_token(ord(Encode::decode("UTF-8", local_to_UCS_string($char))));
}


sub convert_token
{
    my ($C) = @_;

    return $1
        if $C =~ m/^([a-z][a-z0-9]*)$/; # key token
    return local_to_UCS_code(chr($1))
        if $C =~ m/^(\d+)$/;            # decimal number
    return local_to_UCS_code(chr(hex($1)))
        if $C =~ m/^0x([0-9a-f]+)$/i;   # hex number
    return local_to_UCS_code($1)
        if $C =~ m/^'(.)'$/;            # character
    return "<?$C?>";                    # uncovered case
}

sub tokenize { # split on white space and parentheses (but not within token)
    my ($line) = @_;

    $line =~ s/' '/ _spc_ /g; # prevent splitting of ' '
    $line =~ s/'\('/ _lpar_ /g; # prevent splitting of '('
    $line =~ s/'\)'/ _rpar_ /g; # prevent splitting of ')'
    $line =~ s/([()])/ $1 /g; # insert blanks around remaining parentheses
    my @KEYTOKEN = split (" ", $line);
    grep(s/_spc_/' '/, @KEYTOKEN);
    grep(s/_lpar_/'('/, @KEYTOKEN);
    grep(s/_rpar_/')'/, @KEYTOKEN);
    return @KEYTOKEN;
}

# main program
open FH, "<$ARGV[0]";
while (<FH>) {
    if (m/^#/) {
	print local_to_UCS_string($_);
    } elsif (m/^\s*$/) {
	print "\n";
    } else {
	my @KEYTOKEN = tokenize($_);
	my $at_bol = 1;
	my $C;
	foreach $C (@KEYTOKEN) {
	    if ($at_bol) {
		if ($C =~ m/^\s*\d/) { # line begins with key code number
		    printf "  %03d   ", $C;
		} elsif ($C =~ m/^[a-z]/) { # line begins with accent name or paren
		    printf "  %-4s ", $C; # accent name starts accent definition
		} elsif ($C eq "(") {
		    printf "%17s", "( "; # paren continues accent definition
		} else {
		    print "UNKNOWN DEFINITION: $_";
		}
		$at_bol = 0;
	    } else {
		if ($C =~ m/^([BCNO])$/) {
		    print " $1"; # special case: effect of Caps Lock/Num Lock
		} elsif ($C eq "(") {
		    print " ( ";
		} elsif ($C eq ")") {
		    print " )";
		} else {
		    printf "%-6s ", convert_token($C);
		}
	    }
	}
	print "\n";
    }
}
close FH;
