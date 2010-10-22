#!/bin/sh
#-
# Copyright (c) 2010 iXsystems, Inc.  All rights reserved.
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

# Functions which perform the final cleanup after an install

# Finishes up with ZFS setup before unmounting
zfs_cleanup_unmount()
{
  # Loop through our FS and see if we have any ZFS partitions to cleanup
  for PART in `ls ${PARTDIR}`
  do
    PARTFS="`cat ${PARTDIR}/${PART} | cut -d ':' -f 1`"
    PARTMNT="`cat ${PARTDIR}/${PART} | cut -d ':' -f 2`"
    ZPOOLNAME=$(get_zpool_name "${PART}")

    if [ "$PARTFS" = "ZFS" ]
    then
      # Check if we have multiple zfs mounts specified
      for ZMNT in `echo ${PARTMNT} | sed 's|,| |g'`
      do
        if [ "${ZMNT}" = "/" ]
        then
          # Make sure we haven't already added the zfs boot line when
          # Creating a dedicated "/boot" partition
          cat ${FSMNT}/boot/loader.conf 2>/dev/null | grep "vfs.root.mountfrom=" >/dev/null 2>/dev/null
          if [ "$?" != "0" ] ; then
            echo "vfs.root.mountfrom=\"zfs:${ZPOOLNAME}\"" >> ${FSMNT}/boot/loader.conf
          fi
          FOUNDZFSROOT="${ZPOOLNAME}" ; export FOUNDZFSROOT
        fi
      done
      FOUNDZFS="1"
    fi
  done

  if [ ! -z "${FOUNDZFS}" ]
  then
    # Check if we need to add our ZFS flags to rc.conf, src.conf and loader.conf
    cat ${FSMNT}/boot/loader.conf 2>/dev/null | grep 'zfs_load="YES"' >/dev/null 2>/dev/null
    if [ "$?" != "0" ]
    then
      echo 'zfs_load="YES"' >>${FSMNT}/boot/loader.conf
    fi
    cat ${FSMNT}/etc/rc.conf 2>/dev/null | grep 'zfs_enable="YES"' >/dev/null 2>/dev/null
    if [ "$?" != "0" ]
    then
      echo 'zfs_enable="YES"' >>${FSMNT}/etc/rc.conf
    fi

    sleep 2
    # Copy over any ZFS cache data
    cp /boot/zfs/* ${FSMNT}/boot/zfs/

    # Copy the hostid so that our zfs cache works
    cp /etc/hostid ${FSMNT}/etc/hostid
  fi

  # Loop through our FS and see if we have any ZFS partitions to cleanup
  for PART in `ls ${PARTDIR}`
  do
    PARTFS="`cat ${PARTDIR}/${PART} | cut -d ':' -f 1`"
    PARTMNT="`cat ${PARTDIR}/${PART} | cut -d ':' -f 2`"
    PARTENC="`cat ${PARTDIR}/${PART} | cut -d ':' -f 3`"
    ZPOOLNAME=$(get_zpool_name "${PART}")

    if [ "$PARTFS" = "ZFS" ]
    then
      # Check if we have multiple zfs mounts specified
      for ZMNT in `echo ${PARTMNT} | sed 's|,| |g'`
      do
        PARTMNTREV="${ZMNT} ${PARTMNTREV}"
      done

      for ZMNT in ${PARTMNTREV}
      do
        if [ "${ZMNT}" != "/" ]
        then
          rc_halt "zfs set mountpoint=${ZMNT} ${ZPOOLNAME}${ZMNT}"
          rc_halt "zfs unmount ${ZPOOLNAME}${ZMNT}"
          sleep 2
        fi
      done
    fi
  done

};

# Function which performs the specific setup for using a /boot partition
setup_dedicated_boot_part()
{
  ROOTFS="${1}"
  ROOTFSTYPE="${2}"
  BOOTFS="${3}"
  BOOTMNT="${4}"

  # Set the root mount in loader.conf
  echo "vfs.root.mountfrom=\"${ROOTFSTYPE}:${ROOTFS}\"" >> ${FSMNT}/boot/loader.conf
  rc_halt "mkdir -p ${FSMNT}/${BOOTMNT}/boot"
  rc_halt "mv ${FSMNT}/boot/* ${FSMNT}${BOOTMNT}/boot/"
  rc_halt "mv ${FSMNT}${BOOTMNT}/boot ${FSMNT}/boot/"
  rc_halt "umount /dev/${BOOTFS}"
  rc_halt "mount /dev/${BOOTFS} ${FSMNT}${BOOTMNT}"
  rc_halt "rmdir ${FSMNT}/boot"

  # Strip the '/' from BOOTMNT before making symlink
  BOOTMNTNS="`echo ${BOOTMNT} | sed 's|/||g'`"
  rc_halt "chroot ${FSMNT} ln -s ${BOOTMNTNS}/boot /boot"
  
};

# Function which creates the /etc/fstab for the installed system
setup_fstab()
{
  FSTAB="${FSMNT}/etc/fstab"
  rm ${FSTAB} >/dev/null 2>/dev/null

  # Create the header
  echo "# Device		Mountpoint		FStype		Options	Dump Pass" >> ${FSTAB}

  # Loop through the partitions, and start creating /etc/fstab
  for PART in `ls ${PARTDIR}`
  do
    PARTFS="`cat ${PARTDIR}/${PART} | cut -d ':' -f 1`"
    PARTMNT="`cat ${PARTDIR}/${PART} | cut -d ':' -f 2`"
    PARTENC="`cat ${PARTDIR}/${PART} | cut -d ':' -f 3`"
    PARTLABEL="`cat ${PARTDIR}/${PART} | cut -d ':' -f 4`"

    DRIVE="`echo ${PART} | rev | cut -b 4- | rev`"
    # Check if this device is being mirrored
    if [ -e "${MIRRORCFGDIR}/${DRIVE}" ]
    then
      # This device is apart of a gmirror, lets reset PART to correct value
      MDRIVE="mirror/`cat ${MIRRORCFGDIR}/${DRIVE} | cut -d ':' -f 3`"
      TMP="`echo ${PART} | rev | cut -b -3 | rev`"
      PART="${MDRIVE}${TMP}"
      PARTLABEL=""
    fi

    # Unset EXT
    EXT=""

    # Set mount options for file-systems
    case $PARTFS in
      UFS+J) MNTOPTS="rw,noatime,async" ;;
      SWAP) MNTOPTS="sw" ;;
      *) MNTOPTS="rw,noatime" ;;
    esac


    # Figure out if we are using a glabel, or the raw name for this entry
    if [ ! -z "${PARTLABEL}" ]
    then
      DEVICE="label/${PARTLABEL}"
    else
      # Check if using encryption 
      if [ "${PARTENC}" = "ON" ] ; then
        EXT=".eli"
      fi

      if [ "${PARTFS}" = "UFS+J" ] ; then
        EXT="${EXT}.journal"
      fi
      DEVICE="${PART}${EXT}"
    fi


    # Set our ROOTFSTYPE for loader.conf if necessary
    check_for_mount "${PARTMNT}" "/"
    if [ "$?" = "0" ] ; then
      if [ "${PARTFS}" = "ZFS" ] ; then
        ROOTFSTYPE="zfs"
        XPOOLNAME=$(get_zpool_name "${PART}")
        ROOTFS="${ZPOOLNAME}"
      else
        ROOTFS="${DEVICE}"
        ROOTFSTYPE="ufs"
      fi
    fi

    # Only create non-zfs partitions
    if [ "${PARTFS}" != "ZFS" ]
    then

      # Make sure geom_journal is loaded
      if [ "${PARTFS}" = "UFS+J" ] ; then
        setup_gjournal
      fi

      # Save the BOOTFS for call at the end
      if [ "${PARTMNT}" = "/boot" ] ; then
        BOOTFS="${PART}${EXT}"
        BOOTMNT="${BOOT_PART_MOUNT}"
        PARTMNT="${BOOTMNT}"
      fi

      # Echo out the fstab entry now
      if [ "${PARTFS}" = "SWAP" ]
      then
        echo "/dev/${DEVICE}	none			swap		${MNTOPTS}		0	0" >> ${FSTAB}
      else
        echo "/dev/${DEVICE}  	${PARTMNT}			ufs		${MNTOPTS}	1	1" >> ${FSTAB}
      fi

    fi # End of ZFS Check
  done

  # Setup some specific PC-BSD fstab options
  if [ "$INSTALLTYPE" != "FreeBSD" ]
  then
    echo "procfs			/proc			procfs		rw		0	0" >> ${FSTAB}
    echo "linprocfs		/compat/linux/proc	linprocfs	rw		0	0" >> ${FSTAB}
    echo "tmpfs				/tmp			tmpfs		rw,mode=1777	0	0" >> ${FSTAB}
  fi

  # If we have a dedicated /boot, run the post-install setup of it now
  if [ ! -z "${BOOTMNT}" ] ; then 
    setup_dedicated_boot_part "${ROOTFS}" "${ROOTFSTYPE}" "${BOOTFS}" "${BOOTMNT}"
  fi

};

# Setup our disk mirroring with gmirror
setup_gmirror()
{
  NUM="0"

  cd ${MIRRORCFGDIR}
  for DISK in `ls *`
  do
    MIRRORDISK="`cat ${DISK} | cut -d ':' -f 1`"
    MIRRORBAL="`cat ${DISK} | cut -d ':' -f 2`"

    # Create this mirror device
    gmirror label -vb $MIRRORBAL gm${NUM} /dev/${DISK} 

    sleep 3

    # Save the gm<num> device in our config
    echo "${MIRRORDISK}:${MIRRORBAL}:gm${NUM}" > ${DISK}

    sleep 3

    NUM="`expr ${NUM} + 1`"
  done

  
  cat ${FSMNT}/boot/loader.conf 2>/dev/null | grep 'geom_mirror_load="YES"' >/dev/null 2>/dev/null
  if [ "$?" != "0" ]
  then
    echo 'geom_mirror_load="YES"' >>${FSMNT}/boot/loader.conf
  fi

};

# Function which saves geli keys and sets up loading of them at boot
setup_geli_loading()
{

  # Make our keys dir
  mkdir -p ${FSMNT}/boot/keys >/dev/null 2>/dev/null

  cd ${GELIKEYDIR}
  for KEYFILE in `ls *`
  do
     # Figure out the partition name based on keyfile name removing .key
     PART="`echo ${KEYFILE} | cut -d '.' -f 1`"

     # Add the entries to loader.conf to start this geli provider at boot
     echo "geli_${PART}_keyfile0_load=\"YES\"" >> ${FSMNT}/boot/loader.conf 
     echo "geli_${PART}_keyfile0_type=\"${PART}:geli_keyfile0\"" >> ${FSMNT}/boot/loader.conf 
     echo "geli_${PART}_keyfile0_name=\"/boot/keys/${KEYFILE}\"" >> ${FSMNT}/boot/loader.conf 

     # If we have a passphrase, set it up now
     if [ -e "${PARTDIR}-enc/${PART}-encpass" ] ; then
       geli setkey -J ${PARTDIR}-enc/${PART}-encpass -n 0 -p -k ${KEYFILE} -K ${KEYFILE} ${PART}
       geli configure -b ${PART}
     fi

     # Copy the key to the disk
     cp ${KEYFILE} ${FSMNT}/boot/keys/${KEYFILE}
  done

  # Make sure we have geom_eli set to load at boot
  cat ${FSMNT}/boot/loader.conf 2>/dev/null | grep 'geom_eli_load="YES"' >/dev/null 2>/dev/null
  if [ "$?" != "0" ]
  then
    echo 'geom_eli_load="YES"' >>${FSMNT}/boot/loader.conf
  fi

};


# Function to generate a random hostname if none was specified
gen_hostname()
{
  RAND="`jot -r 1 1 9000`"

  if [ "$INSTALLTYPE" = "FreeBSD" ]
  then
    VAL="freebsd-${RAND}" 
  else
    VAL="pcbsd-${RAND}" 
  fi

  export VAL

};

# Function which sets up the hostname for the system
setup_hostname()
{

  get_value_from_cfg hostname
  HOSTNAME="${VAL}"

  # If we don't have a hostname, make one up
  if [ -z "${HOSTNAME}" ]
  then
    gen_hostname
    HOSTNAME="${VAL}"
  fi

  # Clean up any saved hostname
  cat ${FSMNT}/etc/rc.conf | grep -v "hostname=" >${FSMNT}/etc/rc.conf.new
  mv ${FSMNT}/etc/rc.conf.new ${FSMNT}/etc/rc.conf

  # Set the hostname now
  echo_log "Setting hostname: ${HOSTNAME}"
  echo "hostname=\"${HOSTNAME}\"" >> ${FSMNT}/etc/rc.conf
  sed -i -e "s|my.domain|${HOSTNAME} ${HOSTNAME}|g" ${FSMNT}/etc/hosts

};


# Check and make sure geom_journal is enabled on the system
setup_gjournal()
{

  # Make sure we have geom_journal set to load at boot
  cat ${FSMNT}/boot/loader.conf 2>/dev/null | grep 'geom_journal_load="YES"' >/dev/null 2>/dev/null
  if [ "$?" != "0" ]
  then
    echo 'geom_journal_load="YES"' >>${FSMNT}/boot/loader.conf
  fi

};

# Function which sets the root password from the install config
set_root_pw()
{
  get_value_from_cfg_with_spaces rootPass
  PW="${VAL}"

  # If we don't have a root pass, return
  if [ -z "${PW}" ]
  then
    return 0
  fi

  echo_log "Setting root password"
  echo "${PW}" > ${FSMNT}/.rootpw
  run_chroot_cmd "cat /.rootpw | pw usermod root -h 0"
  rc_halt "rm ${FSMNT}/.rootpw"

};


run_final_cleanup()
{
  # Check if we need to run any gmirror setup
  ls ${MIRRORCFGDIR}/* >/dev/null 2>/dev/null
  if [ "$?" = "0" ]
  then
    # Lets setup gmirror now
    setup_gmirror
  fi

  # Check if we need to save any geli keys
  ls ${GELIKEYDIR}/* >/dev/null 2>/dev/null
  if [ "$?" = "0" ]
  then
    # Lets setup geli loading
    setup_geli_loading
  fi

  # Set a hostname on the install system
  setup_hostname

  # Set the root_pw if it is specified
  set_root_pw

  # Generate the fstab for the installed system
  setup_fstab
};
