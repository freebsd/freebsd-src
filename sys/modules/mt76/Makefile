.include <kmod.opts.mk>

SUBDIR=		core
SUBDIR+=	mt7615
.if ${KERN_OPTS:MDEV_PCI}
SUBDIR+=	mt7915
.endif
SUBDIR+=	mt7921
SUBDIR+=	mt7925
.if ${KERN_OPTS:MDEV_PCI}
SUBDIR+=	mt7996
.endif

.include <bsd.subdir.mk>
