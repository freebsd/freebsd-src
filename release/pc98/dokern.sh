#!/bin/sh

# From the kernel config file on stdin (usually GENERIC), pare out items as
# determined by whether or not the kernel is being prepared to contain
# an MFS ($1 = YES) or will have the floppy image all to itself ($1 = NO or
# not specified).

if [ $# -lt 1 ]; then
	MFS=NO
else
	MFS=$1
fi

if [ "$MFS" = "YES" ]; then
	sed	-e '/pty/d' \
		-e '/acd0/d' \
		-e '/cd0/d' \
		-e '/mcd0/d' \
		-e '/matcd0/d' \
		-e '/scd0/d' \
		-e '/ft0/d' \
		-e '/wt0/d' \
		-e '/wfd0/d' \
		-e '/pass0/d' \
		-e '/pnp0/d' \
		-e '/apm0/d' \
		-e '/ppp/d' \
		-e '/eisa0/d' \
		-e '/aha0/d' \
		-e '/ahb0/d' \
		-e '/bt0/d' \
		-e '/fd1/d' \
		-e '/splash/d' \
		-e '/gzip/d' \
		-e '/NFS/d' \
		-e '/PROCFS/d' \
		-e '/MSDOSFS/d' \
		-e '/CD9660/d' \
		-e '/SYSVSHM/d' \
		-e '/KTRACE/d' \
		-e '/MATH_EMULATE/d' \
		-e 's/GENERIC/BOOTMFS/g' \
		-e '/maxusers/s/32/4/'
else
	sed	-e '/pty/d' \
		-e '/pass0/d' \
		-e '/apm0/d' \
		-e '/ppp/d' \
		-e '/gzip/d' \
		-e '/splash/d' \
		-e '/PROCFS/d' \
		-e '/KTRACE/d' \
		-e 's/GENERIC/BOOTMFS/g'
fi
echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
