case $CONFIG in
'') . ./config.sh ;;
esac
echo "Extracting Policy.sh (with variable substitutions)"
$spitshell <<!GROK!THIS! >Policy.sh
$startsh
#
#  This file was produced by running the Policy_sh.SH script, which
#  gets its values from config.sh, which is generally produced by
#  running Configure.  The Policy.sh file gets overwritten each time
#  Configure is run.  Any variables you add to Policy.sh will be lost
#  unless you copy Policy.sh somewhere else before running Configure.
#
#  The idea here is to distill in one place the common site-wide
#  "policy" answers (such as installation directories) that are
#  to be "sticky".  If you keep the file Policy.sh around in
#  the same directory as you are building Perl, then Configure will
#  (by default) load up the Policy.sh file just before the
#  platform-specific hints file.
#

#  Allow Configure command-line overrides; usually these won't be
#  needed, but something like -Dprefix=/test/location can be quite
#  useful for testing out new versions.

#Site-specific values:

case "\$perladmin" in
'') perladmin='$perladmin' ;;
esac

# Installation prefix.  Allow a Configure -D override.  You
# may wish to reinstall perl under a different prefix, perhaps
# in order to test a different configuration.
case "\$prefix" in
'') prefix='$prefix' ;;
esac

# Installation directives.  Note that each one comes in three flavors.
# For example, we have privlib, privlibexp, and installprivlib.
# privlib is for private (to perl) library files.
# privlibexp is the same, except any '~' the user gave to Configure
#     is expanded to the user's home directory.  This is figured
#     out automatically by Configure, so you don't have to include it here.
# installprivlib is for systems (such as those running AFS) that
#     need to distinguish between the place where things
#     get installed and where they finally will reside.
# 
# In each case, if your previous value was the default, leave it commented
# out.  That way, if you override prefix, all of these will be
# automatically adjusted.
#
# WARNING:  Be especially careful about architecture-dependent and
# version-dependent names, particularly if you reuse this file for
# different versions of perl.

!GROK!THIS!

for var in bin scriptdir privlib archlib \
	man1dir man3dir sitelib sitearch \
	installbin installscript installprivlib installarchlib \
	installman1dir installman3dir installsitelib installsitearch \
	man1ext man3ext; do
	
    case "$var" in
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
    archlib)	
	case "$prefix" in
	*perl*)	dflt=$prefix/lib/$version/$archname ;;
	*)	dflt=$prefix/lib/$package/$version/$archname ;;
	esac
	;;
    sitelib)
	case "$prefix" in
	*perl*)	dflt=$prefix/lib/site_perl/$apiversion ;;
	*)	dflt=$prefix/lib/$package/site_perl/$apiversion ;;
	esac
	;;
    sitearch)	
	case "$prefix" in
	*perl*)	dflt=$prefix/lib/site_perl/$apiversion/$archname ;;
	*)	dflt=$prefix/lib/$package/site_perl/$apiversion/$archname ;;
	esac
	;;
    man1dir)	dflt="$prefix/man/man1" ;;
    man3dir)
	case "$prefix" in
	*perl*) dflt=`echo $man1dir |
		    sed -e 's/man1/man3/g' -e 's/man\.1/man\.3/g'` ;;
	*)	dflt=$privlib/man/man3 ;;
	esac
	;;

    # Can we assume all sed's have greedy matching?
    man1ext)	dflt=`echo $man1dir | sed -e 's!.*man!!' -e 's!^\.!!'` ;;
    man3ext)	dflt=`echo $man3dir | sed -e 's!.*man!!' -e 's!^\.!!'` ;;

    # It might be possible to fool these next tests.  Please let
    # me know if they don't work right for you.
    installbin)		dflt=`echo $binexp | sed 's#^/afs/#/afs/.#'`;;
    installscript)	dflt=`echo $scriptdirexp | sed 's#^/afs/#/afs/.#'`;;
    installprivlib)	dflt=`echo $privlibexp | sed 's#^/afs/#/afs/.#'`;;
    installarchlib)	dflt=`echo $archlibexp | sed 's#^/afs/#/afs/.#'`;;
    installsitelib)	dflt=`echo $sitelibexp | sed 's#^/afs/#/afs/.#'`;;
    installsitearch)	dflt=`echo $sitearchexp | sed 's#^/afs/#/afs/.#'`;;
    installman1dir)	dflt=`echo $man1direxp | sed 's#^/afs/#/afs/.#'`;;
    installman3dir)	dflt=`echo $man3direxp | sed 's#^/afs/#/afs/.#'`;;
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
#   Andy Dougherty <doughera@lafcol.lafayette.edu>.
#   This file may be distributed under the same terms as Perl itself.

