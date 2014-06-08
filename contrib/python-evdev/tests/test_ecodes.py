# encoding: utf-8

from evdev import ecodes

prefixes = 'KEY ABS REL SW MSC LED BTN REP SND ID EV BUS SYN FF_STATUS FF'

def to_tuples(l):
    t = lambda x: tuple(x) if isinstance(x, list) else x
    return map(t, l)

def test_equality():
    keys = []
    for i in prefixes.split():
        keys.extend(getattr(ecodes, i, {}).keys())

    assert set(keys) == set(ecodes.ecodes.values())

def test_access():
    assert ecodes.KEY_A == ecodes.ecodes['KEY_A'] == ecodes.KEY_A
    assert ecodes.KEY[ecodes.ecodes['KEY_A']] == 'KEY_A'
    assert ecodes.REL[0] == 'REL_X'

def test_overlap():
    vals_ff = set(to_tuples(ecodes.FF.values()))
    vals_ff_status = set(to_tuples(ecodes.FF_STATUS.values()))
    assert bool(vals_ff & vals_ff_status) == False
