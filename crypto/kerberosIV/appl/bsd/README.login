This login has additional functionalities. They are all based on (part of)
Wietse Venema's logdaemon package.


The following defines can be used:
1) LOGIN_ACCESS to allow access control on a per tty/user combination
2) LOGALL to log all logins

-Guido

This login has some of Berkeley's paranoid/broken (depending on your point
of view) Kerberos code conditionalized out, so that by default it works like
klogin does at MIT-LCS.  You can define KLOGIN_PARANOID to re-enable this code.
This define also controls whether a warning message is printed when logging
into a system with no krb.conf file, which usually means that Kerberos is
not configured.

-GAWollman

(removed S/Key,	/assar)
