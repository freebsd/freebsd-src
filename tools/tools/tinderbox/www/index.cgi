#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003 Dag-Erling Coïdan Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

use 5.006_001;
use strict;
use POSIX qw(strftime);

my @BRANCHES = (
    'RELENG_4',
    'CURRENT'
);

my %ARCHES = (
    'alpha'	=> [ 'alpha' ],
    'amd64'	=> [ 'amd64' ],
    'i386'	=> [ 'i386', 'pc98' ],
    'ia64'	=> [ 'ia64' ],
    'powerpc'	=> [ 'powerpc' ],
    'sparc64'	=> [ 'sparc64' ],
);

my $DIR = ".";

sub success($) {
    my $log = shift;

    local *FILE;
    if (open(FILE, "<", $log)) {
	while (<FILE>) {
	    if (m/tinderbox run completed/) {
		close(FILE);
		return 1;
	    }
	}
	close(FILE);
    }
    return undef;
}

MAIN:{
    if ($ENV{'GATEWAY_INTERFACE'}) {
	$| = 1;
	print "Content-Type: text/html\n\n";
    } else {
	if ($0 =~ m|^(/[\w/._-]+)/[^/]+$|) {
	    $DIR = $1;
	}
	open(STDOUT, ">", "$DIR/index.html")
	    or die("index.html: $!\n");
    }
    print "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>
<!DOCTYPE html
     PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"
     \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">
  <head>
    <title>FreeBSD tinderbox logs</title>
    <meta name=\"robots\" content=\"nofollow\" />
    <meta http-equiv=\"refresh\" content=\"600\" />
    <link rel=\"stylesheet\" type=\"text/css\" media=\"screen\" href=\"tb.css\" />
    <link rel=\"shortcut icon\" type=\"image/png\" href=\"daemon.png\" />
  </head>
  <body>
    <h1>FreeBSD tinderbox logs</h1>

    <table border=\"1\" cellpadding=\"3\">
      <tr>
        <th>Architecture</th>
        <th>Machine</th>
";
    foreach my $branch (@BRANCHES) {
	print("        <th>$branch</th>\n");
    }
    print "      </tr>\n";

    foreach my $arch (sort(keys(%ARCHES))) {
	foreach my $machine (sort(@{$ARCHES{$arch}})) {
	    my $have_logs = 0;
	    my $html =  "      <tr>
        <td>$arch</td>
        <td>$machine</td>
";
	    foreach my $branch (@BRANCHES) {
		my $log = "tinderbox-$branch-$arch-$machine";
		my $links = "";
		if (-f "$DIR/$log.brief") {
		    my @stat = stat("$DIR/$log.brief");
		    my $class = success("$DIR/$log.brief") ? "ok" : "fail";
		    $links .= "<span class=\"$class\">" .
			strftime("%Y-%m-%d&nbsp;%H:%M", gmtime($stat[9])) .
			"</span><br />";
		    my $size = sprintf("[%.1f&nbsp;kB]", $stat[7] / 1024);
		    $links .= " <span class=\"tiny\">" .
			"<a href=\"$log.brief\">summary&nbsp;$size</a>" .
			"</span><br />";
		}
		if (-f "$DIR/$log.full") {
		    my @stat = stat("$DIR/$log.full");
		    my $size = sprintf("[%.1f&nbsp;MB]", $stat[7] / 1048576);
		    $links .= " <span class=\"tiny\">" .
			"<a href=\"$log.full\">full&nbsp;log&nbsp;$size</a>" .
			"</span><br />";
		}
		if ($links eq "") {
		    $html .= "        <td>n/a</td>\n";
		} else {
		    $html .= "        <td>$links</td>\n";
		    $have_logs++;
		}
	    }
	    $html .= "      </tr>\n";
	    print $html
		if $have_logs > 0;
	}
    }
    my $date = strftime("%Y-%m-%d %H:%M GMT", gmtime());
    print "    </table>
    <p class=\"update\">Last updated: $date</p>
    <p>
      <a href=\"http://validator.w3.org/check/referer\"><img
          src=\"valid-xhtml10.png\"
          alt=\"Valid XHTML 1.0!\" height=\"31\" width=\"88\" /></a>
      <a href=\"http://jigsaw.w3.org/css-validator/check/referer\"><img
          src=\"valid-css.png\"
          alt=\"Valid CSS!\" height=\"31\" width=\"88\" /></a>
    </p>
  </body>
</html>
";
    exit(0);
}
