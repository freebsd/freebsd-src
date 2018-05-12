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
        xchacha20=1
else
        xchacha20=0
fi
