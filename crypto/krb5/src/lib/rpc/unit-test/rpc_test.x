/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id$
 * $Source$
 * 
 * $Log$
 * Revision 1.2  1996/07/22 20:41:42  marc
 * this commit includes all the changes on the OV_9510_INTEGRATION and
 * OV_MERGE branches.  This includes, but is not limited to, the new openvision
 * admin system, and major changes to gssapi to add functionality, and bring
 * the implementation in line with rfc1964.  before committing, the
 * code was built and tested for netbsd and solaris.
 *
 * Revision 1.1.4.1  1996/07/18 04:20:04  marc
 * merged in changes from OV_9510_BP to OV_9510_FINAL1
 *
# Revision 1.1.2.1  1996/06/20  23:42:06  marc
# File added to the repository on a branch
#
# Revision 1.1  1993/11/03  23:53:58  bjaspan
# Initial revision
#
 */

program RPC_TEST_PROG {
	version RPC_TEST_VERS_1 {
		string RPC_TEST_ECHO(string) = 1;
	} = 1;
} = 1000001;
