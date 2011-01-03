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

# Functions related to disk operations using gpart

# See if device is a full disk or partition/slice
is_disk()
{
  for _dsk in `sysctl -n kern.disks`
  do
    if [ "$_dsk" = "${1}" ] ; then return 0 ; fi
  done

  return 1
}

# Get a MBR partitions sysid
get_partition_sysid_mbr()
{
  INPART="0"
  DISK="$1"
  PARTNUM=`echo ${2} | sed "s|${DISK}s||g"`
  fdisk ${DISK} >${TMPDIR}/disk-${DISK} 2>/dev/null
  while read i
  do
    echo "$i" | grep "The data for partition"  >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
       INPART="0"
       PART="`echo ${i} | cut -d ' ' -f 5`"
       if [ "$PART" = "$PARTNUM" ] ; then
          INPART="1"
       fi
    fi

    # In the partition section
    if [ "$INPART" = "1" ] ; then
       echo "$i" | grep "^sysid" >/dev/null 2>/dev/null
       if [ "$?" = "0" ] ; then
         SYSID="`echo ${i} | tr -s '\t' ' ' | cut -d ' ' -f 2`"
         break
       fi

    fi

  done < ${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  VAL="${SYSID}"
  export VAL
};

# Get the partitions MBR label
get_partition_label_mbr()
{
  INPART="0"
  DISK="$1"
  PARTNUM=`echo ${2} | sed "s|${DISK}s||g"`
  fdisk ${DISK} >${TMPDIR}/disk-${DISK} 2>/dev/null
  while read i
  do
    echo "$i" | grep "The data for partition"  >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
       INPART="0"
       PART="`echo ${i} | cut -d ' ' -f 5`"
       if [ "$PART" = "$PARTNUM" ] ; then
          INPART="1"
       fi
    fi

    # In the partition section
    if [ "$INPART" = "1" ] ; then
       echo "$i" | grep "^sysid" >/dev/null 2>/dev/null
       if [ "$?" = "0" ] ; then
         LABEL="`echo ${i} | tr -s '\t' ' ' | cut -d ',' -f 2-10`"
         break
       fi

    fi

  done < ${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  VAL="${LABEL}"
  export VAL
};

# Get a GPT partitions label
get_partition_label_gpt()
{
  DISK="${1}"
  PARTNUM=`echo ${2} | sed "s|${DISK}p||g"`

  gpart show ${DISK} >${TMPDIR}/disk-${DISK}
  while read i
  do
     SLICE="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 3`"
     if [ "${SLICE}" = "${PARTNUM}" ] ; then
       LABEL="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 4`"
       break
     fi
  done <${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  VAL="${LABEL}"
  export VAL
};

# Get a partitions startblock
get_partition_startblock()
{
  DISK="${1}"
  PARTNUM=`echo ${2} | sed "s|${DISK}p||g" | sed "s|${DISK}s||g"`

  gpart show ${DISK} >${TMPDIR}/disk-${DISK}
  while read i
  do
     SLICE="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 3`"
     if [ "$SLICE" = "${PARTNUM}" ] ; then
       SB="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 1`"
       break
     fi
  done <${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  VAL="${SB}"
  export VAL
};

# Get a partitions blocksize
get_partition_blocksize()
{
  DISK="${1}"
  PARTNUM=`echo ${2} | sed "s|${DISK}p||g" | sed "s|${DISK}s||g"`

  gpart show ${DISK} >${TMPDIR}/disk-${DISK}
  while read i
  do
     SLICE="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 3`"
     if [ "$SLICE" = "${PARTNUM}" ] ; then
       BS="`echo ${i} | grep -v ${DISK} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 2`"
       break
     fi
  done <${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  VAL="${BS}"
  export VAL
};

# Function which returns the partitions on a target disk
get_disk_partitions()
{
  gpart show ${1} >/dev/null 2>/dev/null
  if [ "$?" != "0" ] ; then
    VAL="" ; export VAL
    return
  fi

  gpart show ${1} | grep "MBR" >/dev/null 2>/dev/null
  if [ "$?" = "0" ] ; then
    type="MBR"
  else
    type="GPT"
  fi

  SLICES="`gpart show ${1} | grep -v ${1} | grep -v ' free ' |tr -s '\t' ' ' | cut -d ' ' -f 4 | sed '/^$/d'`"
  for i in ${SLICES}
  do
    case $type in
      MBR) name="${1}s${i}" ;;
      GPT) name="${1}p${i}";;
      *) name="${1}s${i}";;
    esac
    if [ -z "${RSLICES}" ]
    then
      RSLICES="${name}"
    else
      RSLICES="${RSLICES} ${name}"
    fi
  done

  VAL="${RSLICES}" ; export VAL
};

# Function which returns a target disks cylinders
get_disk_cyl()
{
  cyl=`diskinfo -v ${1} | grep "# Cylinders" | tr -s ' ' | cut -f 2`
  VAL="${cyl}" ; export VAL
};

# Function which returns a target disks sectors
get_disk_sectors()
{
  sec=`diskinfo -v ${1} | grep "# Sectors" | tr -s ' ' | cut -f 2`
  VAL="${sec}" ; export VAL
};

# Function which returns a target disks heads
get_disk_heads()
{
  head=`diskinfo -v ${1} | grep "# Heads" | tr -s ' ' | cut -f 2`
  VAL="${head}" ; export VAL
};

# Function which returns a target disks mediasize in sectors
get_disk_mediasize()
{
  mediasize=`diskinfo -v ${1} | grep "# mediasize in sectors" | tr -s ' ' | cut -f 2`
  VAL="${mediasize}" ; export VAL
};

# Function which exports all zpools, making them safe to overwrite potentially
export_all_zpools()
{
  # Export any zpools
  for i in `zpool list -H -o name`
  do
    zpool export -f ${i}
  done
};

# Function to delete all gparts before starting an install
delete_all_gpart()
{
  echo_log "Deleting all gparts"
  DISK="$1"

  # Check for any swaps to stop
  for i in `gpart show ${DISK} 2>/dev/null | grep 'freebsd-swap' | tr -s ' ' | cut -d ' ' -f 4`
  do
    swapoff /dev/${DISK}s${i}b >/dev/null 2>/dev/null
    swapoff /dev/${DISK}p${i} >/dev/null 2>/dev/null
  done

  # Delete the gparts now
  for i in `gpart show ${DISK} 2>/dev/null | tr -s ' ' | cut -d ' ' -f 4`
  do
   if [ "${i}" != "${DISK}" -a "${i}" != "-" ] ; then
     rc_nohalt "gpart delete -i ${i} ${DISK}"
   fi
  done

  rc_nohalt "dd if=/dev/zero of=/dev/${DISK} count=3000"

};

# Function to export all zpools before starting an install
stop_all_zfs()
{
  # Export all zpools again, so that we can overwrite these partitions potentially
  for i in `zpool list -H -o name`
  do
    zpool export -f ${i}
  done
};

# Function which stops all gmirrors before doing any disk manipulation
stop_all_gmirror()
{
  DISK="${1}"
  GPROV="`gmirror list | grep ". Name: mirror/" | cut -d '/' -f 2`"
  for gprov in $GPROV 
  do
    gmirror list | grep "Name: ${DISK}" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      echo_log "Stopping mirror $gprov $DISK"
      rc_nohalt "gmirror remove $gprov $DISK"
      rc_nohalt "dd if=/dev/zero of=/dev/${DISK} count=4096"
    fi
  done
};

# Make sure we don't have any geli providers active on this disk
stop_all_geli()
{
  _geld="${1}"
  cd /dev

  for i in `ls ${_geld}*`
  do
    echo $i | grep '.eli' >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      echo_log "Detaching GELI on ${i}"
      rc_halt "geli detach ${i}"
    fi
  done

};

# Function which reads in the disk slice config, and performs it
setup_disk_slice()
{

  # Cleanup any slice / mirror dirs
  rm -rf ${SLICECFGDIR} >/dev/null 2>/dev/null
  mkdir ${SLICECFGDIR}
  rm -rf ${MIRRORCFGDIR} >/dev/null 2>/dev/null
  mkdir ${MIRRORCFGDIR}

  # Start with disk0
  disknum="0"

  # Make sure all zpools are exported
  export_all_zpools

  # We are ready to start setting up the disks, lets read the config and do the actions
  while read line
  do
    echo $line | grep "^disk${disknum}=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      DISK="$VAL"
     
      # Before we go further, lets confirm this disk really exists
      if [ ! -e "/dev/${DISK}" ]
      then
        exit_err "ERROR: The disk ${DISK} does not exist!"
      fi

      # Make sure we stop any gmirrors on this disk
      stop_all_gmirror ${DISK}

      # Make sure we stop any geli stuff on this disk
      stop_all_geli ${DISK}

      # Make sure we don't have any zpools loaded
      stop_all_zfs

    fi

    # Lets look if this device will be mirrored on another disk
    echo $line | grep "^mirror=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      MIRRORDISK="$VAL"
     
      # Before we go further, lets confirm this disk really exists
      if [ ! -e "/dev/${MIRRORDISK}" ]
      then
        exit_err "ERROR: The mirror disk ${MIRRORDISK} does not exist!"
      fi
    fi

    # Lets see if we have been given a mirror balance choice
    echo $line | grep "^mirrorbal=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      MIRRORBAL="$VAL"
    fi

    echo $line | grep "^partition=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      # Found a partition= entry, lets read / set it 
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      PTYPE=`echo $VAL|tr A-Z a-z`

      # We are using free space, figure out the slice number
      if [ "${PTYPE}" = "free" ]
      then
        # Lets figure out what number this slice will be
        LASTSLICE="`gpart show ${DISK} \
          | grep -v ${DISK} \
          | grep -v ' free' \
          | tr -s '\t' ' ' \
          | cut -d ' ' -f 4 \
          | sed '/^$/d' \
          | tail -n 1`"

        if [ -z "${LASTSLICE}" ]
        then
          LASTSLICE="1"
        else
          LASTSLICE="`expr $LASTSLICE + 1`"
        fi

        if [ $LASTSLICE -gt 4 ]
        then
          exit_err "ERROR: BSD only supports primary partitions, and there are none availble on $DISK"
        fi

      fi
    fi

    # Check if we have an image file defined
    echo $line | grep "^image=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
      # Found an image= entry, lets read / set it
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      IMAGE="$VAL"
      if [ ! -f "$IMAGE" ] ; then
        exit_err "$IMAGE file does not exist"
      fi
    fi

    # Check if we have a partscheme specified
    echo $line | grep "^partscheme=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ] ; then
      # Found a partscheme= entry, lets read / set it 
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      PSCHEME="$VAL"
      if [ "$PSCHEME" != "GPT" -a "$PSCHEME" != "MBR" ] ; then
        exit_err "Unknown partition scheme: $PSCHEME" 
      fi
    fi

    echo $line | grep "^bootManager=" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      # Found a bootManager= entry, lets read /set it
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      BMANAGER="$VAL"
    fi

    echo $line | grep "^commitDiskPart" >/dev/null 2>/dev/null
    if [ "$?" = "0" ]
    then
      # Found our flag to commit this disk setup / lets do sanity check and do it
      if [ ! -z "${DISK}" -a ! -z "${PTYPE}" ]
      then
        case ${PTYPE} in
          all)
            if [ "$PSCHEME" = "MBR" -o -z "$PSCHEME" ] ; then
              PSCHEME="MBR"
              tmpSLICE="${DISK}s1"  
            else
              tmpSLICE="${DISK}p1"  
            fi

            run_gpart_full "${DISK}" "${BMANAGER}" "${PSCHEME}"
            ;;

          s1|s2|s3|s4)
            tmpSLICE="${DISK}${PTYPE}" 
            # Get the number of the slice we are working on
            s="`echo ${PTYPE} | awk '{print substr($0,length,1)}'`" 
            run_gpart_slice "${DISK}" "${BMANAGER}" "${s}"
            ;;

          free)
            tmpSLICE="${DISK}s${LASTSLICE}"
            run_gpart_free "${DISK}" "${LASTSLICE}" "${BMANAGER}"
            ;;

          image)
            if [ -z "${IMAGE}" ]
            then
              exit_err "ERROR: partition type image specified with no image!"
            fi 
            ;;

          *) exit_err "ERROR: Unknown PTYPE: $PTYPE" ;;
        esac
        

		if [ -n "${IMAGE}" ]
		then 
          local DEST
          
		  if [ -n "${tmpSLICE}" ]
          then
			DEST="${tmpSLICE}"
          else 
			DEST="${DISK}"
          fi 

          write_image "${IMAGE}" "${DEST}"
          check_disk_layout "${DEST}"
		fi

        # Now save which disk<num> this is, so we can parse it later during slice partition setup
        if [ -z "${IMAGE}" ]
        then
          echo "disk${disknum}" >${SLICECFGDIR}/$tmpSLICE
        fi

        # Save any mirror config
        if [ ! -z "$MIRRORDISK" ]
        then
          # Default to round-robin if the user didn't specify
          if [ -z "$MIRRORBAL" ]
          then
            MIRRORBAL="round-robin"
          fi
          echo "$MIRRORDISK:$MIRRORBAL" >${MIRRORCFGDIR}/$DISK
        fi

        # Increment our disk counter to look for next disk and unset
        unset BMANAGER PTYPE DISK MIRRORDISK MIRRORBAL PSCHEME IMAGE
        disknum="`expr $disknum + 1`"
      else
        exit_err "ERROR: commitDiskPart was called without procceding disk<num>= and partition= entries!!!" 
      fi
    fi

  done <${CFGF}

};

