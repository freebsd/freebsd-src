case $CONFIGDOTSH in
'') . ./config.sh ;;
esac
echo "Extracting Policy.sh (with variable substitutions)"
$spitshell <<!GROK!THIS! >Policy.sh
$startsh
#
#  This file was produced by running the Policy_sh.SH script, which
#  gets its values from config.sh, which is generally produced by
#  running Configure.  
#
#  The idea here is to distill in one place the common site-wide
#  "policy" answers (such as installation directories) that are
#  to be "sticky".  If you keep the file Policy.sh around in
#  the same directory as you are building Perl, then Configure will
#  (by default) load up the Policy.sh file just before the
#  platform-specific hints file and rewrite it at the end.
#
#   The sequence of events is as follows:
#   A:  If you are NOT re-using an old config.sh:
#   1.  At start-up, Configure loads up the defaults from the
#	os-specific  hints/osname_osvers.sh file and any previous
#	Policy.sh file.
#   2.  At the end, Configure runs Policy_sh.SH, which creates
#	Policy.sh, overwriting a previous Policy.sh if necessary.
#
#   B: If you are re-using an old config.sh:
#   1.  At start-up, Configure loads up the defaults from config.sh, 
#	ignoring any previous Policy.sh file.
#   2.  At the end, Configure runs Policy_sh.SH, which creates
#	Policy.sh, overwriting a previous Policy.sh if necessary.
#
#  Thus the Policy.sh file gets overwritten each time
#  Configure is run.  Any variables you add to Policy.sh will be lost
#  unless you copy Policy.sh somewhere else before running Configure.
#
#  Allow Configure command-line overrides; usually these won't be
#  needed, but something like -Dprefix=/test/location can be quite
#  useful for testing out new versions.

#Site-specific values:

case "\$perladmin" in
'') perladmin='$perladmin' ;;
esac

# Installation prefixes.  Allow a Configure -D override.  You
# may wish to reinstall perl under a different prefix, perhaps
# in order to test a different configuration.
# For an explanation of the installation directories, see the
# INSTALL file section on "Installation Directories".
case "\$prefix" in
'') prefix='$prefix' ;;
esac

# By default, the next three are the same as \$prefix.  
# If the user changes \$prefix, and previously \$siteprefix was the
# same as \$prefix, then change \$siteprefix as well.
# Use similar logic for \$vendorprefix and \$installprefix.

case "\$siteprefix" in
'') if test "$siteprefix" = "$prefix"; then
	siteprefix="\$prefix"
    else
	siteprefix='$siteprefix'
    fi
    ;;
esac
case "\$vendorprefix" in
'') if test "$vendorprefix" = "$prefix"; then
	vendorprefix="\$prefix"
    else
	vendorprefix='$vendorprefix'
    fi
    ;;
esac

# Where installperl puts things.
case "\$installprefix" in
'') if test "$installprefix" = "$prefix"; then
	installprefix="\$prefix"
    else
	installprefix='$installprefix'
    fi
    ;;
esac

# Installation directives.  Note that each one comes in three flavors.
# For example, we have privlib, privlibexp, and installprivlib.
# privlib is for private (to perl) library files.
# privlibexp is the same, except any '~' the user gave to Configure
#     is expanded to the user's home directory.  This is figured
#     out automatically by Configure, so you don't have to include it here.
# installprivlib is for systems (such as those running AFS) that
#     need to distinguish between the place where things
#     get installed and where they finally will reside.  As of 5.005_6x,
#     this too is handled automatically by Configure based on
#     $installprefix, so it isn't included here either.
#
# Note also that there are three broad hierarchies of installation 
# directories, as discussed in the INSTALL file under 
# "Installation Directories":
#
#  =item Directories for the perl distribution
#
#  =item Directories for site-specific add-on files
#
#  =item Directories for vendor-supplied add-on files
#
#  See Porting/Glossary for the definitions of these names, and see the
#  INSTALL file for further explanation and some examples.
# 
# In each case, if your previous value was the default, leave it commented
# out.  That way, if you override prefix, all of these will be
# automatically adjusted.
#
# WARNING:  Be especially careful about architecture-dependent and
# version-dependent names, particularly if you reuse this file for
# different versions of perl.

!GROK!THIS!

