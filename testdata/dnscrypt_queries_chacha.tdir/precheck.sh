# dnscrypt precheck.sh

# if no dnscrypt; exit
if grep "define USE_DNSCRYPT 1" $PRE/config.h; then
        echo "have dnscrypt"
else
        echo "no dnscrypt"
        exit 0
fi

# if no xchacha20 support in unbound; exit
if grep "define USE_DNSCRYPT_XCHACHA20 1" $PRE/config.h; then
        echo "have xchacha20"
        xchacha20=1
else
        echo "no xchacha20"
        xchacha20=0
        exit 0
fi

# if dnscrypt-proxy does not support xchacha20; exit
if (dnscrypt-proxy -h 2>&1 | grep -q 'XChaCha20-Poly1305 cipher: present'); then
		echo "dnscrypt-proxy has xchacha20"
else
		echo "dnscrypt-proxy does not have xchacha20"
		exit 0
fi
