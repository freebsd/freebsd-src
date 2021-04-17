#
# HWSIM generic netlink controller code
# Copyright (c) 2014	Intel Corporation
#
# Author: Johannes Berg <johannes.berg@intel.com>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import netlink, os

# constants
HWSIM_CMD_CREATE_RADIO = 4
HWSIM_CMD_DESTROY_RADIO = 5

HWSIM_ATTR_CHANNELS = 9
HWSIM_ATTR_RADIO_ID = 10
HWSIM_ATTR_SUPPORT_P2P_DEVICE = 14
HWSIM_ATTR_USE_CHANCTX = 15

# the controller class
class HWSimController(object):
    def __init__(self):
        self._conn = netlink.Connection(netlink.NETLINK_GENERIC)
        self._fid = netlink.genl_controller.get_family_id(b'MAC80211_HWSIM')

    def create_radio(self, n_channels=None, use_chanctx=False,
                     use_p2p_device=False):
        attrs = []
        if n_channels:
            attrs.append(netlink.U32Attr(HWSIM_ATTR_CHANNELS, n_channels))
        if use_chanctx:
            attrs.append(netlink.FlagAttr(HWSIM_ATTR_USE_CHANCTX))
        if use_p2p_device:
            attrs.append(netlink.FlagAttr(HWSIM_ATTR_SUPPORT_P2P_DEVICE))

        msg = netlink.GenlMessage(self._fid, HWSIM_CMD_CREATE_RADIO,
                                  flags=netlink.NLM_F_REQUEST |
                                        netlink.NLM_F_ACK,
                                  attrs=attrs)
        return msg.send_and_recv(self._conn).ret

    def destroy_radio(self, radio_id):
        attrs = [netlink.U32Attr(HWSIM_ATTR_RADIO_ID, radio_id)]
        msg = netlink.GenlMessage(self._fid, HWSIM_CMD_DESTROY_RADIO,
                                  flags=netlink.NLM_F_REQUEST |
                                        netlink.NLM_F_ACK,
                                  attrs=attrs)
        msg.send_and_recv(self._conn)

class HWSimRadio(object):
    def __init__(self, n_channels=None, use_chanctx=False,
                 use_p2p_device=False):
        self._controller = HWSimController()
        self._n_channels = n_channels
        self._use_chanctx = use_chanctx
        self._use_p2p_dev = use_p2p_device

    def __enter__(self):
        self._radio_id = self._controller.create_radio(
              n_channels=self._n_channels,
              use_chanctx=self._use_chanctx,
              use_p2p_device=self._use_p2p_dev)
        if self._radio_id < 0:
            raise Exception("Failed to create radio (err:%d)" % self._radio_id)
        try:
            iface = os.listdir('/sys/class/mac80211_hwsim/hwsim%d/net/' % self._radio_id)[0]
        except Exception as e:
            self._controller.destroy_radio(self._radio_id)
            raise e
        return self._radio_id, iface

    def __exit__(self, type, value, traceback):
        self._controller.destroy_radio(self._radio_id)


def create(args):
    print('Created radio %d' % c.create_radio(n_channels=args.channels,
                                              use_chanctx=args.chanctx))

def destroy(args):
    print(c.destroy_radio(args.radio))

if __name__ == '__main__':
    import argparse
    c = HWSimController()

    parser = argparse.ArgumentParser(description='send hwsim control commands')
    subparsers = parser.add_subparsers(help="Commands", dest='command')
    parser_create = subparsers.add_parser('create', help='create a radio')
    parser_create.add_argument('--channels', metavar='<number_of_channels>', type=int,
                               default=0,
                               help='Number of concurrent channels supported ' +
                               'by the radio. If not specified, the number ' +
                               'of channels specified in the ' +
                               'mac80211_hwsim.channels module parameter is ' +
                               'used')
    parser_create.add_argument('--chanctx', action="store_true",
                               help='Use channel contexts, regardless of ' +
                               'whether the number of channels is 1 or ' +
                               'greater. By default channel contexts are ' +
                               'only used if the number of channels is ' +
                               'greater than 1.')
    parser_create.set_defaults(func=create)

    parser_destroy = subparsers.add_parser('destroy', help='destroy a radio')
    parser_destroy.add_argument('radio', metavar='<radio>', type=int,
                                default=0,
                                help='The number of the radio to be ' +
                                'destroyed (i.e., 0 for phy0, 1 for phy1...)')
    parser_destroy.set_defaults(func=destroy)

    args = parser.parse_args()
    args.func(args)
