perl_cflags='optimize="-g"'
d_volatile=undef
d_castneg=undef
cc=cc
glibpth="/usr/lib/cmplrs/cc $glibpth"
groupstype=int
nm_opt='-B'
case $PATH in
*bsd*:/bin:*) cat <<END >&4
NOTE:  Some people have reported having much better luck with Mips CC than
with the BSD cc.  Put /bin first in your PATH if you have difficulties.
END
;;
esac
