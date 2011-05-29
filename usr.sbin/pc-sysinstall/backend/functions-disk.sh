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
    [ "$_dsk" = "${1}" ] && return 0
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
    echo "$i" | grep -q "The data for partition" 2>/dev/null
    if [ $? -eq 0 ] ; then
       INPART="0"
       PART="`echo ${i} | cut -d ' ' -f 5`"
       if [ "$PART" = "$PARTNUM" ] ; then
          INPART="1"
       fi
    fi

    # In the partition section
    if [ "$INPART" = "1" ] ; then
       echo "$i" | grep -q "^sysid" 2>/dev/null
       if [ $? -eq 0 ] ; then
         SYSID="`echo ${i} | tr -s '\t' ' ' | cut -d ' ' -f 2`"
         break
       fi

    fi

  done < ${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  export VAL="${SYSID}"
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
    echo "$i" | grep -q "The data for partition" 2>/dev/null
    if [ $? -eq 0 ] ; then
       INPART="0"
       PART="`echo ${i} | cut -d ' ' -f 5`"
       if [ "$PART" = "$PARTNUM" ] ; then
          INPART="1"
       fi
    fi

    # In the partition section
    if [ "$INPART" = "1" ] ; then
       echo "$i" | grep -q "^sysid" 2>/dev/null
       if [ $? -eq 0 ] ; then
         LABEL="`echo ${i} | tr -s '\t' ' ' | cut -d ',' -f 2-10`"
         break
       fi

    fi

  done < ${TMPDIR}/disk-${DISK}
  rm ${TMPDIR}/disk-${DISK}

  export VAL="${LABEL}"
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

  export VAL="${LABEL}"
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

  export VAL="${SB}"
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

  export VAL="${BS}"
};

# Function which returns the partitions on a target disk
get_disk_partitions()
{
  gpart show ${1} >/dev/null 2>/dev/null
  if [ $? -ne 0 ] ; then
    export VAL=""
    return
  fi

  type=`gpart show ${1} | awk '/^=>/ { printf("%s",$5); }'`

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

  export VAL="${RSLICES}"
};

# Function which returns a target disks cylinders
get_disk_cyl()
{
  cyl=`diskinfo -v ${1} | grep "# Cylinders" | tr -s ' ' | cut -f 2`
  export VAL="${cyl}"
};

# Function which returns a target disks sectors
get_disk_sectors()
{
  sec=`diskinfo -v ${1} | grep "# Sectors" | tr -s ' ' | cut -f 2`
  export VAL="${sec}"
};

# Function which returns a target disks heads
get_disk_heads()
{
  head=`diskinfo -v ${1} | grep "# Heads" | tr -s ' ' | cut -f 2`
  export VAL="${head}"
};

