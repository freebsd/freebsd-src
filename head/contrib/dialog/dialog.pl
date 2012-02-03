# Functions that handle calling dialog(1) -*-perl-*-
# $Id: dialog.pl,v 1.4 2001/10/13 00:40:22 tom Exp $

# Return values are 1 for success and 0 for failure (or cancel)
# Resultant text (if any) is in dialog_result

# Unfortunately, the gauge requires use of /bin/sh to get going.
# I didn't bother to make the others shell-free, although it
# would be simple to do.

# Note that dialog generally returns 0 for success, so I invert the
# sense of the return code for more readable boolean expressions.

$scr_lines = 24;

require "flush.pl";

sub rhs_clear {
    return system("dialog --clear");
}

sub rhs_textbox {
    local ( $title, $file, $width, $height ) = @_;

    system("dialog --title \"$title\" --textbox $file $height $width");

    return 1;
}

sub rhs_msgbox {
    local ( $title, $message, $width ) = @_;
    local ( $tmp, $height, $message_len );

    $message = &rhs_wordwrap($message, $width);
    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }
    $height = 4 + $message_len;

    $tmp = system("dialog --title \"$title\" --msgbox \"$message\" $height $width");
    if ($tmp) {
	return 0;
    } else {
	return 1;
    }
}

sub rhs_infobox {
    local ( $title, $message, $width ) = @_;
    local ( $tmp, $height, $message_len );

    $message = &rhs_wordwrap($message, $width);
    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }
    $height = 2 + $message_len;

    return system("dialog --title \"$title\" --infobox \"$message\" $height $width");
}

sub rhs_yesno {
    local ( $title, $message, $width ) = @_;
    local ( $tmp, $height, $message_len );

    $message = &rhs_wordwrap($message, $width);
    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }
    $height = 4 + $message_len;

    $tmp = system("dialog --title \"$title\" --yesno \"$message\" $height $width");
    # Dumb: dialog returns 0 for "yes" and 1 for "no"
    if (! $tmp) {
	return 1;
    } else {
	return 0;
    }
}

sub rhs_gauge {
    local ( $title, $message, $width, $percent ) = @_;
    local ( $tmp, $height, $message_len );

    $gauge_width = $width;

    $message = &rhs_wordwrap($message, $width);
    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }
    $height = 5 + $message_len;

    open(GAUGE, "|dialog --title \"$title\" --gauge \"$message\" $height $width $percent");
}

sub rhs_update_gauge {
    local ( $percent ) = @_;

    &printflush(GAUGE, "$percent\n");
}

sub rhs_update_gauge_and_message {
    local ( $message, $percent ) = @_;

    $message = &rhs_wordwrap($message, $gauge_width);
    $message =~ s/\n/\\n/g;
    &printflush(GAUGE, "XXX\n$percent\n$message\nXXX\n");
}

sub rhs_stop_gauge {
    close GAUGE;
}

sub rhs_inputbox {
    local ( $title, $message, $width, $instr ) = @_;
    local ( $tmp, $height, $message_len );

    $message = &rhs_wordwrap($message, $width);
    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }
    $height = 7 + $message_len;

    return &return_output(0, "dialog --title \"$title\" --inputbox \"$message\" $height $width \"$instr\"");
}

