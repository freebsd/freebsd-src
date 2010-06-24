#!/bin/sh
#-
# Copyright (c) 2010 iX Systems, Inc.  All rights reserved.
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
# $FreeBSD$

# Main install configuration parsing script
#

# Source our functions scripts
. ${BACKEND}/functions.sh
. ${BACKEND}/functions-bsdlabel.sh
. ${BACKEND}/functions-cleanup.sh
. ${BACKEND}/functions-disk.sh
. ${BACKEND}/functions-extractimage.sh
. ${BACKEND}/functions-installcomponents.sh
. ${BACKEND}/functions-localize.sh
. ${BACKEND}/functions-mountdisk.sh
. ${BACKEND}/functions-networking.sh
. ${BACKEND}/functions-newfs.sh
. ${BACKEND}/functions-parse.sh
. ${BACKEND}/functions-runcommands.sh
. ${BACKEND}/functions-unmount.sh
. ${BACKEND}/functions-upgrade.sh
. ${BACKEND}/functions-users.sh

# Check that the config file exists
if [ ! -e "${1}" ]
then
  echo "ERROR: Install configuration $1 does not exist!"
  exit 1
fi

# Set our config file variable
CFGF="$1"

# Check the dirname of the provided CFGF and make sure its a full path
DIR="`dirname ${CFGF}`"
if [ "${DIR}" = "." ]
then
  CFGF="`pwd`/${CFGF}"
fi
export CFGF

# Start by doing a sanity check, which will catch any obvious mistakes in the config
file_sanity_check "installMode disk0 installType installMedium packageType"

# We passed the Sanity check, lets grab some of the universal config settings and store them
check_value installMode "fresh upgrade"
check_value bootManager "bsd none"
check_value installType "PCBSD FreeBSD"
check_value installMedium "dvd usb ftp rsync"
check_value packageType "uzip tar rsync split"
if_check_value_exists partition "all ALL s1 s2 s3 s4 free FREE"
if_check_value_exists mirrorbal "load prefer round-robin split"

# We passed all sanity checks! Yay, lets start the install
echo "File Sanity Check -> OK"

# Lets load the various universal settings now
get_value_from_cfg installMode
INSTALLMODE="${VAL}" ; export INSTALLMODE

get_value_from_cfg installType
INSTALLTYPE="${VAL}" ; export INSTALLTYPE

get_value_from_cfg installMedium
INSTALLMEDIUM="${VAL}" ; export INSTALLMEDIUM

get_value_from_cfg packageType
PACKAGETYPE="${VAL}" ; export PACKAGETYPE

# Check if we are doing any networking setup
start_networking

# If we are not doing an upgrade, lets go ahead and setup the disk
if [ "${INSTALLMODE}" = "fresh" ]
then

  # Lets start setting up the disk slices now
  setup_disk_slice
  
  # Disk setup complete, now lets parse WORKINGSLICES and setup the bsdlabels
  setup_disk_label
  
  # Now we've setup the bsdlabels, lets go ahead and run newfs / zfs 
  # to setup the filesystems
  setup_filesystems

  # Lets mount the partitions now
  mount_all_filesystems

  # We are ready to begin extraction, lets start now
  init_extraction

  # Check if we have any optional modules to load 
  install_components

  # Do any localization in configuration
  run_localize
  
  # Save any networking config on the installed system
  save_networking_install

  # Now add any users
  setup_users

  # Now run any commands specified
  run_commands
  
  # Do any last cleanup / setup before unmounting
  run_final_cleanup

  # Unmount and finish up
  unmount_all_filesystems

  echo_log "Installation finished!"
  exit 0

else
  # We're going to do an upgrade, skip all the disk setup 
  # and start by mounting the target drive/slices
  mount_upgrade
  
  # Start the extraction process
  init_extraction

  # Do any localization in configuration
  run_localize

  # Now run any commands specified
  run_commands
  
  # Merge any old configuration files
  merge_old_configs

  # Check if we have any optional modules to load 
  install_components

  # All finished, unmount the file-systems
  unmount_upgrade

  echo_log "Upgrade finished!"
  exit 0
fi

