#!/bin/sh

set -e

if [ "_$1" = "_" ]
then
	echo "requires argument: path/to/sys/priv.h"
	exit 1
fi

SYS_PRIV_H=$1

ECHO="printf"

FILE_NTOS=priv_ntos.c
FILE_STON=priv_ston.c

rm -f $FILE_NTOS
rm -f $FILE_STON

${ECHO} "/* AUTOMATICALLY GENERATED FILE */\n"		>> $FILE_NTOS
${ECHO} "\n"						>> $FILE_NTOS
${ECHO} "#include <sys/priv.h>\n"			>> $FILE_NTOS
${ECHO} "\n"						>> $FILE_NTOS
${ECHO} "const char * priv_ntos(int priv);\n"		>> $FILE_NTOS
${ECHO} "\n"						>> $FILE_NTOS
${ECHO} "const char *\n"				>> $FILE_NTOS
${ECHO} "priv_ntos(int priv)\n"				>> $FILE_NTOS
${ECHO} "{\n"						>> $FILE_NTOS
${ECHO} "\n"						>> $FILE_NTOS
${ECHO} "    switch (priv) {\n"				>> $FILE_NTOS

${ECHO} "/* AUTOMATICALLY GENERATED FILE */\n"		>> $FILE_STON
${ECHO} "\n"						>> $FILE_STON
${ECHO} "#include <sys/priv.h>\n"			>> $FILE_STON
${ECHO} "#include <string.h>\n"				>> $FILE_STON
${ECHO} "\n"						>> $FILE_STON
${ECHO} "int priv_ston(const char *str);\n"		>> $FILE_STON
${ECHO} "\n"						>> $FILE_STON
${ECHO} "int\n"						>> $FILE_STON
${ECHO} "priv_ston(const char *str)\n"			>> $FILE_STON
${ECHO} "{\n"						>> $FILE_STON
${ECHO} "\n"						>> $FILE_STON


###

PRIVLIST=$( \
	grep -E '^#define\W+PRIV_.*\W+[0-9]+' ${SYS_PRIV_H} | \
	sed 's/\/\*.*//' | \
	sed -r 's/[	 ]+/;/g')

for PRIV in $PRIVLIST
do
	NAME=$(echo ${PRIV} | sed -r 's/[^;]*;([^;]*);([^;]*).*/\1/')
	NUMBER=$(echo ${PRIV} | sed -r 's/[^;]*;([^;]*);([^;]*).*/\2/')

	#echo "NAME=[${NAME}] NUMBER=[${NUMBER}]"

	printf "    case %s: return (\"%s\");\n" ${NUMBER} ${NAME} \
		>> $FILE_NTOS

	printf "    if (strcmp(str, \"%s\") == 0) return (%s);\n" \
		${NAME} ${NUMBER} >> $FILE_STON
done

###

${ECHO} "    default: return (\"unknown\");\n"		>> $FILE_NTOS
${ECHO} "    }\n"					>> $FILE_NTOS
${ECHO} "}\n"						>> $FILE_NTOS

${ECHO} "    return (0);\n"				>> $FILE_STON
${ECHO} "}\n"						>> $FILE_STON

exit 0

# EOF
