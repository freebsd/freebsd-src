#ifndef __XFS_SUPPORT_SV_H__
#define __XFS_SUPPORT_SV_H__

#include <sys/condvar.h>

/*
 * Synchronisation variables
 *
 * parameters "pri", "svf" and "rts" are not (yet?) implemented
 *
 */

typedef struct cv sv_t;

#define init_sv(sv,type,name,flag) \
            cv_init(sv, name)
#define sv_init(sv,flag,name) \
            cv_init(sv, name)
#define sv_wait(sv, pri, lock, spl) \
            cv_wait_unlock(sv, lock)
#define sv_signal(sv) \
            cv_signal(sv)
#define sv_broadcast(sv) \
            cv_broadcast(sv)
#define sv_destroy(sv) \
            cv_destroy(sv)

#define SV_FIFO         0x0             /* sv_t is FIFO type */
#define SV_LIFO         0x2             /* sv_t is LIFO type */
#define SV_PRIO         0x4             /* sv_t is PRIO type */
#define SV_KEYED        0x6             /* sv_t is KEYED type */
#define SV_DEFAULT      SV_FIFO

#endif /* __XFS_SUPPORT_SV_H__ */
