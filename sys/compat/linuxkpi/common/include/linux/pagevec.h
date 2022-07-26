/* Public domain. */

#ifndef	_LINUXKPI_LINUX_PAGEVEC_H_
#define	_LINUXKPI_LINUX_PAGEVEC_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <linux/pagemap.h>

#define PAGEVEC_SIZE 15

struct pagevec {
	uint8_t	nr;
	struct vm_page *pages[PAGEVEC_SIZE];
};

static inline unsigned int
pagevec_space(struct pagevec *pvec)
{
	return PAGEVEC_SIZE - pvec->nr;
}

static inline void
pagevec_init(struct pagevec *pvec)
{
	pvec->nr = 0;
}

static inline void
pagevec_reinit(struct pagevec *pvec)
{
	pvec->nr = 0;
}

static inline unsigned int
pagevec_count(struct pagevec *pvec)
{
	return pvec->nr;
}

static inline unsigned int
pagevec_add(struct pagevec *pvec, struct vm_page *page)
{
	pvec->pages[pvec->nr++] = page;
	return PAGEVEC_SIZE - pvec->nr;
}

static inline void
__pagevec_release(struct pagevec *pvec)
{
	release_pages(pvec->pages, pagevec_count(pvec));
	pagevec_reinit(pvec);
}

static inline void
pagevec_release(struct pagevec *pvec)
{
	if (pagevec_count(pvec))
		__pagevec_release(pvec);
}

static inline void
check_move_unevictable_pages(struct pagevec *pvec)
{
}

#endif	/* _LINUXKPI_LINUX_PAGEVEC_H_ */
