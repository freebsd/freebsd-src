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

class XSLock
{
public:
	XSLock() { g_XSLock.Enter(); };
	~XSLock() { g_XSLock.Leave(); };
};

CPerlObj* pPerl;

#undef dXSARGS
#define dXSARGS	\
	dSP; dMARK;		\
	I32 ax = mark - PL_stack_base + 1;	\
	I32 items = sp - mark; \
	XSLock localLock; \
	::pPerl = pPerl


#endif
