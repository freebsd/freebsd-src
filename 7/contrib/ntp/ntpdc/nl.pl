#! /usr/local/bin/perl -w

$found = 0;
$last = 0;
$debug = 0;

while (<>) {
    next if /^#/;
    next if /^\s*$/;
    if (/^struct req_pkt/) {
	$found = 1;
    }
    if (/^struct info_dns_assoc/) {
	$last = 1;
    }
    if ($found) {
	if (/^(struct\s*\w*)\s*{\s*$/) {
	    $type = $1;
	    print STDERR "type = '$type'\n" if $debug;
	    printf "  printf(\"sizeof($type) = %%d\\n\", \n\t (int) sizeof($type));\n";
	    next;
	}
	if (/\s*\w+\s+(\w*)\s*(\[.*\])?\s*;\s*$/) {
	    $field = $1;
	    print STDERR "\tfield = '$field'\n" if $debug;
	    printf "  printf(\"offsetof($field) = %%d\\n\", \n\t (int) offsetof($type, $field));\n";
	    next;
	}
	if (/^}\s*;\s*$/) {
	    printf "  printf(\"\\n\");\n\n";
	    $found = 0 if $last;
	    next;
	}
	print STDERR "Unmatched line: $_";
	exit 1;
    }
}
