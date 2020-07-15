divert(-1)
#
# Copyright (c) 2019 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)dnl
VERSIONID(`$Id: block_bad_helo.m4,v 1.2 2013-11-22 20:51:11 ca Exp $')
divert(-1)
define(`_FFR_TLS_ALTNAMES', `1')
divert(6)dnl
O SetCertAltnames=true
