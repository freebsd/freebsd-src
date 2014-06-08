# encoding: utf-8

import os
import stat
import glob

from evdev import ecodes
from evdev.events import event_factory


def list_devices(input_device_dir='/dev/input'):
    '''List readable, character devices.'''

    fns = glob.glob('{}/event*'.format(input_device_dir))
    fns = list(filter(is_device, fns))

    return fns


def is_device(fn):
    '''Check if ``fn`` is a readable and writable character device.'''

    if not os.path.exists(fn):
        return False

    m = os.stat(fn)[stat.ST_MODE]
    if not stat.S_ISCHR(m):
        return False

    if not os.access(fn, os.R_OK | os.W_OK):
        return False

    return True


def categorize(event):
    '''
    Categorize an event according to its type.

    The :data:`event_factory <evdev.events.event_factory>` dictionary
    maps event types to sub-classes of :class:`InputEvent
    <evdev.events.InputEvent>`. If there is no corresponding key, the
    event is returned as it is.
    '''

    if event.type in event_factory:
        return event_factory[event.type](event)
    else:
        return event


def resolve_ecodes(typecodemap, unknown='?'):
    '''
    Resolve event codes and types to their verbose names.

    :param typecodemap: mapping of event types to lists of event codes.
    :param unknown: symbol to which unknown types or codes will be resolved.

    Example::

        resolve_ecodes({ 1: [272, 273, 274] })
        { ('EV_KEY', 1): [('BTN_MOUSE',  272),
                          ('BTN_RIGHT',  273),
                          ('BTN_MIDDLE', 274)] }

    If typecodemap contains absolute axis info (instances of
    :class:`AbsInfo <evdev.device.AbsInfo>` ) the result would look
    like::

        resolve_ecodes({ 3: [(0, AbsInfo(...))] })
        { ('EV_ABS', 3L): [(('ABS_X', 0L), AbsInfo(...))] }
    '''

    for etype, codes in typecodemap.items():
        type_name = ecodes.EV[etype]

        # ecodes.keys are a combination of KEY_ and BTN_ codes
        if etype == ecodes.EV_KEY:
            code_names = ecodes.keys
        else:
            code_names = getattr(ecodes, type_name.split('_')[-1])

        res = []
        for i in codes:
            # elements with AbsInfo(), eg { 3 : [(0, AbsInfo(...)), (1, AbsInfo(...))] }
            if isinstance(i, tuple):
                l = ((code_names[i[0]], i[0]), i[1]) if i[0] in code_names \
                    else ((unknown, i[0]), i[1])

            # just ecodes { 0 : [0, 1, 3], 1 : [30, 48] }
            else:
                l = (code_names[i], i) if i in code_names else (unknown, i)

            res.append(l)

        yield (type_name, etype), res


__all__ = ('list_devices', 'is_device', 'categorize', 'resolve_ecodes')