# Function which returns a target disks mediasize in sectors
get_disk_mediasize()
{
  mediasize=`diskinfo -v ${1} | grep "# mediasize in sectors" | tr -s ' ' | cut -f 2`
  export VAL="${mediasize}"
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
  local DISK="$1"

  # Check for any swaps to stop
  for i in `swapctl -l | grep "$DISK" | awk '{print $1}'`
  do
    swapoff ${i} >/dev/null 2>/dev/null
  done

  # Delete the gparts now
  for i in `gpart show ${DISK} 2>/dev/null | tr -s ' ' | cut -d ' ' -f 4`
  do
   if [ "/dev/${i}" != "${DISK}" -a "${i}" != "-" ] ; then
     rc_nohalt "gpart delete -i ${i} ${DISK}"
   fi
  done

  # Destroy the disk geom
  rc_nohalt "gpart destroy ${DISK}"

  # Make sure we clear any hidden gpt tables
  clear_backup_gpt_table "${DISK}"

  # Wipe out front of disk
  rc_nohalt "dd if=/dev/zero of=${DISK} count=3000"

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
  local DISK="`echo ${1} | sed 's|/dev/||g'`"
  GPROV="`gmirror list | grep ". Name: mirror/" | cut -d '/' -f 2`"
  for gprov in $GPROV 
  do
    gmirror list | grep -q "Name: ${DISK}" 2>/dev/null
    if [ $? -eq 0 ]
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
  local _geld="`echo ${1} | sed 's|/dev/||g'`"
  cd /dev

  for i in `ls ${_geld}*`
  do
    echo $i | grep -q '.eli' 2>/dev/null
    if [ $? -eq 0 ]
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

  # Start with disk0 and gm0
  disknum="0"
  gmnum="0"

  # Make sure all zpools are exported
  export_all_zpools

  # We are ready to start setting up the disks, lets read the config and do the actions
  while read line
  do
    echo $line | grep -q "^disk${disknum}=" 2>/dev/null
    if [ $? -eq 0 ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      DISK="$VAL"

      echo "${DISK}" | grep -q '^/dev/'
      if [ $? -ne 0 ] ; then DISK="/dev/$DISK" ; fi
     
      # Before we go further, lets confirm this disk really exists
      if [ ! -e "${DISK}" ] ; then
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
    echo $line | grep -q "^mirror=" 2>/dev/null
    if [ $? -eq 0 ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      MIRRORDISK="$VAL"
      echo "${MIRRORDISK}" | grep -q '^/dev/'
      if [ $? -ne 0 ] ; then MIRRORDISK="/dev/$MIRRORDISK" ; fi
     
      # Before we go further, lets confirm this disk really exists
      if [ ! -e "${MIRRORDISK}" ]
      then
        exit_err "ERROR: The mirror disk ${MIRRORDISK} does not exist!"
      fi
    fi

    # Lets see if we have been given a mirror balance choice
    echo $line | grep -q "^mirrorbal=" 2>/dev/null
    if [ $? -eq 0 ]
    then

      # Found a disk= entry, lets get the disk we are working on
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      MIRRORBAL="$VAL"
    fi

    echo $line | grep -q "^partition=" 2>/dev/null
    if [ $? -eq 0 ]
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
            LASTSLICE=$((LASTSLICE+1))
        fi

        if [ $LASTSLICE -gt 4 ]
        then
          exit_err "ERROR: BSD only supports primary partitions, and there are none availble on $DISK"
        fi

      fi
    fi

    # Check if we have an image file defined
    echo $line | grep -q "^image=" 2>/dev/null
    if [ $? -eq 0 ] ; then
      # Found an image= entry, lets read / set it
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      IMAGE="$VAL"
      if [ ! -f "$IMAGE" ] ; then
        exit_err "$IMAGE file does not exist"
      fi
    fi

    # Check if we have a partscheme specified
    echo $line | grep -q "^partscheme=" 2>/dev/null
    if [ $? -eq 0 ] ; then
      # Found a partscheme= entry, lets read / set it 
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      PSCHEME="$VAL"
      if [ "$PSCHEME" != "GPT" -a "$PSCHEME" != "MBR" ] ; then
        exit_err "Unknown partition scheme: $PSCHEME" 
      fi
    fi

    echo $line | grep -q "^bootManager=" 2>/dev/null
    if [ $? -eq 0 ]
    then
      # Found a bootManager= entry, lets read /set it
      get_value_from_string "${line}"
      strip_white_space "$VAL"
      BMANAGER="$VAL"
    fi

    echo $line | grep -q "^commitDiskPart" 2>/dev/null
    if [ $? -eq 0 ]
    then
      # Found our flag to commit this disk setup / lets do sanity check and do it
      if [ ! -z "${DISK}" -a ! -z "${PTYPE}" ]
      then

        case ${PTYPE} in
          all)
            # If we have a gmirror, lets set it up
            if [ -n "$MIRRORDISK" ]; then
              # Default to round-robin if the user didn't specify
              if [ -z "$MIRRORBAL" ]; then MIRRORBAL="round-robin" ; fi

              echo "$MIRRORDISK:$MIRRORBAL:gm${gmnum}" >${MIRRORCFGDIR}/$DISK
	      init_gmirror "$gmnum" "$MIRRORBAL" "$DISK" "$MIRRORDISK"

	      # Reset DISK to the gmirror device
	      DISK="/dev/mirror/gm${gmnum}"
              gmnum=$((gmknum+1))
            fi

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
	  _sFile=`echo $tmpSLICE | sed 's|/|-|g'`
          echo "disk${disknum}" >${SLICECFGDIR}/$_sFile
        fi

        # Increment our disk counter to look for next disk and unset
        unset BMANAGER PTYPE DISK MIRRORDISK MIRRORBAL PSCHEME IMAGE
        disknum=$((disknum+1))
      else
        exit_err "ERROR: commitDiskPart was called without procceding disk<num>= and partition= entries!!!" 
      fi
    fi

  done <${CFGF}

};


# Init the gmirror device
init_gmirror()
{
    local _mNum=$1
    local _mBal=$2
    local _mDisk=$3

    # Create this mirror device
    rc_halt "gmirror label -vb ${_mBal} gm${_mNum} /dev/${_mDisk}"

    sleep 3

}

# Stop all gjournals on disk / slice
stop_gjournal()
{
  _gdsk="`echo $1 | sed 's|/dev/||g'`"
  # Check if we need to shutdown any journals on this drive
  ls /dev/${_gdsk}*.journal >/dev/null 2>/dev/null
  if [ $? -eq 0 ]
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


# Function to wipe the potential backup gpt table from a disk
clear_backup_gpt_table()
{
  echo_log "Clearing gpt backup table location on disk"
  rc_nohalt "dd if=/dev/zero of=${1} bs=1m count=1"
  rc_nohalt "dd if=/dev/zero of=${1} bs=1m oseek=`diskinfo ${1} | awk '{print int($3 / (1024*1024)) - 4;}'`"
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
 
  startblock="2016"

  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  # Stop any journaling
  stop_gjournal "${_intDISK}"

  # Remove any existing partitions
  delete_all_gpart "${_intDISK}"

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
  totalblocks="`expr ${cyl} \* ${head} 2>/dev/null`"
  totalblocks="`expr ${totalblocks} \* ${sec} 2>/dev/null`"
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
  rc_halt "dd if=/dev/zero of=${_intDISK}s1 count=1024"
  
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
    slice=`echo "${DISK}:1:mbr" | sed 's|/|-|g'`
  else
    init_gpt_full_disk "$DISK"
    slice=`echo "${DISK}:1:gpt" | sed 's|/|-|g'`
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
  if [ -n "$2" ]
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
  rc_halt "dd if=/dev/zero of=${DISK}s${slicenum} count=1024"

  sleep 1

  if [ "${BMANAGER}" = "bsd" ]
  then
    echo_log "Stamping boot sector on ${DISK}"
    rc_halt "gpart bootcode -b /boot/boot0 ${DISK}"
  fi

  # Set the slice to the format we'll be using for gpart later
  slice=`echo "${1}:${3}:mbr" | sed 's|/|-|g'`

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
  if [ -n "$3" ]
  then
    BMANAGER="$3"
  fi

  # Set our sysctl so we can overwrite any geom using drives
  sysctl kern.geom.debugflags=16 >>${LOGOUT} 2>>${LOGOUT}

  slice="${DISK}s${SLICENUM}"
  slicenum="${SLICENUM}" 

  # Working on the first slice, make sure we have MBR setup
  gpart show ${DISK} >/dev/null 2>/dev/null
  if [ $? -ne 0 -a "$SLICENUM" = "1" ] ; then
    echo_log "Initializing disk, no existing MBR setup"
    rc_halt "gpart create -s mbr ${DISK}"
  fi

  # Lets get the starting block first
  if [ "${slicenum}" = "1" ]
  then
     startblock="63"
  else
     # Lets figure out where the prior slice ends
     checkslice=$((slicenum-1))

     # Get starting block of this slice
     sblk=`gpart show ${DISK} | grep -v ${DISK} | tr -s '\t' ' ' | sed '/^$/d' | grep " ${checkslice} " | cut -d ' ' -f 2`
     blksize=`gpart show ${DISK} | grep -v ${DISK} | tr -s '\t' ' ' | sed '/^$/d' | grep " ${checkslice} " | cut -d ' ' -f 3`
     startblock=$((sblk+blksiz))
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
  totalblocks=$((cyl*head))
  totalblocks=$((totalblocks*sec))


  # Now set the ending block to the total disk block size
  sizeblock=$((totalblocks-startblock))

  # Install new partition setup
  echo_log "Running gpart on ${DISK}"
  rc_halt "gpart add -b ${startblock} -s ${sizeblock} -t freebsd -i ${slicenum} ${DISK}"
  sleep 2
  
  echo_log "Cleaning up $slice"
  rc_halt "dd if=/dev/zero of=${slice} count=1024"

  sleep 1

  if [ "${BMANAGER}" = "bsd" ]
  then
    echo_log "Stamping boot sector on ${DISK}"
    rc_halt "gpart bootcode -b /boot/boot0 ${DISK}"
  fi

  slice=`echo "${DISK}:${SLICENUM}:mbr" | sed 's|/|-|g'`
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
