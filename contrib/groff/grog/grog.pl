#!/usr/bin/perl
# grog -- guess options for groff command
# Inspired by doctype script in Kernighan & Pike, Unix Programming
# Environment, pp 306-8.

$prog = $0;
$prog =~ s@.*/@@;

push(@command, "groff");

while ($ARGV[0] =~ /^-./) {
    $arg = shift(@ARGV);
    last if $arg eq "--";
    push(@command, $arg);
}

if (@ARGV) {
    foreach $arg (@ARGV) {
	&process($arg, 0);
    }
}
else {
    &process("-", 0);
}

sub process {
    local($filename, $level) = @_;
    local(*FILE);

    if (!open(FILE, $filename eq "-" ? $filename : "< $filename")) {
	print STDERR "$prog: can't open \`$filename': $!\n";
	exit 1 unless $level;
	return;
    }
    while (<FILE>) {
	if (/^\.TS/) {
	    $_ = <FILE>;
	    if (!/^\./) {
		$tbl++;
		$soelim++ if $level;
	    }
	}
	elsif (/^\.EQ/) {
	    $_ = <FILE>;
	    if (!/^\./ || /^\.[0-9]/) {
		$eqn++;
		$soelim++ if $level;
	    }
	}
	elsif (/^\.GS/) {
	    $_ = <FILE>;
	    if (!/^\./) {
		$grn++;
		$soelim++ if $level;
	    }
	}
	elsif (/^\.G1/) {
	    $_ = <FILE>;
	    if (!/^\./) {
		$grap++;
		$pic++;
		$soelim++ if $level;
	    }
	}
	elsif (/^\.PS([ 0-9.<].*)?$/) {
	    if (/^\.PS\s*<\s*(\S+)/) {
		$pic++;
		$soelim++ if $level;
		&process($1, $level);
	    }
	    else {
	    	$_ = <FILE>;
	    	if (!/^\./ || /^\.ps/) {
		    $pic++;
		    $soelim++ if $level;
		}
	    }
	}
	elsif (/^\.R1/ || /^\.\[/) {
	    $refer++;
	    $soelim++ if $level;
	}
	elsif (/^\.[PLI]P/) {
	    $PP++;
	}
	elsif (/^\.P$/) {
	    $P++;
	}
        elsif (/^\.(PH|SA)/) {
            $mm++;
	}
	elsif (/^\.TH/) {
	    $TH++;
	}
	elsif (/^\.SH/) {
	    $SH++;
	}
	elsif (/^\.([pnil]p|sh)/) {
	    $me++;
	}
	elsif (/^\.Dd/) {
	    $mdoc++;
	}
	elsif (/^\.(Tp|Dp|De|Cx|Cl)/) {
	    $mdoc_old = 1;
	}
        # In the old version of -mdoc `Oo' is a toggle, in the new it's
	# closed by `Oc'.
	elsif (/^\.Oo/) {
	    $Oo++;
	}
	elsif (/^\.Oc/) {
	    $Oo--;
	}
	if (/^\.so/) {
	    chop;
	    s/^.so *//;
	    s/\\\".*//;
	    s/ .*$//;
	    &process($_, $level + 1) unless /\\/ || $_ eq "";
	}
    }
    close(FILE);
}

if ($pic || $tbl || $eqn || $grn || $grap || $refer) {
    $s = "-";
    $s .= "s" if $soelim;
    $s .= "R" if $refer;
    # grap must be run before pic
    $s .= "G" if $grap;
    $s .= "p" if $pic;
    $s .= "g" if $grn;
    $s .= "t" if $tbl;
    $s .= "e" if $eqn;
    push(@command, $s);
}

if ($me > 0) {
    push(@command, "-me");
}
elsif ($SH > 0 && $TH > 0) {
    push(@command, "-man");
}
elsif ($PP > 0) {
    push(@command, "-ms");
}
elsif ($P > 0 || $mm > 0) {
    push(@command, "-mm");
}
elsif ($mdoc > 0) {
    push(@command, ($mdoc_old || $Oo > 0) ? "-mdoc.old" : "-mdoc");
}

push(@command, "--") if @ARGV && $ARGV[0] =~ /^-./;

push(@command, @ARGV);

# We could implement an option to execute the command here.

foreach (@command) {
    next unless /[\$\\\"\';&()|<> \t\n]/;
    s/\'/\'\\\'\'/;
    $_ = "'" . $_ . "'";
}

print join(' ', @command), "\n";
