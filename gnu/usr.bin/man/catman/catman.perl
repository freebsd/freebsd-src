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
# /usr/bin/catman - preformat man pages
#
# /etc/weekly: catman `manpath -q`
#
# Bugs: sure
#   Email: Wolfram Schneider <wosch@cs.tu-berlin.de>
#
# $Id: catman.perl,v 1.1 1995/03/15 22:47:38 joerg Exp $
#

sub usage {

warn <<EOF;
usage: catman [-h|-help] [-f|-force] [-p|-print]
	      [-v|-verbose] directories ...
EOF

exit 1;
}

sub variables {
    $force = 0;			# force overwriting existing catpages
    $verbose = 0;		# more warnings
    $print = 0;			# show only, do nothing
    $remove = 0;		# unlink forgotten man/catpages

    # if no argument for directories given
    @defaultmanpath = ( '/usr/share/man' ); 

    $exit = 0;			# exit code
    $ext = ".gz";		# extension
    umask(022);

    # Signals
    $SIG{'INT'} = 'Exit';
    $SIG{'HUP'} = 'Exit';
    $SIG{'TRAP'} = 'Exit';
    $SIG{'QUIT'} = 'Exit';
    $SIG{'TERM'} = 'Exit';
    $tmp = '';			# tmp file

    $ENV{'PATH'} = "/bin:/usr/bin:$ENV{'PATH'}";
}

sub  Exit {
    unlink($tmp) if $tmp =~ /^\//; # unlink if a filename
    die "catman: die on signal SIG@_\n";
}

sub parse {
    local(@argv) = @_;

    while($_ = $argv[0], /^-/) {
	shift @argv;
	last if /^--$/;
	if    (/^--?(f|force)$/)     { $force = 1 }
	elsif (/^--?(p|print)$/)     { $print = 1 }
#	elsif (/^--?(r|remove)$/)    { $remove = 1 }
	elsif (/^--?(v|verbose)$/)   { $verbose = 1 }
	else { &usage }
    }

    return @argv if $#argv >= 0;
    return @defaultmanpath if $#defaultmanpath >= 0;

    warn "Missing directories\n"; &usage;
}

# stript unused '/'
# e.g.: //usr///home// -> /usr/home
sub stripdir {
    local($dir) = @_;

    $dir =~ s|/+|/|g;		# delete double '/'
    $dir =~ s|/$||;		# delete '/' at end
    return $dir if $dir ne "";
    return '/';
}

# read man directory
sub parse_dir {
    local($dir) = @_;
    local($subdir, $catdir);
    local($pwd);

    # not absolute path
    if ($dir !~ /^\//) {
	chop($cwd = `pwd`);
	$dir = "$cwd/$dir";
    }

    if ($dir =~ /man$/) {
	warn "open manpath directory ``$dir''\n" if $verbose;
	if (!opendir(DIR, $dir)) {
	    warn "opendir ``$dir'':$!\n"; $exit = 1; return 0;
	}
	foreach $subdir (sort(readdir(DIR))) {
	    if ($subdir =~ /^man\w+$/) {
		$subdir = "$dir/$subdir";
		&catdir_create($subdir) && &parse_subdir($subdir);
	    }
	}
	closedir DIR

    } elsif ($dir =~ /man\w+$/) {
	&catdir_create($dir) && &parse_subdir($dir);
    } else {
	warn "Assume ``$dir'' is not a man directory.\n";
	$exit = 1;
    }
}

# create cat subdirectory if neccessary
# e.g.: man9 exist, but cat9 not
sub catdir_create {
    local($subdir) = @_;
    local($catdir) = $subdir;

    $catdir = &man2cat($subdir);
    if (-d $catdir) {
	return 1 if -w _;
	if (!chmod(755, $catdir)) {
	    warn "Cannot write $catdir, chmod: $!\n";
	    $exit = 1;
	    return 0;
	}
    }

    warn "mkdir ``$catdir''\n" if $verbose || $print;
    unless ($print) {
	unlink($catdir);	# be paranoid
	if (!mkdir($catdir, 0755)) {
	    warn "Cannot make $catdir: $!\n";
	    $exit = 1;
	    return 0;
	}
    }
    return 1;
}

# I: /usr/share/man/man9
# O: usr/share/man/cat9
sub man2cat {
    local($man) = @_;

    $man =~ s/man(\w+)/cat$1/;
    return $man;
}

