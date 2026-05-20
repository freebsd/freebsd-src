.include <src.opts.mk>

WORLDPACKAGE=	sendmail

# sendmail requires make to build its configuration file.  However, sendmail
# is still built even if WITHOUT_MAKE is set; in that case, just assume the
# user has some other way to create the configuration file.
.if ${MK_MAKE} != "no"
PKG_DEPS.sendmail+=	bmake
.endif

.include <bsd.pkg.mk>
