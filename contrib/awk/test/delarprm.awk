# From dragon!unagi.cis.upenn.edu!sjanet Tue Mar 25 17:12:20 1997
# Return-Path: <dragon!unagi.cis.upenn.edu!sjanet>
# Received: by skeeve.atl.ga.us (/\==/\ Smail3.1.22.1 #22.1)
# 	id <m0w9eS4-000GWyC@skeeve.atl.ga.us>; Tue, 25 Mar 97 17:12 EST
# Received: by vecnet.com (DECUS UUCP /2.0/2.0/2.0/);
#           Tue, 25 Mar 97 16:58:36 EDT
# Received: from gnu-life.ai.mit.edu by antaries.vec.net (MX V4.2 VAX) with SMTP;
#           Tue, 25 Mar 1997 16:58:26 EST
# Received: from linc.cis.upenn.edu by gnu-life.ai.mit.edu (8.8.5/8.6.12GNU) with
#           ESMTP id QAA24350 for <bug-gnu-utils@prep.ai.mit.edu>; Tue, 25 Mar
#           1997 16:56:59 -0500 (EST)
# Received: from unagi.cis.upenn.edu (UNAGI.CIS.UPENN.EDU [158.130.8.153]) by
#           linc.cis.upenn.edu (8.8.5/8.8.5) with ESMTP id QAA09424; Tue, 25 Mar
#           1997 16:56:54 -0500 (EST)
# Received: (from sjanet@localhost) by unagi.cis.upenn.edu (8.8.5/8.8.5) id
#           QAA03969; Tue, 25 Mar 1997 16:56:50 -0500 (EST)
# Date: Tue, 25 Mar 1997 16:56:50 -0500 (EST)
# From: Stan Janet <sjanet@unagi.cis.upenn.edu>
# Message-ID: <199703252156.QAA03969@unagi.cis.upenn.edu>
# To: bug-gnu-utils@prep.ai.mit.edu
# CC: arnold@gnu.ai.mit.edu
# Subject: GNU awk 3.0.2 bug: fatal error deleting local array inside function
# Status: ORf
# 
# Version: GNU Awk 3.0.2
# Platforms: SunOS 4.1.1 (compiled with Sun cc)
#            IRIX 5.3 (compiled with SGI cc)
# Problem: Deleting local array inside function causes fatal internal error (and
# 	core dump. The error does not occur when the variable "x", unused in
# 	the example, is removed or when the function is declared foo(x,p).
# 	When the function is declared foo(p,x), adding a dummy line that uses
# 	"x", e.g. "x=1" does not prevent the error. If "p" is not deleted,
# 	there is no error. If "p[1]" is used to delete the lone element, there
# 	is no error.
# 
# ==== The program x.gawk ====

function foo(p,x) {
	p[1]="bar"
	delete p
	return 0
}

BEGIN {
	foo()
}

# ==== The output for "gawk -f x.gawk" (SunOS) ====
# 
# gawk: x.gawk:4: fatal error: internal error
