#!/usr/bin/perl
#
# Copyright (c) March 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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
# kbdmap/vidfont - front end for syscons
#
# $FreeBSD$


# simple test if syscons works
$x11 = system("kbdcontrol -d >/dev/null");
if ($x11) {
    warn "You are not on a virtual console - " .
	"expect certain strange side-effects\n"; 
    sleep 2;
}

sub variables_static {
    $lang_default = "en";	# set default language
    $lang = $ENV{'LC_CTYPE'} || $ENV{'LANG'} || $lang_default;
    $lang = &lang($lang);
    $program = $0; $program =~ s|.*/||; $program =~ s/\.(pl|perl)$//;
    $keymapdir = "/usr/share/syscons/keymaps";
    $fontdir = "/usr/share/syscons/fonts";
    $sysconfig = "/etc/sysconfig";

    # for test only
    #$keymapdir = "/tmp/kbdmap/syscons/keymaps";
    #$fontdir = "/tmp/kbdmap/syscons/fonts";

    # read current font from sysconfig
    $font_default = "cp437-8x16.fnt";
    $font_current = &font_current($font_default);

    if ($program eq "kbdmap") {
	$dir = $keymapdir;
    } else {
	$dir = $fontdir;
    }

    @langsupport = ('MENU', 'FONT'); # lang depended variables
    $show = 0;			# show which languages currently supported
    $index = "INDEX";		# Keyboard language database
    $verbose = 0;
    %keymap = '';
}

sub lang {
    local($lang) = @_;

    #$lang =~ s/_.*//;		# strip country and font
    $lang =~ s/^(C)$/en/;	# aliases
    #$lang =~ s/^(..).*/$1/;	# use only first to characters

    return $lang;
}

sub font_current {
    local($font) = @_;
    local($font_current);

    open(F, "$sysconfig") || warn "$sysconfig: $!\n";

    while(<F>) {
	/^#/ && next;
	if (/^\s*font[0-9]+x[0-9]+\s*=\s*(\S+)/) {
	    $font_current = $1 if $1 ne "NO";
	}
    }
    close F;

    return $font_current if $font_current;
    return $font;
}

sub vidcontrol {
    local($font) = @_;

    return $x11 if $x11;	# syscons test failed

    if ($font =~ /.*([0-9]+x[0-9]+)(\.fnt)?$/) {
	warn "vidcontrol -f $1 $font\n" if $verbose;
	return system("vidcontrol -f $1 $font");
    } else {
	warn "Which font size? ``$font''\n";
	return 1;
    }
}

sub menu_read {
    local($e,@a,$mark,$ext);
    local($keym, $lg, $dialect, $desc);
    local(@langlist) = $lang_default;

    $ext = $dir; $ext =~ s|.*/||;
    # en_US.ISO8859-1 -> en_..\.ISO8859-1
    ($dialect = $lang) =~ s/^(..)_..(.+)$/$1_..$2/;
    # en_US.ISO8859-1 -> en
    ($lang_abk = $lang) =~ s/^(..)_.*$/$1/; 

    # read index database
    open(I, "$dir/$index.$ext") || warn "$dir/$index.$ext: $!\n";
    while(<I>) {
	# skip blank lines and comments
	/^#/ && next;
	s/^\s+//;
	/^\w/ || next;
	s/\s+$//;

	($keym, $lg, $desc) = split(/:/);
	if (! -r "$keym" && ! -r "$dir/$keym" &&
	    !grep(/$keym/, @langsupport)) {
	    warn "$keym not found!\n" if $verbose;
	    next;
	}

	# set empty language to default language
	$lg = $lang_default if $lg eq "";

	# save language
	if ($show) {
	    foreach $e (split(/,/, $lg)) {
		push(@langlist, $e) if !grep($_ eq $e, @langlist);
	    }
	}

	# 4) your choise if exist
	# 3) long match e.g. en_GB.ISO8859-1 is equal to en_..\.ISO8859-1
	# 2) short match 'de'
	# 1) default langlist 'en'
	# 0) any language
	#
	# language may be a kommalist
	# higher match overwrite lower
	# last entry overwrite previous if exist twice in database

	# found your favorite language :-)
	if ($lg =~  /^(.+,)?$lang(,.+)?$/) {
	    $keymap{$keym} = $desc; 
	    $mark{$keym} = 4;
	} elsif ($mark{$keym} <= 3 && $lg =~  /^(.+,)?$dialect(,.+)?$/) {
	    # dialect
	    $keymap{$keym} = $desc;
	    $mark{$keym} = 3; 
	} elsif ($mark{$keym} <= 2 && $lg =~  /^(.+,)?$lang_abk(,.+)?$/) {
	    # abrevation
	    $keymap{$keym} = $desc;
	    $mark{$keym} = 2; 
	} elsif ($mark{$keym} <= 1 && $lg =~  /^(.+,)?$lang_default(,.+)?$/) {
	    # default
	    $keymap{$keym} = $desc;
	    $mark{$keym} = 1; 
	} elsif ($mark{$keym} <= 0) {
	    # any
	    $keymap{$keym} = $desc;
	    $mark{$keym} = 0; 
	}
    }
    close I;

    if ($show) {
	@langlist = sort(@langlist);
	print "Currently supported languages: @langlist\n";
	exit(0);
    }

    # remove variables from list
    local($ee);
    foreach $e (@langsupport) {
	($ee = $e) =~ y/A-Z/a-z/;
	eval "\$$ee = \"$keymap{$e}\"";
	#warn "$e \$$ee = \"$keymap{$e}\"";
	delete $keymap{$e};
    }
    #warn "$font $font_default $font_current\n";


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

    # sort menu, font 8x8 is less than 8x14 and 8x16
    foreach $e (sort(keys %keymap)) {
	push(@a, "\"$keymap{$e}\" \"\"");
    }
    # side effects to @a
    grep(s/8x8/8x08/, @a);
    @a = sort @a;
    grep(s/8x08/8x8/, @a);

    if ($print) {
	foreach (@a) {
	    s/"//g; #"
	    print "$_\n";
	}
	exit;
    }

    return @a;
}

