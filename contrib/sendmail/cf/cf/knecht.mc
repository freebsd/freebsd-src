divert(-1)
#
# Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

#
#  This is specific to Eric's home machine.
#
#	Run daemon with -bd -q5m
#

divert(0)
VERSIONID(`$Id: knecht.mc,v 8.55 2001/08/01 22:20:40 eric Exp $')
OSTYPE(bsd4.4)
DOMAIN(generic)

define(`ALIAS_FILE', ``/etc/mail/aliases, /var/listmanager/aliases'')
define(`confFORWARD_PATH', `$z/.forward.$w:$z/.forward+$h:$z/.forward')
define(`confDEF_USER_ID', `mailnull')
define(`confHOST_STATUS_DIRECTORY', `.hoststat')
define(`confTO_ICONNECT', `10s')
define(`confCOPY_ERRORS_TO', `Postmaster')
define(`confTO_QUEUEWARN', `8h')
define(`confMIN_QUEUE_AGE', `27m')
define(`confTRUSTED_USERS', ``www listmgr'')
define(`confPRIVACY_FLAGS', ``authwarnings,noexpn,novrfy'')

define(`CERT_DIR', `MAIL_SETTINGS_DIR`'certs')
define(`confCACERT_PATH', `CERT_DIR')
define(`confCACERT', `CERT_DIR/CAcert.pem')
define(`confSERVER_CERT', `CERT_DIR/MYcert.pem')
define(`confSERVER_KEY', `CERT_DIR/MYkey.pem')
define(`confCLIENT_CERT', `CERT_DIR/MYcert.pem')
define(`confCLIENT_KEY', `CERT_DIR/MYkey.pem')

FEATURE(access_db)
FEATURE(local_lmtp)
FEATURE(virtusertable)

FEATURE(`nocanonify', `canonify_hosts')
CANONIFY_DOMAIN(`sendmail.org')
CANONIFY_DOMAIN_FILE(`/etc/mail/canonify-domains')

dnl #  at most 10 queue runners
define(`confMAX_QUEUE_CHILDREN', `20')

define(`confMAX_RUNNERS_PER_QUEUE', `5')

dnl #  run at most 10 concurrent processes for initial submission
define(`confFAST_SPLIT', `10')

dnl #  10 runners, split into at most 15 recipients per envelope
QUEUE_GROUP(`mqueue', `P=/var/spool/mqueue, R=5, r=15, F=f')

MAILER(local)
MAILER(smtp)

LOCAL_CONFIG
#
#  Regular expression to reject:
#    * numeric-only localparts from aol.com and msn.com
#    * localparts starting with a digit from juno.com
#
Kcheckaddress regex -a@MATCH
   ^([0-9]+<@(aol|msn)\.com|[0-9][^<]*<@juno\.com)\.?>

#
#  Names that won't be allowed in a To: line (local-part and domains)
#
C{RejectToLocalparts}	friend you
C{RejectToDomains}	public.com

LOCAL_RULESETS
HTo: $>CheckTo

SCheckTo
R$={RejectToLocalparts}@$*	$#error $: "553 Header error"
R$*@$={RejectToDomains}		$#error $: "553 Header error"

HMessage-Id: $>CheckMessageId

SCheckMessageId
R< $+ @ $+ >			$@ OK
R$*				$#error $: "554 Header error"

HReceived: $>CheckReceived

SCheckReceived
R$* ......................................................... $*
				$#error $: "554 Header error"

#
# Reject certain senders
#	Regex match to catch things in quotes
#
HFrom: $>+CheckFrom
KCheckFrom regex -a@MATCH
	[^a-z]?(Net-Pa)[^a-z]

SCheckFrom
R$*				$: $( CheckFrom $1 $)
R@MATCH				$#error $: "553 Header error"

LOCAL_RULESETS
SLocal_check_mail
# check address against various regex checks
R$*				$: $>Parse0 $>3 $1
R$+				$: $(checkaddress $1 $)
R@MATCH				$#error $: "553 Header error"

#
#  Following code from Anthony Howe <achowe@snert.com>.  The check
#  for the Outlook Express marker may hit some legal messages, but
#  the Content-Disposition is clearly illegal.
#

#########################################################################
#
# w32.sircam.worm@mm
#
# There are serveral patterns that appear common ONLY to SirCam worm and
# not to Outlook Express, which claims to have sent the worm.  There are
# four headers that always appear together and in this order:
#
#  X-MIMEOLE: Produced By Microsoft MimeOLE V5.50.4133.2400
#  X-Mailer: Microsoft Outlook Express 5.50.4133.2400
#  Content-Type: multipart/mixed; boundary="----27AA9124_Outlook_Express_message_boundary"
#  Content-Disposition: Multipart message
#
# Empirical study of the worm message headers vs. true Outlook Express
# (5.50.4133.2400 & 5.50.4522.1200) messages with multipart/mixed attachments
# shows Outlook Express does:
#
#  a) NOT supply a Content-Disposition header for multipart/mixed messages.
#  b) NOT specify the header X-MimeOLE header name in all-caps
#  c) NOT specify boundary tag with the expression "_Outlook_Express_message_boundary"
#
# The solution below catches any one of this three issues. This is not an ideal
# solution, but a temporary measure. A correct solution would be to check for
# the presence of ALL three header attributes. Also the solution is incomplete
# since Outlook Express 5.0 and 4.0 were not compared.
#
# NOTE regex keys are first dequoted and spaces removed before matching.
# This caused me no end of grief.
#
#########################################################################

LOCAL_RULESETS

KSirCamWormMarker regex -f -aSUSPECT multipart/mixed;boundary=----.+_Outlook_Express_message_boundary
HContent-Type:		$>CheckContentType

SCheckContentType
R$+			$: $(SirCamWormMarker $1 $)
RSUSPECT		$#error $: "553 Possible virus, see http://www.symantec.com/avcenter/venc/data/w32.sircam.worm@mm.html"

HContent-Disposition:	$>CheckContentDisposition

SCheckContentDisposition
R$-			$@ OK
R$- ; $+		$@ OK
R$*			$#error $: "553 Illegal Content-Disposition"