# Stop all gjournals on disk / slice
stop_gjournal()
{
  _gdsk="$1"
  # Check if we need to shutdown any journals on this drive
  ls /dev/${_gdsk}*.journal >/dev/null 2>/dev/null
  if [ "$?" = "0" ]
  then
    cd /dev
    for i in `ls ${_gdsk}*.journal`
    do
      rawjournal="`echo ${i} | cut -d '.' -f 1`"
      gjournal stop -f ${rawjournal} >>${LOGOUT} 2>>${LOGOUT}
      gjournal clear ${rawjournal} >>${LOGOUT} 2>>${LOGOUT}
    done
  fi
} ;

# Function which runs gpart and creates a single large GPT partition scheme
init_gpt_full_disk()
{
  _intDISK=$1
 
  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  # Stop any journaling
  stop_gjournal "${_intDISK}"

  # Remove any existing partitions
  delete_all_gpart "${_intDISK}"

  #Erase any existing bootloader
  echo_log "Cleaning up ${_intDISK}"
  rc_halt "dd if=/dev/zero of=/dev/${_intDISK} count=2048"

  sleep 2

  echo_log "Running gpart on ${_intDISK}"
  rc_halt "gpart create -s GPT ${_intDISK}"
  rc_halt "gpart add -b 34 -s 128 -t freebsd-boot ${_intDISK}"
  
  echo_log "Stamping boot sector on ${_intDISK}"
  rc_halt "gpart bootcode -b /boot/pmbr ${_intDISK}"

}

