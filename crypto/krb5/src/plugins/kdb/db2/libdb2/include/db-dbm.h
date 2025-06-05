#ifndef _DBM_H_
#define	_DBM_H_

#include "db.h"

#define dbminit		kdb2_dbminit
#define fetch		kdb2_fetch
#define firstkey	kdb2_firstkey
#define nextkey		kdb2_nextkey
#define delete		kdb2_delete
#define store		kdb2_store

__BEGIN_DECLS
int	 dbminit __P((char *));
datum	 fetch __P((datum));
datum	 firstkey __P((void));
datum	 nextkey __P((datum));
int	 delete __P((datum));
int	 store __P((datum, datum));
__END_DECLS


#endif
