
#ifndef _MACHINE_IEEEFP_H_
#define	_MACHINE_IEEEFP_H_

/* TODO */
typedef int fp_except_t;

__BEGIN_DECLS
extern fp_except_t fpgetmask(void);
extern fp_except_t fpsetmask(fp_except_t);
__END_DECLS

#endif /* _MACHINE_IEEEFP_H_ */
