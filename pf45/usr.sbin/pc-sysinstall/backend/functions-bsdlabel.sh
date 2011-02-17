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

# Functions related to disk operations using bsdlabel

# Check if we are are provided a geli password on the nextline of the config
check_for_enc_pass()
{
  CURLINE="${1}"
 
  get_next_cfg_line "${CFGF}" "${CURLINE}" 
  echo ${VAL} | grep "^encpass=" >/dev/null 2>/dev/null
  if [ "$?" = "0" ] ; then
    # Found a password, return it
    get_value_from_string "${VAL}"
    return
  fi

  VAL="" ; export VAL
  return
};

# On check on the disk-label line if we have any extra vars for this device
# Only enabled for ZFS devices now, may add other xtra options in future for other FS's
get_fs_line_xvars()
{
  ACTIVEDEV="${1}"
  LINE="${2}"

  echo $LINE | grep ' (' >/dev/null 2>/dev/null
  if [ "$?" = "0" ] ; then

    # See if we are looking for ZFS specific options
    echo $LINE | grep '^ZFS' >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
      ZTYPE="NONE"
      ZFSVARS="`echo $LINE | cut -d '(' -f 2- | cut -d ')' -f 1 | xargs`"

      echo $ZFSVARS | grep -E "^(disk|file|mirror|raidz(1|2)?|spare|log|cache):" >/dev/null 2>/dev/null
	  if [ "$?" = "0" ] ; then
       ZTYPE=`echo $ZFSVARS | cut -f1 -d:`
       ZFSVARS=`echo $ZFSVARS | sed "s|$ZTYPE: ||g" | sed "s|$ZTYPE:||g"`
	  fi

      # Return the ZFS options
      if [ "${ZTYPE}" = "NONE" ] ; then
        VAR="${ACTIVEDEV} ${ZFSVARS}"
      else
        VAR="${ZTYPE} ${ACTIVEDEV} ${ZFSVARS}"
      fi
      export VAR
      return
    fi # End of ZFS block

  fi # End of xtra-options block

  # If we got here, set VAR to empty and export
  VAR=""
  export VAR
  return
};

# Init each zfs mirror disk with a boot sector so we can failover
setup_zfs_mirror_parts()
{
  _nZFS=""

  # Using mirroring, setup boot partitions on each disk
  _mirrline="`echo ${1} | sed 's|mirror ||g'`"
  for _zvars in $_mirrline
  do
    echo "Looping through _zvars: $_zvars" >>${LOGOUT}
    echo "$_zvars" | grep "${2}" >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then continue ; fi
    if [ -z "$_zvars" ] ; then continue ; fi

    is_disk "$_zvars" >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
      echo "Setting up ZFS mirror disk $_zvars" >>${LOGOUT}
      init_gpt_full_disk "$_zvars" >/dev/null 2>/dev/null
      rc_halt "gpart bootcode -p /boot/gptzfsboot -i 1 ${_zvars}" >/dev/null 2>/dev/null
      rc_halt "gpart add -t freebsd-zfs ${_zvars}" >/dev/null 2>/dev/null
      _nZFS="$_nZFS ${_zvars}p2"	
    else
      _nZFS="$_nZFS ${_zvars}"	
    fi	
  done
  echo "mirror $2 `echo $_nZFS | tr -s ' '`"
} ;

# Function which creates a unique label name for the specified mount
gen_glabel_name()
{
  MOUNT="$1"
  TYPE="$2"
  NUM="0"
  MAXNUM="20"

  # Check if we are doing /, and rename it
  if [ "$MOUNT" = "/" ]
  then
    NAME="rootfs"
  else
    # If doing a swap partition, also rename it
    if [ "${TYPE}" = "SWAP" ]
    then
      NAME="swap"
    else
      NAME="`echo $MOUNT | sed 's|/||g' | sed 's| ||g'`"
    fi
  fi

  # Loop through and break when we find our first available label
  while
  Z=1
  do
    glabel status | grep "${NAME}${NUM}" >/dev/null 2>/dev/null
    if [ "$?" != "0" ]
    then
      break
    else
      NUM="`expr ${NUM} + 1`"
    fi

    if [ $NUM -gt $MAXNUM ]
    then
      exit_err "Cannot allocate additional glabel name for $NAME"
      break
    fi
  done 
   

  VAL="${NAME}${NUM}" 
  export VAL
};

