#!/usr/bin/perl
#
# Copyright (c) March 1995 Wolfram Schneider. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#    This product includes software developed by Wolfram Schneider
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
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
# kbdmap/kbdfont - front end for syscons 
#
# E-Mail: Wolfram Schneider <wosch@cs.tu-berlin.de>
#

system("kbdcontrol -d >/dev/null");
die "You are on a virtual console?\n" .
    "This program does not work with X11\n" if $?;

sub variables_static {
    $lang = $ENV{'LANG'};	# use standard enviroment variable $LANG
    $lang = "en" unless $lang;
    $lang_default = "en";
    $program = $0; $program =~ s|.*/||;
    $keymapdir = "/usr/share/syscons/keymaps";
    $fontdir = "/usr/share/syscons/fonts";
    $index = "INDEX";			# Keyboard language database
    $verbose = 0;

    # menu
    $menu_map{en} = "Choise your keyboard language";
    $menu_map{de} = "Wähle Deine Tastaturbelegung";
    $menu_font{en} = "Choise your keyboard font";
    $menu_font{de} = "Wähle Deine Schrift";

    %keymap = '';
}

sub variables_dynamic {
    if ($program eq "kbdmap") { 
	$menu = $menu_map{$lang}; 
	$dir = $keymapdir;
    } else {
	$menu = $menu_font{$lang}; 
	$dir = $fontdir;
    }

    $dialog = "/usr/bin/dialog \\
--clear \\
--title \"Keyboard Menu\" \\
--menu \"$menu\" \\
-1 -1 8";
}

sub menu_read {
    local($e,@a,$mark,$ext);
    local($keym, $lg, $desc);

	$ext = $dir; $ext =~ s|.*/||;
    # read index database
    open(I, "$dir/$index.$ext") || warn "$dir/$index.$ext: $!\n";
    while(<I>) {
	chop;
	/^#/ && next;

	($keym, $lg, $desc) = split(/:/);
	if (! -r "$keym" && ! -r "$dir/$keym") {
	    warn "$keym not found!\n" if $verbose;
	    next;
	}

	# set empty language to default language
	$lg = $lang_default if $lg eq "";

	# ----> 1) your choise if exist
	#   --> 2) default language if exist and not 1)
	#    -> 3) unknown language if 1) and 2) not exist
	if ($lg eq $lang) {
	    # found your favorite language :-)
	    $keymap{$keym} = $desc;
	} elsif (!$keymap{$keym}) {
	    # found a language, but not your
	    # set mark if unknown language
	    $mark{$keym} = 1 if ($lg ne $lang_default);
	    $keymap{$keym} = $desc;
	} elsif ($lg eq $lang_default && $mark{$keym}) {
	    # overwrite unknown language with default language
	    $keymap{$keym} = $desc;
	}
    }
    close I;

    # look for keymaps which are not in database
    opendir(D, "$dir") || warn "$dir: $!\n";
    foreach $e (readdir(D)) {
	if ($e =~ /^[a-z].*(kbd|fnt)$/ && !$keymap{$e}) {
	    warn "$e not in database\n" if $verbose;
	    $keymap{$e} = $e;
	    $keymap{$e} =~ s/\.(kbd|fnt)$//;
	}
    }
    closedir D;

    # sort menu
    foreach $e (sort(keys %keymap)) {
	push(@a, "\"$keymap{$e}\" \"\"");
    }
    @a = sort @a;
    @a;
}

sub dialog {
    local(@argv) = @_;
    local($tmp) = "/tmp/_kbd_lang$$";

    # start dialog
    system("$dialog @argv 2> $tmp");

    if (!$?) {
	$choise = `cat $tmp`;
	foreach $e (keys %keymap) {
	    if ($keymap{$e} eq $choise) {
		if ($program eq "kbdmap") {
		    system("kbdcontrol -l $dir/$e\n");
		    print "keymap=$e", "\n";
		} else {
		    $f = $e; $f =~ s/\.fnt$//; $f =~ s/.*-//;
		    system("vidcontrol -f $f $dir/$e\n");
		    $_ = $e;
		    if (/^.*\-(.*)\.fnt/) {
			$font=$1
		    } else { $font="unknown" }
		    print "font$font=$e", "\n";
		}
		last;
	    }
	}
    }
    unlink $tmp;
    exit($?);
}

sub usage {
    warn <<EOF;
usage: $program [-v|-verbose] [-h|-help] [-l|-lang language]
EOF
    exit 1;
}

# Argumente lesen
sub parse {
    local(@argv) = @_;

    while($_ = $argv[0], /^-/) {
        shift @argv;
        last if /^--$/;
	if (/^--?(h|help|\?)$/)    { &usage; }
	elsif (/^--?(v|verbose)$/) { $verbose = 1; }
	elsif (/^--?(l|lang)$/)    { $lang = $argv[0]; shift @argv; }
	else { &usage }
    }
}

# main
&variables_static;		# read variables
&parse(@ARGV);			# parse arguments
&variables_dynamic;		# read variable after parsing
&dialog(&menu_read);		# start dialog and kbdcontrol/vidcontrol

