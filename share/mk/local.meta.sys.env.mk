
# local configuration specific to meta mode

.-include <site.meta.sys.env.mk>

.if !defined(NO_META_MISSING)
META_MODE+=    missing-meta=yes
.endif
# silent will hide command output if a .meta file is created.
.if !defined(NO_SILENT)
META_MODE+=    silent=yes
.endif
.if empty(META_MODE:Mnofilemon)
META_MODE+=	missing-filemon=yes
.endif

.if make(showconfig)
# this does not need/want filemon
UPDATE_DEPENDFILE= NO
.endif