# Function to setup partitions using gpart
setup_gpart_partitions()
{
  local _dTag="$1"
  local _pDisk="$2"
  local _wSlice="$3"
  local _sNum="$4"
  local _pType="$5"
  FOUNDPARTS="1"

  # Lets read in the config file now and setup our partitions
  if [ "${_pType}" = "gpt" ] ; then
    CURPART="2"
  else
    PARTLETTER="a"
    CURPART="1"
    rc_halt "gpart create -s BSD ${_wSlice}"
  fi

  while read line
  do
    # Check for data on this slice
    echo $line | grep "^${_dTag}-part=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      FOUNDPARTS="0"
      # Found a slice- entry, lets get the slice info
      get_value_from_string "${line}"
      STRING="$VAL"

      # We need to split up the string now, and pick out the variables
      FS=`echo $STRING | tr -s '\t' ' ' | cut -d ' ' -f 1` 
      SIZE=`echo $STRING | tr -s '\t' ' ' | cut -d ' ' -f 2` 
      MNT=`echo $STRING | tr -s '\t' ' ' | cut -d ' ' -f 3` 

      # Check if we have a .eli extension on this FS
      echo ${FS} | grep ".eli" >/dev/null 2>/dev/null
      if [ "$?" = "0" ]
      then
        FS="`echo ${FS} | cut -d '.' -f 1`"
        ENC="ON"
        check_for_enc_pass "${line}"
        if [ "${VAL}" != "" ] ; then
          # We have a user supplied password, save it for later
          ENCPASS="${VAL}" 
        fi
      else
        ENC="OFF"
      fi

      # Check if the user tried to setup / as an encrypted partition
      check_for_mount "${MNT}" "/"
      if [ "${?}" = "0" -a "${ENC}" = "ON" ]
      then
        USINGENCROOT="0" ; export USINGENCROOT
      fi
          
      # Now check that these values are sane
      case $FS in
        UFS|UFS+S|UFS+J|UFS+SUJ|ZFS|SWAP) ;;
       *) exit_err "ERROR: Invalid file system specified on $line" ;;
      esac

      # Check that we have a valid size number
      expr $SIZE + 1 >/dev/null 2>/dev/null
      if [ "$?" != "0" ]; then
        exit_err "ERROR: The size specified on $line is invalid"
      fi

      # Check that the mount-point starts with /
      echo "$MNT" | grep -e "^/" -e "^none" >/dev/null 2>/dev/null
      if [ "$?" != "0" ]; then
        exit_err "ERROR: The mount-point specified on $line is invalid"
      fi

      if [ "$SIZE" = "0" ]
      then
        SOUT=""
      else
        SOUT="-s ${SIZE}M"
      fi

      # Check if we found a valid root partition
      check_for_mount "${MNT}" "/"
      if [ "${?}" = "0" ] ; then
        FOUNDROOT="1" ; export FOUNDROOT
        if [ "${CURPART}" = "2" -a "$_pType" = "gpt" ] ; then
          FOUNDROOT="0" ; export FOUNDROOT
        fi
        if [ "${CURPART}" = "1" -a "$_pType" = "mbr" ] ; then
          FOUNDROOT="0" ; export FOUNDROOT
        fi
      fi

      check_for_mount "${MNT}" "/boot"
      if [ "${?}" = "0" ] ; then
        USINGBOOTPART="0" ; export USINGBOOTPART
        if [ "${CURPART}" != "2" -a "${_pType}" = "gpt" ] ; then
            exit_err "/boot partition must be first partition"
        fi
        if [ "${CURPART}" != "1" -a "${_pType}" = "mbr" ] ; then
            exit_err "/boot partition must be first partition"
        fi

        if [ "${FS}" != "UFS" -a "${FS}" != "UFS+S" -a "${FS}" != "UFS+J" -a "${FS}" != "UFS+SUJ" ] ; then
          exit_err "/boot partition must be formatted with UFS"
        fi
      fi

      # Generate a unique label name for this mount
      gen_glabel_name "${MNT}" "${FS}"
      PLABEL="${VAL}"

      # Get any extra options for this fs / line
      if [ "${_pType}" = "gpt" ] ; then
        get_fs_line_xvars "${_pDisk}p${CURPART}" "${STRING}"
      else
        get_fs_line_xvars "${_wSlice}" "${STRING}"
      fi
      XTRAOPTS="${VAR}"

      # Check if using zfs mirror
      echo ${XTRAOPTS} | grep "mirror" >/dev/null 2>/dev/null
      if [ "$?" = "0" ] ; then
        if [ "${_pType}" = "gpt" ] ; then
       	  XTRAOPTS=$(setup_zfs_mirror_parts "$XTRAOPTS" "${_pDisk}p${CURPART}")
        else
       	  XTRAOPTS=$(setup_zfs_mirror_parts "$XTRAOPTS" "${_wSlice}")
        fi
      fi

      # Figure out the gpart type to use
      case ${FS} in
        ZFS) PARTYPE="freebsd-zfs" ;;
        SWAP) PARTYPE="freebsd-swap" ;;
        *) PARTYPE="freebsd-ufs" ;;
      esac

      # Create the partition
      if [ "${_pType}" = "gpt" ] ; then
	if [ "$CURPART" = "2" ] ; then
	  # If this is GPT, make sure first partition is aligned to 4k
          rc_halt "gpart add -b 2016 ${SOUT} -t ${PARTYPE} ${_pDisk}"
	else
          rc_halt "gpart add ${SOUT} -t ${PARTYPE} ${_pDisk}"
	fi
      else
        rc_halt "gpart add ${SOUT} -t ${PARTYPE} -i ${CURPART} ${_wSlice}"
      fi

      # Check if this is a root / boot partition, and stamp the right loader
      for TESTMNT in `echo ${MNT} | sed 's|,| |g'`
      do
        if [ "${TESTMNT}" = "/" -a -z "${BOOTTYPE}" ] ; then
           BOOTTYPE="${PARTYPE}" 
        fi 
        if [ "${TESTMNT}" = "/boot" ]  ; then
           BOOTTYPE="${PARTYPE}" 
        fi 
      done 

      # Save this data to our partition config dir
      if [ "${_pType}" = "gpt" ] ; then
        echo "${FS}:${MNT}:${ENC}:${PLABEL}:GPT:${XTRAOPTS}" >${PARTDIR}/${_pDisk}p${CURPART}

        # Clear out any headers
        sleep 2
        dd if=/dev/zero of=${_pDisk}p${CURPART} count=2048 >/dev/null 2>/dev/null

        # If we have a enc password, save it as well
        if [ ! -z "${ENCPASS}" ] ; then
          echo "${ENCPASS}" >${PARTDIR}-enc/${_pDisk}p${CURPART}-encpass
        fi
      else
	# MBR Partition
        echo "${FS}:${MNT}:${ENC}:${PLABEL}:MBR:${XTRAOPTS}:${IMAGE}" >${PARTDIR}/${_wSlice}${PARTLETTER}
        # Clear out any headers
        sleep 2
        dd if=/dev/zero of=${_wSlice}${PARTLETTER} count=2048 >/dev/null 2>/dev/null

        # If we have a enc password, save it as well
        if [ ! -z "${ENCPASS}" ] ; then
          echo "${ENCPASS}" >${PARTDIR}-enc/${_wSlice}${PARTLETTER}-encpass
        fi
      fi


      # Increment our parts counter
      if [ "$_pType" = "gpt" ] ; then 
        CURPART="`expr ${CURPART} + 1`"
        # If this is a gpt partition, we can continue and skip the MBR part letter stuff
        continue
      else
        CURPART="`expr ${CURPART} + 1`"
        if [ "$CURPART" = "3" ] ; then CURPART="4" ; fi
      fi


      # This partition letter is used, get the next one
      case ${PARTLETTER} in
        a) PARTLETTER="b" ;;
        b) PARTLETTER="d" ;;
        d) PARTLETTER="e" ;;
        e) PARTLETTER="f" ;;
        f) PARTLETTER="g" ;;
        g) PARTLETTER="h" ;;
        h) PARTLETTER="ERR" ;;
        *) exit_err "ERROR: bsdlabel only supports up to letter h for partitions." ;;
      esac

    fi # End of subsection locating a slice in config

    echo $line | grep "^commitDiskLabel" >/dev/null 2>/dev/null
    if [ "$?" = "0" -a "${FOUNDPARTS}" = "0" ]
    then

      # If this is the boot disk, stamp the right gptboot
      if [ ! -z "${BOOTTYPE}" -a "$_pType" = "gpt" ] ; then
        case ${BOOTTYPE} in
          freebsd-ufs) rc_halt "gpart bootcode -p /boot/gptboot -i 1 ${_pDisk}" ;;
          freebsd-zfs) rc_halt "gpart bootcode -p /boot/gptzfsboot -i 1 ${_pDisk}" ;;
        esac 
      fi

      # Make sure to stamp the MBR loader
      if [ "$_pType" = "mbr" ] ; then
	rc_halt "gpart bootcode -b /boot/boot ${_wSlice}"
      fi

      # Found our flag to commit this label setup, check that we found at least 1 partition
      if [ "${CURPART}" = "2" ] ; then
        exit_err "ERROR: commitDiskLabel was called without any partition entries for it!"
      fi

      break
    fi
  done <${CFGF}
};