# Function which runs gpart and creates a single large MBR partition scheme
init_mbr_full_disk()
{
  _intDISK=$1
  _intBOOT=$2
 
  startblock="63"

  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  # Stop any journaling
  stop_gjournal "${_intDISK}"

  # Remove any existing partitions
  delete_all_gpart "${_intDISK}"

  #Erase any existing bootloader
  echo_log "Cleaning up ${_intDISK}"
  rc_halt "dd if=/dev/zero of=/dev/${_intDISK} count=2048"

  sleep 2

  echo_log "Running gpart on ${_intDISK}"
  rc_halt "gpart create -s mbr ${_intDISK}"

  # Lets figure out disk size in blocks
  # Get the cyl of this disk
  get_disk_cyl "${_intDISK}"
  cyl="${VAL}"

  # Get the heads of this disk
  get_disk_heads "${_intDISK}"
  head="${VAL}"

  # Get the tracks/sectors of this disk
  get_disk_sectors "${_intDISK}"
  sec="${VAL}"

  # Multiply them all together to get our total blocks
  totalblocks="`expr ${cyl} \* ${head}`"
  totalblocks="`expr ${totalblocks} \* ${sec}`"
  if [ -z "${totalblocks}" ]
  then
    totalblocks=`gpart show "${_intDISK}"|tail -2|head -1|awk '{ print $2 }'`
  fi

  # Now set the ending block to the total disk block size
  sizeblock="`expr ${totalblocks} - ${startblock}`"

  # Install new partition setup
  echo_log "Running gpart add on ${_intDISK}"
  rc_halt "gpart add -b ${startblock} -s ${sizeblock} -t freebsd -i 1 ${_intDISK}"
  sleep 2
  
  echo_log "Cleaning up ${_intDISK}s1"
  rc_halt "dd if=/dev/zero of=/dev/${_intDISK}s1 count=1024"
  
  if [ "$_intBOOT" = "bsd" ] ; then
    echo_log "Stamping boot0 on ${_intDISK}"
    rc_halt "gpart bootcode -b /boot/boot0 ${_intDISK}"
  else
    echo_log "Stamping boot1 on ${_intDISK}"
    rc_halt "gpart bootcode -b /boot/boot1 ${_intDISK}"
  fi

}

