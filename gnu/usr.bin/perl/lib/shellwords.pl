;# shellwords.pl
;#
;# Usage:
;#	require 'shellwords.pl';
;#	@words = &shellwords($line);
;#	or
;#	@words = &shellwords(@lines);
;#	or
;#	@words = &shellwords;		# defaults to $_ (and clobbers it)

sub shellwords {
    package shellwords;
    local($_) = join('', @_) if @_;
    local(@words,$snippet,$field);

    s/^\s+//;
    while ($_ ne '') {
	$field = '';
	for (;;) {
	    if (s/^"(([^"\\]|\\[\\"])*)"//) {
		($snippet = $1) =~ s#\\(.)#$1#g;
	    }
	    elsif (/^"/) {
		die "Unmatched double quote: $_\n";
	    }
	    elsif (s/^'(([^'\\]|\\[\\'])*)'//) {
		($snippet = $1) =~ s#\\(.)#$1#g;
	    }
	    elsif (/^'/) {
		die "Unmatched single quote: $_\n";
	    }
	    elsif (s/^\\(.)//) {
		$snippet = $1;
	    }
	    elsif (s/^([^\s\\'"]+)//) {
		$snippet = $1;
	    }
	    else {
		s/^\s+//;
		last;
	    }
	    $field .= $snippet;
	}
	push(@words, $field);
    }
    @words;
}
1;