# Reads through the config and sets up a BSDLabel for the given slice
populate_disk_label()
{
  if [ -z "${1}" ]
  then
    exit_err "ERROR: populate_disk_label() called without argument!"
  fi

  # Set some vars from the given working slice
  disk="`echo $1 | cut -d '-' -f 1`" 
  slicenum="`echo $1 | cut -d '-' -f 2`" 
  type="`echo $1 | cut -d '-' -f 3`" 
  
  # Set WRKSLICE based upon format we are using
  if [ "$type" = "mbr" ] ; then
    wrkslice="${disk}s${slicenum}"
  fi
  if [ "$type" = "gpt" ] ; then
    wrkslice="${disk}p${slicenum}"
  fi

  if [ -e "${SLICECFGDIR}/${wrkslice}" ]
  then
    disktag="`cat ${SLICECFGDIR}/${wrkslice}`"
  else
    exit_err "ERROR: Missing SLICETAG data. This shouldn't happen - please let the developers know"
  fi


  # Setup the partitions with gpart
  setup_gpart_partitions "${disktag}" "${disk}" "${wrkslice}" "${slicenum}" "${type}"

};

# Function which reads in the disk slice config, and performs it
setup_disk_label()
{
  # We are ready to start setting up the label, lets read the config and do the actions
  # First confirm that we have a valid WORKINGSLICES
  if [ -z "${WORKINGSLICES}" ]; then
    exit_err "ERROR: No slices were setup! Please report this to the maintainers"
  fi

  # Check that the slices we have did indeed get setup and gpart worked
  for i in $WORKINGSLICES
  do
    disk="`echo $i | cut -d '-' -f 1`" 
    pnum="`echo $i | cut -d '-' -f 2`" 
    type="`echo $i | cut -d '-' -f 3`" 
    if [ "$type" = "mbr" -a ! -e "/dev/${disk}s${pnum}" ] ; then
      exit_err "ERROR: The partition ${i} doesn't exist! gpart failure!"
    fi
    if [ "$type" = "gpt" -a ! -e "/dev/${disk}p${pnum}" ] ; then
      exit_err "ERROR: The partition ${i} doesn't exist! gpart failure!"
    fi
  done

  # Setup some files which we'll be referring to
  LABELLIST="${TMPDIR}/workingLabels"
  export LABELLIST
  rm $LABELLIST >/dev/null 2>/dev/null

  # Set our flag to determine if we've got a valid root partition in this setup
  FOUNDROOT="-1"
  export FOUNDROOT

  # Check if we are using a /boot partition
  USINGBOOTPART="1"
  export USINGBOOTPART
 
  # Set encryption on root check
  USINGENCROOT="1" ; export USINGENCROOT
  
  # Make the tmp directory where we'll store FS info & mount-points
  rm -rf ${PARTDIR} >/dev/null 2>/dev/null
  mkdir -p ${PARTDIR} >/dev/null 2>/dev/null
  rm -rf ${PARTDIR}-enc >/dev/null 2>/dev/null
  mkdir -p ${PARTDIR}-enc >/dev/null 2>/dev/null

  for i in $WORKINGSLICES
  do
    populate_disk_label "${i}"
  done

  # Check if we made a root partition
  if [ "$FOUNDROOT" = "-1" ]
  then
    exit_err "ERROR: No root (/) partition specified!!"
  fi

  # Check if we made a root partition
  if [ "$FOUNDROOT" = "1" -a "${USINGBOOTPART}" != "0" ]
  then
    exit_err "ERROR: (/) partition isn't first partition on disk!"
  fi

  if [ "${USINGENCROOT}" = "0" -a "${USINGBOOTPART}" != "0" ]
  then
    exit_err "ERROR: Can't encrypt (/) with no (/boot) partition!"
  fi
};

