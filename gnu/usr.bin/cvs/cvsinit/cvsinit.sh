#! /bin/sh
:
#
#ident	"@(#)cvs:$Name:  $:$Id: cvsinit.sh,v 1.7 1995/11/14 23:44:18 woods Exp $"
# Copyright (c) 1992, Brian Berliner
#
# You may distribute under the terms of the GNU General Public License as
# specified in the README file that comes with the CVS 1.4 kit.

# This script should be run for each repository you create to help you
# setup your site for CVS.  You may also run it to update existing
# repositories if you install a new version of CVS.

# this line is edited by Makefile when creating cvsinit.inst
CVSLIB="xLIBDIRx"

CVS_VERSION="xVERSIONx"

# All purpose usage message, also suffices for --help and --version.
if test $# -gt 0; then
  echo "cvsinit version $CVS_VERSION"
  echo "usage: $0"
  echo "(set CVSROOT to the repository that you want to initialize)"
  exit 0
fi

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
    read CVSROOT junk
    unset junk
    remind_cvsroot=yes
else
    remind_cvsroot=no
fi

# Now, create the $CVSROOT if it is not already there
if [ ! -d $CVSROOT ]; then
    echo "Creating $CVSROOT..."
    path=
    for comp in `echo $CVSROOT | sed -e 's,/, ,g'`; do
	path=$path/$comp
	if [ ! -d $path ]; then
	    mkdir $path
	fi
    done
else
    true
fi

# Next, check for $CVSROOT/CVSROOT
if [ ! -d $CVSROOT/CVSROOT ]; then
    if [ -d $CVSROOT/CVSROOT.adm ]; then
	echo "You have the old $CVSROOT/CVSROOT.adm directory."
	echo "I will rename it to $CVSROOT/CVSROOT for you..."
	mv $CVSROOT/CVSROOT.adm $CVSROOT/CVSROOT
    else
	echo "Creating the $CVSROOT/CVSROOT directory..."
	mkdir $CVSROOT/CVSROOT
    fi
else
    true
fi
if [ ! -d $CVSROOT/CVSROOT ]; then
    echo "Unable to create $CVSROOT/CVSROOT."
    echo "I give up."
    exit 1
fi

# Create the special control files and templates within $CVSROOT/CVSROOT

EXAMPLES="checkoutlist commitinfo cvswrappers editinfo loginfo modules 
rcsinfo rcstemplate taginfo wrap unwrap"

NEWSAMPLE=false
for info in $EXAMPLES; do
    if [ -f $CVSROOT/CVSROOT/${info},v ]; then
	if [ ! -f $CVSROOT/CVSROOT/$info ]; then
	    echo "Checking out $CVSROOT/CVSROOT/$info"
	    echo "  from $CVSROOT/CVSROOT/${info},v..."
	    (cd $CVSROOT/CVSROOT; co -q $info)
	fi
    else
	NEWSAMPLE=true
	if [ -f $CVSROOT/CVSROOT/$info ]; then
	    echo "Checking in $CVSROOT/CVSROOT/${info},v"
	    echo "  from $CVSROOT/CVSROOT/$info..."
	else
	    echo "Creating a sample $CVSROOT/CVSROOT/$info file..."
	    case $info in
	      modules)
		sed -n -e '/END_REQUIRED_CONTENT/q' \
		    -e p $CVSLIB/examples/modules > $CVSROOT/CVSROOT/modules
		;;
	      rcstemplate)
		cp $CVSLIB/examples/$info $CVSROOT/CVSROOT/$info
		;;
	      wrap|unwrap)
		cp $CVSLIB/examples/$info $CVSROOT/CVSROOT/$info
		chmod +x $CVSROOT/CVSROOT/$info
		;;
	      *)
		# comment out everything in all the other examples....
		sed -e 's/^\([^#]\)/#\1/' $CVSLIB/examples/$info > $CVSROOT/CVSROOT/$info
		;;
	    esac
	fi
	(cd $CVSROOT/CVSROOT; ci -q -u -t/dev/null -m"initial checkin of $info" $info)
    fi
done

if $NEWSAMPLE ; then
    echo "NOTE:  You may wish to check out the CVSROOT module and edit any new"
    echo "configuration files to match your local requirements."
    echo ""
fi

# check to see if there are any references to the old CVSROOT.adm directory
if grep CVSROOT.adm $CVSROOT/CVSROOT/modules >/dev/null 2>&1; then
    echo "Warning: your $CVSROOT/CVSROOT/modules file still"
    echo "	contains references to the old CVSROOT.adm directory"
    echo "	You should really change these to the new CVSROOT directory"
    echo ""
fi

# These files are generated from the contrib files.
# FIXME: Is it really wise to overwrite possible local changes like this?
# Normal folks will keep these up to date by modifying the source in
# their CVS module and re-installing CVS, but is everyone OK with that?
#
#
CONTRIBS="log commit_prep log_accum cln_hist"
#
for contrib in $CONTRIBS; do
    echo "Copying the new version of '${contrib}'"
    echo "  to $CVSROOT/CVSROOT for you..."
    cp $CVSLIB/contrib/$contrib $CVSROOT/CVSROOT/$contrib
done

# XXX - also add a stub for the cvsignore file

# Turn on history logging by default
if [ ! -f $CVSROOT/CVSROOT/history ]; then
    echo "Enabling CVS history logging..."
    touch $CVSROOT/CVSROOT/history
    chmod g+w $CVSROOT/CVSROOT/history
    echo "(Remove $CVSROOT/CVSROOT/history to disable.)"
fi

# finish up by running mkmodules
echo "All done!  Running 'mkmodules' as my final step..."
mkmodules $CVSROOT/CVSROOT

exit 0
