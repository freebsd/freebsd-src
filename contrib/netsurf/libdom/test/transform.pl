#!/usr/bin/perl
# This file is part of libdom.
# It is used to generate libdom test files from the W3C DOMTS.
#
# Licensed under the MIT License,
#                http://www.opensource.org/licenses/mit-license.php
# Author: Bo Yang <struggleyb.nku@gmail.com>

use warnings;
use strict;

use lib qw(test);

use XML::Parser::PerlSAX;
use DOMTSHandler;

if ($#ARGV ne 2) {
	die "Usage: perl transform.pl dtd-file testcase basedir testcase-file";
}

my $handler = DOMTSHandler->new($ARGV[0], $ARGV[1]);
my $parser = XML::Parser::PerlSAX->new(Handler => $handler);
$parser->parse(Source => {SystemId => "$ARGV[2]"});