check_fstab_mbr()
{
  local SLICE
  local FSTAB

  if [ -z "$2" ]
  then
	return 1
  fi

  SLICE="$1"
  FSTAB="$2/etc/fstab"

  if [ -f "${FSTAB}" ]
  then
    PARTLETTER=`echo "$SLICE" | sed -E 's|^.+([a-h])$|\1|'`

    cat "${FSTAB}" | awk '{ print $2 }' | grep -E '^/$' >/dev/null 2>&1
    if [ "$?" = "0" ]
    then
      if [ "${PARTLETTER}" = "a" ]
      then
        FOUNDROOT="0"
      else
        FOUNDROOT="1"
      fi

      ROOTIMAGE="1"

      export FOUNDROOT
      export ROOTIMAGE
    fi

    cat "${FSTAB}" | awk '{ print $2 }' | grep -E '^/boot$' >/dev/null 2>&1
    if [ "$?" = "0" ]
    then
      if [ "${PARTLETTER}" = "a" ]
      then
        USINGBOOTPART="0"
      else 
        exit_err "/boot partition must be first partition"
      fi 
      export USINGBOOTPART
    fi

    return 0
  fi

  return 1
};

check_fstab_gpt()
{
  local SLICE
  local FSTAB

  if [ -z "$2" ]
  then
	return 1
  fi

  SLICE="$1"
  FSTAB="$2/etc/fstab"

  if [ -f "${FSTAB}" ]
  then
    PARTNUMBER=`echo "${SLICE}" | sed -E 's|^.+p([0-9]*)$|\1|'`

    cat "${FSTAB}" | awk '{ print $2 }' | grep -E '^/$' >/dev/null 2>&1
    if [ "$?" = "0" ]
    then
      if [ "${PARTNUMBER}" = "2" ]
      then
        FOUNDROOT="0"
      else
        FOUNDROOT="1"
      fi

      ROOTIMAGE="1"

      export FOUNDROOT
      export ROOTIMAGE
    fi

    cat "${FSTAB}" | awk '{ print $2 }' | grep -E '^/boot$' >/dev/null 2>&1
    if [ "$?" = "0" ]
    then
      if [ "${PARTNUMBER}" = "2" ]
      then
        USINGBOOTPART="0"
      else 
        exit_err "/boot partition must be first partition"
      fi 
      export USINGBOOTPART
    fi

    return 0
  fi


  return 1
};

