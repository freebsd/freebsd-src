#!/usr/bin/perl

## Copyright (c) 1996, 1997 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

## $Id: named-bootconf.pl,v 8.16 1998/02/13 19:48:25 halley Exp $

# This is a filter.  Input is a named.boot.  Output is a named.conf.

$new_config = "";

$have_options = 0;
%options = ();
%options_comments = ();
@topology = ();
@topology_comments = ();
@bogus = ();
@bogus_comments = ();
@transfer_acl = ();
@transfer_comments = ();
$logging = "";

while(<>) {
    next if /^$/;

    # skip comment-only lines
    if (/^\s*;+\s*(.*)$/) {
	$new_config .= "// $1\n";
	next;
    }

    # handle continued lines
    while (/\\$/) {
	s/\\$/ /;
        $_ .= <>;
    }
    
    chop;
    
    # deal with lines ending in a coment
    if (s/\s*;+\s*(.*)$//) {
	$comment = "// $1";
    } else {
	$comment = "";
    }

    ($directive, @rest) = split;

    $class = "";
    if ($directive =~ /^(.*)\/(.*)$/) {
	$directive = $1;
	$class = $2;
    }
    
    if ($directive eq "primary") {
	$zname = shift(@rest);
	&maybe_print_comment("","\n");
	$new_config .= "zone \"$zname\" ";
	if ($class ne "") {
	    $new_config .= "$class ";
	}
	$new_config .= "{\n";
	$new_config .= "\ttype master;\n";
	$filename = shift(@rest);
	$new_config .= "\tfile \"$filename\";\n";
	$new_config .= "};\n\n";
    } elsif ($directive eq "secondary" || $directive eq "stub") {
	if ($directive eq "secondary") {
	    $type = "slave";
	} else {
	    $type = "stub";
	}
	$zname = shift(@rest);
	&maybe_print_comment("","\n");
	$new_config .= "zone \"$zname\" ";
	if ($class ne "") {
	    $new_config .= "$class ";
	}
	$new_config .= "{\n";
	$new_config .= "\ttype $type;\n";
	$filename = pop(@rest);
	if ($filename =~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/) {
	    push(@rest, $filename);
	    $filename = "";
	} else {
	    $new_config .= "\tfile \"$filename\";\n";
	}
	$new_config .= "\tmasters {\n";
	foreach $master (@rest) {
	    $new_config .= "\t\t$master;\n";
	}
	$new_config .= "\t};\n";
	$new_config .= "};\n\n";
    } elsif ($directive eq "cache") {
    	$zname = shift(@rest);
	&maybe_print_comment("","\n");
	$new_config .= "zone \"$zname\" {\n";
	$new_config .= "\ttype hint;\n";
	$filename = shift(@rest);
	$new_config .= "\tfile \"$filename\";\n";
	$new_config .= "};\n\n";
    } elsif ($directive eq "directory") {
	$options{"directory"} = "\"$rest[0]\"";
	$options_comments{"directory"} = $comment;
	$have_options = 1;
    } elsif ($directive eq "check-names") {
	$type = shift(@rest);
	if ($type eq "primary") {
	    $type = "master";
	} elsif ($type eq "secondary") {
	    $type = "slave";
	}
	$action = shift(@rest);
	$options{"check-names $type"} = $action;
	$options_comments{"check-names $type"} = $comment;
	$have_options = 1;
    } elsif ($directive eq "forwarders") {
	$options{"forwarders"}="{\n";
	foreach $forwarder (@rest) {
	    $options{"forwarders"} .= "\t\t$forwarder;\n";
	}
	$options{"forwarders"} .= "\t}";
	$options_comments{"forwarders"} = $comment;
	$have_options = 1;
    } elsif ($directive eq "slave") {
	&handle_options("forward-only");
    } elsif ($directive eq "options") {
	&handle_options(@rest);
    } elsif ($directive eq "limit") {
	&handle_limit(@rest);
    } elsif ($directive eq "include") {
	$new_config .= 
	    "// make sure your include is still in the right place\n";
	$comment = "\t" . $comment;
	$new_config .= "include \"$rest[0]\";$comment\n\n";
    } elsif ($directive eq "xfrnets" || $directive eq "tcplist") {
	if ($comment ne "") {
	    $comment = "\t$comment";
	}
	foreach $elt (@rest) {
	    push(@transfer_acl, $elt);
	    push(@transfer_comments, $comment);
	}
	$have_options = 1;
    } elsif ($directive eq "sortlist") {
	if ($comment ne "") {
	    $comment = "\t$comment";
	}
	foreach $elt (@rest) {
	    push(@topology, $elt);
	    push(@topology_comments, $comment);
	}
    } elsif ($directive eq "bogusns") {
	if ($comment ne "") {
	    $comment = "\t$comment";
	}
	foreach $elt (@rest) {
	    push(@bogus, $elt);
	    push(@bogus_comments, $comment);
	}
    } elsif ($directive eq "max-fetch") {
	$options{"transfers-in"}=$rest[0];
	$options_comments{"transfers-in"}=$comment;
	$have_options = 1;
    } else {
	$new_config .= "// NOTE: unconverted directive '$directive @rest'\n\n";
    }
}

