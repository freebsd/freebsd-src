#!/bin/sh -e
#
# Copyright (c) 2015 Colin Percival
#
# SPDX-License-Identifier: BSD-2-Clause
#
# mkami.sh: Create an AMI from the currently running EC2 instance.
#

export PATH=$PATH:/usr/local/bin

NAME=$1
if [ -z "$NAME" ]; then
	echo "usage: mkami <AMI name> [<AMI description>]"
	exit 1
fi
DESC=$2
if ! [ -z "$DESC" ]; then
	DESCOPT="--description '$DESC'"
fi

# Get the instance ID and region from the EC2 Instance Metadata Service:
# https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/instancedata-data-retrieval.html
TMPFILE=`mktemp`
fetch -qo $TMPFILE http://169.254.169.254/latest/dynamic/instance-identity/document
INST=`awk -F \" '/"instanceId"/ { print $4 }' $TMPFILE`
REGION=`awk -F \" '/"region"/ { print $4 }' $TMPFILE`
rm $TMPFILE
CMD="aws --region $REGION ec2 create-image --instance-id $INST --output text --no-reboot --name '$NAME' $DESCOPT"

# Unmount the new system image
if mount -p | grep -q '/mnt.*ufs'; then
	echo -n "Unmounting new system image..."
	sync
	umount /mnt
	sync
	sleep 5
	sync
	echo " done."
elif mount -p | grep -q '/mnt.*zfs'; then
	echo -n "Unmounting new system image..."
	sync
	zfs umount -a
	zfs umount zroot/ROOT/default
	sync
	sleep 5
	sync
	echo " done."
fi

if eval "$CMD" --dry-run 2>&1 |
    grep -qE 'UnauthorizedOperation|Unable to locate credentials'; then
	echo "This EC2 instance does not have permission to create AMIs."
	echo "Launch an AMI-builder instance with an appropriate IAM Role,"
	echo "create an AMI from this instance via the AWS Console, or run"
	echo "the following command from a system with the necessary keys:"
	echo
	echo "$CMD"
	exit
fi

echo -n "Creating AMI..."
AMINAME=`eval "$CMD"`
echo " done."
echo "AMI created in $REGION: $AMINAME"