check_disk_layout()
{
  local SLICES
  local TYPE
  local DISK
  local RES
  local F

  DISK="$1"
  TYPE="MBR"

  if [ -z "${DISK}" ]
  then
	return 1
  fi

  SLICES_MBR=`ls /dev/${DISK}s[1-4]*[a-h]* 2>/dev/null`
  SLICES_GPT=`ls /dev/${DISK}p[0-9]* 2>/dev/null`
  SLICES_SLICE=`ls /dev/${DISK}[a-h]* 2>/dev/null`

  if [ -n "${SLICES_MBR}" ]
  then
    SLICES="${SLICES_MBR}"
    TYPE="MBR"
    RES=0
  fi
  if [ -n "${SLICES_GPT}" ]
  then
    SLICES="${SLICES_GPT}"
    TYPE="GPT"
    RES=0
  fi
  if [ -n "${SLICES_SLICE}" ]
  then
    SLICES="${SLICES_SLICE}"
    TYPE="MBR"
    RES=0
  fi
  
  for slice in ${SLICES}
  do
    F=1
    mount ${slice} /mnt 2>/dev/null
    if [ "$?" != "0" ]
    then
      continue
    fi 

    if [ "${TYPE}" = "MBR" ]
    then
	  check_fstab_mbr "${slice}" "/mnt"
      F="$?"

    elif [ "${TYPE}" = "GPT" ]
    then
	  check_fstab_gpt "${slice}" "/mnt"
      F="$?"
    fi 

    if [ "${F}" = "0" ]
    then
      umount /mnt
      break 
    fi

    umount /mnt
  done

  return ${RES}
};