print "// generated by named-bootconf.pl\n\n";
if ($have_options) {
    print "options {\n";
    foreach $option (sort(keys(%options))) {
	print "\t$option $options{$option};";
	if ($options_comments{$option} ne "") {
	    print "\t$options_comments{$option}";
	}
	print "\n";
    }
    if (@transfer_acl > 0) {
	print "\tallow-transfer {\n";
	for ($i = 0; $i <= $#transfer_acl; $i++) {
	    &print_maybe_masked("\t\t", $transfer_acl[$i],
				$transfer_comments[$i]);
	}
	print "\t};\n";
    }
    print "\t/*
\t * If there is a firewall between you and nameservers you want
\t * to talk to, you might need to uncomment the query-source
\t * directive below.  Previous versions of BIND always asked
\t * questions using port 53, but BIND 8.1 uses an unprivileged
\t * port by default.
\t */
\t// query-source address * port 53;
";

    print "};\n\n";
}
if ($logging ne "") {
    print "logging {\n$logging};\n\n";
}
if (@topology > 0) {
    print "// Note: the following will be supported in a future release.\n";
    print "/*\n";
    print "host { any; } {\n\ttopology {\n";
    for ($i = 0; $i <= $#topology; $i++) {
	&print_maybe_masked("\t\t", $topology[$i],
			    $topology_comments[$i]);
    }
    print "\t};\n};\n";
    print "*/\n";
    print "\n";
}
if (@bogus > 0) {
    for ($i = 0; $i <= $#bogus; $i++) {
	print "server $bogus[$i] { bogus yes; };$bogus_comments[$i]\n";
    }
    print "\n";
}
print $new_config;

exit 0;

sub maybe_print_comment {
    $prefix = shift;
    $suffix = shift;
    if ($comment ne "") {
	$new_config .= sprintf("%s%s%s", $prefix, $comment, $suffix);
    }
}

sub handle_options {
    foreach $option (@_) {
	if ($option eq "forward-only") {
	    $options{"forward"}="only";
	    $options_comments{"forward"}=$comment;
	    $have_options = 1;
	} elsif ($option eq "no-recursion") {
	    $options{"recursion"}="no";
	    $options_comments{"recursion"}=$comment;
	    $have_options = 1;
	} elsif ($option eq "no-fetch-glue") {
	    $options{"fetch-glue"}="no";
	    $options_comments{"fetch-glue"}=$comment;
	    $have_options = 1;
	} elsif ($option eq "fake-iquery") {
	    $options{"fake-iquery"}="yes";
	    $options_comments{"fake-iquery"}=$comment;
	    $have_options = 1;
	} elsif ($option eq "query-log") {
	    if ($comment ne "") {
		$logging .= "\t$comment\n";
	    }
	    $logging .= "\tcategory queries { default_syslog; };\n";
	} else {
	    $options{"// NOTE: unconverted option '$option'"}="";
	    $options_comments{"// NOTE: unconverted option '$option'"}=
		$comment;
	    $have_options = 1;
	}
    }
}

sub handle_limit {
    $limit = shift;
    if ($limit eq "datasize" || $limit eq "transfers-in"
	|| $limit eq "transfers-per-ns" || $limit eq "files") {
	$options{$limit}=$_[0];
	$options_comments{$limit}=$comment;
	$have_options = 1;
    } else {
	$options{"// NOTE: unconverted limit '$limit @_'"}="";
	$options_comments{"// NOTE: unconverted limit '$limit @_'"}=$comment;
	$have_options = 1;
    }
}

sub print_maybe_masked {
    # this assumes a contiguous netmask starting at the MSB
    $prefix = shift;
    $elt = shift;
    $elt_comment = shift;
    if ($elt =~ /^(.*)&(.*)$/) {
	$address = $1;
	$mask = $2;
	($m1,$m2,$m3,$m4) = split(/\./, $mask);
	$mask_val = ($m1 << 24) + ($m2 << 16) +($m3 << 8) + $m4;
	$zero_bits = 0;
	while (($mask_val % 2) == 0) {
	    $mask_val /= 2;
	    $zero_bits++;
	}
	$mask_bits = 32 - $zero_bits;
    } else {
	$address = $elt;
	($a1,$a2,$a3,$a4) = split(/\./, $address);
	if ($a1 < 128) {
	    $mask_bits = 8;
	} elsif ($a1 < 192) {
	    $mask_bits = 16;
	} else {
	    $mask_bits = 24;
	}
    }
	
    print "$prefix$address";
    if ($mask_bits != 32) {
	print "/$mask_bits";
    }
    print ";$elt_comment\n";
}
