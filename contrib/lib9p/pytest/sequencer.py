#! /usr/bin/env python

from __future__ import print_function

#__all__ = ['EncDec', 'EncDecSimple', 'EncDecTyped', 'EncDecA',
#    'SequenceError', 'Sequencer']

import abc
import struct
import sys

_ProtoStruct = {
    '1': struct.Struct('<B'),
    '2': struct.Struct('<H'),
    '4': struct.Struct('<I'),
    '8': struct.Struct('<Q'),
    '_string_': None,   # handled specially
}
for _i in (1, 2, 4, 8):
    _ProtoStruct[_i] = _ProtoStruct[str(_i)]
del _i

class EncDec(object):
    __metaclass__ = abc.ABCMeta
    """
    Base class for en/de-coders, which are put into sequencers.

    All have a name and arbitrary user-supplied auxiliary data
    (default=None).

    All provide a pack() and unpack().  The pack() function
    returns a "bytes" value.  This is internally implemented as a
    function apack() that returns a list of struct.pack() bytes,
    and pack() just joins them up as needed.

    The pack/unpack functions take a dictionary of variable names
    and values, and a second dictionary for conditionals, but at
    this level conditionals don't apply: they are just being
    passed through.  Variable names do apply to array encoders

    EncDec also provide b2s() and s2b() static methods, which
    convert strings to bytes and vice versa, as reversibly as
    possible (using surrogateescape encoding). In Python2 this is
    a no-op since the string type *is* the bytes type (<type
    'unicode'>) is the unicode-ized string type).

    EncDec also provides b2u() and u2b() to do conversion to/from
    Unicode.

    These are partly for internal use (all strings get converted
    to UTF-8 byte sequences when coding a _string_ type) and partly
    for doctests, where we just want some py2k/py3k compat hacks.
    """
    def __init__(self, name, aux):
        self.name = name
        self.aux = aux

    @staticmethod
    def b2u(byte_sequence):
        "transform bytes to unicode"
        return byte_sequence.decode('utf-8', 'surrogateescape')

    @staticmethod
    def u2b(unicode_sequence):
        "transform unicode to bytes"
        return unicode_sequence.encode('utf-8', 'surrogateescape')

    if sys.version_info[0] >= 3:
        b2s = b2u
        @staticmethod
        def s2b(string):
            "transform string to bytes (leaves raw byte sequence unchanged)"
            if isinstance(string, bytes):
                return string
            return string.encode('utf-8', 'surrogateescape')
    else:
        @staticmethod
        def b2s(byte_sequence):
            "transform bytes to string - no-op in python2.7"
            return byte_sequence
        @staticmethod
        def s2b(string):
            "transform string or unicode to bytes"
            if isinstance(string, unicode):
                return string.encode('utf-8', 'surrogateescape')
            return string

    def pack(self, vdict, cdict, val):
        "encode value <val> into a byte-string"
        return b''.join(self.apack(vdict, cdict, val))

    @abc.abstractmethod
    def apack(self, vdict, cdict, val):
        "encode value <val> into [bytes1, b2, ..., bN]"

    @abc.abstractmethod
    def unpack(self, vdict, cdict, bstring, offset, noerror=False):
        "unpack bytes from <bstring> at <offset>"