sub rhs_menu {
    local ( $title, $message, $width, $numitems ) = @_;
    local ( $i, $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    for ($i = 0; $i < $numitems; $i++) {
        $ent = shift;
        $list[@list] = "\"$ent\"";
	$ent = shift;
        $list[@list] = "\"$ent\"";
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output(0, "dialog --title \"$title\" --menu \"$message\" $height $width $menuheight @list");
}

sub rhs_menul {
    local ( $title, $message, $width, $numitems ) = @_;
    local ( $i, $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    for ($i = 0; $i < $numitems; $i++) {
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $list[@list] = "\"\"";
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output(0, "dialog --title \"$title\" --menu \"$message\" $height $width $menuheight @list");
}

sub rhs_menua {
    local ( $title, $message, $width, %items ) = @_;
    local ( $tmp, $ent, $height, $menuheight, @list, $message_len );

    @list = ();
    foreach $ent (sort keys (%items)) {
        $list[@list] = "\"$ent\"";
        $list[@list] = "\"$items{$ent}\"";
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $numitems = keys(%items);
    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output(0, "dialog --title \"$title\" --menu \"$message\" $height $width $menuheight @list");
}

sub rhs_checklist {
    local ( $title, $message, $width, $numitems ) = @_;
    local ( $i, $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    for ($i = 0; $i < $numitems; $i++) {
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $ent = shift;
	if ($ent) {
	    $list[@list] = "ON";
	} else {
	    $list[@list] = "OFF";
	}
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output("list", "dialog --title \"$title\" --separate-output --checklist \"$message\" $height $width $menuheight @list");
}

sub rhs_checklistl {
    local ( $title, $message, $width, $numitems ) = @_;
    local ( $i, $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    for ($i = 0; $i < $numitems; $i++) {
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $list[@list] = "\"\"";
	$list[@list] = "OFF";
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }
    return &return_output("list", "dialog --title \"$title\" --separate-output --checklist \"$message\" $height $width $menuheight @list");
}

sub rhs_checklista {
    local ( $title, $message, $width, %items ) = @_;
    local ( $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    foreach $ent (sort keys (%items)) {
	$list[@list] = "\"$ent\"";
	$list[@list] = "\"$items{$ent}\"";
	$list[@list] = "OFF";
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $numitems = keys(%items);
    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output("list", "dialog --title \"$title\" --separate-output --checklist \"$message\" $height $width $menuheight @list");
}

sub rhs_radiolist {
    local ( $title, $message, $width, $numitems ) = @_;
    local ( $i, $tmp, $ent, $height, $menuheight, @list, $message_len );

    shift; shift; shift; shift;

    @list = ();
    for ($i = 0; $i < $numitems; $i++) {
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $ent = shift;
        $list[@list] = "\"$ent\"";
        $ent = shift;
	if ($ent) {
	    $list[@list] = "ON";
	} else {
	    $list[@list] = "OFF";
	}
    }

    $message = &rhs_wordwrap($message, $width);

    $message_len = split(/^/, $message);
    $tmp = $message;
    if (chop($tmp) eq "\n") {
	$message_len++;
    }

    $height = $message_len + 6 + $numitems;
    if ($height <= $scr_lines) {
        $menuheight = $numitems;
    } else {
        $height = $scr_lines;
        $menuheight = $scr_lines - $message_len - 6;
    }

    return &return_output(0 , "dialog --title \"$title\" --radiolist \"$message\" $height $width $menuheight @list");
}

sub return_output {
    local ( $listp, $command ) = @_;
    local ( $res ) = 1;

    pipe(PARENT_READER, CHILD_WRITER);
    # We have to fork (as opposed to using "system") so that the parent
    # process can read from the pipe to avoid deadlock.
    my ($pid) = fork;
    if ($pid == 0) {	# child
	close(PARENT_READER);
	open(STDERR, ">&CHILD_WRITER");
	exec($command);
	die("no exec");
    }
    if ($pid > 0) {	# parent
	close( CHILD_WRITER );
    	if ($listp) {
	    @dialog_result = ();
	    while (<PARENT_READER>) {
		chop;
		$dialog_result[@dialog_result] = $_;
	    }
	}
	else { $dialog_result = <PARENT_READER>; }
	close(PARENT_READER);
	waitpid($pid,0);
	$res = $?;
    }

    # Again, dialog returns results backwards
    if (! $res) {
	return 1;
    } else {
	return 0;
    }
}

sub rhs_wordwrap {
    local ( $intext, $width ) = @_;
    local ( $outtext, $i, $j, @lines, $wrap, @words, $pos, $pad );

    $outtext = "";
    $pad = 3;			# leave 3 spaces around each line
    $pos = $pad;		# current insert position
    $wrap = 0;			# 1 if we have been auto wraping
    $insert_nl = 0;		# 1 if we just did an absolute
				# and we should preface any new text
				# with a new line
    @lines = split(/\n/, $intext);
    for ($i = 0; $i <= $#lines; $i++) {
        if ($lines[$i] =~ /^>/) {
	    $outtext .= "\n" if ($insert_nl);
            $outtext .= "\n" if ($wrap);
	    $lines[$i] =~ /^>(.*)$/;
            $outtext .= $1;
	    $insert_nl = 1;
            $wrap = 0;
            $pos = $pad;
        } else {
            $wrap = 1;
            @words = split(/\s+/,$lines[$i]);
            for ($j = 0; $j <= $#words; $j++) {
		if ($insert_nl) {
		    $outtext .= "\n";
		    $insert_nl = 0;
		}
                if ((length($words[$j]) + $pos) > $width - $pad) {
                    $outtext .= "\n";
                    $pos = $pad;
                }
                $outtext .= $words[$j] . " ";
                $pos += length($words[$j]) + 1;
            }
        }
    }

    return $outtext;
}

############
1;
