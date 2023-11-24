
SUBDIR=	

# Do we have the TCP_HHOOK symbol defined? If not, there is no point in
# building this modules by default.
# We will default to building this module if $OPT_GLOBAL does contain
# the TCP_HHOOK option.
.if defined(ALL_MODULES) || ${OPT_GLOBAL:UTCP_HHOOK:MTCP_HHOOK} != ""
SUBDIR+=	h_ertt
.endif

.include <bsd.subdir.mk>
