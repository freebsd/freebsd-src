sub make {
    open MAKE, "make 2>&1 1>/dev/null |";
    while(<MAKE>) {
	if (($file, $line, $msg) = /([^: ]*):(\d*):(.+)/) {
	    if ($file == $prevfile && $line == $prevline) {
		$error[-1]->[2] .= "\n$msg";
	    } else {
		push @error, [$file, $line, $msg];
		($prevline, $prevfile) = ($line, $file);
	    }
	}
    }
    close MAKE;
}

sub nexterror {
    if ($index <= $#error) {
    	my $error = $error[$index++];
    	$curscr->Edit($error->[0]);
	$curscr->SetCursor($error->[1],0);
	$curscr->Msg($error->[2]);
    }
}

# preverror is left as an exercise

1;
