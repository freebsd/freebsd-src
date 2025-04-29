#!/bin/sh
#
#

set -e

export ASSUME_ALWAYS_YES="YES"
export PKG_DBDIR="/tmp/pkg"
export PERMISSIVE="YES"
export REPO_AUTOUPDATE="NO"
export ROOTDIR="$PWD/dvd"
export PKGCMD="/usr/sbin/pkg -d --rootdir ${ROOTDIR}"
export PORTSDIR="${PORTSDIR:-/usr/ports}"

_DVD_PACKAGES="devel/git@lite
graphics/drm-kmod
graphics/drm-510-kmod
graphics/drm-515-kmod
misc/freebsd-doc-all
net/mpd5
net/rsync
net/wifi-firmware-kmod@release
ports-mgmt/pkg
shells/bash
shells/zsh
security/sudo@default
sysutils/screen
sysutils/seatd
sysutils/tmux
www/firefox
www/links
x11/gnome
x11/kde
x11/sddm
x11/xorg
x11-wm/sway"

# If NOPORTS is set for the release, do not attempt to build pkg(8).
if [ ! -f ${PORTSDIR}/Makefile ]; then
	echo "*** ${PORTSDIR} is missing!    ***"
	echo "*** Skipping pkg-stage.sh     ***"
	echo "*** Unset NOPORTS to fix this ***"
	exit 0
fi

if [ ! -x /usr/local/sbin/pkg ]; then
	/etc/rc.d/ldconfig restart
	/usr/bin/make -C ${PORTSDIR}/ports-mgmt/pkg install clean
fi

export PKG_ABI=$(pkg --rootdir ${ROOTDIR} config ABI)
export PKG_ALTABI=$(pkg --rootdir ${ROOTDIR} config ALTABI 2>/dev/null)
export PKG_REPODIR="packages/${PKG_ABI}"

/bin/mkdir -p ${ROOTDIR}/${PKG_REPODIR}
if [ ! -z "${PKG_ALTABI}" ]; then
	ln -s ${PKG_ABI} ${ROOTDIR}/packages/${PKG_ALTABI}
fi

# Ensure the ports listed in _DVD_PACKAGES exist to sanitize the
# final list.
for _P in ${_DVD_PACKAGES}; do
	if [ -d "${PORTSDIR}/${_P%%@*}" ]; then
		DVD_PACKAGES="${DVD_PACKAGES} ${_P}"
	else
		echo "*** Skipping nonexistent port: ${_P%%@*}"
	fi
done

# Make sure the package list is not empty.
if [ -z "${DVD_PACKAGES}" ]; then
	echo "*** The package list is empty."
	echo "*** Something is very wrong."
	# Exit '0' so the rest of the build process continues
	# so other issues (if any) can be addressed as well.
	exit 0
fi

# Print pkg(8) information to make debugging easier.
${PKGCMD} -vv
${PKGCMD} update -f
${PKGCMD} fetch -o ${PKG_REPODIR} -d ${DVD_PACKAGES}

# Create the 'Latest/pkg.txz' symlink so 'pkg bootstrap' works
# using the on-disc packages.
export LATEST_DIR="${ROOTDIR}/${PKG_REPODIR}/Latest"
mkdir -p ${LATEST_DIR}
ln -s ../All/$(${PKGCMD} rquery %n-%v pkg).pkg ${LATEST_DIR}/pkg.pkg
ln -sf pkg.pkg ${LATEST_DIR}/pkg.txz

${PKGCMD} repo ${PKG_REPODIR}

# Always exit '0', even if pkg(8) complains about conflicts.
exit 0