class EncDecSimple(EncDec):
    r"""
    Encode/decode a simple (but named) field.  The field is not an
    array, which requires using EncDecA, nor a typed object
    like a qid or stat instance -- those require a Sequence and
    EncDecTyped.

    The format is one of '1'/1, '2'/2, '4'/4, '8'/8, or '_string_'.

    Note: using b2s here is purely a doctest/tetsmod python2/python3
    compat hack.  The output of e.pack is <type 'bytes'>; b2s
    converts it to a string, purely for display purposes.  (It might
    be better to map py2 output to bytes but they just print as a
    string anyway.)  In normal use, you should not call b2s here.

    >>> e = EncDecSimple('eggs', 2)
    >>> e.b2s(e.pack({}, {}, 0))
    '\x00\x00'
    >>> e.b2s(e.pack({}, {}, 256))
    '\x00\x01'

    Values that cannot be packed produce a SequenceError:

    >>> e.pack({}, {}, None)
    Traceback (most recent call last):
        ...
    SequenceError: failed while packing 'eggs'=None
    >>> e.pack({}, {}, -1)
    Traceback (most recent call last):
        ...
    SequenceError: failed while packing 'eggs'=-1

    Unpacking both returns a value, and tells how many bytes it
    used out of the bytestring or byte-array argument.  If there
    are not enough bytes remaining at the starting offset, it
    raises a SequenceError, unless noerror=True (then unset
    values are None)

    >>> e.unpack({}, {}, b'\x00\x01', 0)
    (256, 2)
    >>> e.unpack({}, {}, b'', 0)
    Traceback (most recent call last):
        ...
    SequenceError: out of data while unpacking 'eggs'
    >>> e.unpack({}, {}, b'', 0, noerror=True)
    (None, 2)

    Note that strings can be provided as regular strings, byte
    strings (same as regular strings in py2k), or Unicode strings
    (same as regular strings in py3k).  Unicode strings will be
    converted to UTF-8 before being packed.  Since this leaves
    7-bit characters alone, these examples work in both py2k and
    py3k.  (Note: the UTF-8 encoding of u'\u1234' is
    '\0xe1\0x88\0xb4' or 225, 136, 180. The b2i trick below is
    another py2k vs py3k special case just for doctests: py2k
    tries to display the utf-8 encoded data as a string.)

    >>> e = EncDecSimple('spam', '_string_')
    >>> e.b2s(e.pack({}, {}, 'p3=unicode,p2=bytes'))
    '\x13\x00p3=unicode,p2=bytes'

    >>> e.b2s(e.pack({}, {}, b'bytes'))
    '\x05\x00bytes'

    >>> import sys
    >>> ispy3k = sys.version_info[0] >= 3

    >>> b2i = lambda x: x if ispy3k else ord(x)
    >>> [b2i(x) for x in e.pack({}, {}, u'\u1234')]
    [3, 0, 225, 136, 180]

    The byte length of the utf-8 data cannot exceed 65535 since
    the encoding has the length as a 2-byte field (a la the
    encoding for 'eggs' here).  A too-long string produces
    a SequenceError as well.

    >>> e.pack({}, {}, 16384 * 'spam')
    Traceback (most recent call last):
        ...
    SequenceError: string too long (len=65536) while packing 'spam'

    Unpacking strings produces byte arrays.  (Of course,
    in py2k these are also known as <type 'str'>.)

    >>> unpacked = e.unpack({}, {}, b'\x04\x00data', 0)
    >>> etype = bytes if ispy3k else str
    >>> print(isinstance(unpacked[0], etype))
    True
    >>> e.b2s(unpacked[0])
    'data'
    >>> unpacked[1]
    6

    You may use e.b2s() to conver them to unicode strings in py3k,
    or you may set e.autob2s.  This still only really does
    anything in py3k, since py2k strings *are* bytes, so it's
    really just intended for doctest purposes (see EncDecA):

    >>> e.autob2s = True
    >>> e.unpack({}, {}, b'\x07\x00stringy', 0)
    ('stringy', 9)
    """
    def __init__(self, name, fmt, aux=None):
        super(EncDecSimple, self).__init__(name, aux)
        self.fmt = fmt
        self.struct = _ProtoStruct[fmt]
        self.autob2s = False

    def __repr__(self):
        if self.aux is None:
            return '{0}({1!r}, {2!r})'.format(self.__class__.__name__,
                self.name, self.fmt)
        return '{0}({1!r}, {2!r}, {3!r})'.format(self.__class__.__name__,
            self.name, self.fmt, self.aux)

    __str__ = __repr__

    def apack(self, vdict, cdict, val):
        "encode a value"
        try:
            if self.struct:
                return [self.struct.pack(val)]
            sval = self.s2b(val)
            if len(sval) > 65535:
                raise SequenceError('string too long (len={0:d}) '
                    'while packing {1!r}'.format(len(sval), self.name))
            return [EncDecSimple.string_len.pack(len(sval)), sval]
        # Include AttributeError in case someone tries to, e.g.,
        # pack name=None and self.s2b() tries to use .encode on it.
        except (struct.error, AttributeError):
            raise SequenceError('failed '
                'while packing {0!r}={1!r}'.format(self.name, val))

    def _unpack1(self, via, bstring, offset, noerror):
        "internal function to unpack single item"
        try:
            tup = via.unpack_from(bstring, offset)
        except struct.error as err:
            if 'unpack_from requires a buffer of at least' in str(err):
                if noerror:
                    return None, offset + via.size
                raise SequenceError('out of data '
                    'while unpacking {0!r}'.format(self.name))
            # not clear what to do here if noerror
            raise SequenceError('failed '
                'while unpacking {0!r}'.format(self.name))
        assert len(tup) == 1
        return tup[0], offset + via.size

    def unpack(self, vdict, cdict, bstring, offset, noerror=False):
        "decode a value; return the value and the new offset"
        if self.struct:
            return self._unpack1(self.struct, bstring, offset, noerror)
        slen, offset = self._unpack1(EncDecSimple.string_len, bstring, offset,
            noerror)
        if slen is None:
            return None, offset
        nexto = offset + slen
        if len(bstring) < nexto:
            if noerror:
                val = None
            else:
                raise SequenceError('out of data '
                    'while unpacking {0!r}'.format(self.name))
        else:
            val = bstring[offset:nexto]
            if self.autob2s:
                val = self.b2s(val)
        return val, nexto

