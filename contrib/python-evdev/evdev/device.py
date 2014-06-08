# encoding: utf-8

import os
from select import select
from collections import namedtuple

from evdev import _input, _uinput, ecodes, util
from evdev.events import InputEvent


_AbsInfo = namedtuple('AbsInfo', ['value', 'min', 'max', 'fuzz', 'flat', 'resolution'])
_KbdInfo = namedtuple('KbdInfo', ['repeat', 'delay'])
_DeviceInfo = namedtuple('DeviceInfo', ['bustype', 'vendor', 'product', 'version'])


class AbsInfo(_AbsInfo):
    '''
    A ``namedtuple`` for storing absolut axis information -
    corresponds to the ``input_absinfo`` struct:

     - value
        Latest reported value for the axis.

     - min
        Specifies minimum value for the axis.

     - max
        Specifies maximum value for the axis.

     - fuzz
        Specifies fuzz value that is used to filter noise from the
        event stream.

     - flat
        Values that are within this value will be discarded by joydev
        interface and reported as 0 instead.

     - resolution
        Specifies resolution for the values reported for the axis.
        Resolution for main axes (``ABS_X, ABS_Y, ABS_Z``) is reported
        in units per millimeter (units/mm), resolution for rotational
        axes (``ABS_RX, ABS_RY, ABS_RZ``) is reported in units per
        radian.

    .. note: The input core does not clamp reported values to the
       ``[minimum, maximum]`` limits, such task is left to userspace.
    '''

    def __str__(self):
        return 'val {}, min {}, max {}, fuzz {}, flat {}, res {}'.format(*self)


class KbdInfo(_KbdInfo):
    '''
    Keyboard repeat rate:

    - repeat:
       Keyboard repeat rate in characters per second.

    - delay:
       Amount of time that a key must be depressed before it will start
       to repeat (in milliseconds).
    '''

    def __str__(self):
        return 'repeat {}, delay {}'.format(*self)


class DeviceInfo(_DeviceInfo):
    def __str__(self):
        msg = 'bus: {:04x}, vendor {:04x}, product {:04x}, version {:04x}'
        return msg.format(*self)


