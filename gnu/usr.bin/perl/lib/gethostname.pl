#
# Simple package to get the hostname via __sysctl(2).
#
# Written 13-Feb-96 by Jörg Wunsch, interface business GmbH Dresden.
# Placed in the public domain.
#
# $Id: gethostname.pl,v 1.1 1996/02/13 13:17:49 joerg Exp $
#

package gethostname;

require "sys/syscall.ph";
require "sys/sysctl.ph";

#
# usage:
#
# require "gethostname.pl";
# printf "This machine is named \"%s\".\n", &gethostname;
#

sub main'gethostname {
    # get hostname via sysctl(2)
    local($name, $oldval, $oldlen, $len);
    $name = pack("LL", &CTL_KERN, &KERN_HOSTNAME);
    # 64-byte string to get the hostname
    $oldval = " " x 64;
    $oldlen = pack("L", length($oldval));
    syscall(&SYS___sysctl, $name, 2, $oldval, $oldlen, 0, 0) != -1 ||
	die "Cannot get hostname via sysctl(2), errno = $!\n";

    ($len) = unpack("L", $oldlen);
    return substr($oldval, 0, $len - 1);
}

1;