# string length: 2 byte unsigned field
EncDecSimple.string_len = _ProtoStruct[2]

class EncDecTyped(EncDec):
    r"""
    EncDec for typed objects (which are build from PFODs, which are
    a sneaky class variant of OrderedDict similar to namedtuple).

    Calling the klass() function with no arguments must create an
    instance with all-None members.

    We also require a Sequencer to pack and unpack the members of
    the underlying pfod.

    >>> qid_s = Sequencer('qid')
    >>> qid_s.append_encdec(None, EncDecSimple('type', 1))
    >>> qid_s.append_encdec(None, EncDecSimple('version', 4))
    >>> qid_s.append_encdec(None, EncDecSimple('path', 8))
    >>> len(qid_s)
    3

    >>> from pfod import pfod
    >>> qid = pfod('qid', ['type', 'version', 'path'])
    >>> len(qid._fields)
    3
    >>> qid_inst = qid(1, 2, 3)
    >>> qid_inst
    qid(type=1, version=2, path=3)

    >>> e = EncDecTyped(qid, 'aqid', qid_s)
    >>> e.b2s(e.pack({}, {}, qid_inst))
    '\x01\x02\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00'
    >>> e.unpack({}, {},
    ... b'\x01\x02\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00', 0)
    (qid(type=1, version=2, path=3), 13)

    If an EncDecTyped instance has a conditional sequencer, note
    that unpacking will leave un-selected items set to None (see
    the Sequencer example below):

    >>> breakfast = pfod('breakfast', 'eggs spam ham')
    >>> breakfast()
    breakfast(eggs=None, spam=None, ham=None)
    >>> bfseq = Sequencer('breakfast')
    >>> bfseq.append_encdec(None, EncDecSimple('eggs', 1))
    >>> bfseq.append_encdec('yuck', EncDecSimple('spam', 1))
    >>> bfseq.append_encdec(None, EncDecSimple('ham', 1))
    >>> e = EncDecTyped(breakfast, 'bfname', bfseq)
    >>> e.unpack({}, {'yuck': False}, b'\x02\x01\x04', 0)
    (breakfast(eggs=2, spam=None, ham=1), 2)

    This used just two of the three bytes: eggs=2, ham=1.

    >>> e.unpack({}, {'yuck': True}, b'\x02\x01\x04', 0)
    (breakfast(eggs=2, spam=1, ham=4), 3)

    This used the third byte, so ham=4.
    """
    def __init__(self, klass, name, sequence, aux=None):
        assert len(sequence) == len(klass()._fields) # temporary
        super(EncDecTyped, self).__init__(name, aux)
        self.klass = klass
        self.name = name
        self.sequence = sequence

    def __repr__(self):
        if self.aux is None:
            return '{0}({1!r}, {2!r}, {3!r})'.format(self.__class__.__name__,
                self.klass, self.name, self.sequence)
        return '{0}({1!r}, {2!r}, {3!r}, {4!r})'.format(self.__class__.__name__,
            self.klass, self.name, self.sequence, self.aux)

    __str__ = __repr__

    def apack(self, vdict, cdict, val):
        """
        Pack each of our instance variables.

        Note that some packing may be conditional.
        """
        return self.sequence.apack(val, cdict)

    def unpack(self, vdict, cdict, bstring, offset, noerror=False):
        """
        Unpack each instance variable, into a new object of
        self.klass.  Return the new instance and new offset.

        Note that some unpacking may be conditional.
        """
        obj = self.klass()
        offset = self.sequence.unpack_from(obj, cdict, bstring, offset, noerror)
        return obj, offset

