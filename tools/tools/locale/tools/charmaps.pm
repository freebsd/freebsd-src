#!/usr/local/bin/perl -w

# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2009 Edwin Groothuis <edwin@FreeBSD.org>
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
#

use strict;
use XML::Parser;
use Data::Dumper;

my %data = ();
my %d = ();
my $index = -1;

sub get_xmldata {
	my $etcdir = shift;

	open(FIN, "$etcdir/charmaps.xml");
	my @xml = <FIN>;
	chomp(@xml);
	close(FIN);

	my $xml = new XML::Parser(Handlers => {
					Start	=> \&h_start,
					End	=> \&h_end,
					Char	=> \&h_char
					});
	$xml->parse(join("", @xml));
	return %d;
}

sub h_start {
	my $expat = shift;
	my $element = shift;
	my @attrs = @_;
	my %attrs = ();


	while ($#attrs >= 0) {
		$attrs{$attrs[0]} = $attrs[1];
		shift(@attrs);
		shift(@attrs);
	}

	$data{element}{++$index} = $element;

	if ($index == 2
	 && $data{element}{1} eq "languages"
	 && $element eq "language") {
		my $name = $attrs{name};
		my $countries = $attrs{countries};
		my $encoding = $attrs{encoding};
		my $family = $attrs{family};
		my $f = defined $attrs{family} ? $attrs{family} : "x";
		my $nc_link = $attrs{namecountry_link};
		my $e_link = $attrs{encoding_link};
		my $fallback = $attrs{fallback};
		my $definitions = $attrs{definitions};

		$d{L}{$name}{$f}{fallback} = $fallback;
		$d{L}{$name}{$f}{e_link} = $e_link;
		$d{L}{$name}{$f}{nc_link} = $nc_link;
		$d{L}{$name}{$f}{family} = $family;
		$d{L}{$name}{$f}{encoding} = $encoding;
		$d{L}{$name}{$f}{definitions} = $definitions;
		$d{L}{$name}{$f}{countries} = $countries;
		foreach my $c (split(" ", $countries)) {
			if (defined $encoding) {
				foreach my $e (split(" ", $encoding)) {
					$d{L}{$name}{$f}{data}{$c}{$e} = undef;
					$d{E}{$e} = 0;	# not read
				}
			}
			$d{L}{$name}{$f}{data}{$c}{"UTF-8"} = undef;
		}
		return;
	}

	if ($index == 2
	 && $data{element}{1} eq "translations"
	 && $element eq "translation") {
		foreach my $e (split(" ", $attrs{encoding})) {
			if (defined $attrs{hex}) {
				my $k = $attrs{cldr};
				my $hs = $attrs{hex};
				$d{T}{$e}{$k}{hex} = $hs;
			}
			if (defined $attrs{string}) {
				my $s = "";
				for (my $i = 0; $i < length($attrs{string}); $i++) {
					$s .= sprintf("%02x",
					    ord(substr($attrs{string}, $i, 1)));
				}
				$d{T}{$e}{$attrs{cldr}}{hex} = $s;
			}
			if (defined $attrs{unicode}) {
				my $k = $attrs{cldr};
				my $uc = $attrs{unicode};
				$d{T}{$e}{$k}{unicode} = $uc;
			}
			if (defined $attrs{ucc}) {
				my $k = $attrs{cldr};
				my $uc = $attrs{ucc};
				$d{T}{$e}{$k}{ucc} = $uc;
			}
		}
		return;
	}

	if ($index == 2
	 && $data{element}{1} eq "alternativemonths"
	 && $element eq "language") {
		my $name = $attrs{name};
		my $countries = $attrs{countries};

		$data{fields}{name} = $name;
		$data{fields}{countries} = $countries;
		$data{fields}{text} = "";

		return;
	}
}

sub h_end {
	my $expat = shift;
	my $element = shift;

	if ($index == "2") {
		if ($data{element}{1} eq "alternativemonths"
		 && $data{element}{2} eq "language") {
			foreach my $c (split(/,/, $data{fields}{countries})) {
				my $m = $data{fields}{text};

				$m =~ s/^[\t ]//g;
				$m =~ s/[\t ]$//g;
				$d{AM}{$data{fields}{name}}{$c} = $m;
			}
			$data{fields} = ();
		}
	}

	$index--;
}

sub h_char {
	my $expat = shift;
	my $string = shift;

	if ($index == "2") {
		if ($data{element}{1} eq "alternativemonths"
		 && $data{element}{2} eq "language") {
			$data{fields}{text} .= $string;
		}
	}
}

#use Data::Dumper;
#my %D = get_xmldata();
#print Dumper(%D);
1;