for var in \
	bin scriptdir privlib archlib man1dir man3dir html1dir html3dir \
	sitebin sitescript sitelib sitearch \
		siteman1 siteman3 sitehtml1 sitehtml3 \
	vendorbin vendorscript vendorlib vendorarch \
		vendorman1 vendorman3 vendorhtml1 vendorhtml3
do
	
    case "$var" in

    # Directories for the core perl components
    bin)	dflt=$prefix/bin ;;
    # The scriptdir test is more complex, but this is probably usually ok.
    scriptdir)
	if $test -d $prefix/script; then
	    dflt=$prefix/script
	else
	    dflt=$bin
	fi
	;;
    privlib)
	case "$prefix" in
	*perl*)	dflt=$prefix/lib/$version ;;
	*)	dflt=$prefix/lib/$package/$version ;;
	esac
	;;
    archlib)	dflt="$privlib/$archname" ;;

    man1dir)	dflt="$prefix/man/man1" ;;
    man3dir)	dflt="$prefix/man/man3" ;;
    # Can we assume all sed's have greedy matching?
    man1ext)	dflt=`echo $man1dir | sed -e 's!.*man!!' -e 's!^\.!!'` ;;
    man3ext)	dflt=`echo $man3dir | sed -e 's!.*man!!' -e 's!^\.!!'` ;;

    # We don't know what to do with these yet.
    html1dir)	dflt='' ;;
    htm31dir)	dflt='' ;;

    # Directories for site-specific add-on files
    sitebin)	dflt=$siteprefix/bin ;;
    sitescript)
	if $test -d $siteprefix/script; then
	    dflt=$siteprefix/script
	else
	    dflt=$sitebin
	fi
	;;
    sitelib)
	case "$siteprefix" in
	*perl*)	dflt=$prefix/lib/site_perl/$version ;;
	*)	dflt=$prefix/lib/$package/site_perl/$version ;;
	esac
	;;
    sitearch)	dflt="$sitelib/$archname" ;;

    siteman1)	dflt="$siteprefix/man/man1" ;;
    siteman3)	dflt="$siteprefix/man/man3" ;;
    # We don't know what to do with these yet.
    sitehtml1)	dflt='' ;;
    sitehtm31dir)	dflt='' ;;
    
    # Directories for vendor-supplied add-on files
    # These are all usually empty.
    vendor*)
	if test X"$vendorprefix" = X""; then
	    dflt=''
	else
	    case "$var" in
	    vendorbin)	dflt=$vendorprefix/bin ;;
	    vendorscript)
		if $test -d $vendorprefix/script; then
		    dflt=$vendorprefix/script
		else
		    dflt=$vendorbin
		fi
		;;
	    vendorlib)
		case "$vendorprefix" in
		*perl*)	dflt=$prefix/lib/vendor_perl/$version ;;
		*)	dflt=$prefix/lib/$package/vendor_perl/$version ;;
		esac
		;;
	    vendorarch)	dflt="$vendorlib/$archname" ;;

	    vendorman1)	dflt="$vendorprefix/man/man1" ;;
	    vendorman3)	dflt="$vendorprefix/man/man3" ;;
	    # We don't know what to do with these yet.
	    vendorhtml1)	dflt='' ;;
	    vendorhtm3)	dflt='' ;;

	    esac  # End of vendorprefix != ''
	fi
	;;
    esac
    
    eval val="\$$var"
    if test X"$val" = X"$dflt"; then
	echo "# $var='$dflt'"
    else
	echo "# Preserving custom $var"
	echo "$var='$val'"
    fi

done >> Policy.sh

$spitshell <<!GROK!THIS! >>Policy.sh

#  Lastly, you may add additional items here.  For example, to set the
#  pager to your local favorite value, uncomment the following line in
#  the original Policy_sh.SH file and re-run   sh Policy_sh.SH.
#
#  pager='$pager'
#
#  A full Glossary of all the config.sh variables is in the file
#  Porting/Glossary.

!GROK!THIS!

#Credits:
#   The original design for this Policy.sh file came from Wayne Davison,
#   maintainer of trn.
#   This version for Perl5.004_61 originally written by
#   Andy Dougherty <doughera@lafayette.edu>.
#   This file may be distributed under the same terms as Perl itself.
