#! /bin/sh
# $FreeBSD$

ABI="$1"
CONF="$2"

header="${ABI}_syscalls.h"

cat > "${CONF}" << EOF
sysnames="${header}.tmp"
sysproto="/dev/null"
sysproto_h="/dev/null"
syshdr="/dev/null"
sysmk="/dev/null"
syssw="/dev/null"
syshide="/dev/null"
syscallprefix="SYS_"
switchname="sysent"
namesname="syscallnames"
systrace="/dev/null"
EOF
