#!/usr/bin/env bash

TARGETS=$@

PACKAGES=""
INSTALL_FIDO_PPA="no"

#echo "Setting up for '$TARGETS'"

set -ex

lsb_release -a

for TARGET in $TARGETS; do
    case $TARGET in
    ""|--without-openssl|--without-zlib)
        # nothing to do
        ;;
    "--with-kerberos5")
        PACKAGES="$PACKAGES heimdal-dev"
        #PACKAGES="$PACKAGES libkrb5-dev"
        ;;
    "--with-libedit")
        PACKAGES="$PACKAGES libedit-dev"
        ;;
    "--with-pam")
        PACKAGES="$PACKAGES libpam0g-dev"
        ;;
    "--with-security-key-builtin")
        INSTALL_FIDO_PPA="yes"
        PACKAGES="$PACKAGES libfido2-dev libu2f-host-dev"
        ;;
    "--with-selinux")
        PACKAGES="$PACKAGES libselinux1-dev selinux-policy-dev"
        ;;
    *) echo "Invalid option"
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
