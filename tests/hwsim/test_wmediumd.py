# wmediumd validity checks
# Copyright (c) 2015, Intel Deutschland GmbH
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import tempfile, os, subprocess, errno, hwsim_utils, time
from utils import HwsimSkip
from wpasupplicant import WpaSupplicant
from tshark import run_tshark
from test_ap_open import _test_ap_open
from test_scan import test_scan_only_one as _test_scan_only_one
from test_wpas_mesh import check_mesh_support, check_mesh_group_added
from test_wpas_mesh import check_mesh_peer_connected, add_open_mesh_network
from test_wpas_mesh import check_mesh_group_removed

class LocalVariables:
    revs = []

CFG = """
ifaces :
{
    ids = ["%s", "%s"]
    links = (
        (0, 1, 30)
    )
}
"""

CFG2 = """
ifaces :
{
    ids = ["%s", "%s", "%s"]
}

model:
{
    type = "prob"

    links = (
        (0, 1, 0.000000),
        (0, 2, 0.000000),
        (1, 2, 1.000000)
    )
}
"""

CFG3 = """
ifaces :
{
    ids = ["%s", "%s", "%s", "%s", "%s"]
}

model:
{
    type = "prob"

    default_prob = 1.0
    links = (
        (0, 1, 0.000000),
        (1, 2, 0.000000),
        (2, 3, 0.000000),
        (3, 4, 0.000000)
    )
}
"""

def get_wmediumd_version():
    if len(LocalVariables.revs) > 0:
        return LocalVariables.revs

    try:
        verstr = subprocess.check_output(['wmediumd', '-V']).decode()
    except OSError as e:
        if e.errno == errno.ENOENT:
            raise HwsimSkip('wmediumd not available')
        raise

    vernum = verstr.split(' ')[1][1:]
    LocalVariables.revs = vernum.split('.')
    for i in range(0, len(LocalVariables.revs)):
        LocalVariables.revs[i] = int(LocalVariables.revs[i])
    while len(LocalVariables.revs) < 3:
        LocalVariables.revs += [0]

    return LocalVariables.revs

def require_wmediumd_version(major, minor, patch):
    revs = get_wmediumd_version()
    if revs[0] < major or revs[1] < minor or revs[2] < patch:
        raise HwsimSkip('wmediumd v%s.%s.%s is too old for this test' %
                        (revs[0], revs[1], revs[2]))

def output_wmediumd_log(p, params, data):
    log_file = open(os.path.abspath(os.path.join(params['logdir'],
                                                 'wmediumd.log')), 'a')
    log_file.write(data)
    log_file.close()