# Function which runs gpart and creates a single large slice
run_gpart_full()
{
  DISK=$1
  BOOT=$2
  SCHEME=$3

  if [ "$SCHEME" = "MBR" ] ; then
    init_mbr_full_disk "$DISK" "$BOOT"
    slice="${DISK}-1-mbr"
  else
    init_gpt_full_disk "$DISK"
    slice="${DISK}-1-gpt"
  fi

  # Lets save our slice, so we know what to look for in the config file later on
  if [ -z "$WORKINGSLICES" ]
  then
    WORKINGSLICES="${slice}"
    export WORKINGSLICES
  else
    WORKINGSLICES="${WORKINGSLICES} ${slice}"
    export WORKINGSLICES
  fi
};

# Function which runs gpart on a specified s1-4 slice
run_gpart_slice()
{
  DISK=$1
  if [ ! -z "$2" ]
  then
    BMANAGER="$2"
  fi

  # Set the slice we will use later
  slice="${1}s${3}"
 
  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  # Get the number of the slice we are working on
  slicenum="$3"

  # Stop any journaling
  stop_gjournal "${slice}"

  # Make sure we have disabled swap on this drive
  if [ -e "${slice}b" ]
  then
   swapoff ${slice}b >/dev/null 2>/dev/null
   swapoff ${slice}b.eli >/dev/null 2>/dev/null
  fi

  # Modify partition type
  echo_log "Running gpart modify on ${DISK}"
  rc_halt "gpart modify -t freebsd -i ${slicenum} ${DISK}"
  sleep 2

  # Clean up old partition
  echo_log "Cleaning up $slice"
  rc_halt "dd if=/dev/zero of=/dev/${DISK}s${slicenum} count=1024"

  sleep 1

  if [ "${BMANAGER}" = "bsd" ]
  then
    echo_log "Stamping boot sector on ${DISK}"
    rc_halt "gpart bootcode -b /boot/boot0 ${DISK}"
  fi

  # Set the slice to the format we'll be using for gpart later
  slice="${1}-${3}-mbr"

  # Lets save our slice, so we know what to look for in the config file later on
  if [ -z "$WORKINGSLICES" ]
  then
    WORKINGSLICES="${slice}"
    export WORKINGSLICES
  else
    WORKINGSLICES="${WORKINGSLICES} ${slice}"
    export WORKINGSLICES
  fi
};

