# $NetBSD: jobs-empty-commands.mk,v 1.2 2021/01/30 12:46:38 rillig Exp $
#
# In jobs mode, the shell commands for creating a target are written to a
# temporary file first, which is then run by the shell.  In chains of
# dependencies, these files would end up empty.  Since job.c 1.399 from
# 2021-01-29, these empty files are no longer created.
#
# https://mail-index.netbsd.org/current-users/2021/01/26/msg040215.html

.MAKEFLAGS: -j1
#.MAKEFLAGS: -dn		# to see the created temporary files

all: .PHONY step-1
.for i i_plus_1 in ${:U:range=100:@i@$i $i@:[2..199]}
step-$i: .PHONY step-${i_plus_1}
.endfor
step-100: .PHONY
	@echo 'action'
