/*	$NetBSD$	*/

#include "ipf.h"

#define	EMM_MAGIC	0x9d7adba3

void eMmutex_enter(mtx, file, line)
eMmutex_t *mtx;
char *file;
int line;
{
	if (mtx->eMm_magic != EMM_MAGIC) {
		fprintf(stderr, "%s:eMmutex_enter(%p): bad magic: %#x\n",
			mtx->eMm_owner, mtx, mtx->eMm_magic);
		abort();
	}
	if (mtx->eMm_held != 0) {
		fprintf(stderr, "%s:eMmutex_enter(%p): already locked: %d\n",
			mtx->eMm_owner, mtx, mtx->eMm_held);
		abort();
	}
	mtx->eMm_held++;
	mtx->eMm_heldin = file;
	mtx->eMm_heldat = line;
}


void eMmutex_exit(mtx)
eMmutex_t *mtx;
{
	if (mtx->eMm_magic != EMM_MAGIC) {
		fprintf(stderr, "%s:eMmutex_exit(%p): bad magic: %#x\n",
			mtx->eMm_owner, mtx, mtx->eMm_magic);
		abort();
	}
	if (mtx->eMm_held != 1) {
		fprintf(stderr, "%s:eMmutex_exit(%p): not locked: %d\n",
			mtx->eMm_owner, mtx, mtx->eMm_held);
		abort();
	}
	mtx->eMm_held--;
	mtx->eMm_heldin = NULL;
	mtx->eMm_heldat = 0;
}


void eMmutex_init(mtx, who)
eMmutex_t *mtx;
char *who;
{
	if (mtx->eMm_magic == EMM_MAGIC) {	/* safe bet ? */
		fprintf(stderr,
			"%s:eMmutex_init(%p): already initialised?: %#x\n",
			mtx->eMm_owner, mtx, mtx->eMm_magic);
		abort();
	}
	mtx->eMm_magic = EMM_MAGIC;
	mtx->eMm_held = 0;
	if (who != NULL)
		mtx->eMm_owner = strdup(who);
	else
		mtx->eMm_owner = NULL;
}


void eMmutex_destroy(mtx)
eMmutex_t *mtx;
{
	if (mtx->eMm_magic != EMM_MAGIC) {
		fprintf(stderr, "%s:eMmutex_destroy(%p): bad magic: %#x\n",
			mtx->eMm_owner, mtx, mtx->eMm_magic);
		abort();
	}
	if (mtx->eMm_held != 0) {
		fprintf(stderr, "%s:eMmutex_enter(%p): still locked: %d\n",
			mtx->eMm_owner, mtx, mtx->eMm_held);
		abort();
	}
	memset(mtx, 0xa5, sizeof(*mtx));
}
