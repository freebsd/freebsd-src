#!/usr/bin/perl
#
# Copyright (c) Nov 1994 Wolfram Schneider. All rights reserved.
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
# makewhatis -- update the whatis database in the man directories.
#
# /etc/weekly: makewhatis `manpath -q`
#

# Bugs: You need perl!
#       My English :(
# Features: faster!!!
# tested with /usr/share/man (1414 Files)
# uncompressed manpages:
# perl:		  53.65 real	 27.20 user	  6.81 sys
# shell:	1036.27 real	597.27 user	654.93 sys
# compressed manpages:
# perl:		 192.70 real	 80.06 user	 90.26 sys
# shell:        1077.26 real    671.64 user     664.27 sys
#
# Send bugs and comments to: Wolfram Schneider <wosch@cs.tu-berlin.de>

sub usage {

    warn <<EOF;
usage: makewhatis [-verbose] [-help] [-format colum] [-name name]
                  [-outfile file] directory [...]	      
EOF

    exit 1;
}


# Format output
sub open_output {
    local($dir) = @_;

    die "Name for whatis is empty\n" if $whatis_name eq "";

    if ($outfile) {		# Write all Output to $outfile
	$whatisdb = $outfile;
    } else {		# Use man/whatis
	$dir =~ s|/$||;
	$whatisdb = $dir . "/$whatis_name.tmp";
    }

    if (!open(A, ">> $whatisdb")) {
	if ($outfile) {
	    die "$whatisdb: $!\n";
	} else {
	    warn "$whatisdb: $!\n"; $err++;
	    return 0;
        }
    }
    close A;

    if ($unix_sort) {
	open(A, "|sort -u > $whatisdb");
    } else {
	open(A, "> $whatisdb");
	@a = '';
    }
    warn "Open $whatisdb\n" if $debug;
    select A;
    return 1;
}

sub close_output {
    local($success) = @_;
    local($w) = $whatisdb;
    local($counter) = 0;


    $w =~ s/\.tmp$//;
    if ($success) {		# success 
	if ($unix_sort) {
	    close A; select STDOUT;
	    open(R, "$whatisdb");
	    while(<R>) { $counter++; }
	    close R;
	} else {
	    local($i, $last,@b);
	    # uniq
	    foreach $i (sort @a) {
		if ($i ne $last) {
		    push(@b, $i);
		    $counter++;
		}
		$last =$i;
	    }
	    print @b; close A; select STDOUT;
	}

	if (!$outfile) {
	    rename($whatisdb, $w);
	    warn "Rename $whatisdb to $w\n" if $debug;
	    $counter_all += $counter;
	    warn "$counter entries in $w\n" if $debug;
	} else {
	    $counter_all = $counter;
	}
    } else {		# building whatisdb failed
	unlink($whatisdb);
	warn "building whatisdb: $whatisdb failed\n" if $debug;
    }
    return 1;
}


# find manpages (recursive) 
#
# find /man/man* \( -type f -or -type l \) -print
sub find_manuals {
    local(@dirlist) = @_;
    local($subdir,$file,$flag,$dir);
    local($m) = "man/man";

  line:  
    while($dir = $dirlist[0]) {	# 
	shift @dirlist;
	$flag = 0;
	$dir =~ s|/$||;
	warn "traverse $dir\n" if $debug;

	if (! -e $dir) {
	    warn "$dir does not exist!\n"; $err++; next line;
	} elsif (-d _) {
	    opendir(M, $dir);
	# } elsif ($debug && (-f _ || -l _)) { 
        # allow files as arguments for testing
        #   return &manual($dir);
	} else {
	    warn "$dir is not a dir\n"; $err++; next line;
	}

	foreach $subdir (sort (readdir(M))) {
	    if ($subdir !~ /^(\.|\.\.)$/ && "$dir/$subdir" =~ $m) {
		$flag++;
		if (! -e "$dir/$subdir") {
		    warn "Cannot find file: $dir/$subdir\n"; $err++;
		} elsif (-d _) {
		    &find_manuals("$dir/$subdir");                    
		} elsif (-f _ || -l _) {
		    &manual("$dir/$subdir");
		} else {
		    warn "Cannot find file: $dir/$subdir\n"; $err++;
		}
	    } elsif ($subdir eq "." && $dir =~ $m) 	{ 
		# Empty subdir, no manpages
		$flag++;
	    }
	}
	closedir M;
	if (!$flag) {
	    warn <<EOF;
No subdirs found. Maybe ``$dir'' is not a ``*/man/'' dir.
Please use full path name, e. g.: makewhatis /usr/local/man
EOF
	    $err++;
            return 0;
	}
    }
    return 1;
}

sub dir_redundant {
    local($dir) = @_;

    local ($dev,$ino) = (stat($dir))[0..1];

    if ($dir_redundant{"$dev.$ino"}) {
	warn "$dir is equal to: $dir_redundant{\"$dev.$ino\"}\n" if $debug;
	return 0;
    }
    $dir_redundant{"$dev.$ino"} = $dir;
    return 1;
}


# ``/usr/man/man1/foo.l'' -> ``l''
sub ext {
    local($filename) = @_;
    local($ext) = $filename;

    $ext =~ s/\.gz$//g;
    $ext =~ s/.*\///g;

    if ($ext !~ /\./) {
	$ext = $filename;
	$ext =~ s|/[^/]+$||;
	$ext =~ s/.*(.)/\1/;
	warn "$filename has no extension, try section ``$ext''\n" if $debug;
    } else {
	$ext =~ s/.*\.//g;
    }
    return "$ext";
}