class EncDecA(EncDec):
    r"""
    EncDec for arrays (repeated objects).

    We take the name of repeat count variable, and a sub-coder
    (Sequencer instance).  For instance, we can en/de-code
    repeat='nwname' copies of name='wname', or nwname of
    name='wqid', in a Twalk en/de-code.

    Note that we don't pack or unpack the repeat count itself --
    that must be done by higher level code.  We just get its value
    from vdict.

    >>> subcode = EncDecSimple('wname', '_string_')
    >>> e = EncDecA('nwname', 'wname', subcode)
    >>> e.b2s(e.pack({'nwname': 2}, {}, ['A', 'BC']))
    '\x01\x00A\x02\x00BC'

    >>> subcode.autob2s = True # so that A and BC decode to py3k str
    >>> e.unpack({'nwname': 2}, {}, b'\x01\x00A\x02\x00BC', 0)
    (['A', 'BC'], 7)

    When using noerror, the first sub-item that fails to decode
    completely starts the None-s.  Strings whose length fails to
    decode are assumed to be zero bytes long as well, for the
    purpose of showing the expected packet length:

    >>> e.unpack({'nwname': 2}, {}, b'\x01\x00A\x02\x00', 0, noerror=True)
    (['A', None], 7)
    >>> e.unpack({'nwname': 2}, {}, b'\x01\x00A\x02', 0, noerror=True)
    (['A', None], 5)
    >>> e.unpack({'nwname': 3}, {}, b'\x01\x00A\x02', 0, noerror=True)
    (['A', None, None], 7)

    As a special case, supplying None for the sub-coder
    makes the repeated item pack or unpack a simple byte
    string.  (Note that autob2s is not supported here.)
    A too-short byte string is simply truncated!

    >>> e = EncDecA('count', 'data', None)
    >>> e.b2s(e.pack({'count': 5}, {}, b'12345'))
    '12345'
    >>> x = list(e.unpack({'count': 3}, {}, b'123', 0))
    >>> x[0] = e.b2s(x[0])
    >>> x
    ['123', 3]
    >>> x = list(e.unpack({'count': 3}, {}, b'12', 0, noerror=True))
    >>> x[0] = e.b2s(x[0])
    >>> x
    ['12', 3]
    """
    def __init__(self, repeat, name, sub, aux=None):
        super(EncDecA, self).__init__(name, aux)
        self.repeat = repeat
        self.name = name
        self.sub = sub

    def __repr__(self):
        if self.aux is None:
            return '{0}({1!r}, {2!r}, {3!r})'.format(self.__class__.__name__,
                self.repeat, self.name, self.sub)
        return '{0}({1!r}, {2!r}, {3!r}, {4!r})'.format(self.__class__.__name__,
            self.repeat, self.name, self.sub, self.aux)

    __str__ = __repr__

    def apack(self, vdict, cdict, val):
        "pack each val[i], for i in range(vdict[self.repeat])"
        num = vdict[self.repeat]
        assert num == len(val)
        if self.sub is None:
            assert isinstance(val, bytes)
            return [val]
        parts = []
        for i in val:
            parts.extend(self.sub.apack(vdict, cdict, i))
        return parts

    def unpack(self, vdict, cdict, bstring, offset, noerror=False):
        "unpack repeatedly, per self.repeat, into new array."
        num = vdict[self.repeat]
        if num is None and noerror:
            num = 0
        else:
            assert num >= 0
        if self.sub is None:
            nexto = offset + num
            if len(bstring) < nexto and not noerror:
                raise SequenceError('out of data '
                    'while unpacking {0!r}'.format(self.name))
            return bstring[offset:nexto], nexto
        array = []
        for i in range(num):
            obj, offset = self.sub.unpack(vdict, cdict, bstring, offset,
                noerror)
            array.append(obj)
        return array, offset

