/* ccapi/common/win/OldCC/autolock.hxx */
/*
 * Copyright (C) 1998 by Danilo Almeida.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __AUTOLOCK_HXX__
#define __AUTOLOCK_HXX__

#include <windows.h>

class CcOsLock {
    CRITICAL_SECTION cs;
    bool valid;
public:
    CcOsLock()      {InitializeCriticalSection(&cs);   valid = true; }
    ~CcOsLock()     {DeleteCriticalSection(&cs);       valid = false;}
    void lock()     {if (valid) EnterCriticalSection(&cs);}
    void unlock()   {if (valid) LeaveCriticalSection(&cs);}
    bool trylock()  {return valid ? (TryEnterCriticalSection(&cs) ? true : false)
                                  : false; }
};

class CcAutoLock {
    CcOsLock& m_lock;
public:
    static void Start(CcAutoLock*& a, CcOsLock& lock) { a = new CcAutoLock(lock); };
    static void Stop (CcAutoLock*& a) { delete a; a = 0; };
    CcAutoLock(CcOsLock& lock):m_lock(lock) { m_lock.lock(); }
    ~CcAutoLock() { m_lock.unlock(); }
};

class CcAutoTryLock {
    CcOsLock& m_lock;
    bool m_locked;
public:
    CcAutoTryLock(CcOsLock& lock):m_lock(lock) { m_locked = m_lock.trylock(); }
    ~CcAutoTryLock() { if (m_locked) m_lock.unlock(); m_locked = false; }
    bool IsLocked() const { return m_locked; }
};

#endif /* __AUTOLOCK_HXX */
