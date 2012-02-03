#! @PERL@ -T
# -*-Perl-*-

# Copyright (C) 1994-2005 The Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

###############################################################################
###############################################################################
###############################################################################
#
# THIS SCRIPT IS PROBABLY BROKEN.  REMOVING THE -T SWITCH ON THE #! LINE ABOVE
# WOULD FIX IT, BUT THIS IS INSECURE.  WE RECOMMEND FIXING THE ERRORS WHICH THE
# -T SWITCH WILL CAUSE PERL TO REPORT BEFORE RUNNING THIS SCRIPT FROM A CVS
# SERVER TRIGGER.  PLEASE SEND PATCHES CONTAINING THE CHANGES YOU FIND
# NECESSARY TO RUN THIS SCRIPT WITH THE TAINT-CHECKING ENABLED BACK TO THE
# <@PACKAGE_BUGREPORT@> MAILING LIST.
#
# For more on general Perl security and taint-checking, please try running the
# `perldoc perlsec' command.
#
###############################################################################
###############################################################################
###############################################################################

# Perl filter to handle pre-commit checking of files.  This program
# records the last directory where commits will be taking place for
# use by the log_accum.pl script.
#
# IMPORTANT: this script interacts with log_accum, they have to agree
# on the tmpfile name to use.  See $LAST_FILE below.
#
# Contributed by David Hampton <hampton@cisco.com>
# Stripped to minimum by Roy Fielding
#
############################################################
$TMPDIR        = $ENV{'TMPDIR'} || '/tmp';
$FILE_PREFIX   = '#cvs.';

# If see a "-u $USER" argument, then destructively remove it from the
# argument list, so $ARGV[0] will be the repository dir again, as it
# used to be before we added the -u flag.
if ($ARGV[0] eq '-u') {
  shift @ARGV;
  $CVS_USERNAME = shift (@ARGV);
}

# This needs to match the corresponding var in log_accum.pl, including
# the appending of the pgrp and username suffixes (see uses of this
# var farther down).
$LAST_FILE = "$TMPDIR/${FILE_PREFIX}lastdir";

sub write_line {
    my ($filename, $line) = @_;

# A check of some kind is needed here, but the rules aren't apparent
# at the moment:

#    foreach($filename, $line){	
#        $_ =~ m#^([-\@\w.\#]+)$#;
#        $_ = $1;
#    }

    open(FILE, ">$filename") || die("Cannot open $filename: $!\n");
    print(FILE $line, "\n");
    close(FILE);
}

#
# Record this directory as the last one checked.  This will be used
# by the log_accumulate script to determine when it is processing
# the final directory of a multi-directory commit.
#
$id = getpgrp();

&write_line("$LAST_FILE.$id.$CVS_USERNAME", $ARGV[0]);

exit(0);
