#! /usr/bin/perl

# This is a skeleton of a perl script for processing the output of
# sgmls.  You must change the parts marked with "XXX".

# XXX This is for troff: in data, turn \ into \e (which prints as \).
# Backslashes in SDATA entities are left as backslashes.

$backslash_in_data = "\\e";

$prog = $0;

$prog =~ s|.*/||;

$level = 0;

while (<STDIN>) {
    chop;
    $command = substr($_, 0, 1);
    substr($_, 0, 1) = "";
    if ($command eq '(') {
	&start_element($_);
	$level++;
    }
    elsif ($command eq ')') {
	$level--;
	&end_element($_);
	foreach $key (keys %attribute_value) {
	    @splitkey = split($;, $key);
	    if ($splitkey[0] == $level) {
		delete $attribute_value{$key};
		delete $attribute_type{$key};
	    }
	}
    }
    elsif ($command eq '-') {
	&unescape_data($_);
	&data($_);
    }
    elsif ($command eq 'A') {
	@field = split(/ /, $_, 3);
	$attribute_type{$level,$field[0]} = $field[1];
	&unescape_data($field[2]);
	$attribute_value{$level,$field[0]} = $field[2];
    }
    elsif ($command eq '&') {
	&entity($_);
    }
    elsif ($command eq 'D') {
	@field = split(/ /, $_, 4);
	$data_attribute_type{$field[0], $field[1]} = $field[2];
	&unescape_data($field[3]);
	$data_attribute_value{$field[0], $field[1]} = $field[3];
    }
    elsif ($command eq 'N') {
	$notation{$_} = 1;
	if (defined($sysid)) {
	    $notation_sysid{$_} = $sysid;
	    undef($sysid);
	}
	if (defined($pubid)) {
	    $notation_pubid{$_} = $pubid;
	    undef($pubid);
	}
    }
    elsif ($command eq 'I') {
        @field = split(/ /, $_, 3);
	$entity_type{$field[0]} = $field[1];
	&unescape($field[2]);
	# You may want to substitute \e for \ if the type is CDATA.
	$entity_text{$field[0]} = $field[2];
	$entity_code{$field[0]} = 'I';
    }
    elsif ($command eq 'E') {
	@field = split(/ /, $_);
	$entity_code{$field[0]} = 'E';
	$entity_type{$field[0]} = $field[1];
	$entity_notation{$field[0]} = $field[2];
	if (defined(@files)) {
	    foreach $i (0..$#files) {
		$entity_filename{$field[0], $i} = $files[i];
	    }
	    undef(@files);
	}
	if (defined($sysid)) {
	    $entity_sysid{$field[0]} = $sysid;
	    undef($sysid);
	}
	if (defined($pubid)) {
	    $entity_pubid{$field[0]} = $pubid;
	    undef($pubid);
	}
    }
    elsif ($command eq 'S') {
	$entity_code{$_} = 'S';
	if (defined(@files)) {
	    foreach $i (0..$#files) {
		$entity_filename{$_, $i} = $files[i];
	    }
	    undef(@files);
	}
	if (defined($sysid)) {
	    $entity_sysid{$_} = $sysid;
	    undef($sysid);
	}
	if (defined($pubid)) {
	    $entity_pubid{$_} = $pubid;
	    undef($pubid);
	}
    }
    elsif ($command eq '?') {
	&unescape($_);
	&pi($_);
    }
    elsif ($command eq 'L') {
	@field = split(/ /, $_);
	$lineno = $field[0];
	if ($#field >= 1) {
	    &unescape($field[1]);
	    $filename = $field[1];
	}
    }
    elsif ($command eq 'V') {
	@field = split(/ /, $_, 2);
	&unescape($field[1]);
	$environment{$field[0]} = $field[1];
    }
    elsif ($command eq '{') {
	&start_subdoc($_);
    }
    elsif ($command eq '}') {
	&end_subdoc($_);
    }
    elsif ($command eq 'f') {
	&unescape($_);
	push(@files, $_);
    }
    elsif ($command eq 'p') {
	&unescape($_);
	$pubid = $_;
    }
    elsif ($command eq 's') {
	&unescape($_);
	$sysid = $_;
    }
    elsif ($command eq 'C') {
	$conforming = 1;
    }
    else {
	warn "$prog:$ARGV:$.: unrecognized command \`$command'\n";
    }
}

sub unescape {
    $_[0] =~ s/\\([0-7][0-7]?[0-7]?|.)/&esc($1)/eg;
}

sub esc {
    local($_) = $_[0];
    if ($_ eq '012' || $_ eq '12') {
	"";			# ignore RS
    }
    elsif (/^[0-7]/) {
	sprintf("%c", oct);
    }
    elsif ($_ eq 'n') {
	"\n";
    }
    elsif ($_ eq '|') {
	"";
    }
    elsif ($_ eq "\\") {
	"\\";
    }
    else {
	$_;
    }
}

sub unescape_data {
    local($sdata) = 0;
    $_[0] =~ s/\\([0-7][0-7]?[0-7]?|.)/&esc_data($1)/eg;
}

sub esc_data {
    local($_) = $_[0];
    if ($_ eq '012' || $_ eq '12') {
	"";			# ignore RS
    }
    elsif (/^[0-7]/) {
	sprintf("%c", oct);
    }
    elsif ($_ eq 'n') {
	"\n";
    }
    elsif ($_ eq '|') {
	$sdata = !$sdata;
	"";
    }
    elsif ($_ eq "\\") {
	$sdata ? "\\" : $backslash_in_data;
    }
    else {
	$_;
    }
}


sub start_element {
    local($gi) = $_[0];
    # XXX
}

sub end_element {
    local($gi) = $_[0];
    # XXX
}

sub data {
    local($data) = $_[0];
    # XXX
}

# A processing instruction.

sub pi {
    local($data) = $_[0];
    # XXX
}

# A reference to an external entity.

sub entity {
    local($name) = $_[0];
    # XXX
}

sub start_subdoc {
    local($name) = $_[0];
    # XXX
}

sub end_subdoc {
    local($name) = $_[0];
    # XXX
}

