# $NetBSD: dep-double-colon-indep.mk,v 1.1 2020/10/23 19:11:30 rillig Exp $
#
# Tests for the :: operator in dependency declarations, which allows multiple
# dependency groups with the same target.  Each group is evaluated on its own,
# independent of the other groups.
#
# This is useful for targets that are updatable, such as a database or a log
# file.  Be careful with parallel mode though, to avoid lost updates and
# other inconsistencies.
#
# The target 1300 depends on 1200, 1400 and 1500.  The target 1200 is older
# than 1300, therefore nothing is done for it.  The other targets are newer
# than 1300, therefore each of them is made, independently from the other.

.END:
	@rm -f dep-double-colon-1???

_!=	touch -t 202001011200 dep-double-colon-1200
_!=	touch -t 202001011300 dep-double-colon-1300
_!=	touch -t 202001011400 dep-double-colon-1400
_!=	touch -t 202001011500 dep-double-colon-1500

all: dep-double-colon-1300

dep-double-colon-1300:: dep-double-colon-1200
	: 'Making 1200 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'

dep-double-colon-1300:: dep-double-colon-1400
	: 'Making 1400 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'

dep-double-colon-1300:: dep-double-colon-1500
	: 'Making 1500 ${.TARGET} from ${.ALLSRC} oodate ${.OODATE}'
