#! /usr/bin/env python

"""
Error number definitions for Linux.
"""

EPERM =                 1
ENOENT =                2
ESRCH =                 3
EINTR =                 4
EIO =                   5
ENXIO =                 6
E2BIG =                 7
ENOEXEC =               8
EBADF =                 9
ECHILD =                10
EAGAIN =                11
ENOMEM =                12
EACCES =                13
EFAULT =                14
ENOTBLK =               15
EBUSY =                 16
EEXIST =                17
EXDEV =                 18
ENODEV =                19
ENOTDIR =               20
EISDIR =                21
EINVAL =                22
ENFILE =                23
EMFILE =                24
ENOTTY =                25
ETXTBSY =               26
EFBIG =                 27
ENOSPC =                28
ESPIPE =                29
EROFS =                 30
EMLINK =                31
EPIPE =                 32
EDOM =                  33
ERANGE =                34
EDEADLK =               35
ENAMETOOLONG =          36
ENOLCK =                37
ENOSYS =                38
ENOTEMPTY =             39
ELOOP =                 40
#                       41 unused
ENOMSG =                42
EIDRM =                 43
ECHRNG =                44
EL2NSYNC =              45
EL3HLT =                46
EL3RST =                47
ELNRNG =                48
EUNATCH =               49
ENOCSI =                50
EL2HLT =                51
EBADE =                 52
EBADR =                 53
EXFULL =                54
ENOANO =                55
EBADRQC =               56
EBADSLT =               57
#                       58 unused
EBFONT =                59
ENOSTR =                60
ENODATA =               61
ETIME =                 62
ENOSR =                 63
ENONET =                64
ENOPKG =                65
EREMOTE =               66
ENOLINK =               67
EADV =                  68
ESRMNT =                69
ECOMM =                 70
EPROTO =                71
EMULTIHOP =             72
EDOTDOT =               73
EBADMSG =               74
EOVERFLOW =             75
ENOTUNIQ =              76
EBADFD =                77
EREMCHG =               78
ELIBACC =               79
ELIBBAD =               80
ELIBSCN =               81
ELIBMAX =               82
ELIBEXEC =              83
EILSEQ =                84
ERESTART =              85
ESTRPIPE =              86
EUSERS =                87
ENOTSOCK =              88
EDESTADDRREQ =          89
EMSGSIZE =              90
EPROTOTYPE =            91
ENOPROTOOPT =           92
EPROTONOSUPPORT =       93
ESOCKTNOSUPPORT =       94
EOPNOTSUPP =            95
EPFNOSUPPORT =          96
EAFNOSUPPORT =          97
EADDRINUSE =            98
EADDRNOTAVAIL =         99
ENETDOWN =              100
ENETUNREACH =           101
ENETRESET =             102
ECONNABORTED =          103
ECONNRESET =            104
ENOBUFS =               105
EISCONN =               106
ENOTCONN =              107
ESHUTDOWN =             108
ETOOMANYREFS =          109
ETIMEDOUT =             110
ECONNREFUSED =          111
EHOSTDOWN =             112
EHOSTUNREACH =          113
EALREADY =              114
EINPROGRESS =           115
ESTALE =                116
EUCLEAN =               117
ENOTNAM =               118
ENAVAIL =               119
EISNAM =                120
EREMOTEIO =             121
EDQUOT =                122
ENOMEDIUM =             123
EMEDIUMTYPE =           124
ECANCELED =             125
ENOKEY =                126
EKEYEXPIRED =           127
EKEYREVOKED =           128
EKEYREJECTED =          129
EOWNERDEAD =            130
ENOTRECOVERABLE =       131
ERFKILL =               132
EHWPOISON =             133