sub parse_subdir {
    local($subdir) = @_;
    local($file, $f, $catdir, $catdir_short);
    local($mtime_man, $mtime_cat);
    local($read);

    if (!opendir(D, $subdir)) {
	warn "opendir ``$subdir'': $!\n"; return 0;
    }
    
    $catdir = &man2cat($subdir);

    # optimize NAMI lookup, use short filenames
    warn "chdir to: $subdir\n" if $verbose;
    chdir($subdir); 

    $catdir_short = $catdir;
    $catdir_short =~ s|.*/(.*)|../$1|;

    warn "open man directory: ``$subdir''\n" if $verbose;

    foreach $file (readdir(D)) {
	next if $file =~ /^(\.|\.\.)$/; # skip current and parent directory

	$read{$file} = 1;

	# replace readable_file with  stat && ...
	# faster, hackers choise :-)
	if (!(($mtime_man = ((stat("$file"))[9])) && -r _ && -f _)) {
	    if (! -d _) {
		warn "Cannot read file: ``$subdir/$file''\n";
		$exit = 1;
		next;
	    }
	    warn "Ignore subsubdirectory: ``$subdir/$file''\n"
		if $verbose;
	    next;
	}

	# fo_09-o.bar0
	if ($file !~ /^[\w\-\[\.]+\.\w+$/) {
	    warn "Assume garbage: ``$subdir/$file''\n";
	    next;
	}

	# Assume catpages always compressed
	# if ($mtime_cat = &readable_file("$catdir_short/$file")) {
	if (($mtime_cat = ((stat("$catdir_short/$file"))[9])) 
	    && -r _ && -f _) {
	    if ($mtime_man > $mtime_cat || $force) {
		&nroff("$subdir/$file", "$catdir/$file");
	    } else {
		warn "up to date: $subdir/$file\n" if $verbose;
	    }
	} elsif (($mtime_cat = ((stat("$catdir_short/$file$ext"))[9]))
		 && -r _ && -f _) {
	    if ($mtime_man > $mtime_cat || $force) {
		&nroff("$subdir/$file", "$catdir/$file");
	    } else {
		warn "up to date: $subdir/$file\n" if $verbose;
	    }
	} else {
	    # be paranoid
	    unlink("$catdir/$file");

	    &nroff("$subdir/$file", "$catdir/$file");
	}
    }
    closedir D;


    if (!opendir(D, $catdir)) {
	warn "opendir ``$catdir'': $!\n"; return 0;
    }

    warn "open cat directory: ``$catdir''\n" if $verbose;
    foreach $file (readdir(D)) {
	next if $file =~ /^(\.|\.\.)$/;	# skip current and parent directory

	if ($file !~ /^[\w\-\[\.]+\.\w+$/) {
	    warn "Assume garbage: ``$catdir/$file''\n"
		unless -d "$catdir/$file";
	}

	unless ($read{$file}) {
	    # maybe a bug in man(1)
	    # if both manpage and catpage are uncompressed, man reformats
	    # the manpage and puts a compressed catpage to the
	    # already existing uncompressed catpage
	    $f = $file; $f =~ s/$ext$//;
	    # man page is uncompressed
	    next if $read{$f};

	    warn "Catpage without manpage: $catdir/$file\n";
	}
    }
    closedir D;
}

sub nroff {
    local($man,$cat) = @_;
    local($nroff) = "/usr/bin/nroff -Tascii -man | col";
    local($dev, $ino) = (stat($man))[01];

    # It's a link
    if ($link{"$dev.$ino"}) {
	warn "Link: $link{\"$dev.$ino\"} -> $cat\n" if $verbose || $print;

	if (!$print && !link($link{"$dev.$ino"}, $cat)) {
	    warn "Link $cat: $!\n";
	    $exit = 1;
	}
    } else {
	$cat = "$cat$ext" if $cat !~ /$ext$/;
	warn "Format: $man -> $cat\n" if $verbose || $print;

	unless($print) {
	    # man page is compressed
	    if ($man =~ /$ext$/) {
		$nroff = "zcat $man | tbl | $nroff";
	    } else {
		$nroff = "tbl $man | $nroff";
	    }

	    # start formatting
	    $tmp = "$cat.$tmp";	       # for cleanup after signals
	    system("$nroff | gzip > $cat.tmp");
	    if ($?) {
		# assume a fatal signal to nroff
		&Exit("INT to system() funktion") if ($? == 2); 
	    } else {
		rename("$cat.tmp", $cat);
	    }
	}
    }

    # dev/ino from manpage, path from catpage
    $link{"$dev.$ino"} = $cat;
}

#############
# main
warn "Don't start this program as root, use:\n" .
    "echo $0 @ARGV | nice -5 su -m man\n" unless $>;

&variables;
foreach $dir (&parse(split(/[ :]/, join($", @ARGV)))) {	#" 
    if (-e $dir && -d _ && -r _ && -x _) {
	warn "``$dir'' is not writable for you,\n" .
	    "can only write to existing cat subdirs (if any)\n"
		if ! -w _ && $verbose;
	&parse_dir(&stripdir($dir));
    } else {
	warn "``$dir'' is not a directory or not read-/searchable for you\n";
	$exit = 1;
    }
}
exit($exit);
