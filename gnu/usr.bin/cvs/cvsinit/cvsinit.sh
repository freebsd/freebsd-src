#! /bin/sh
:
#
# Copyright (c) 1992, Brian Berliner
#
# You may distribute under the terms of the GNU General Public License as
# specified in the README file that comes with the CVS 1.4 kit.
#
# $CVSid: @(#)cvsinit.sh 1.1 94/10/22 $
#
# This script should be run once to help you setup your site for CVS.

# this line is edited by Makefile when creating cvsinit.inst
CVSLIB="/usr/src/gnu/usr.bin/cvs"

# Make sure that the CVSROOT variable is set
if [ "x$CVSROOT" = x ]; then
    echo "The CVSROOT environment variable is not set."
    echo ""
    echo "You should choose a location for your source repository"
    echo "that can be shared by many developers.  It also helps to"
    echo "place the source repository on a file system that has"
    echo "plenty of free space."
    echo ""
    echo "Please enter the full path for your CVSROOT source repository:"
    read CVSROOT
    remind_cvsroot=yes
else
    echo "Using $CVSROOT as the source repository."
    remind_cvsroot=no
fi
echo ""

# Now, create the $CVSROOT if it is not already there
if [ ! -d $CVSROOT ]; then
    echo "Hmmm... $CVSROOT does not exist; trying to make it..."
    path=
    for comp in `echo $CVSROOT | sed -e 's,/, ,g'`; do
	path=$path/$comp
	if [ ! -d $path ]; then
	    mkdir $path
	fi
    done
else
    echo "Good... $CVSROOT already exists."
fi

# Next, check for $CVSROOT/CVSROOT
if [ ! -d $CVSROOT/CVSROOT ]; then
    if [ -d $CVSROOT/CVSROOT.adm ]; then
	echo "You have the old $CVSROOT/CVSROOT.adm directory."
	echo "I will rename it to $CVSROOT/CVSROOT for you..."
	mv $CVSROOT/CVSROOT.adm $CVSROOT/CVSROOT
    else
	echo "Making the $CVSROOT/CVSROOT directory..."
	mkdir $CVSROOT/CVSROOT
    fi
else
    echo "Wow!... so does $CVSROOT/CVSROOT."
fi
echo ""
if [ ! -d $CVSROOT/CVSROOT ]; then
    echo "You still don't have a $CVSROOT/CVSROOT directory."
    echo "I give up."
    exit 1
fi

# Create the special *info files within $CVSROOT/CVSROOT

# Trump up a simple modules file, if one doesn't exist
if [ -f $CVSROOT/CVSROOT/modules,v ]; then
    if [ ! -f $CVSROOT/CVSROOT/modules ]; then
	echo "You have a $CVSROOT/CVSROOT/modules,v file,"
	echo "But no $CVSROOT/CVSROOT/modules file.  This is OK."
	echo "I'll checkout a fresh copy..."
	(cd $CVSROOT/CVSROOT; co -q modules)
	echo ""
    fi
else
    if [ -f $CVSROOT/CVSROOT/modules ]; then
	echo "You have a $CVSROOT/CVSROOT/modules file,"
	echo "But no $CVSROOT/CVSROOT/modules,v file."
	echo "I'll create one for you, but otherwise leave it alone..."
    else
	echo "The $CVSROOT/CVSROOT/modules file does not exist."
	echo "Making a simple one for you..."
	cat > $CVSROOT/CVSROOT/modules <<"HERE"
#
# The CVS modules file
#
# Three different line formats are valid:
#	key	-a    aliases...
#	key [options] directory
#	key [options] directory files...
#
# Where "options" are composed of:
#	-i prog		Run "prog" on "cvs commit" from top-level of module.
#	-o prog		Run "prog" on "cvs checkout" of module.
#	-t prog		Run "prog" on "cvs rtag" of module.
#	-u prog		Run "prog" on "cvs update" of module.
#	-d dir		Place module in directory "dir" instead of module name.
#	-l		Top-level directory only -- do not recurse.
#
# And "directory" is a path to a directory relative to $CVSROOT.
#
# The "-a" option specifies an alias.  An alias is interpreted as if
# everything on the right of the "-a" had been typed on the command line.
#
# You can encode a module within a module by using the special '&'
# character to interpose another module into the current module.  This
# can be useful for creating a module that consists of many directories
# spread out over the entire source repository.
#

# Convenient aliases
world		-a .

# CVSROOT support; run mkmodules whenever anything changes.
CVSROOT		-i mkmodules CVSROOT
modules		-i mkmodules CVSROOT modules
loginfo		-i mkmodules CVSROOT loginfo
commitinfo	-i mkmodules CVSROOT commitinfo
rcsinfo		-i mkmodules CVSROOT rcsinfo
editinfo	-i mkmodules CVSROOT editinfo

# Add other modules here...
HERE
    fi
    (cd $CVSROOT/CVSROOT; ci -q -u -t/dev/null -m'initial checkin of modules' modules)
    echo ""
fi

