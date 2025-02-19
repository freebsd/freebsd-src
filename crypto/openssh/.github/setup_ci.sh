#!/bin/sh

PACKAGES=""

 . .github/configs $@

host=`./config.guess`
echo "config.guess: $host"
case "$host" in
*cygwin)
	PACKAGER=setup
	echo Setting CYGWIN system environment variable.
	setx CYGWIN "binmode"
	echo Removing extended ACLs so umask works as expected.
	setfacl -b . regress
	PACKAGES="$PACKAGES,autoconf,automake,cygwin-devel,gcc-core"
	PACKAGES="$PACKAGES,make,openssl,libssl-devel,zlib-devel"
	;;
*-darwin*)
	PACKAGER=brew
	PACKAGES="automake"
	;;
*)
	PACKAGER=apt
esac

TARGETS=$@

INSTALL_FIDO_PPA="no"
export DEBIAN_FRONTEND=noninteractive

set -e

if [ -x "`which lsb_release 2>&1`" ]; then
	lsb_release -a
fi

if [ ! -z "$SUDO" ]; then
	# Ubuntu 22.04 defaults to private home dirs which prevent the
	# agent-getpeerid test from running ssh-add as nobody.  See
	# https://github.com/actions/runner-images/issues/6106
	if ! "$SUDO" -u nobody test -x ~; then
		echo ~ is not executable by nobody, adding perms.
		chmod go+x ~
	fi
	# Some of the Mac OS X runners don't have a nopasswd sudo rule. Regular
	# sudo still works, but sudo -u doesn't.  Restore the sudo rule.
	if ! "$SUDO" grep  -E 'runner.*NOPASSWD' /etc/passwd >/dev/null; then
		echo "Restoring runner nopasswd rule to sudoers."
		echo 'runner ALL=(ALL) NOPASSWD: ALL' |$SUDO tee -a /etc/sudoers
	fi
	if ! "$SUDO" -u nobody -S test -x ~ </dev/null; then
		echo "Still can't sudo to nobody."
		exit 1
	fi
fi

if [ "${TARGETS}" = "kitchensink" ]; then
	TARGETS="krb5 libedit pam sk selinux"
fi

for flag in $CONFIGFLAGS; do
    case "$flag" in
    --with-pam)		TARGETS="${TARGETS} pam" ;;
    --with-libedit)	TARGETS="${TARGETS} libedit" ;;
    esac
done

echo "Setting up for '$TARGETS'"
for TARGET in $TARGETS; do
    case $TARGET in
    default|without-openssl|without-zlib|c89)
        # nothing to do
        ;;
    clang-sanitize*)
        PACKAGES="$PACKAGES clang-12"
        ;;
    cygwin-release)
        PACKAGES="$PACKAGES libcrypt-devel libfido2-devel libkrb5-devel"
        ;;
    gcc-sanitize*)
        ;;
    clang-*|gcc-*)
        compiler=$(echo $TARGET | sed 's/-Werror//')
        PACKAGES="$PACKAGES $compiler"
        ;;
    krb5)
        PACKAGES="$PACKAGES libkrb5-dev"
	;;
    heimdal)
        PACKAGES="$PACKAGES heimdal-dev"
        ;;
    libedit)
	case "$PACKAGER" in
	setup)	PACKAGES="$PACKAGES libedit-devel" ;;
	apt)	PACKAGES="$PACKAGES libedit-dev" ;;
	esac
        ;;
    *pam)
	case "$PACKAGER" in
	apt)	PACKAGES="$PACKAGES libpam0g-dev" ;;
	esac
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
    musl)
	PACKAGES="$PACKAGES musl-tools"
	;;
    tcmalloc)
        PACKAGES="$PACKAGES libgoogle-perftools-dev"
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
        PACKAGES="${PACKAGES} putty-tools dropbear-bin"
       ;;
    libressl-*)
        INSTALL_LIBRESSL=$(echo ${TARGET} | cut -f2 -d-)
        case ${INSTALL_LIBRESSL} in
          master) ;;
          *) INSTALL_LIBRESSL="$(echo ${TARGET} | cut -f2 -d-)" ;;
        esac
        PACKAGES="${PACKAGES} putty-tools dropbear-bin"
       ;;
    boringssl)
        INSTALL_BORINGSSL=1
        PACKAGES="${PACKAGES} cmake ninja-build"
       ;;
    putty-*)
	INSTALL_PUTTY=$(echo "${TARGET}" | cut -f2 -d-)
	PACKAGES="${PACKAGES} cmake"
	;;
    valgrind*)
       PACKAGES="$PACKAGES valgrind"
       ;;
    zlib-*)
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

