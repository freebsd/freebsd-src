#include <sys/param.h>
#include <support/mrlock.h>

void
_sx_xfs_destroy(struct sx *sx)
{
	if (sx->sx_cnt == -1)
		sx_xunlock(sx);
	sx_destroy(sx);
}

void
_sx_xfs_lock(struct sx *sx, int type, const char *file, int line)
{
	if (type == MR_ACCESS)
		_sx_slock(sx, file, line);
	else if (type == MR_UPDATE)
		_sx_sunlock(sx, file, line);
	else
		panic("Invalid lock type passed");
}


void
_sx_xfs_unlock(struct sx *sx, const char *file, int line)
{
	if (_sx_xfs_xowned(sx))
		_sx_xunlock(sx, file, line);
	else if (_sx_xfs_sowned(sx))
		_sx_sunlock(sx, file, line);
	else
		panic("lock is not locked");
}

int
ismrlocked(mrlock_t *mrp, int type)
{	
	if (type == MR_ACCESS)
		return _sx_xfs_sowned(mrp); /* Read lock */
	else if (type == MR_UPDATE)
		return _sx_xfs_xowned(mrp); /* Write lock */
	else if (type == (MR_UPDATE | MR_ACCESS))
		return  _sx_xfs_sowned(mrp) ||
		        _sx_xfs_xowned(mrp); /* Any type of lock held */
	return (mrp->sx_shrd_wcnt > 0 || mrp->sx_excl_wcnt > 0);
}



