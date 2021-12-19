#!/bin/sh

case $(./config.guess) in
*-darwin*)
	brew install automake
	exit 0
	;;
esac

TARGETS=$@

PACKAGES=""
INSTALL_FIDO_PPA="no"
export DEBIAN_FRONTEND=noninteractive

#echo "Setting up for '$TARGETS'"

set -ex

lsb_release -a

if [ "${TARGETS}" = "kitchensink" ]; then
	TARGETS="kerberos5 libedit pam sk selinux"
fi

for TARGET in $TARGETS; do
    case $TARGET in
    default|without-openssl|without-zlib|c89)
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
    openssl-noec)
	INSTALL_OPENSSL=OpenSSL_1_1_1k
	SSLCONFOPTS="no-ec"
	;;
    openssl-*)
        INSTALL_OPENSSL=$(echo ${TARGET} | cut -f2 -d-)
        case ${INSTALL_OPENSSL} in
          1.1.1_stable)	INSTALL_OPENSSL="OpenSSL_1_1_1-stable" ;;
          1.*)	INSTALL_OPENSSL="OpenSSL_$(echo ${INSTALL_OPENSSL} | tr . _)" ;;
          3.*)	INSTALL_OPENSSL="openssl-${INSTALL_OPENSSL}" ;;
        esac
        PACKAGES="${PACKAGES} putty-tools"
       ;;
    libressl-*)
        INSTALL_LIBRESSL=$(echo ${TARGET} | cut -f2 -d-)
        case ${INSTALL_LIBRESSL} in
          master) ;;
          *) INSTALL_LIBRESSL="v$(echo ${TARGET} | cut -f2 -d-)" ;;
        esac
        PACKAGES="${PACKAGES} putty-tools"
       ;;
    valgrind*)
       PACKAGES="$PACKAGES valgrind"
       ;;
    *) echo "Invalid option '${TARGET}'"
        exit 1
        ;;
    esac
done

if [ "yes" = "$INSTALL_FIDO_PPA" ]; then
    sudo apt update -qq
    sudo apt install -qy software-properties-common
    sudo apt-add-repository -y ppa:yubico/stable
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

if [ ! -z "${INSTALL_OPENSSL}" ]; then
    (cd ${HOME} &&
     git clone https://github.com/openssl/openssl.git &&
     cd ${HOME}/openssl &&
     git checkout ${INSTALL_OPENSSL} &&
     ./config no-threads shared ${SSLCONFOPTS} \
         --prefix=/opt/openssl &&
     make && sudo make install_sw)
fi

if [ ! -z "${INSTALL_LIBRESSL}" ]; then
    (mkdir -p ${HOME}/libressl && cd ${HOME}/libressl &&
     git clone https://github.com/libressl-portable/portable.git &&
     cd ${HOME}/libressl/portable &&
     git checkout ${INSTALL_LIBRESSL} &&
     sh update.sh && sh autogen.sh &&
     ./configure --prefix=/opt/libressl &&
     make -j2 && sudo make install)
fi