tries=3
while [ ! -z "$PACKAGES" ] && [ "$tries" -gt "0" ]; do
    case "$PACKAGER" in
    apt)
	sudo apt update -qq
	if sudo apt install -qy $PACKAGES; then
		PACKAGES=""
	fi
	;;
    brew)
	if [ ! -z "PACKAGES" ]; then
		if brew install $PACKAGES; then
			PACKAGES=""
		fi
	fi
	;;
    setup)
	if /cygdrive/c/setup.exe -q -P `echo "$PACKAGES" | tr ' ' ,`; then
		PACKAGES=""
	fi
	;;
    esac
    if [ ! -z "$PACKAGES" ]; then
	sleep 90
    fi
    tries=$(($tries - 1))
done
if [ ! -z "$PACKAGES" ]; then
	echo "Package installation failed."
	exit 1
fi

if [ "${INSTALL_HARDENED_MALLOC}" = "yes" ]; then
    (cd ${HOME} &&
     git clone https://github.com/GrapheneOS/hardened_malloc.git &&
     cd ${HOME}/hardened_malloc &&
     make && sudo cp out/libhardened_malloc.so /usr/lib/)
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
    if [ "${INSTALL_LIBRESSL}" = "master" ]; then
        (mkdir -p ${HOME}/libressl && cd ${HOME}/libressl &&
         git clone https://github.com/libressl-portable/portable.git &&
         cd ${HOME}/libressl/portable &&
         git checkout ${INSTALL_LIBRESSL} &&
         sh update.sh && sh autogen.sh &&
         ./configure --prefix=/opt/libressl &&
         make && sudo make install)
    else
        LIBRESSL_URLBASE=https://cdn.openbsd.org/pub/OpenBSD/LibreSSL
        (cd ${HOME} &&
         wget ${LIBRESSL_URLBASE}/libressl-${INSTALL_LIBRESSL}.tar.gz &&
         tar xfz libressl-${INSTALL_LIBRESSL}.tar.gz &&
         cd libressl-${INSTALL_LIBRESSL} &&
         ./configure --prefix=/opt/libressl && make && sudo make install)
    fi
fi

if [ ! -z "${INSTALL_BORINGSSL}" ]; then
    (cd ${HOME} && git clone https://boringssl.googlesource.com/boringssl &&
     cd ${HOME}/boringssl && mkdir build && cd build &&
     cmake -GNinja  -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && ninja &&
     mkdir -p /opt/boringssl/lib &&
     cp ${HOME}/boringssl/build/crypto/libcrypto.a /opt/boringssl/lib &&
     cp -r ${HOME}/boringssl/include /opt/boringssl)
fi

if [ ! -z "${INSTALL_ZLIB}" ]; then
    (cd ${HOME} && git clone https://github.com/madler/zlib.git &&
     cd ${HOME}/zlib && ./configure && make &&
     sudo make install prefix=/opt/zlib)
fi

if [ ! -z "${INSTALL_PUTTY}" ]; then
    ver="${INSTALL_PUTTY}"
    case "${INSTALL_PUTTY}" in
    snapshot)
	tarball=putty.tar.gz
	(cd /tmp && wget https://tartarus.org/~simon/putty-snapshots/${tarball})
	;;
    *)
	tarball=putty-${ver}.tar.gz
	(cd /tmp && wget https://the.earth.li/~sgtatham/putty/${ver}/${tarball})
	;;
    esac
    (cd ${HOME} && tar xfz /tmp/${tarball} && cd putty-*
     if [ -f CMakeLists.txt ]; then
	cmake . && cmake --build . && sudo cmake --build . --target install
     else
	./configure && make && sudo make install
     fi
    )
    /usr/local/bin/plink -V
fi
