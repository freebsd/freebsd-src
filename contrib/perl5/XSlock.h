#ifndef __XSlock_h__
#define __XSlock_h__

class XSLockManager
{
public:
	XSLockManager() { InitializeCriticalSection(&cs); };
	~XSLockManager() { DeleteCriticalSection(&cs); };
	void Enter(void) { EnterCriticalSection(&cs); };
	void Leave(void) { LeaveCriticalSection(&cs); };
protected:
	CRITICAL_SECTION cs;
};

XSLockManager g_XSLock;
CPerlObj* pPerl;

class XSLock
{
public:
	XSLock(CPerlObj *p) {
	    g_XSLock.Enter();
	    ::pPerl = p;
	};
	~XSLock() { g_XSLock.Leave(); };
};

/* PERL_CAPI does its own locking in xs_handler() */
#if defined(PERL_OBJECT) && !defined(PERL_CAPI)
#undef dXSARGS
#define dXSARGS	\
	XSLock localLock(pPerl);			\
	dSP; dMARK;					\
	I32 ax = mark - PL_stack_base + 1;		\
	I32 items = sp - mark
#endif	/* PERL_OBJECT && !PERL_CAPI */

#endif
