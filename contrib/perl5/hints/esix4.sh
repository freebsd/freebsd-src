# hints/esix4.sh
# Original esix4 hint file courtesy of
# Kevin O'Gorman ( kevin@kosman.UUCP, kevin%kosman.uucp@nrc.com )
#
# Use Configure -Dcc=gcc to use gcc.

# Why can't we just use PATH?  It contains /usr/ccs/bin.
case "$cc" in
'') cc='/bin/cc'
    test -f $cc || cc='/usr/ccs/bin/cc'
    ;;
esac

ldflags="$ldflags -L/usr/ccs/lib -L/usr/ucblib"
test -d /usr/local/man || mansrc='none'
# Do we really need to tell cc to look in /usr/include?
ccflags="$ccflags -I/usr/include -I/usr/ucbinclude"
libswanted=`echo " $libswanted " | sed -e 's/ malloc / /' `
d_index='undef'
d_suidsafe=define
usevfork='false'
if test "$osvers" = "3.0"; then
	d_gconvert='undef'
	grep 'define[ 	]*AF_OSI[ 	]' /usr/include/sys/socket.h | grep '/\*[^*]*$' >esix$$
	if test -s esix$$; then
		cat <<EOM >&2

WARNING: You are likely to have problems compiling the Socket extension
unless you fix the unterminated comment for AF_OSI in the file
/usr/include/sys/socket.h.

EOM
	fi
	rm -f esix$$
fi

