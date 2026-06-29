#!/bin/sh
#-
# Copyright (c) 2026 Dag-Erling Smørgrav
#
# SPDX-License-Identifier: BSD-2-Clause
#

set -eu
exec 3>&2

progname="${0##*/}"
localbase="$(/sbin/sysctl -qn user.localbase || echo /usr/local)"

#
# All the top-level sets except src
#
topsets="base base-dbg kernels kernels-dbg lib32 lib32-dbg tests"

#
# Default repository
#
OS=FreeBSD
BASEREPO=${BASEREPO:-${OS}-base}

#
# Command-line options
#
auto=
base=${BASEDIR:-}
dryrun=
jail=${JAIL:-}
quiet=
repo=
trim=
verbose=
yes=

#
# Print a message
#
msg() {
	local width=$((${COLUMNS:-80} - ${#progname} - 4))
	echo "$@" | /usr/bin/fmt -sw${width} |
	    /usr/bin/sed "s|^|${progname}: |"
}

#
# Print an error message and exit
#
err() {
	msg "$@" >&3
	exit 1
}

#
# Print a warning message
#
warn() {
	if ! [ "${quiet}" ]; then
		msg "$@" >&3
	fi
}

#
# Print an informational message
#
info() {
	if ! [ "${quiet}" ]; then
		msg "$@"
	fi
}

#
# Ask the user for confirmation
#
confirm() {
	local ans=
	if [ "${yes}" ]; then
		return 0
	fi
	while :; do
		read -p "$@ " -r ans
		case ${ans} in
		[Yy]|[Yy][Ee][Ss])
			return 0
			;;
		[Nn]|[Nn][Oo])
			return 1
			;;
		*)
			echo "Please answer with yes or no." >&3
			;;
		esac
	done
}

#
# Execute a command, but print it first if verbose
#
v() {
	if [ "${verbose}" ]; then
		echo "$@" >&3
	fi
	"$@"
}

#
# Compute the path of a file in the target environment
#
path() {
	local path="${base}$1"
	if [ -n "${jail}" ]; then
		path="$(/usr/sbin/jls -j "${jail}" path)${path}"
	fi
	echo "${path}"
}

#
# Check if a file exists in the target environment
#
exists() {
	v test -e "$(path "$@")"
}

#
# Run pkg(7)
#
# pkg(7) currently lacks support for -j and -r, otherwise we could run
# every pkg command through it, which would prepare us for when pkg
# eventually moves into base.  For now, we can use jexec for -j, but
# there is no way to emulate -r.
#
pkg7() {
	if [ -n "${base}" ]; then
		err "pkg(7) currently lacks -r option"
	elif [ -n "${jail}" ]; then
		v /usr/sbin/jexec "${jail}" /usr/sbin/pkg "$@"
	else
		v /usr/sbin/pkg "$@"
	fi
}

#
# Run pkg(8)
#
pkg() {
	v "${localbase}"/sbin/pkg ${jail:+-j"${jail}"} ${base:+-r"${base}"} "$@"
}

#
# Run pkg bootstrap
#
pkg_bootstrap() {
	if ! pkg7 -N 2>/dev/null; then
		warn "Package manager not installed"
		if confirm "Bootstrap the package manager?"; then
			pkg7 bootstrap -r"${repo}" -y
		else
			err "Unable to proceed without package manager"
		fi
	fi
}

#
# Check if a package exists in the repository
#
pkg_exists() {
	pkg rquery -r"${repo}" -U %v "$@" >/dev/null
}

#
# Run pkg fetch
#
pkg_fetch() {
	pkg fetch -r"${repo}" -U ${quiet:+-q} "$@"
}

#
# Run pkg install
#
pkg_install() {
	pkg install -r"${repo}" -U ${dryrun:+-n} ${quiet:+-q} "$@"
}

#
# Check if a package is installed
#
pkg_installed() {
	pkg info -q "$@"
}

#
# Run pkg query
#
pkg_query() {
	pkg query "$@"
}

#
# Run pkg install --register-only
#
pkg_register() {
	pkg install -r"${repo}" --register-only ${dryrun:+-n} ${quiet:+-q} "$@"
}

#
# Run pkg remove
#
pkg_remove() {
	pkg remove ${dryrun:+-n} ${quiet:+-q} "$@"
}

#
# Run pkg rquery
#
pkg_rquery() {
	pkg rquery -r"${repo}" -U "$@"
}

#
# Run pkg unregister
#
pkg_unregister() {
	pkg unregister ${dryrun:+-n} ${quiet:+-q} "$@"
}

