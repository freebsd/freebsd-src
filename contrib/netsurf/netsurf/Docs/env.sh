#!/bin/sh
#
# NetSurf Library, tool and browser support script
#
# Usage: source env.sh
# TARGET_ABI sets the target for library builds
# TARGET_WORKSPACE is the workspace directory to keep the sandboxes
#
# This script allows NetSurf and its libraries to be built without
#   requiring installation into a system.
#
# Copyright 2013 Vincent Sanders <vince@netsurf-browser.org>
# Released under the MIT Licence

# parameters
if [ "x${TARGET_ABI}" = "x" ]; then
    TARGET_ABI=$(uname -s)
fi

if [ "x${TARGET_WORKSPACE}" = "x" ]; then
    TARGET_WORKSPACE=${HOME}/dev-netsurf/workspace
fi

if [ "x${USE_CPUS}" = "x" ]; then
    NCPUS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null)
    NCPUS="${NCPUS:-1}"
    NCPUS=$((NCPUS * 2))
    USE_CPUS="-j${NCPUS}"
fi

# The GTK version to build for (either 2 or 3 currently)
if [ "x${NETSURF_GTK_MAJOR}" = "x" ]; then
    NETSURF_GTK_MAJOR=3
fi

# The host system doing the building
HOST_ABI=$(uname -s)

# setup environment
echo "HOST_ABI=${HOST_ABI}"
echo "TARGET_ABI=${TARGET_ABI}"
echo "TARGET_WORKSPACE=${TARGET_WORKSPACE}"
echo "USE_CPUS=${USE_CPUS}"

export PREFIX=${TARGET_WORKSPACE}/inst-${TARGET_ABI}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}::
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin
export NETSURF_GTK_MAJOR

# NetSurf GIT repositories
NS_GIT="git://git.netsurf-browser.org"

# internal libraries all frontends require (order is important)
NS_INTERNAL_LIBS="buildsystem libwapcaplet libparserutils libhubbub libdom libcss libnsgif libnsbmp"

# The browser itself
NS_BROWSER="netsurf"

# add target specific libraries
if [ "x${TARGET_ABI}" = "xHaiku" ]; then
    # tools required to build the browser
    NS_TOOLS=""
    NS_FRONTEND_LIBS=""
elif [ "x${TARGET_ABI}" = "xriscos" ]; then
    # tools required to build the browser
    NS_TOOLS="nsgenbind"
    # libraries required for the risc os target abi
    NS_FRONTEND_LIBS="libsvgtiny librufl libpencil librosprite"
else
    # tools required to build the browser
    NS_TOOLS="nsgenbind"
    # internal libraries only required by some frontends
    NS_FRONTEND_LIBS="libsvgtiny libnsfb"
fi

################ OS Package installation ################

# deb packages for dpkg based systems
NS_DEV_DEB="build-essential pkg-config git gperf libcurl3-dev libpng-dev libjpeg-dev libmozjs185-dev"
NS_TOOL_DEB="flex bison libhtml-parser-perl"
if [ "x${NETSURF_GTK_MAJOR}" = "x3" ]; then
    NS_GTK_DEB="libgtk-3-dev librsvg2-dev"
else
    NS_GTK_DEB="libgtk2.0-dev librsvg2-dev"
fi

# apt get commandline to install necessary dev packages
ns-apt-get-install()
{
    sudo apt-get install $(echo ${NS_DEV_DEB} ${NS_TOOL_DEB} ${NS_GTK_DEB})
}

# RPM packages for rpm based systems (tested on fedora 20)
NS_DEV_RPM="git gcc pkgconfig libexpat-devel openssl-devel js-devel-1.8.5 libcurl-devel perl-Digest-MD5-File libjpeg-devel libpng-devel"
NS_TOOL_RPM="flex bison"
if [ "x${NETSURF_GTK_MAJOR}" = "x3" ]; then
    NS_GTK_RPM="gtk3-devel librsvg2-devel"
else
    NS_GTK_RPM="gtk2-devel librsvg2-devel"
fi

