#!/bin/sh
#
# Script to recover nvi edit sessions.

RECDIR="@vi_cv_path_preserve@"

[ -d ${RECDIR} ] || exit 1
find ${RECDIR} ! -type f -a ! -type d -delete

# Check editor backup files.
vibackup=`echo ${RECDIR}/vi.*`
if [ "${vibackup}" != '${RECDIR}/vi.*' ]; then
	echo -n 'Recovering vi editor sessions:'
	for i in ${RECDIR}/vi.*; do
		# Only test files that are readable.
		if [ ! -r "${i}" ]; then
			continue
		fi

		# Unmodified nvi editor backup files either have the
		# execute bit set or are zero length.  Delete them.
		if [ -x "${i}" -o ! -s "${i}" ]; then
			rm -f "${i}"
		fi
	done
else exit
fi

# It is possible to get incomplete recovery files, if the editor crashes
# at the right time.
virecovery=`echo ${RECDIR}/recover.*`
if [ "${virecovery}" != "${RECDIR}/recover.*" ]; then
	for i in ${RECDIR}/recover.*; do
		# Only test files that are readable.
		if [ ! -r "${i}" ]; then
			continue
		fi

		# Delete any recovery files that are zero length, corrupted,
		# or that have no corresponding backup file.  Else send mail
		# to the user.
		recfile=`awk '/^X-vi-data: *file;/ { sub(/^.*;/, " "); \
		    do { if (substr($0,1,1) == " ") print; else exit } \
		    while(getline) }' < "${i}" | uudecode -mr`
		if [ -n "${recfile}" -a -s "${recfile}" ]; then
			sendmail -odb -t < "${i}"
			echo -n '.'
		else
			rm -f "${i}"
		fi
	done
fi
echo ' done.'
