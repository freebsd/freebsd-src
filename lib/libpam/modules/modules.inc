# $FreeBSD$

MODULES		 =
MODULES		+= pam_deny
MODULES		+= pam_ftp
.if defined(MAKE_KERBEROS4) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
MODULES		+= pam_kerberosIV
.endif
.if defined(MAKE_KERBEROS5) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
MODULES		+= pam_krb5
.endif
MODULES		+= pam_lastlog
MODULES		+= pam_login_access
MODULES		+= pam_nologin
MODULES		+= pam_opie
MODULES		+= pam_opieaccess
#MODULES		+= pam_passwdqc
MODULES		+= pam_permit
MODULES		+= pam_radius
MODULES		+= pam_rhosts
MODULES		+= pam_rootok
MODULES		+= pam_securetty
MODULES		+= pam_self
.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
MODULES		+= pam_ssh
.endif
MODULES		+= pam_tacplus
MODULES		+= pam_unix
MODULES		+= pam_wheel