# check to see if there are any references to the old CVSROOT.adm directory
if grep CVSROOT.adm $CVSROOT/CVSROOT/modules >/dev/null 2>&1; then
    echo "Warning: your $CVSROOT/CVSROOT/modules file still"
    echo "	contains references to the old CVSROOT.adm directory"
    echo "	You should really change these to the new CVSROOT directory"
    echo ""
fi

# loginfo, like modules, is special-cased
if [ -f $CVSROOT/CVSROOT/loginfo,v ]; then
    if [ ! -f $CVSROOT/CVSROOT/loginfo ]; then
	echo "You have a $CVSROOT/CVSROOT/loginfo,v file,"
	echo "But no $CVSROOT/CVSROOT/loginfo file.  This is OK."
	echo "I'll checkout a fresh copy..."
	(cd $CVSROOT/CVSROOT; co -q loginfo)
	echo ""
    fi
else
    if [ -f $CVSROOT/CVSROOT/loginfo ]; then
	echo "You have a $CVSROOT/CVSROOT/loginfo file,"
	echo "But no $CVSROOT/CVSROOT/loginfo,v file."
	echo "I'll create one for you, but otherwise leave it alone..."
    else
	echo "The $CVSROOT/CVSROOT/loginfo file does not exist."
	echo "Making a simple one for you..."
	# try to find perl; use fancy log script if we can
	for perlpath in `echo $PATH | sed -e 's/:/ /g'` x; do
	    if [ -f $perlpath/perl -a -r $CVSLIB/contrib/log.pl ]; then
		echo "#!$perlpath/perl" > $CVSROOT/CVSROOT/log.pl
		cat $CVSLIB/contrib/log.pl >> $CVSROOT/CVSROOT/log.pl
		chmod 755 $CVSROOT/CVSROOT/log.pl
		cp $CVSLIB/examples/loginfo $CVSROOT/CVSROOT/loginfo
		break
	    fi
	done
	if [ $perlpath = x -o ! -r $CVSLIB/contrib/log.pl ]; then
	    # we did not find perl anywhere, so make a simple loginfo file
	    cat > $CVSROOT/CVSROOT/loginfo <<"HERE"
#
# The "loginfo" file is used to control where "cvs commit" log information
# is sent.  The first entry on a line is a regular expression which is tested
# against the directory that the change is being made to, relative to the
# $CVSROOT.  If a match is found, then the remainder of the line is a filter
# program that should expect log information on its standard input.
#
# The filter program may use one and only one % modifier (ala printf).  If
# %s is specified in the filter program, a brief title is included (enclosed
# in single quotes) showing the modified file names.
#
# If the repository name does not match any of the regular expressions in this
# file, the "DEFAULT" line is used, if it is specified.
#
# If the name ALL appears as a regular expression it is always used
# in addition to the first matching regex or DEFAULT.
#
DEFAULT		(echo ""; echo $USER; date; cat) >> $CVSROOT/CVSROOT/commitlog
HERE
	fi
    fi
    (cd $CVSROOT/CVSROOT; ci -q -u -t/dev/null -m'initial checkin of loginfo' loginfo)
    echo ""
fi

# The remaining files are generated from the examples files.
for info in commitinfo rcsinfo editinfo; do
    if [ -f $CVSROOT/CVSROOT/${info},v ]; then
	if [ ! -f $CVSROOT/CVSROOT/$info ]; then
	    echo "You have a $CVSROOT/CVSROOT/${info},v file,"
	    echo "But no $CVSROOT/CVSROOT/$info file.  This is OK."
	    echo "I'll checkout a fresh copy..."
	    (cd $CVSROOT/CVSROOT; co -q $info)
	    echo ""
	fi
    else
	if [ -f $CVSROOT/CVSROOT/$info ]; then
	    echo "You have a $CVSROOT/CVSROOT/$info file,"
	    echo "But no $CVSROOT/CVSROOT/${info},v file."
	    echo "I'll create one for you, but otherwise leave it alone..."
	    (cd $CVSROOT/CVSROOT; ci -q -u -t/dev/null -m"initial checkin of $info" $info)
	else
	    echo "The $CVSROOT/CVSROOT/$info file does not exist."
	    if [ -r $CVSLIB/examples/$info ]; then
	        echo "Making a simple one for you..."
	        sed -e 's/^\([^#]\)/#\1/' $CVSLIB/examples/$info > $CVSROOT/CVSROOT/$info
	    fi
	fi
	echo ""
    fi
done

# XXX - also add a stub for the cvsignore file

# Turn on history logging by default
if [ ! -f $CVSROOT/CVSROOT/history ]; then
    echo "Enabling CVS history logging..."
    touch $CVSROOT/CVSROOT/history
    echo ""
fi

# finish up by running mkmodules
echo "All done!  Running 'mkmodules' as my final step..."
mkmodules $CVSROOT/CVSROOT

# and, if necessary, remind them about setting CVSROOT
if [ $remind_cvsroot = yes ]; then
    echo "Remember to set the CVSROOT environment variable in your login script"
fi

exit 0
