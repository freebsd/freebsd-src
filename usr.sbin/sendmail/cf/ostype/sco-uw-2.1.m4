#
#  SCO UnixWare 2.1.2 ostype file
#
#	Contributed by Christopher Durham <chrisdu@SCO.COM> of SCO.
#
divert(0)
VERSIONID(`@(#)sco-uw-2.1.m4	8.1 (Berkeley) 7/6/97')

define(`ALIAS_FILE', /usr/lib/mail/aliases)dnl
ifdef(`HELP_FILE',,`define(`HELP_FILE', /usr/ucblib/sendmail.hf)')dnl
ifdef(`STATUS_FILE',,`define(`STATUS_FILE', /usr/ucblib/sendmail.st)')dnl
define(`LOCAL_MAILER_PATH', `/usr/bin/rmail')dnl
define(`LOCAL_MAILER_FLAGS', `fhCEn9')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`LOCAL_MAILER_ARGS',`rmail $u')dnl
