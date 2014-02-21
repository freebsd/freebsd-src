# $FreeBSD$

# capability sources
.PATH: ${.CURDIR}/../../sys/kern ${.CURDIR}/capability

SRCS+=	subr_capability.c

SYM_MAPS+=	${.CURDIR}/capability/Symbol.map

MAN+=	cap_rights_init.3

MLINKS+=cap_rights_init.3 cap_rights_set.3
MLINKS+=cap_rights_init.3 cap_rights_clear.3
MLINKS+=cap_rights_init.3 cap_rights_is_set.3
MLINKS+=cap_rights_init.3 cap_rights_is_valid.3
MLINKS+=cap_rights_init.3 cap_rights_merge.3
MLINKS+=cap_rights_init.3 cap_rights_remove.3
MLINKS+=cap_rights_init.3 cap_rights_contains.3