def start_wmediumd(fn, params):
    try:
        p = subprocess.Popen(['wmediumd', '-c', fn],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    except OSError as e:
        if e.errno == errno.ENOENT:
            raise HwsimSkip('wmediumd not available')
        raise

    logs = ''
    while True:
        line = p.stdout.readline().decode()
        if not line:
            output_wmediumd_log(p, params, logs)
            raise Exception('wmediumd was terminated unexpectedly')
        if line.find('REGISTER SENT!') > -1:
            break
        logs += line
    return p

def stop_wmediumd(p, params):
    p.terminate()
    p.wait()
    stdoutdata, stderrdata = p.communicate()
    output_wmediumd_log(p, params, stdoutdata.decode())

def test_wmediumd_simple(dev, apdev, params):
    """test a simple wmediumd configuration"""
    fd, fn = tempfile.mkstemp()
    try:
        f = os.fdopen(fd, 'w')
        f.write(CFG % (apdev[0]['bssid'], dev[0].own_addr()))
        f.close()
        p = start_wmediumd(fn, params)
        try:
            _test_ap_open(dev, apdev)
        finally:
            stop_wmediumd(p, params)
        # test that releasing hwsim works correctly
        _test_ap_open(dev, apdev)
    finally:
        os.unlink(fn)

def test_wmediumd_path_simple(dev, apdev, params):
    """test a mesh path"""
    # 0 and 1 is connected
    # 0 and 2 is connected
    # 1 and 2 is not connected
    # 1 --- 0 --- 2
    # |           |
    # +-----X-----+
    # This tests if 1 and 2 can communicate each other via 0.
    require_wmediumd_version(0, 3, 1)
    fd, fn = tempfile.mkstemp()
    try:
        f = os.fdopen(fd, 'w')
        f.write(CFG2 % (dev[0].own_addr(), dev[1].own_addr(),
                        dev[2].own_addr()))
        f.close()
        p = start_wmediumd(fn, params)
        try:
            _test_wmediumd_path_simple(dev, apdev)
        finally:
            stop_wmediumd(p, params)
    finally:
        os.unlink(fn)

def _test_wmediumd_path_simple(dev, apdev):
    for i in range(0, 3):
        check_mesh_support(dev[i])
        add_open_mesh_network(dev[i], freq="2462", basic_rates="60 120 240")

    # Check for mesh joined
    for i in range(0, 3):
        check_mesh_group_added(dev[i])

        state = dev[i].get_status_field("wpa_state")
        if state != "COMPLETED":
            raise Exception("Unexpected wpa_state on dev" + str(i) + ": " + state)

        mode = dev[i].get_status_field("mode")
        if mode != "mesh":
            raise Exception("Unexpected mode: " + mode)

    # Check for peer connected
    check_mesh_peer_connected(dev[0])
    check_mesh_peer_connected(dev[0])
    check_mesh_peer_connected(dev[1])
    check_mesh_peer_connected(dev[2])

    # Test connectivity 1->2 and 2->1
    hwsim_utils.test_connectivity(dev[1], dev[2])

    # Check mpath table on 0
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev0")
    if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) == -1 or \
       data.find(dev[2].own_addr() + ' ' +  dev[2].own_addr()) == -1:
        raise Exception("mpath not found on dev0:\n" + data)
    if data.find(dev[0].own_addr()) > -1:
        raise Exception("invalid mpath found on dev0:\n" + data)

    # Check mpath table on 1
    res, data = dev[1].cmd_execute(['iw', dev[1].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev1")
    if data.find(dev[0].own_addr() + ' ' +  dev[0].own_addr()) == -1 or \
       data.find(dev[2].own_addr() + ' ' +  dev[0].own_addr()) == -1:
        raise Exception("mpath not found on dev1:\n" + data)
    if data.find(dev[2].own_addr() + ' ' +  dev[2].own_addr()) > -1 or \
       data.find(dev[1].own_addr()) > -1:
        raise Exception("invalid mpath found on dev1:\n" + data)

    # Check mpath table on 2
    res, data = dev[2].cmd_execute(['iw', dev[2].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev2")
    if data.find(dev[0].own_addr() + ' ' +  dev[0].own_addr()) == -1 or \
       data.find(dev[1].own_addr() + ' ' +  dev[0].own_addr()) == -1:
        raise Exception("mpath not found on dev2:\n" + data)
    if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) > -1 or \
       data.find(dev[2].own_addr()) > -1:
        raise Exception("invalid mpath found on dev2:\n" + data)

    # remove mesh groups
    for i in range(0, 3):
        dev[i].mesh_group_remove()
        check_mesh_group_removed(dev[i])
        dev[i].dump_monitor()

def test_wmediumd_path_ttl(dev, apdev, params):
    """Mesh path request TTL"""
    # 0 --- 1 --- 2 --- 3 --- 4
    # Test the TTL of mesh path request.
    # If the TTL is shorter than path, the mesh path request should be dropped.
    require_wmediumd_version(0, 3, 1)

    local_dev = []
    for i in range(0, 3):
        local_dev.append(dev[i])

    for i in range(5, 7):
        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add("wlan" + str(i))
        check_mesh_support(wpas)
        temp_dev = wpas.request("MESH_INTERFACE_ADD ifname=mesh" + str(i))
        if "FAIL" in temp_dev:
            raise Exception("MESH_INTERFACE_ADD failed")
        local_dev.append(WpaSupplicant(ifname=temp_dev))

    fd, fn = tempfile.mkstemp()
    try:
        f = os.fdopen(fd, 'w')
        f.write(CFG3 % (local_dev[0].own_addr(), local_dev[1].own_addr(),
                        local_dev[2].own_addr(), local_dev[3].own_addr(),
                        local_dev[4].own_addr()))
        f.close()
        p = start_wmediumd(fn, params)
        try:
            _test_wmediumd_path_ttl(local_dev, True)
            _test_wmediumd_path_ttl(local_dev, False)
        finally:
            stop_wmediumd(p, params)
    finally:
        os.unlink(fn)
        for i in range(5, 7):
            wpas.interface_remove("wlan" + str(i))

def _test_wmediumd_path_ttl(dev, ok):
    for i in range(0, 5):
        check_mesh_support(dev[i])
        add_open_mesh_network(dev[i], freq="2462", basic_rates="60 120 240")

    # Check for mesh joined
    for i in range(0, 5):
        check_mesh_group_added(dev[i])

        state = dev[i].get_status_field("wpa_state")
        if state != "COMPLETED":
            raise Exception("Unexpected wpa_state on dev" + str(i) + ": " + state)

        mode = dev[i].get_status_field("mode")
        if mode != "mesh":
            raise Exception("Unexpected mode: " + mode)

    # set mesh path request ttl
    subprocess.check_call(["iw", "dev", dev[0].ifname, "set", "mesh_param",
                           "mesh_element_ttl=" + ("4" if ok else "3")])

    # Check for peer connected
    for i in range(0, 5):
        check_mesh_peer_connected(dev[i])
    for i in range(1, 4):
        check_mesh_peer_connected(dev[i])

    # Test connectivity 0->4 and 0->4
    hwsim_utils.test_connectivity(dev[0], dev[4], success_expected=ok)

    # Check mpath table on 0
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev0")
    if ok:
        if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) == -1 or \
           data.find(dev[4].own_addr() + ' ' +  dev[1].own_addr()) == -1:
            raise Exception("mpath not found on dev0:\n" + data)
    else:
        if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) == -1 or \
           data.find(dev[4].own_addr() + ' 00:00:00:00:00:00') == -1:
            raise Exception("mpath not found on dev0:\n" + data)
    if data.find(dev[0].own_addr()) > -1 or \
       data.find(dev[2].own_addr()) > -1 or \
       data.find(dev[3].own_addr()) > -1:
        raise Exception("invalid mpath found on dev0:\n" + data)

    # remove mesh groups
    for i in range(0, 3):
        dev[i].mesh_group_remove()
        check_mesh_group_removed(dev[i])
        dev[i].dump_monitor()

def test_wmediumd_path_rann(dev, apdev, params):
    """Mesh path with RANN"""
    # 0 and 1 is connected
    # 0 and 2 is connected
    # 1 and 2 is not connected
    # 2 is mesh root and RANN enabled
    # 1 --- 0 --- 2
    # |           |
    # +-----X-----+
    # This tests if 1 and 2 can communicate each other via 0.
    require_wmediumd_version(0, 3, 1)
    fd, fn = tempfile.mkstemp()
    try:
        f = os.fdopen(fd, 'w')
        f.write(CFG2 % (dev[0].own_addr(), dev[1].own_addr(),
                        dev[2].own_addr()))
        f.close()
        p = start_wmediumd(fn, params)
        try:
            _test_wmediumd_path_rann(dev, apdev)
        finally:
            stop_wmediumd(p, params)
    finally:
        os.unlink(fn)

    capfile = os.path.join(params['logdir'], "hwsim0.pcapng")

    # check Root STA address in root announcement element
    filt = "wlan.fc.type_subtype == 0x000d && " + \
           "wlan_mgt.fixed.mesh_action == 0x01 && " + \
           "wlan_mgt.tag.number == 126"
    out = run_tshark(capfile, filt, ["wlan.rann.root_sta"])
    if out is None:
        raise Exception("No captured data found\n")
    if out.find(dev[2].own_addr()) == -1 or \
       out.find(dev[0].own_addr()) > -1 or \
       out.find(dev[1].own_addr()) > -1:
        raise Exception("RANN should be sent by dev2 only:\n" + out)

    # check RANN interval is in range
    filt = "wlan.sa == 02:00:00:00:02:00 && " + \
           "wlan.fc.type_subtype == 0x000d && " + \
           "wlan_mgt.fixed.mesh_action == 0x01 && " + \
           "wlan_mgt.tag.number == 126"
    out = run_tshark(capfile, filt, ["frame.time_relative"])
    if out is None:
        raise Exception("No captured data found\n")
    lines = out.splitlines()
    prev = float(lines[len(lines) - 1])
    for i in reversed(list(range(1, len(lines) - 1))):
        now = float(lines[i])
        if prev - now < 1.0 or 3.0 < prev - now:
            raise Exception("RANN interval " + str(prev - now) +
                            "(sec) should be close to 2.0(sec)\n")
        prev = now

    # check no one uses broadcast path request
    filt = "wlan.da == ff:ff:ff:ff:ff:ff && " + \
           "wlan.fc.type_subtype == 0x000d && " + \
           "wlan_mgt.fixed.mesh_action == 0x01 && " + \
           "wlan_mgt.tag.number == 130"
    out = run_tshark(capfile, filt, ["wlan.sa", "wlan.da"])
    if out is None:
        raise Exception("No captured data found\n")
    if len(out) > 0:
        raise Exception("invalid broadcast path requests\n" + out)

def _test_wmediumd_path_rann(dev, apdev):
    for i in range(0, 3):
        check_mesh_support(dev[i])
        add_open_mesh_network(dev[i], freq="2462", basic_rates="60 120 240")

    # Check for mesh joined
    for i in range(0, 3):
        check_mesh_group_added(dev[i])

        state = dev[i].get_status_field("wpa_state")
        if state != "COMPLETED":
            raise Exception("Unexpected wpa_state on dev" + str(i) + ": " + state)

        mode = dev[i].get_status_field("mode")
        if mode != "mesh":
            raise Exception("Unexpected mode: " + mode)

    # set node 2 as RANN supported root
    subprocess.check_call(["iw", "dev", dev[0].ifname, "set", "mesh_param",
                          "mesh_hwmp_rootmode=0"])
    subprocess.check_call(["iw", "dev", dev[1].ifname, "set", "mesh_param",
                          "mesh_hwmp_rootmode=0"])
    subprocess.check_call(["iw", "dev", dev[2].ifname, "set", "mesh_param",
                          "mesh_hwmp_rootmode=4"])
    subprocess.check_call(["iw", "dev", dev[2].ifname, "set", "mesh_param",
                          "mesh_hwmp_rann_interval=2000"])

    # Check for peer connected
    check_mesh_peer_connected(dev[0])
    check_mesh_peer_connected(dev[0])
    check_mesh_peer_connected(dev[1])
    check_mesh_peer_connected(dev[2])

    # Wait for RANN frame
    time.sleep(10)

    # Test connectivity 1->2 and 2->1
    hwsim_utils.test_connectivity(dev[1], dev[2])

    # Check mpath table on 0
    res, data = dev[0].cmd_execute(['iw', dev[0].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev0")
    if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) == -1 or \
       data.find(dev[2].own_addr() + ' ' +  dev[2].own_addr()) == -1:
        raise Exception("mpath not found on dev0:\n" + data)
    if data.find(dev[0].own_addr()) > -1:
        raise Exception("invalid mpath found on dev0:\n" + data)

    # Check mpath table on 1
    res, data = dev[1].cmd_execute(['iw', dev[1].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev1")
    if data.find(dev[0].own_addr() + ' ' +  dev[0].own_addr()) == -1 or \
       data.find(dev[2].own_addr() + ' ' +  dev[0].own_addr()) == -1:
        raise Exception("mpath not found on dev1:\n" + data)
    if data.find(dev[2].own_addr() + ' ' +  dev[2].own_addr()) > -1 or \
       data.find(dev[1].own_addr()) > -1:
        raise Exception("invalid mpath found on dev1:\n" + data)

    # Check mpath table on 2
    res, data = dev[2].cmd_execute(['iw', dev[2].ifname, 'mpath', 'dump'])
    if res != 0:
        raise Exception("iw command failed on dev2")
    if data.find(dev[0].own_addr() + ' ' +  dev[0].own_addr()) == -1 or \
       data.find(dev[1].own_addr() + ' ' +  dev[0].own_addr()) == -1:
        raise Exception("mpath not found on dev2:\n" + data)
    if data.find(dev[1].own_addr() + ' ' +  dev[1].own_addr()) > -1 or \
       data.find(dev[2].own_addr()) > -1:
        raise Exception("invalid mpath found on dev2:\n" + data)

    # remove mesh groups
    for i in range(0, 3):
        dev[i].mesh_group_remove()
        check_mesh_group_removed(dev[i])
        dev[i].dump_monitor()

def test_wmediumd_scan_only_one(dev, apdev, params):
    """Test that scanning with a single active AP only returns that one (wmediund)"""
    fd, fn = tempfile.mkstemp()
    try:
        f = os.fdopen(fd, 'w')
        f.write(CFG % (apdev[0]['bssid'], dev[0].own_addr()))
        f.close()
        p = start_wmediumd(fn, params)
        try:
            _test_scan_only_one(dev, apdev)
        finally:
            stop_wmediumd(p, params)
    finally:
        os.unlink(fn)