class InputDevice(object):
    '''
    A linux input device from which input events can be read.
    '''

    __slots__ = ('fn', 'fd', 'info', 'name', 'phys', '_rawcapabilities',
                 'version', 'ff_effects_count')

    def __init__(self, dev):
        '''
        :param dev: path to input device
        '''

        #: Path to input device.
        self.fn = dev

        #: A non-blocking file descriptor to the device file.
        self.fd = os.open(dev, os.O_RDWR | os.O_NONBLOCK)

        # Returns (bustype, vendor, product, version, name, phys, capabilities).
        info_res = _input.ioctl_devinfo(self.fd)

        #: A :class:`DeviceInfo <evdev.device.DeviceInfo>` instance.
        self.info = DeviceInfo(*info_res[:4])

        #: The name of the event device.
        self.name = info_res[4]

        #: The physical topology of the device.
        self.phys = info_res[5]

        #: The evdev protocol version.
        self.version = _input.ioctl_EVIOCGVERSION(self.fd)

        #: The raw dictionary of device capabilities - see `:func:capabilities()`.
        self._rawcapabilities = _input.ioctl_capabilities(self.fd)

        #: The number of force feedback effects the device can keep in its memory.
        self.ff_effects_count = _input.ioctl_EVIOCGEFFECTS(self.fd)

    def __del__(self):
        if hasattr(self, 'fd') and self.fd is not None:
            try:
                self.close()
            except OSError:
                pass

    def _capabilities(self, absinfo=True):
        res = {}
        for etype, ecodes in self._rawcapabilities.items():
            for code in ecodes:
                l = res.setdefault(etype, [])
                if isinstance(code, tuple):
                    if absinfo:
                        a = code[1]  # (0, 0, 0, 255, 0, 0)
                        i = AbsInfo(*a)
                        l.append((code[0], i))
                    else:
                        l.append(code[0])
                else:
                    l.append(code)

        return res

    def capabilities(self, verbose=False, absinfo=True):
        '''
        Return the event types that this device supports as a mapping of
        supported event types to lists of handled event codes. Example::

          { 1: [272, 273, 274],
            2: [0, 1, 6, 8] }

        If ``verbose`` is ``True``, event codes and types will be resolved
        to their names. Example::

          { ('EV_KEY', 1): [('BTN_MOUSE', 272),
                            ('BTN_RIGHT', 273),
                            ('BTN_MIDDLE', 273)],
            ('EV_REL', 2): [('REL_X', 0),
                            ('REL_Y', 1),
                            ('REL_HWHEEL', 6),
                            ('REL_WHEEL', 8)] }

        Unknown codes or types will be resolved to ``'?'``.

        If ``absinfo`` is ``True``, the list of capabilities will also
        include absolute axis information in the form of
        :class:`AbsInfo` instances::

          { 3: [ (0, AbsInfo(min=0, max=255, fuzz=0, flat=0)),
                 (1, AbsInfo(min=0, max=255, fuzz=0, flat=0)) ]}

        Combined with ``verbose`` the above becomes::

          { ('EV_ABS', 3): [ (('ABS_X', 0), AbsInfo(min=0, max=255, fuzz=0, flat=0)),
                             (('ABS_Y', 1), AbsInfo(min=0, max=255, fuzz=0, flat=0)) ]}

        '''

        if verbose:
            return dict(util.resolve_ecodes(self._capabilities(absinfo)))
        else:
            return self._capabilities(absinfo)

    def leds(self, verbose=False):
        '''
        Return currently set LED keys. Example::

          [0, 1, 8, 9]

        If ``verbose`` is ``True``, event codes are resolved to
        their names. Unknown codes are resolved to ``'?'``. Example::

          [('LED_NUML', 0), ('LED_CAPSL', 1), ('LED_MISC', 8), ('LED_MAIL', 9)]

        '''
        leds = _input.get_sw_led_snd(self.fd, ecodes.EV_LED)
        if verbose:
            return [(ecodes.LED[l] if l in ecodes.LED else '?', l) for l in leds]

        return leds

    def set_led(self, led_num, value):
        '''
        Set the state of the selected LED. Example::

           device.set_led(ecodes.LED_NUML, 1)

        ..
        '''
        _uinput.write(self.fd, ecodes.EV_LED, led_num, value)

    def __eq__(self, other):
        '''Two devices are considered equal if their :data:`info` attributes are equal.'''
        return self.info == other.info

    def __str__(self):
        msg = 'device {}, name "{}", phys "{}"'
        return msg.format(self.fn, self.name, self.phys)

    def __repr__(self):
        msg = (self.__class__.__name__, self.fn)
        return '{}({!r})'.format(*msg)

    def close(self):
        if self.fd > -1:
            try:
                os.close(self.fd)
            finally:
                self.fd = -1

    def fileno(self):
        '''
        Return the file descriptor to the open event device. This
        makes it possible to pass pass ``InputDevice`` instances
        directly to :func:`select.select()` and
        :class:`asyncore.file_dispatcher`.'''

        return self.fd

    def read_one(self):
        '''
        Read and return a single input event as an instance of
        :class:`InputEvent <evdev.events.InputEvent>`.

        Return ``None`` if there are no pending input events.
        '''

        # event -> (sec, usec, type, code, val)
        event = _input.device_read(self.fd)

        if event:
            return InputEvent(*event)

    def read_loop(self):
        '''Enter an endless ``select()`` loop that yields input events.'''

        while True:
            r, w, x = select([self.fd], [], [])
            for event in self.read():
                yield event

    def read(self):
        '''
        Read multiple input events from device. Return a generator
        object that yields :class:`InputEvent
        <evdev.events.InputEvent>` instances.
        '''

        # events -> [(sec, usec, type, code, val), ...]
        events = _input.device_read_many(self.fd)

        for i in events:
            yield InputEvent(*i)

    def grab(self):
        '''
        Grab input device using ``EVIOCGRAB`` - other applications will
        be unable to receive events until the device is released. Only
        one process can hold a ``EVIOCGRAB`` on a device.

        .. warning:: Grabbing an already grabbed device will raise an
                     ``IOError``.'''

        _input.ioctl_EVIOCGRAB(self.fd, 1)

    def ungrab(self):
        '''Release device if it has been already grabbed (uses
        `EVIOCGRAB`).

        .. warning:: Releasing an already released device will raise an
                     ``IOError('Invalid argument')``.'''

        _input.ioctl_EVIOCGRAB(self.fd, 0)

    def upload_effect(self, effect):
        '''Upload a force feedback effect to a force feedback device.'''

        data = bytes(buffer(effect)[:])
        ff_id = _input.upload_effect(self.fd, data)
        return ff_id

    def erase_effect(self, ff_id):
        '''Erase a force effect from a force feedback device. This
        also stops the effect.'''

        _input.erase_effect(self.fd, ff_id)

    @property
    def repeat(self):
        '''Get or set the keyboard repeat rate (in characters per
        minute) and delay (in milliseconds).'''

        return KbdInfo(*_input.ioctl_EVIOCGREP(self.fd))

    @repeat.setter
    def repeat(self, value):
        return _input.ioctl_EVIOCSREP(self.fd, *value)
