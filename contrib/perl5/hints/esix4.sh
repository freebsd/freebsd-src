# hints/esix4.sh
# Original esix4 hint file courtesy of
# Kevin O'Gorman ( kevin@kosman.UUCP, kevin%kosman.uucp@nrc.com )
#
# Use Configure -Dcc=gcc to use gcc.
case "$cc" in
'') cc='/bin/cc'
    test -f $cc || cc='/usr/ccs/bin/cc'
    ;;
esac
ldflags='-L/usr/ccs/lib -L/usr/ucblib'
test -d /usr/local/man || mansrc='none'
ccflags='-I/usr/include -I/usr/ucbinclude'
libswanted=`echo " $libswanted " | sed -e 's/ malloc / /' `
d_index='undef'
d_suidsafe=define
usevfork='false'
if test "$osvers" = "3.0"; then
	d_gconvert='undef'
	grep 'define[ 	]*AF_OSI[ 	]' /usr/include/sys/socket.h | grep '/\*[^*]*$' >/tmp/esix$$
	if test -s /tmp/esix$$; then
		cat <<EOM >&2

WARNING: You are likely to have problems compiling the Socket extension
unless you fix the unterminated comment for AF_OSI in the file
/usr/include/sys/socket.h.

EOM
	fi
	rm -f /tmp/esix$$
fi

cat <<'EOM' >&4

If you wish to use dynamic linking, you must use 
	LD_LIBRARY_PATH=`pwd`; export LD_LIBRARY_PATH
or
	setenv LD_LIBRARY_PATH `pwd`
before running make.

EOM
