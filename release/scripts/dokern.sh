#!/bin/sh

sed	-e '/pty/d' \
	-e '/pass0/d' \
	-e '/apm0/d' \
	-e '/ppp/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e 's/GENERIC/BOOTMFS/g'

# So dhclient will work (just on boot floppy).
echo "pseudo-device bpfilter 4"

echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
