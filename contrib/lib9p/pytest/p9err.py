#! /usr/bin/env python

"""
Error number definitions for 9P2000, .u, and .L.

Note that there is no native-to-9P2000 (plain) translation
table since 9P2000 takes error *strings* rather than error
*numbers*.
"""

import errno as _errno
import lerrno as _lerrno
import os as _os

_native_to_dotu = {
    # These are in the "standard" range(1, errno.ERANGE)
    # but do not map to themselves, so map them here first.
    _errno.ENOTEMPTY: _errno.EPERM,
    _errno.EDQUOT: _errno.EPERM,
    _errno.ENOSYS: _errno.EPERM,
}

_native_to_dotl = {}

# Add standard errno's.
for _i in range(1, _errno.ERANGE):
    _native_to_dotu.setdefault(_i, _i)
    _native_to_dotl[_i] = _i

# Add linux errno's.  Note that Linux EAGAIN at #11 overrides BSD EDEADLK,
# but Linux has EDEADLK at #35 which overrides BSD EAGAIN, so it all
# works out.
#
# We just list every BSD error name here, since the hasattr()s do
# the real work.
for _i in (
    'EDEADLK',
    'EAGAIN',
    'EINPROGRESS',
    'EALREADY',
    'ENOTSOCK',
    'EDESTADDRREQ',
    'EMSGSIZE',
    'EPROTOTYPE',
    'ENOPROTOOPT',
    'EPROTONOSUPPORT',
    'ESOCKTNOSUPPORT',
    'EOPNOTSUPP',
    'EPFNOSUPPORT',
    'EAFNOSUPPORT',
    'EADDRINUSE',
    'EADDRNOTAVAIL',
    'ENETDOWN',
    'ENETUNREACH',
    'ENETRESET',
    'ECONNABORTED',
    'ECONNRESET',
    'ENOBUFS',
    'EISCONN',
    'ENOTCONN',
    'ESHUTDOWN',
    'ETOOMANYREFS',
    'ETIMEDOUT',
    'ECONNREFUSED',
    'ELOOP',
    'ENAMETOOLONG',
    'EHOSTDOWN',
    'EHOSTUNREACH',
    'ENOTEMPTY',
    'EPROCLIM',
    'EUSERS',
    'EDQUOT',
    'ESTALE',
    'EREMOTE',
    'EBADRPC',
    'ERPCMISMATCH',
    'EPROGUNAVAIL',
    'EPROGMISMATCH',
    'EPROCUNAVAIL',
    'ENOLCK',
    'ENOSYS',
    'EFTYPE',
    'EAUTH',
    'ENEEDAUTH',
    'EIDRM',
    'ENOMSG',
    'EOVERFLOW',
    'ECANCELED',
    'EILSEQ',
    'EDOOFUS',
    'EBADMSG',
    'EMULTIHOP',
    'ENOLINK',
    'EPROTO',
    'ENOTCAPABLE',
    'ECAPMODE',
    'ENOTRECOVERABLE',
    'EOWNERDEAD',
):
    if hasattr(_errno, _i) and hasattr(_lerrno, _i):
        _native_to_dotl[getattr(_errno, _i)] = getattr(_lerrno, _i)
del _i

def to_dotu(errnum):
    """
    Translate native errno to 9P2000.u errno.

    >>> import errno
    >>> to_dotu(errno.EIO)
    5
    >>> to_dotu(errno.EDQUOT)
    1
    >>> to_dotu(errno.ELOOP)
    5

    There is a corresponding dotu_strerror() (which is really
    just os.strerror):

    >>> dotu_strerror(5)
    'Input/output error'

    """
    return _native_to_dotu.get(errnum, _errno.EIO) # default to EIO

def to_dotl(errnum):
    """
    Translate native errno to 9P2000.L errno.

    >>> import errno
    >>> to_dotl(errno.ELOOP)
    40

    There is a corresponding dotl_strerror():

    >>> dotl_strerror(40)
    'Too many levels of symbolic links'
    """
    return _native_to_dotl.get(errnum, _lerrno.ENOTRECOVERABLE)

dotu_strerror = _os.strerror

dotl_strerror = _lerrno.strerror

if __name__ == '__main__':
    import doctest
    doctest.testmod()