# ``/usr/man/man1/foo.1'' -> ``foo''
sub name {
    local($name) = @_;

    $name =~ s/.*\///g;
    $name =~ s/\..*$//;

    return "$name";
}

# output
sub out {
    local($list) = @_;
    local($delim) = " - ";
    $_ = $list;

    # delete italic etc.
    s/^\.[^ -]+[ -]+//;
    s/\\\((em|mi)//;
    s/\\f[IRBP]//g;
    s/\\\*p//g;
    s/\(OBSOLETED\)[ ]?//;
    s/\\&//g;
    s/^@INDOT@//;
    s/[\"\\]//g;		#"
    s/[. \t-]+$//;

    s/ / - / unless / - /;
    ($man,$desc) = split(/ - /);
    
    $man = $name unless $man;
    $man =~ s/[,. ]+$//;
    $man =~ s/,/($ext),/g;
    $man .= "($ext)";
    $desc =~ s/^[ \t]+//;
    for($i = length($man); $i< $format && $desc; $i++) {
	$man .= ' ';
    }
    if ($desc) { $man .=  "$delim$desc\n"  } else { $man .= "\n" }
    if ($unix_sort) { print $man } else { push(@a, $man) }
}

# looking for NAME 
sub manual {
    local($file) = @_;
    local($list, $desc, $ext);
    local($ofile) = $file;

    # Compressed man pages
    if ($ofile =~ /\.gz$/) {
	$ofile = "gzcat $file |";
    }

    if (!open(F, "$ofile")) {
	warn "Cannot open file: $ofile\n"; $err++;
	return 0;
    }
    # extension/section
    $ext = &ext($file);
    $name = &name($file);

    local($source) = 0;
    local($l, $list);
    while(<F>) {
	# ``man'' style pages
	# &&: it takes you only half the user time, regexp is slow!!!
	if (/^\.SH/ && /^\.SH[ \t]+["]?(NAME|Name|NAMN)["]?/) {
	    #while(<F>) { last unless /^\./ } # Skip
	    #chop; $list = $_;
	    while(<F>) {
		last if /^\.SH[ \t]/;
		chop;
		s/^\.[A-Z]+[ ]+[0-9]+$//; # delete commands
		s/^\.[A-Za-z]+[ \t]*//;	  # delete commands
		s/^\.\\".*$//;            #" delete comments
		s/^[ \t]+//;
		if ($_) {
		    $list .= $_;
		    $list .= ' ';
		}
	    }
	    &out($list); close F; return 1;
	} elsif (/^\.Sh/ && /^\.Sh[ \t]+["]?(NAME|Name)["]?/) {
	    # ``doc'' style pages
	    local($flag) = 0;
	    while(<F>) {
		last if /^\.Sh/;
		chop;
		s/^\.\\".*$//;            #" delete comments
		if (/^\.Nm/) {
		    s/^\.Nm[ \t]*//;
		    s/ ,/,/g;
		    s/[ \t]+$//;
		    $list .= $_;
		    $list .= ' ';
		} else {
		    $list .= '- ' if (!$flag && !/-/);
		    $flag++;
		    s/^\.[A-Z][a-z][ \t]*//;
		    s/[ \t]+$//;
		    $list .= $_;
		    $list .= ' ';
		}
	    }
	    &out($list); close F; return 1;

	} elsif(/^\.so/ && /^\.so[ \t]+man/) { 
	    close F; return 1;
            # source File
	    $source++;
	    s/[ \t]*\.so[ \t]+//; 
	    s/[ \t\n]*$//;
	    local($so) = $file;
	    $so =~ s|/[^/]+/[^/]+$|/|;
	    # redundant
	    &manual($so . $_);   
	    return 1;
	}
    }
    warn "Maybe $file is not a manpage\n" if (!$source && $debug);
    return 0;
}


##
## Main
##

$debug = 0;			# Verbose
$unix_sort = 0;			# Use sort(1) instead of builtin sort
@a = '';			# Array for output if $unix_sort=0
$outfile = 0;			# Don't write to ./whatis
$whatis_name = "whatis";	# Default name for DB

$whatisdb = '';
$counter_all = 0;
$err = 0; 
$format = 24;
$dir_redundant = '';			# 


while ($_ = $ARGV[0], /^-/) {
    shift @ARGV;
    last if /^--$/;
    if (/^-(debug|verbose|d|v)$/) { $debug = 1 }
    elsif (/^--?(h|help|\?)$/)      { &usage }
    elsif (/^--?(o|outfile)$/)      { $outfile = $ARGV[0]; shift @ARGV }
    elsif (/^--?(f|format)$/)       { $format  = $ARGV[0]; shift @ARGV }
    elsif (/^--?(n|name)$/)         { $whatis_name = $ARGV[0]; shift @ARGV }
    else                            { &usage }
}
&usage if $#ARGV < 0;

# allow colons in dir: ``makewhatis dir1:dir2:dir3''
@argv = split($", join($", (split(/:/, join($", @ARGV))))); #"

if ($outfile) {
    if(&open_output($outfile)){ 
	foreach $dir (@argv) { &dir_redundant($dir) && &find_manuals($dir); }
    }
    &close_output(1);
} else {
    foreach $dir (@argv) {
	&dir_redundant($dir) && 
	    &close_output(&open_output($dir) && &find_manuals($dir));
    }
}

warn "Total entries: $counter_all\n" if $debug && ($#argv > 0 || $outfile);
exit $err;