class SequenceError(Exception):
    "sequence error: item too big, or ran out of data"
    pass

class Sequencer(object):
    r"""
    A sequencer is an object that packs (marshals) or unpacks
    (unmarshals) a series of objects, according to their EncDec
    instances.

    The objects themselves (and their values) come from, or
    go into, a dictionary: <vdict>, the first argument to
    pack/unpack.

    Some fields may be conditional.  The conditions are in a
    separate dictionary (the second or <cdict> argument).

    Some objects may be dictionaries or PFODs, e.g., they may
    be a Plan9 qid or stat structure.  These have their own
    sub-encoding.

    As with each encoder, we have both an apack() function
    (returns a list of parts) and a plain pack().  Users should
    mostly stick with plain pack().

    >>> s = Sequencer('monty')
    >>> s
    Sequencer('monty')
    >>> e = EncDecSimple('eggs', 2)
    >>> s.append_encdec(None, e)
    >>> s.append_encdec(None, EncDecSimple('spam', 1))
    >>> s[0]
    (None, EncDecSimple('eggs', 2))
    >>> e.b2s(s.pack({'eggs': 513, 'spam': 65}, {}))
    '\x01\x02A'

    When particular fields are conditional, they appear in
    packed output, or are taken from the byte-string during
    unpacking, only if their condition is true.

    As with struct, use unpack_from to start at an arbitrary
    offset and/or omit verification that the entire byte-string
    is consumed.

    >>> s = Sequencer('python')
    >>> s.append_encdec(None, e)
    >>> s.append_encdec('.u', EncDecSimple('spam', 1))
    >>> s[1]
    ('.u', EncDecSimple('spam', 1))
    >>> e.b2s(s.pack({'eggs': 513, 'spam': 65}, {'.u': True}))
    '\x01\x02A'
    >>> e.b2s(s.pack({'eggs': 513, 'spam': 65}, {'.u': False}))
    '\x01\x02'

    >>> d = {}
    >>> s.unpack(d, {'.u': True}, b'\x01\x02A')
    >>> print(d['eggs'], d['spam'])
    513 65
    >>> d = {}
    >>> s.unpack(d, {'.u': False}, b'\x01\x02A', 0)
    Traceback (most recent call last):
        ...
    SequenceError: 1 byte(s) unconsumed
    >>> s.unpack_from(d, {'.u': False}, b'\x01\x02A', 0)
    2
    >>> print(d)
    {'eggs': 513}

    The incoming dictionary-like object may be pre-initialized
    if you like; only sequences that decode are filled-in:

    >>> d = {'eggs': None, 'spam': None}
    >>> s.unpack_from(d, {'.u': False}, b'\x01\x02A', 0)
    2
    >>> print(d['eggs'], d['spam'])
    513 None

    Some objects may be arrays; if so their EncDec is actually
    an EncDecA, the repeat count must be in the dictionary, and
    the object itself must have a len() and be index-able:

    >>> s = Sequencer('arr')
    >>> s.append_encdec(None, EncDecSimple('n', 1))
    >>> ae = EncDecSimple('array', 2)
    >>> s.append_encdec(None, EncDecA('n', 'array', ae))
    >>> ae.b2s(s.pack({'n': 2, 'array': [257, 514]}, {}))
    '\x02\x01\x01\x02\x02'

    Unpacking an array creates a list of the number of items.
    The EncDec encoder that decodes the number of items needs to
    occur first in the sequencer, so that the dictionary will have
    acquired the repeat-count variable's value by the time we hit
    the array's encdec:

    >>> d = {}
    >>> s.unpack(d, {}, b'\x01\x04\x00')
    >>> d['n'], d['array']
    (1, [4])
    """
    def __init__(self, name):
        self.name = name
        self._codes = []
        self.debug = False # or sys.stderr

    def __repr__(self):
        return '{0}({1!r})'.format(self.__class__.__name__, self.name)

    __str__ = __repr__

    def __len__(self):
        return len(self._codes)

    def __iter__(self):
        return iter(self._codes)

    def __getitem__(self, index):
        return self._codes[index]

    def dprint(self, *args, **kwargs):
        if not self.debug:
            return
        if isinstance(self.debug, bool):
            dest = sys.stdout
        else:
            dest = self.debug
        print(*args, file=dest, **kwargs)

    def append_encdec(self, cond, code):
        "add EncDec en/de-coder, conditional on cond"
        self._codes.append((cond, code))

    def apack(self, vdict, cdict):
        """
        Produce packed representation of each field.
        """
        packed_data = []
        for cond, code in self._codes:
            # Skip this item if it's conditional on a false thing.
            if cond is not None and not cdict[cond]:
                self.dprint('skip %r - %r is False' % (code, cond))
                continue

            # Pack the item.
            self.dprint('pack %r - no cond or %r is True' % (code, cond))
            packed_data.extend(code.apack(vdict, cdict, vdict[code.name]))

        return packed_data

    def pack(self, vdict, cdict):
        """
        Flatten packed data.
        """
        return b''.join(self.apack(vdict, cdict))

    def unpack_from(self, vdict, cdict, bstring, offset=0, noerror=False):
        """
        Unpack from byte string.

        The values are unpacked into a dictionary vdict;
        some of its entries may themselves be ordered
        dictionaries created by typedefed codes.

        Raises SequenceError if the string is too short,
        unless you set noerror, in which case we assume
        you want see what you can get out of the data.
        """
        for cond, code in self._codes:
            # Skip this item if it's conditional on a false thing.
            if cond is not None and not cdict[cond]:
                self.dprint('skip %r - %r is False' % (code, cond))
                continue

            # Unpack the item.
            self.dprint('unpack %r - no cond or %r is True' % (code, cond))
            obj, offset = code.unpack(vdict, cdict, bstring, offset, noerror)
            vdict[code.name] = obj

        return offset

    def unpack(self, vdict, cdict, bstring, noerror=False):
        """
        Like unpack_from but unless noerror=True, requires that
        we completely use up the given byte string.
        """
        offset = self.unpack_from(vdict, cdict, bstring, 0, noerror)
        if not noerror and offset != len(bstring):
            raise SequenceError('{0} byte(s) unconsumed'.format(
                len(bstring) - offset))

if __name__ == '__main__':
    import doctest
    doctest.testmod()
