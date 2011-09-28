#	$OpenBSD: cipher-speed.sh,v 1.4 2011/08/02 01:23:41 djm Exp $
#	Placed in the Public Domain.

tid="cipher speed"

getbytes ()
{
	sed -n '/transferred/s/.*secs (\(.* bytes.sec\).*/\1/p'
}

tries="1 2"
DATA=/bin/ls
DATA=/bsd

ciphers="aes128-cbc 3des-cbc blowfish-cbc cast128-cbc 
	arcfour128 arcfour256 arcfour 
	aes192-cbc aes256-cbc rijndael-cbc@lysator.liu.se
	aes128-ctr aes192-ctr aes256-ctr"
macs="hmac-sha1 hmac-md5 umac-64@openssh.com hmac-sha1-96 hmac-md5-96"
config_defined HAVE_EVP_SHA256 &&
    macs="$macs hmac-sha2-256 hmac-sha2-256-96 hmac-sha2-512 hmac-sha2-512-96"

for c in $ciphers; do for m in $macs; do
	trace "proto 2 cipher $c mac $m"
	for x in $tries; do
		echon "$c/$m:\t"
		( ${SSH} -o 'compression no' \
			-F $OBJ/ssh_proxy -2 -m $m -c $c somehost \
			exec sh -c \'"dd of=/dev/null obs=32k"\' \
		< ${DATA} ) 2>&1 | getbytes

		if [ $? -ne 0 ]; then
			fail "ssh -2 failed with mac $m cipher $c"
		fi
	done
done; done

ciphers="3des blowfish"
for c in $ciphers; do
	trace "proto 1 cipher $c"
	for x in $tries; do
		echon "$c:\t"
		( ${SSH} -o 'compression no' \
			-F $OBJ/ssh_proxy -1 -c $c somehost \
			exec sh -c \'"dd of=/dev/null obs=32k"\' \
		< ${DATA} ) 2>&1 | getbytes
		if [ $? -ne 0 ]; then
			fail "ssh -1 failed with cipher $c"
		fi
	done
done