# yum commandline to install necessary dev packages
ns-yum-install()
{
    sudo yum -y install $(echo ${NS_DEV_RPM} ${NS_TOOL_RPM} ${NS_GTK_RPM})
}

# Haiku secondary arch suffix:
# empty for primary (gcc2 on x86),
# "_x86" for gcc4 secondary.
HA=
# Haiku packages
NS_DEV_HPKG="curl${HA}_devel libpng${HA}_devel jpeg${HA}_devel openssl${HA}_devel libiconv${HA}_devel expat${HA}_devel pkgconfig${HA} gperf${HA}"

# pkgman commandline to install necessary dev packages
ns-pkgman-install()
{
    pkgman install $(echo ${NS_DEV_HPKG})
}

# generic for help text
NS_DEV_GEN="git, gcc, pkgconfig, expat library, openssl library, spidermonkey-1.8.5 library, libcurl, perl, perl MD5 digest, libjpeg library, libpng library"
NS_TOOL_GEN="flex tool, bison tool"
if [ "x${NETSURF_GTK_MAJOR}" = "x3" ]; then
    NS_GTK_GEN="gtk+ 3 toolkit library, librsvg2 library"
else
    NS_GTK_GEN="gtk+ 2 toolkit library, librsvg2 library"
fi


# Genertic OS package install
#  looks for package managers and tries to use them if present
ns-package-install()
{
    if [ -x "/usr/bin/apt-get" ]; then
	ns-apt-get-install
    elif [ -x "/usr/bin/yum" ]; then
	ns-yum-install
    elif [ -x "/bin/pkgman" ]; then
	ns-pkgman-install
    else
        echo "Unable to determine OS packaging system in use."
	echo "Please ensure development packages are installed for:"
	echo ${NS_DEV_GEN}"," ${NS_TOOL_GEN}"," ${NS_GTK_GEN}
    fi
}

################ Development helpers ################

# git pull in all repos parameters are passed to git pull
ns-pull()
{
    for REPO in ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_TOOLS} ${NS_BROWSER} ; do 
	echo -n "     GIT: Pulling ${REPO}: "
	if [ -f ${TARGET_WORKSPACE}/${REPO}/.git/config ]; then
	    (cd ${TARGET_WORKSPACE}/${REPO} && git pull $*; )
	else
	    echo "Repository not present"	    
	fi
    done
}

# clone all repositories
ns-clone()
{
    mkdir -p ${TARGET_WORKSPACE}
    for REPO in $(echo ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_RISCOS_LIBS} ${NS_TOOLS} ${NS_BROWSER}) ; do 
	echo -n "     GIT: Cloning ${REPO}: "
	if [ -f ${TARGET_WORKSPACE}/${REPO}/.git/config ]; then
	    echo "Repository already present"
	else
	    (cd ${TARGET_WORKSPACE} && git clone ${NS_GIT}/${REPO}.git; )
	fi
    done

    # put current env.sh in place in workspace
    if [ ! -f "${TARGET_WORKSPACE}/env.sh" -a -f ${TARGET_WORKSPACE}/${NS_BROWSER}/Docs/env.sh ];then
	cp ${TARGET_WORKSPACE}/${NS_BROWSER}/Docs/env.sh ${TARGET_WORKSPACE}/env.sh
    fi
}

# issues a make command to all libraries
ns-make-libs()
{
    for REPO in $(echo ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_TOOLS}); do 
	echo "    MAKE: make -C ${REPO} $USE_CPUS $*"
        make -C ${TARGET_WORKSPACE}/${REPO} TARGET=${TARGET_ABI} $USE_CPUS $*
    done
}

# issues a make command for framebuffer libraries
ns-make-libnsfb()
{
    echo "    MAKE: make -C libnsfb $USE_CPUS $*"
    make -C ${TARGET_WORKSPACE}/libnsfb TARGET=${TARGET_ABI} $USE_CPUS $*
}

# pulls all repos and makes and installs the libraries and tools
ns-pull-install()
{
    ns-pull $*

    ns-make-libs install
}

# Passes appropriate flags to make
ns-make()
{
    make $USE_CPUS "$@"
}

