#! /usr/bin/env python

from __future__ import print_function

__all__ = ['pfod', 'OrderedDict']

### shameless stealing from namedtuple here

"""
pfod - prefilled OrderedDict

This is basically a hybrid of a class and an OrderedDict,
or, sort of a data-only class.  When an instance of the
class is created, all its fields are set to None if not
initialized.

Because it is an OrderedDict you can add extra fields to an
instance, and they will be in inst.keys().  Because it
behaves in a class-like way, if the keys are 'foo' and 'bar'
you can write print(inst.foo) or inst.bar = 3.  Setting an
attribute that does not currently exist causes a new key
to be added to the instance.
"""

import sys as _sys
from keyword import iskeyword as _iskeyword
from collections import OrderedDict
from collections import deque as _deque

_class_template = '''\
class {typename}(OrderedDict):
    '{typename}({arg_list})'
    __slots__ = ()

    _fields = {field_names!r}

    def __init__(self, *args, **kwargs):
        'Create new instance of {typename}()'
        super({typename}, self).__init__()
        args = _deque(args)
        for field in self._fields:
            if field in kwargs:
                self[field] = kwargs.pop(field)
            elif len(args) > 0:
                self[field] = args.popleft()
            else:
                self[field] = None
        if len(kwargs):
            raise TypeError('unexpected kwargs %s' % kwargs.keys())
        if len(args):
            raise TypeError('unconsumed args %r' % tuple(args))

    def _copy(self):
        'copy to new instance'
        new = {typename}()
        new.update(self)
        return new

    def __getattr__(self, attr):
        if attr in self:
            return self[attr]
        raise AttributeError('%r object has no attribute %r' %
            (self.__class__.__name__, attr))

    def __setattr__(self, attr, val):
        if attr.startswith('_OrderedDict_'):
            super({typename}, self).__setattr__(attr, val)
        else:
            self[attr] = val

    def __repr__(self):
        'Return a nicely formatted representation string'
        return '{typename}({repr_fmt})'.format(**self)
'''

_repr_template = '{name}={{{name}!r}}'

# Workaround for py2k exec-as-statement, vs py3k exec-as-function.
# Since the syntax differs, we have to exec the definition of _exec!
if _sys.version_info[0] < 3:
    # py2k: need a real function.  (There is a way to deal with
    # this without a function if the py2k is new enough, but this
    # works in more cases.)
    exec("""def _exec(string, gdict, ldict):
        "Python 2: exec string in gdict, ldict"
        exec string in gdict, ldict""")
else:
    # py3k: just make an alias for builtin function exec
    exec("_exec = exec")

def pfod(typename, field_names, verbose=False, rename=False):
    """
    Return a new subclass of OrderedDict with named fields.

    Fields are accessible by name.  Note that this means
    that to copy a PFOD you must use _copy() - field names
    may not start with '_' unless they are all numeric.

    When creating an instance of the new class, fields
    that are not initialized are set to None.

    >>> Point = pfod('Point', ['x', 'y'])
    >>> Point.__doc__                   # docstring for the new class
    'Point(x, y)'
    >>> p = Point(11, y=22)             # instantiate with positional args or keywords
    >>> p
    Point(x=11, y=22)
    >>> p['x'] + p['y']                 # indexable
    33
    >>> p.x + p.y                       # fields also accessable by name
    33
    >>> p._copy()
    Point(x=11, y=22)
    >>> p2 = Point()
    >>> p2.extra = 2
    >>> p2
    Point(x=None, y=None)
    >>> p2.extra
    2
    >>> p2['extra']
    2
    """

    # Validate the field names.  At the user's option, either generate an error
    if _sys.version_info[0] >= 3:
        string_type = str
    else:
        string_type = basestring
    # message or automatically replace the field name with a valid name.
    if isinstance(field_names, string_type):
        field_names = field_names.replace(',', ' ').split()
    field_names = list(map(str, field_names))
    typename = str(typename)
    if rename:
        seen = set()
        for index, name in enumerate(field_names):
            if (not all(c.isalnum() or c=='_' for c in name)
                or _iskeyword(name)
                or not name
                or name[0].isdigit()
                or name.startswith('_')
                or name in seen):
                field_names[index] = '_%d' % index
            seen.add(name)
    for name in [typename] + field_names:
        if type(name) != str:
            raise TypeError('Type names and field names must be strings')
        if not all(c.isalnum() or c=='_' for c in name):
            raise ValueError('Type names and field names can only contain '
                             'alphanumeric characters and underscores: %r' % name)
        if _iskeyword(name):
            raise ValueError('Type names and field names cannot be a '
                             'keyword: %r' % name)
        if name[0].isdigit():
            raise ValueError('Type names and field names cannot start with '
                             'a number: %r' % name)
    seen = set()
    for name in field_names:
        if name.startswith('_OrderedDict_'):
            raise ValueError('Field names cannot start with _OrderedDict_: '
                             '%r' % name)
        if name.startswith('_') and not rename:
            raise ValueError('Field names cannot start with an underscore: '
                             '%r' % name)
        if name in seen:
            raise ValueError('Encountered duplicate field name: %r' % name)
        seen.add(name)

    # Fill-in the class template
    class_definition = _class_template.format(
        typename = typename,
        field_names = tuple(field_names),
        arg_list = repr(tuple(field_names)).replace("'", "")[1:-1],
        repr_fmt = ', '.join(_repr_template.format(name=name)
                             for name in field_names),
    )
    if verbose:
        print(class_definition,
            file=verbose if isinstance(verbose, file) else _sys.stdout)

    # Execute the template string in a temporary namespace and support
    # tracing utilities by setting a value for frame.f_globals['__name__']
    namespace = dict(__name__='PFOD%s' % typename,
                     OrderedDict=OrderedDict, _deque=_deque)
    try:
        _exec(class_definition, namespace, namespace)
    except SyntaxError as e:
        raise SyntaxError(e.message + ':\n' + class_definition)
    result = namespace[typename]

    # For pickling to work, the __module__ variable needs to be set to the frame
    # where the named tuple is created.  Bypass this step in environments where
    # sys._getframe is not defined (Jython for example) or sys._getframe is not
    # defined for arguments greater than 0 (IronPython).
    try:
        result.__module__ = _sys._getframe(1).f_globals.get('__name__', '__main__')
    except (AttributeError, ValueError):
        pass

    return result

if __name__ == '__main__':
    import doctest
    doctest.testmod()
