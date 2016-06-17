#ifndef __LINUX_UDF_I_H
#define __LINUX_UDF_I_H

#define UDF_I(X)			(&((X)->u.udf_i))

#define UDF_I_LOCATION(X)	( UDF_I(X)->i_location )
#define UDF_I_LENEATTR(X)	( UDF_I(X)->i_lenEAttr )
#define UDF_I_LENALLOC(X)	( UDF_I(X)->i_lenAlloc )
#define UDF_I_LENEXTENTS(X)	( UDF_I(X)->i_lenExtents )
#define UDF_I_UNIQUE(X)		( UDF_I(X)->i_unique )
#define UDF_I_ALLOCTYPE(X)	( UDF_I(X)->i_alloc_type )
#define UDF_I_EXTENDED_FE(X)	( UDF_I(X)->i_extended_fe )
#define UDF_I_STRAT4096(X)	( UDF_I(X)->i_strat_4096 )
#define UDF_I_NEW_INODE(X)	( UDF_I(X)->i_new_inode )
#define UDF_I_NEXT_ALLOC_BLOCK(X)	( UDF_I(X)->i_next_alloc_block )
#define UDF_I_NEXT_ALLOC_GOAL(X)	( UDF_I(X)->i_next_alloc_goal )
#define UDF_I_UMTIME(X)		( UDF_I(X)->i_umtime )
#define UDF_I_UCTIME(X)		( UDF_I(X)->i_uctime )
#define UDF_I_CRTIME(X)		( UDF_I(X)->i_crtime )
#define UDF_I_UCRTIME(X)	( UDF_I(X)->i_ucrtime )

#endif /* !defined(_LINUX_UDF_I_H) */