_strerror = {
    EPERM:              'Permission denied',
    ENOENT:             'No such file or directory',
    ESRCH:              'No such process',
    EINTR:              'Interrupted system call',
    EIO:                'Input/output error',
    ENXIO:              'Device not configured',
    E2BIG:              'Argument list too long',
    ENOEXEC:            'Exec format error',
    EBADF:              'Bad file descriptor',
    ECHILD:             'No child processes',
    EAGAIN:             'Resource temporarily unavailable',
    ENOMEM:             'Cannot allocate memory',
    EACCES:             'Permission denied',
    EFAULT:             'Bad address',
    ENOTBLK:            'Block device required',
    EBUSY:              'Device busy',
    EEXIST:             'File exists',
    EXDEV:              'Cross-device link',
    ENODEV:             'Operation not supported by device',
    ENOTDIR:            'Not a directory',
    EISDIR:             'Is a directory',
    EINVAL:             'Invalid argument',
    ENFILE:             'Too many open files in system',
    EMFILE:             'Too many open files',
    ENOTTY:             'Inappropriate ioctl for device',
    ETXTBSY:            'Text file busy',
    EFBIG:              'File too large',
    ENOSPC:             'No space left on device',
    ESPIPE:             'Illegal seek',
    EROFS:              'Read-only filesystem',
    EMLINK:             'Too many links',
    EPIPE:              'Broken pipe',
    EDOM:               'Numerical argument out of domain',
    ERANGE:             'Result too large',
    EDEADLK:            'Resource deadlock avoided',
    ENAMETOOLONG:       'File name too long',
    ENOLCK:             'No locks available',
    ENOSYS:             'Function not implemented',
    ENOTEMPTY:          'Directory not empty',
    ELOOP:              'Too many levels of symbolic links',
    ENOMSG:             'No message of desired type',
    EIDRM:              'Identifier removed',
    ECHRNG:             'Channel number out of range',
    EL2NSYNC:           'Level 2 not synchronized',
    EL3HLT:             'Level 3 halted',
    EL3RST:             'Level 3 reset',
    ELNRNG:             'Link number out of range',
    EUNATCH:            'Protocol driver not attached',
    ENOCSI:             'No CSI structure available',
    EL2HLT:             'Level 2 halted',
    EBADE:              'Invalid exchange',
    EBADR:              'Invalid request descriptor',
    EXFULL:             'Exchange full',
    ENOANO:             'No anode',
    EBADRQC:            'Invalid request code',
    EBADSLT:            'Invalid slot',
    EBFONT:             'Bad font file format',
    ENOSTR:             'Device not a stream',
    ENODATA:            'No data available',
    ETIME:              'Timer expired',
    ENOSR:              'Out of streams resources',
    ENONET:             'Machine is not on the network',
    ENOPKG:             'Package not installed',
    EREMOTE:            'Object is remote',
    ENOLINK:            'Link has been severed',
    EADV:               'Advertise error',
    ESRMNT:             'Srmount error',
    ECOMM:              'Communication error on send',
    EPROTO:             'Protocol error',
    EMULTIHOP:          'Multihop attempted',
    EDOTDOT:            'RFS specific error',
    EBADMSG:            'Bad message',
    EOVERFLOW:          'Value too large for defined data type',
    ENOTUNIQ:           'Name not unique on network',
    EBADFD:             'File descriptor in bad state',
    EREMCHG:            'Remote address changed',
    ELIBACC:            'Can not access a needed shared library',
    ELIBBAD:            'Accessing a corrupted shared library',
    ELIBSCN:            '.lib section in a.out corrupted',
    ELIBMAX:            'Attempting to link in too many shared libraries',
    ELIBEXEC:           'Cannot exec a shared library directly',
    EILSEQ:             'Invalid or incomplete multibyte or wide character',
    ERESTART:           'Interrupted system call should be restarted',
    ESTRPIPE:           'Streams pipe error',
    EUSERS:             'Too many users',
    ENOTSOCK:           'Socket operation on non-socket',
    EDESTADDRREQ:       'Destination address required',
    EMSGSIZE:           'Message too long',
    EPROTOTYPE:         'Protocol wrong type for socket',
    ENOPROTOOPT:        'Protocol not available',
    EPROTONOSUPPORT:    'Protocol not supported',
    ESOCKTNOSUPPORT:    'Socket type not supported',
    EOPNOTSUPP:         'Operation not supported',
    EPFNOSUPPORT:       'Protocol family not supported',
    EAFNOSUPPORT:       'Address family not supported by protocol',
    EADDRINUSE:         'Address already in use',
    EADDRNOTAVAIL:      'Cannot assign requested address',
    ENETDOWN:           'Network is down',
    ENETUNREACH:        'Network is unreachable',
    ENETRESET:          'Network dropped connection on reset',
    ECONNABORTED:       'Software caused connection abort',
    ECONNRESET:         'Connection reset by peer',
    ENOBUFS:            'No buffer space available',
    EISCONN:            'Transport endpoint is already connected',
    ENOTCONN:           'Transport endpoint is not connected',
    ESHUTDOWN:          'Cannot send after transport endpoint shutdown',
    ETOOMANYREFS:       'Too many references: cannot splice',
    ETIMEDOUT:          'Connection timed out',
    ECONNREFUSED:       'Connection refused',
    EHOSTDOWN:          'Host is down',
    EHOSTUNREACH:       'No route to host',
    EALREADY:           'Operation already in progress',
    EINPROGRESS:        'Operation now in progress',
    ESTALE:             'Stale file handle',
    EUCLEAN:            'Structure needs cleaning',
    ENOTNAM:            'Not a XENIX named type file',
    ENAVAIL:            'No XENIX semaphores available',
    EISNAM:             'Is a named type file',
    EREMOTEIO:          'Remote I/O error',
    EDQUOT:             'Quota exceeded',
    ENOMEDIUM:          'No medium found',
    EMEDIUMTYPE:        'Wrong medium type',
    ECANCELED:          'Operation canceled',
    ENOKEY:             'Required key not available',
    EKEYEXPIRED:        'Key has expired',
    EKEYREVOKED:        'Key has been revoked',
    EKEYREJECTED:       'Key was rejected by service',
    EOWNERDEAD:         'Owner died',
    ENOTRECOVERABLE:    'State not recoverable',
    ERFKILL:            'Operation not possible due to RF-kill',
    EHWPOISON:          'Memory page has hardware error',
}

def strerror(errnum):
    """
    Translate Linux errno to string.

    >>> strerror(ENOKEY)
    'Required key not available'
    >>> strerror(41)
    'Unknown error 41'
    """
    ret = _strerror.get(errnum)
    if ret:
        return ret
    return 'Unknown error {0}'.format(errnum)

if __name__ == '__main__':
    import doctest
    doctest.testmod()