# Function which runs gpart and creates a new slice from free disk space
run_gpart_free()
{
  DISK=$1
  SLICENUM=$2
  if [ ! -z "$3" ]
  then
    BMANAGER="$3"
  fi

  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  slice="${DISK}s${SLICENUM}"
  slicenum="${SLICENUM}" 

  # Working on the first slice, make sure we have MBR setup
  gpart show ${DISK} >/dev/null 2>/dev/null
  if [ "$?" != "0" -a "$SLICENUM" = "1" ] ; then
    echo_log "Initializing disk, no existing MBR setup"
    rc_halt "gpart create -s mbr ${DISK}"
  fi

  # Lets get the starting block first
  if [ "${slicenum}" = "1" ]
  then
     startblock="63"
  else
     # Lets figure out where the prior slice ends
     checkslice="`expr ${slicenum} - 1`"

     # Get starting block of this slice
     sblk=`gpart show ${DISK} | grep -v ${DISK} | tr -s '\t' ' ' | sed '/^$/d' | grep " ${checkslice} " | cut -d ' ' -f 2`
     blksize=`gpart show ${DISK} | grep -v ${DISK} | tr -s '\t' ' ' | sed '/^$/d' | grep " ${checkslice} " | cut -d ' ' -f 3`
     startblock="`expr ${sblk} + ${blksize}`"
  fi

  # No slice after the new slice, lets figure out the free space remaining and use it
  # Get the cyl of this disk
  get_disk_cyl "${DISK}"
  cyl="${VAL}"

  # Get the heads of this disk
  get_disk_heads "${DISK}"
  head="${VAL}"

  # Get the tracks/sectors of this disk
  get_disk_sectors "${DISK}"
  sec="${VAL}"

  # Multiply them all together to get our total blocks
  totalblocks="`expr ${cyl} \* ${head}`"
  totalblocks="`expr ${totalblocks} \* ${sec}`"


  # Now set the ending block to the total disk block size
  sizeblock="`expr ${totalblocks} - ${startblock}`"

  # Install new partition setup
  echo_log "Running gpart on ${DISK}"
  rc_halt "gpart add -b ${startblock} -s ${sizeblock} -t freebsd -i ${slicenum} ${DISK}"
  sleep 2
  
  echo_log "Cleaning up $slice"
  rc_halt "dd if=/dev/zero of=/dev/${slice} count=1024"

  sleep 1

  if [ "${BMANAGER}" = "bsd" ]
  then
    echo_log "Stamping boot sector on ${DISK}"
    rc_halt "gpart bootcode -b /boot/boot0 ${DISK}"
  fi

  slice="${DISK}-${SLICENUM}-mbr"
  # Lets save our slice, so we know what to look for in the config file later on
  if [ -z "$WORKINGSLICES" ]
  then
    WORKINGSLICES="${slice}"
    export WORKINGSLICES
  else
    WORKINGSLICES="${WORKINGSLICES} ${slice}"
    export WORKINGSLICES
  fi
};