sub dialog {
    local(@argv) = @_;
    local($tmp) = "/tmp/_kbd_lang$$";

    $dialog = "/usr/bin/dialog \\
--clear \\
--title \"Keyboard Menu\" \\
--menu \"$menu\" \\
-1 -1 10";

    ## *always* start right font, don't believe that your current font
    ## is equal with default font in /etc/sysconfig
    ## see also at end of this function
    ## if ($font) {

    # start right font, assume that current font is equal
    # to default font in /etc/sysconfig
    #
    # $font is the font which require the language $lang; e.g.
    # russian *need* a koi8 font
    # $font_current is the current font from /etc/sysconfig
    if ($font && $font ne $font_current) {
	&vidcontrol($font);
    }

    # start dialog
    system("$dialog @argv 2> $tmp");

    if (!$?) {
	$choise = `cat $tmp`;
	foreach $e (keys %keymap) {
	    if ($keymap{$e} eq $choise) {
		if ($program eq "kbdmap") {
		    system("kbdcontrol -l $dir/$e\n") unless $x11;
		    print "keymap=$e", "\n";
		} else {
		    &vidcontrol("$dir/$e");
		    $_ = $e;
		    if (/^.*\-(.*)\.fnt/) {
			$font=$1
		    } else { $font="unknown" }
		    print "font$font=$e", "\n";
		}
		last;
	    }
	}
    # } else {
    } elsif ($font && $font ne $font_current) {
	# cancel, restore old font
	&vidcontrol($font_current);
    }
    unlink $tmp;
    exit($?);
}

sub usage {
    warn <<EOF;
usage: $program\t[-K] [-V] [-d|-default] [-h|-help] [-l|-lang language]
\t\t[-p|-print] [-r|-restore] [-s|-show] [-v|-verbose] 
EOF
    exit 1;
}

# Argumente lesen
sub parse {
    local(@argv) = @_;

    while($_ = $argv[0], /^-/) {
	shift @argv;
	last if /^--$/;
	if (/^--?(h|help|\?)$/)	 { &usage; }
	elsif (/^-(v|verbose)$/) { $verbose = 1; }
	elsif (/^-(l|lang)$/)	 { $lang = &lang($argv[0]); shift @argv; }
	elsif (/^-(d|default)$/) { $lang = $lang_default }
	elsif (/^-(s|show)$/)	 { $show = 1 }
	elsif (/^-(p|print)$/)   { $print = 1 }
	elsif (/^-(r|restore)$/) { &vidcontrol($font_current); exit(0) }
	elsif (/^-K$/)           { $dir = $keymapdir; }
	elsif (/^-V$/)           { $dir = $fontdir; }
	else                     { &usage }
    }
}

# main
&variables_static;		# read variables
&parse(@ARGV);			# parse arguments
&dialog(&menu_read);		# start dialog and kbdcontrol/vidcontrol
