#!/bin/sh

SRCDIR=trunk
DSTDIR=head

DIRS=$(cd ${SRCDIR} && find . -type d | grep -v '\.svn')
FILES=$(cd ${SRCDIR} && find . -type f | grep -v '\.svn')


for DIR in ${DIRS}
do
	if [ ! -e "${DSTDIR}/${DIR}" ]
	then
		mkdir -p "${DSTDIR}/${DIR}"
	fi
done

for FILE in ${FILES}
do
	cp -av "${SRCDIR}/${FILE}" "${DSTDIR}/${FILE}"
done 

exit 0

# EOF
