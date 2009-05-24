/* $FreeBSD$ */

static void
filemon_filemon_lock(struct filemon *filemon)
{
	mtx_lock(&filemon->mtx);

	while (filemon->locker != NULL && filemon->locker != curthread)
		cv_wait(&filemon->cv, &filemon->mtx);

	filemon->locker = curthread;

	mtx_unlock(&filemon->mtx);
}

static void
filemon_filemon_unlock(struct filemon *filemon)
{
	mtx_lock(&filemon->mtx);

	if (filemon->locker == curthread)
		filemon->locker = NULL;

	/* Wake up threads waiting. */
	cv_broadcast(&filemon->cv);

	mtx_unlock(&filemon->mtx);
}

static void
filemon_lock_read(void)
{
	mtx_lock(&access_mtx);

	while (access_owner != NULL || access_requester != NULL)
		cv_wait(&access_cv, &access_mtx);

	n_readers++;

	/* Wake up threads waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}

static void
filemon_unlock_read(void)
{
	mtx_lock(&access_mtx);

	if (n_readers > 0)
		n_readers--;

	/* Wake up a thread waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}

static void
filemon_lock_write(void)
{
	mtx_lock(&access_mtx);

	while (access_owner != curthread) {
		if (access_owner == NULL &&
			(access_requester == NULL || access_requester == curthread)) {
			access_owner = curthread;
			access_requester = NULL;
		} else {
			if (access_requester == NULL)
				access_requester = curthread;

			cv_wait(&access_cv, &access_mtx);
		}
	}

	mtx_unlock(&access_mtx);
}

static void
filemon_unlock_write(void)
{
	mtx_lock(&access_mtx);

	/* Sanity check that the current thread actually has the write lock. */
	if (access_owner == curthread)
		access_owner = NULL;

	/* Wake up a thread waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}
