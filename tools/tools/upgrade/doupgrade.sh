#!/bin/sh

# Simple helper script for upgrade target.

# Expects MACHINE to be passed in with the environment, the "pass number"
# as the first argument the name of the file to leave its droppings in
# as the second.  CURDIR is also passed in the environment from ${.CURDIR}

PASS=$1
CONF=$2

cd ${CURDIR}/sys/${MACHINE}/conf

# Create kernel configuration file for pass #1
if [ $PASS -eq 1 ]; then
	echo "The following files are in ${CURDIR}/sys/${MACHINE}/conf:"; echo
	ls -C
	echo; echo -n "Which config file do you wish to use? [GENERIC] "
	read answer
	if [ -z "${answer}" ]; then
		KERN=GENERIC
	else
		KERN="${answer}"
	fi
	if [ ! -f ${KERN} ]; then
		KERN=GENERIC
	fi
	if ! grep -q atkbdc0 ${KERN}; then
		if [ ! -f ${KERN}.bkup ]; then
			cp ${KERN} ${KERN}.bkup
		fi
		sed -e 's/^device.*sc0.*$/ \
controller      atkbdc0 at isa? port IO_KBD tty \
device          atkbd0  at isa? tty irq 1 \
device          vga0    at isa? port ? conflicts \
device          sc0     at isa? tty \
pseudo-device   splash \
/' -e 's/sd\([0-9]\)/da\1/' -e 's/st\([0-9]\)/sa\1/' < ${KERN}.bkup > ${KERN}
	fi

	ROOTDEV=`awk '$2~/\/$/{print substr($1, 6, 3)}' /etc/fstab`
	echo -n "What is your boot device (e.g. wd0 or sd0)? [${ROOTDEV}] "
	read answer
	if [ -n "${answer}" ]; then
		ROOTDEV="${answer}"
	fi
	echo "KERNEL=${KERN}" > ${CONF}
	echo "ROOTDEV=${ROOTDEV}" >> ${CONF}
	if ! file /kernel | grep -q ELF; then
		echo "NEWBOOT=YES" >> ${CONF}
	fi
fi

# Build and install kernel as pass #2
if [ $PASS -eq 2 -a -f ${CONF} ]; then
	. ${CONF}
	if [ "x${NEWBOOT}" = "xYES" ]; then
	   echo "--------------------------------------------------------------"
	   echo " Installing new boot blocks"
	   echo "--------------------------------------------------------------"
	   if [ ! -f /boot/loader ]; then
	      (cd ${CURDIR}/lib/libstand; make obj; make -B depend all install)
	      (cd ${CURDIR}/sys/boot; make obj; make -B depend all install)
	   fi
	   if ! disklabel -B ${ROOTDEV}; then
		echo "Installation of new boot blocks failed!  Please correct"
		echo "this manually BEFORE you reboot your system!"
		exit 1
	   fi
	fi
	if ! file /kernel | grep -q ELF; then
		echo "--------------------------------------------------------------"
		echo " Building an elf kernel for ${KERNEL} using the new tools"
		echo "--------------------------------------------------------------"
		config -r ${KERNEL}
		cd ${CURDIR}/sys/compile/${KERNEL} && make -B depend -DFORCE all install
	fi
fi