#
# Run pkg update
#
pkg_update() {
	pkg update -r"${repo}" ${quiet:+-q} "$@"
}

#
# Run pkg upgrade
#
pkg_upgrade() {
	pkg upgrade -r"${repo}" ${dryrun:+-n} ${quiet:+-q} "$@"
}

#
# Generate a complete transitive list of dependencies
#
pkg_alldeps() {
	local deps=$(pkg rquery %dn "$@")
	if [ "${deps}" ]; then
		echo "${deps}"
		pkg_alldeps ${deps}
	fi | sort -u
}

#
# Check if pkg is new enough
#
check_pkg() {
	local ver=$(pkg -v)
	case ${ver} in
	1.*|2.[0-6].*|2.7.[0-4])
		err "pkg ${ver} is too old, I need pkg 2.7.5 or newer"
		;;
	esac
}

#
# Try to ascertain if the system is running pkgbase
#
ispkgbase() {
	pkg which -q /sbin/init >/dev/null
}

#
# Convert to pkgbase
#
pkgbasify() {
	local sets=base set=

	# Check if we're running pkgbase
	if ispkgbase; then
		err "The system is already running packaged base"
	fi

	# Update the repository
	pkg_update

	# Some paths
	local debug=/usr/lib/debug
	local ld=/libexec/ld-elf.so.1
	local ld32=/libexec/ld-elf32.so.1
	local test=/usr/tests/Kyuafile
	local src=/usr/src

	# Sanity check
	if ! exists "${ld}"; then
		err "No base system found"
	fi

	# Decide whether to register base-dbg
	if ! exists "${debug}${ld}.debug"; then
		warn "No debugging symbols found"
	else
		sets="${sets} base-dbg"
	fi

	# Decide whether to register lib32 and lib32-dbg
	if ! pkg_exists ${OS}-set-lib32; then
		# Not available
	elif ! exists "${ld32}"; then
		warn "No 32-bit loader found"
	else
		sets="${sets} lib32"
		if exists "${debug}${ld32}.debug"; then
			sets="${sets} lib32-dbg"
		fi
	fi

	# Decide whether to register tests
	if ! exists "${test}"; then
		warn "No test suite found"
	else
		sets="${sets} tests"
	fi

	# Decide whether to register kernels and kernels-dbg
	local kernel=/boot/kernel/kernel
	if [ -z "${jail}${base}" ]; then
		if ! kernel=$(/sbin/sysctl -qn kern.bootfile); then
			err "Unable to identify the running kernel"
		fi
		case ${kernel%/kernel} in
		*.old)
			kernel=${kernel%.old/kernel}/kernel
			;;
		esac
		case ${kernel} in
		/boot/kernel/kernel)
			;;
		*)
			kernel=
			;;
		esac
	fi
	if [ -z "${kernel}" ] || ! exists "${kernel}"; then
		warn "Non-standard kernel, or no kernel installed"
		kernel=
	else
		sets="${sets} kernels"
		if exists "${debug}${kernel}.debug"; then
			sets="${sets} kernels-dbg"
		fi
	fi

	# Decide whether to register src
	if ! exists "${src}/Makefile.inc1"; then
		warn "No source tree found"
	elif exists "${src}/.git"; then
		warn "Source tree managed by Git "
	else
		sets="${sets} src"
	fi

	# We now have our list of sets
	sets=$(for set in ${sets}; do echo ${OS}-set-$set; done)

	if ! [ "${quiet}" ] || ! [ "${yes}" ]; then
		echo "The following package sets will be installed:"
		echo
		pkg_rquery "%n %c" ${sets} | \
		    /usr/bin/sed 's/ (metapackage)//' |
		    /usr/bin/column -tl2 |
		    /usr/bin/sed 's/^/    /'
		echo
	fi

	# Final checkpoint
	if ! [ "${auto}" ] && ! confirm "Proceed with conversion?"; then
		err "Conversion abandoned"
	fi

	# Download the packages first
	warn "Downloading packages, this may take a while..."
	pkg_fetch -d -y ${sets}

	# Now register them
	pkg_register -y ${sets}

	# Force-upgrade
	if ! [ "${auto}" ] && confirm "Reinstall base packages?"; then
		pkg_upgrade -f -y
	fi

	# Check if BACKUP_LIBRARIES is turned on
	case $(pkg config BACKUP_LIBRARIES) in
	no)
		warn "The recommended BACKUP_LIBRARIES option is not enabled"
		if ! [ "${auto}" ] &&
		    confirm "Enable BACKUP_LIBRARIES option?"; then
			if ! [ "${dryrun}" ]; then
				v /usr/bin/sed -i.bak \
				    -e $'1i\\\nBACKUP_LIBRARIES = true;' \
				    $(path ${localbase}/etc/pkg.conf)
			fi
			info "BACKUP_LIBRARIES option enabled"
		fi
		;;
	esac

	# Done!
	if ! [ "${dryrun}" ]; then
		info "Conversion complete"
	fi
}

