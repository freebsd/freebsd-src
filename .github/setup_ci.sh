#!/usr/bin/env bash

case $(./config.guess) in
*-darwin*)
	brew install automake
	exit 0
	;;
esac

TARGETS=$@

PACKAGES=""
INSTALL_FIDO_PPA="no"

#echo "Setting up for '$TARGETS'"

set -ex

lsb_release -a

if [ "${TARGETS}" = "kitchensink" ]; then
	TARGETS="kerberos5 libedit pam sk selinux"
fi

for TARGET in $TARGETS; do
    case $TARGET in
    default|without-openssl|without-zlib)
        # nothing to do
        ;;
    kerberos5)
        PACKAGES="$PACKAGES heimdal-dev"
        #PACKAGES="$PACKAGES libkrb5-dev"
        ;;
    libedit)
        PACKAGES="$PACKAGES libedit-dev"
        ;;
    *pam)
        PACKAGES="$PACKAGES libpam0g-dev"
        ;;
    sk)
        INSTALL_FIDO_PPA="yes"
        PACKAGES="$PACKAGES libfido2-dev libu2f-host-dev libcbor-dev"
        ;;
    selinux)
        PACKAGES="$PACKAGES libselinux1-dev selinux-policy-dev"
        ;;
    hardenedmalloc)
        INSTALL_HARDENED_MALLOC=yes
       ;;
    openssl-head)
        INSTALL_OPENSSL_HEAD=yes
       ;;
    libressl-head)
        INSTALL_LIBRESSL_HEAD=yes
       ;;
    valgrind*)
       PACKAGES="$PACKAGES valgrind"
       ;;
    *) echo "Invalid option '${TARGET}'"
        exit 1
        ;;
    esac
done

if [ "yes" == "$INSTALL_FIDO_PPA" ]; then
    sudo apt update -qq
    sudo apt install software-properties-common
    sudo apt-add-repository ppa:yubico/stable
fi

if [ "x" != "x$PACKAGES" ]; then 
    sudo apt update -qq
    sudo apt install -qy $PACKAGES
fi

if [ "${INSTALL_HARDENED_MALLOC}" = "yes" ]; then
    (cd ${HOME} &&
     git clone https://github.com/GrapheneOS/hardened_malloc.git &&
     cd ${HOME}/hardened_malloc &&
     make -j2 && sudo cp libhardened_malloc.so /usr/lib/)
fi

if [ "${INSTALL_OPENSSL_HEAD}" = "yes" ];then
    (cd ${HOME} &&
     git clone https://github.com/openssl/openssl.git &&
     cd ${HOME}/openssl &&
     ./config no-threads no-engine no-fips no-shared --prefix=/opt/openssl/head &&
     make -j2 && sudo make install_sw)
fi

if [ "${INSTALL_LIBRESSL_HEAD}" = "yes" ];then
    (mkdir -p ${HOME}/libressl && cd ${HOME}/libressl &&
     git clone https://github.com/libressl-portable/portable.git &&
     cd ${HOME}/libressl/portable && sh update.sh && sh autogen.sh &&
     ./configure --prefix=/opt/libressl/head &&
     make -j2 && sudo make install_sw)
fi
