.\"$Header: program.nr,v 1.1 88/12/05 18:10:57 nhall Exp $
.\"$Source: /usr/argo/doc/kernel/RCS/program.nr,v $
.\"
.\"
.\" FONT CONVENTIONS
.\"
.\" \fIprocedure()\fR
.\" \fIsyscall()\fR
.\" \fImanpage(3)\fR
.\" \fIdata_structure_name\fR
.\" \fC/file/or/directory/name\fR
.\" \fC
.\"    section of code
.\" \fR
.\"
.\"
.\" LOOK FOR ALL CASES OF 'writing' (as in, "at this writing")
.\" to be sure you've updated everything before distributing this!
.\"
.\"This file uses -me and tbl macros.
.so macros.nr
.pn 1
.IC
.IF
.(l C
.sz 16
Wisconsin ARGO 1.0 Kernel Programmer's Guide for
Academic Operating Systems 4.3
.sz 8
.)l
.so ../icon/ARGO.nr
.he 'ARGO 1.0 Kernel Programmer\'s Guide'''
.fo '%''December 9, 1988'
.bp
.so intro.nr
.so def.nr
.so trans_serv.nr
.so net_serv.nr
.so ipc.nr
.so addr.nr
.so trans_design.nr
.so net_design.nr
.so eicon.nr
.so errors.nr
.so debug.nr
.so appendix_a.nr
.\" Leave manual pages out!
.\".so appendix_b.nr
.fo ''Table of Contents''
.bp
.xp