#
# Convert from pkgbase
#
depkgbasify() {
	local havesrc= pkg= set=

	# Check if we're running pkgbase
	if ! ispkgbase; then
		err "The system is not running packaged base"
	fi

	# Identify and update the repository
	repo=$(pkg_query %R ${OS}-runtime)
	info "Using repository ${repo}"
	pkg_update

	# Check which sets are partially or completely installed
	info "Examining the system..."
	for set in ${topsets}; do
		if pkg_installed ${OS}-set-${set}; then
			info "The ${set} set is installed"
			continue
		elif ! pkg_exists %v ${OS}-set-${set}; then
			# set does not exist
			continue
		fi
		local want=$(pkg_alldeps ${OS}-set-${set} | grep -v ${OS}-set)
		local have=$(pkg_query %n ${want} || true)
		if [ "${have}" = "${want}" ]; then
			info "The ${set} set is complete"
		elif [ "${have}" ]; then
			info "The ${set} set is partially installed"
			if ! [ "${auto}" ] &&
			    confirm "Complete the ${set} set?"; then
				pkg_fetch -d -y ${OS}-set-${set}
				pkg_install -y ${OS}-set-${set}
			fi
		else
			info "The ${set} set is absent"
		fi
	done

	# Check if the source tree is installed
	for pkg in set-src src-sys src; do
		if pkg_installed ${OS}-${pkg}; then
			havesrc="${havesrc} ${OS}-${pkg}"
		fi
	done
	if [ "${havesrc}" ]; then
		info "The source tree appears to be installed.  You may want" \
		     "to uninstall it before converting to make room for a" \
		     "Git clone."
		if ! [ "${auto}" ] && confirm "Uninstall the source tree?"; then
			pkg_remove -fy ${havesrc}
		fi
	fi

	# One final upgrade?
	if ! [ "${auto}" ] && confirm "Upgrade installed base packages?"; then
		pkg_upgrade -y
	fi

	# Final checkpoint
	if ! [ "${auto}" ] && ! confirm "Proceed with conversion?"; then
		err "Conversion abandoned"
	fi

	# Unregister all base packages
	pkg_unregister -f -y $(pkg_query -e '%o ~ base/*' '%n')

	# Done!
	if ! [ "${dryrun}" ]; then
		info "Conversion complete"
	fi
}

#
# Print usage message and exit
#
usage() {
	exec >&3
	echo -n "usage: ${progname} "
	case ${progname%.sh} in
	pkgbasify)
		echo "[-hnqtv] [-a | -y] [-b basedir] [-j jail] [-r repo]"
		;;
	depkgbasify)
		echo "[-hnqv] [-a | -y] [-b basedir] [-j jail]"
		;;
	*)
		echo "-h"
		;;
	esac
	exit 1
}

#
# Entry point
#
main() {
	local opt

	# Check program name and set defaults
	case ${progname%.sh} in
	pkgbasify)
		repo=${BASEREPO}
		;;
	depkgbasify)
		;;
	*)
		usage
		;;
	esac

	# Parse options
	while getopts "ab:hj:nr:qtvy" opt; do
		case ${opt} in
		a)
			auto=true
			;;
		b)
			base="${OPTARG}"
			;;
		h)
			usage
			;;
		j)
			jail="${OPTARG}"
			;;
		n)
			dryrun=true
			;;
		r)
			repo="${OPTARG}"
			;;
		q)
			quiet=true
			;;
		t)
			trim=true
			;;
		v)
			verbose=true
			;;
		y)
			yes=true
			;;
		*)
			usage
			;;
		esac
	done
	shift $((OPTIND - 1))

	# Check for conflicting options
	if [ "${auto}" ] && [ "${yes}" ]; then
		usage
	fi
	case ${progname%.sh} in
	depkgbasify)
		if [ "${repo}" ] || [ "${trim}" ]; then
			usage
		fi
		;;
	esac

	# Bootstrap pkg if necessary
	pkg_bootstrap

	# Check the pkg version
	check_pkg

	# Do what we were asked to do
	case ${progname%.sh} in
	pkgbasify)
		pkgbasify "$@"
		;;
	depkgbasify)
		depkgbasify "$@"
		;;
	esac
}

main "$@"
