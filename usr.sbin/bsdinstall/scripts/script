#!/bin/sh
#-
# Copyright (c) 2013 Nathan Whitehorn
# Copyright (c) 2013-2015 Devin Teske
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
############################################################ INCLUDES

BSDCFG_SHARE="/usr/share/bsdconfig"
. $BSDCFG_SHARE/common.subr || exit 1
f_dprintf "%s: loading includes..." "$0"
f_include $BSDCFG_SHARE/dialog.subr
f_include $BSDCFG_SHARE/variable.subr

############################################################ CONFIGURATION

# VARIABLES:
# PARTITIONS
# DISTRIBUTIONS
# BSDINSTALL_DISTDIR

#
# Default name of the ZFS boot-pool
#
: ${ZFSBOOT_POOL_NAME:=zroot}

############################################################ GLOBALS

: ${TMPDIR:="/tmp"}

#
# Strings that should be moved to an i18n file and loaded with f_include_lang()
#
msg_installation_error="Installation Error!"

############################################################ FUNCTIONS

error()
{
	local file
	f_getvar "$VAR_DEBUG_FILE#+" file
	if [ "$file" ]; then
		f_dialog_title "$msg_installation_error"
		f_dialog_textbox "$file"
		# No need to restore title, pining for the fjords
	fi

	[ -f "$PATH_FSTAB" ] || exit
	if [ "$ZFSBOOT_DISKS" ]; then
		zpool export $ZFSBOOT_POOL_NAME
	else
		bsdinstall umount
	fi

	exit 1
}

############################################################ MAIN

set -e
trap error EXIT

SCRIPT="$1"
shift

f_dprintf "Began Installation at %s" "$( date )"
rm -rf $BSDINSTALL_TMPETC
mkdir $BSDINSTALL_TMPETC
rm -f $TMPDIR/bsdinstall-installscript-setup

# split script into preamble and setup script at first shebang
awk 'BEGIN {pathb=ARGV[2]; ARGV[2]=""} /^#!/{b=1} {
    if (b) print >pathb; else print}' \
    "$SCRIPT" $TMPDIR/bsdinstall-installscript-setup \
    >$TMPDIR/bsdinstall-installscript-preamble

. $TMPDIR/bsdinstall-installscript-preamble
: ${DISTRIBUTIONS="kernel.txz base.txz"}; export DISTRIBUTIONS
export BSDINSTALL_DISTDIR

# Re-initialize a new log if preamble changed BSDINSTALL_LOG
if [ "$BSDINSTALL_LOG" != "${debugFile#+}" ]; then
	export debugFile="$BSDINSTALL_LOG"
	f_quietly f_debug_init
	# NB: Being scripted, let debug go to terminal for invalid debugFile
	f_dprintf "Began Installation at %s" "$( date )"
fi

# Make partitions
rm -f $PATH_FSTAB
touch $PATH_FSTAB
if [ "$ZFSBOOT_DISKS" ]; then
	bsdinstall zfsboot
else
	bsdinstall scriptedpart "$PARTITIONS"
fi
bsdinstall mount

# Fetch missing distribution files, if any
exec 5>&1
export BSDINSTALL_DISTDIR=$(`dirname $0`/fetchmissingdists 2>&1 1>&5)
FETCH_RESULT=$?
exec 5>&-

[ $FETCH_RESULT -ne 0 ] && error "Could not fetch remote distributions"

# Unpack distributions
bsdinstall checksum
if [ -t 0 ]; then
	# If install is a tty, use distextract as normal
	bsdinstall distextract
else
	# Otherwise, we need to use tar (see https://reviews.freebsd.org/D10736)
	for set in $DISTRIBUTIONS; do
		f_dprintf "Extracting $BSDINSTALL_DISTDIR/$set"
		# XXX: The below fails if any mountpoints are FAT, due to
		# inability to set ctime/mtime on the root of FAT partitions,
		# which is needed to support e.g. EFI system partitions. tar has
		# no option to ignore this (distextract ignores them internally
		# through a hack), and returns 1 on any warning or error,
		# effectively turning all warnings into fatal errors.
		# 
		# Work around this in an extremely lame way for the specific
		# case of EFI system partitions only. This *ONLY WORKS* if
		# /boot/efi is empty and does not handle analagous problems on
		# other systems (ARM, PPC64).
		tar -xf "$BSDINSTALL_DISTDIR/$set" -C $BSDINSTALL_CHROOT --exclude boot/efi
		mkdir -p $BSDINSTALL_CHROOT/boot/efi
	done
fi

# Configure bootloader if needed
bsdinstall bootconfig

# Finalize install
bsdinstall config

# Make sure networking is functional, if we can arrange that
if [ ! -f $BSDINSTALL_CHROOT/etc/resolv.conf -a -f /etc/resolv.conf ]; then
	cp /etc/resolv.conf $BSDINSTALL_CHROOT/etc/resolv.conf
fi

# Run post-install script
if [ -f $TMPDIR/bsdinstall-installscript-setup ]; then
	cp $TMPDIR/bsdinstall-installscript-setup \
	    $BSDINSTALL_CHROOT/tmp/installscript
	chmod a+x $BSDINSTALL_CHROOT/tmp/installscript
	chroot $BSDINSTALL_CHROOT /tmp/installscript $@ 2>&1
	rm $BSDINSTALL_CHROOT/tmp/installscript
fi

bsdinstall entropy
bsdinstall umount
if [ "$ZFSBOOT_DISKS" ]; then
	zpool export $ZFSBOOT_POOL_NAME
fi

f_dprintf "Installation Completed at %s" "$( date )"

trap - EXIT
exit $SUCCESS

################################################################################
# END
################################################################################
