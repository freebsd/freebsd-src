import pytest
from atf_python.sys.net.rtsock import RtConst
from atf_python.sys.net.rtsock import Rtsock
from atf_python.sys.net.rtsock import RtsockRtMessage
from atf_python.sys.net.tools import ToolsHelper
from atf_python.sys.net.vnet import SingleVnetTestTemplate


class TestRtmMultipath(SingleVnetTestTemplate):
    def setup_method(self, method):
        method_name = method.__name__
        if "multipath4" in method_name:
            self.IPV4_PREFIXES = ["192.0.2.1/24"]
            self.PREFIX = "128.66.0.0/24"
        elif "multipath6" in method_name:
            self.IPV6_PREFIXES = ["2001:db8::1/64"]
            self.PREFIX = "2001:db8:0:ddbb::/64"
        super().setup_method(method)
        self.rtsock = Rtsock()

    def get_prefix_routes(self):
        family = "inet6" if ":" in self.PREFIX else "inet"
        routes = ToolsHelper.get_routes(family)
        return [r for r in routes if r["destination"] == self.PREFIX]

    @pytest.mark.parametrize(
        "gws",
        [
            pytest.param(["+.10=2", "+.5=3"], id="transition_multi"),
            pytest.param(["+.10=2", "+.5=3", "-.10=2"], id="transition_single1"),
            pytest.param(["+.10=2", "+.5=3", "-.5=3"], id="transition_single2"),
            pytest.param(
                ["+.10", "+.11", "+.50", "+.13", "+.145", "+.72"], id="correctness1"
            ),
            pytest.param(
                ["+.10", "+.11", "+.50", "-.50", "+.145", "+.72"], id="correctness2"
            ),
            pytest.param(["+.10=1", "+.5=2"], id="weight1"),
            pytest.param(["+.10=2", "+.5=7"], id="weight2"),
            pytest.param(["+.10=13", "+.5=21"], id="weight3_max"),
            pytest.param(["+.10=2", "+.5=3", "~.5=4"], id="change_new_weight1"),
            pytest.param(["+.10=2", "+.5=3", "~.10=3"], id="change_new_weight2"),
            pytest.param(
                ["+.10=2", "+.5=3", "+.7=4", "~.10=3"], id="change_new_weight3"
            ),
            pytest.param(["+.10=2", "+.5=3", "~.5=3"], id="change_same_weight1"),
            pytest.param(
                ["+.10=2", "+.5=3", "+.7=4", "~.5=3"], id="change_same_weight2"
            ),
        ],
    )
    @pytest.mark.require_user("root")
    def test_rtm_multipath4(self, gws):
        """Tests RTM_ADD with IPv4 dest transitioning to multipath"""
        self._test_rtm_multipath(gws, "192.0.2")

    @pytest.mark.parametrize(
        "gws",
        [
            pytest.param(["+:10=2", "+:5=3"], id="transition_multi"),
            pytest.param(["+:10=2", "+:5=3", "-:10=2"], id="transition_single1"),
            pytest.param(["+:10=2", "+:5=3", "-:5=3"], id="transition_single2"),
            pytest.param(
                ["+:10", "+:11", "+:50", "+:13", "+:145", "+:72"], id="correctness1"
            ),
            pytest.param(
                ["+:10", "+:11", "+:50", "-:50", "+:145", "+:72"], id="correctness2"
            ),
            pytest.param(["+:10=1", "+:5=2"], id="weight1"),
            pytest.param(["+:10=2", "+:5=7"], id="weight2"),
            pytest.param(["+:10=13", "+:5=21"], id="weight3_max"),
            pytest.param(["+:10=13", "+:5=21"], id="weight3_max"),
            pytest.param(["+:10=2", "+:5=3", "~:5=4"], id="change_new_weight1"),
            pytest.param(["+:10=2", "+:5=3", "~:10=3"], id="change_new_weight2"),
            pytest.param(
                ["+:10=2", "+:5=3", "+:7=4", "~:10=3"], id="change_new_weight3"
            ),
            pytest.param(["+:10=2", "+:5=3", "~:5=3"], id="change_same_weight1"),
            pytest.param(
                ["+:10=2", "+:5=3", "+:7=4", "~:5=3"], id="change_same_weight2"
            ),
        ],
    )
    @pytest.mark.require_user("root")
    def test_rtm_multipath6(self, gws):
        """Tests RTM_ADD with IPv6 dest transitioning to multipath"""
        self._test_rtm_multipath(gws, "2001:db8:")

    def _test_rtm_multipath(self, gws, gw_prefix: str):
        desired_map = {}
        for gw_act in gws:
            # GW format: <+-~>GW[=weight]
            if "=" in gw_act:
                arr = gw_act[1:].split("=")
                weight = int(arr[1])
                gw = gw_prefix + arr[0]
            else:
                weight = None
                gw = gw_prefix + gw_act[1:]
            if gw_act[0] == "+":
                msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
                desired_map[gw] = self.rtsock.get_weight(weight)
            elif gw_act[0] == "-":
                msg = self.rtsock.new_rtm_del(self.PREFIX, gw)
                del desired_map[gw]
            else:
                msg = self.rtsock.new_rtm_change(self.PREFIX, gw)
                desired_map[gw] = self.rtsock.get_weight(weight)

            msg.rtm_flags = RtConst.RTF_GATEWAY
            if weight:
                msg.rtm_inits |= RtConst.RTV_WEIGHT
                msg.rtm_rmx.rmx_weight = weight
            # Prepare SAs to check for
            desired_sa = {
                RtConst.RTA_DST: msg.get_sa(RtConst.RTA_DST),
                RtConst.RTA_NETMASK: msg.get_sa(RtConst.RTA_NETMASK),
                RtConst.RTA_GATEWAY: msg.get_sa(RtConst.RTA_GATEWAY),
            }
            self.rtsock.write_message(msg)

            data = self.rtsock.read_data(msg.rtm_seq)
            msg_in = RtsockRtMessage.from_bytes(data)
            msg_in.print_in_message()
            msg_in.verify(msg.rtm_type, desired_sa)
            assert msg_in.rtm_rmx.rmx_weight == self.rtsock.get_weight(weight)

            routes = self.get_prefix_routes()
            derived_map = {r["gateway"]: r["weight"] for r in routes}
            assert derived_map == desired_map

    @pytest.mark.require_user("root")
    def test_rtm_multipath4_add_same_eexist(self):
        """Tests adding same IPv4 gw to the multipath group (EEXIST)"""
        gws = ["192.0.2.10", "192.0.2.11", "192.0.2.11"]
        self._test_rtm_multipath_add_same_eexist(gws)

    @pytest.mark.require_user("root")
    def test_rtm_multipath6_add_same_eexist(self):
        """Tests adding same IPv4 gw to the multipath group (EEXIST)"""
        gws = ["2001:db8::10", "2001:db8::11", "2001:db8::11"]
        self._test_rtm_multipath_add_same_eexist(gws)

    def _test_rtm_multipath_add_same_eexist(self, gws):
        for idx, gw in enumerate(gws):
            msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
            msg.rtm_flags = RtConst.RTF_GATEWAY
            try:
                self.rtsock.write_message(msg)
            except FileExistsError as e:
                if idx != 2:
                    raise
                print("Succcessfully raised {}".format(e))

    @pytest.mark.require_user("root")
    def test_rtm_multipath4_del_unknown_esrch(self):
        """Tests deleting non-existing dest from the multipath group (ESRCH)"""
        gws = ["192.0.2.10", "192.0.2.11"]
        self._test_rtm_multipath_del_unknown_esrch(gws, "192.0.2.7")

    @pytest.mark.require_user("root")
    def test_rtm_multipath6_del_unknown_esrch(self):
        """Tests deleting non-existing dest from the multipath group (ESRCH)"""
        gws = ["2001:db8::10", "2001:db8::11"]
        self._test_rtm_multipath_del_unknown_esrch(gws, "2001:db8::7")

    @pytest.mark.require_user("root")
    def _test_rtm_multipath_del_unknown_esrch(self, gws, target_gw):
        for gw in gws:
            msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
            msg.rtm_flags = RtConst.RTF_GATEWAY
            self.rtsock.write_message(msg)
        msg = self.rtsock.new_rtm_del(self.PREFIX, target_gw)
        msg.rtm_flags = RtConst.RTF_GATEWAY
        try:
            self.rtsock.write_message(msg)
        except ProcessLookupError as e:
            print("Succcessfully raised {}".format(e))

    @pytest.mark.require_user("root")
    def test_rtm_multipath4_change_unknown_esrch(self):
        """Tests changing non-existing dest in the multipath group (ESRCH)"""
        gws = ["192.0.2.10", "192.0.2.11"]
        self._test_rtm_multipath_change_unknown_esrch(gws, "192.0.2.7")

    @pytest.mark.require_user("root")
    def test_rtm_multipath6_change_unknown_esrch(self):
        """Tests changing non-existing dest in the multipath group (ESRCH)"""
        gws = ["2001:db8::10", "2001:db8::11"]
        self._test_rtm_multipath_change_unknown_esrch(gws, "2001:db8::7")

    @pytest.mark.require_user("root")
    def _test_rtm_multipath_change_unknown_esrch(self, gws, target_gw):
        for gw in gws:
            msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
            msg.rtm_flags = RtConst.RTF_GATEWAY
            self.rtsock.write_message(msg)
        msg = self.rtsock.new_rtm_change(self.PREFIX, target_gw)
        msg.rtm_flags = RtConst.RTF_GATEWAY
        try:
            self.rtsock.write_message(msg)
        except ProcessLookupError as e:
            print("Succcessfully raised {}".format(e))

    @pytest.mark.require_user("root")
    def test_rtm_multipath4_add_zero_weight(self):
        """Tests RTM_ADD with dest transitioning to multipath"""

        desired_map = {}
        for gw in ["192.0.2.10", "192.0.2.11", "192.0.2.13"]:
            msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
            msg.rtm_flags = RtConst.RTF_GATEWAY
            msg.rtm_rmx.rmx_weight = 0
            msg.rtm_inits |= RtConst.RTV_WEIGHT
            self.rtsock.write_message(msg)
            desired_map[gw] = self.rtsock.get_weight(0)

        routes = self.get_prefix_routes()
        derived_map = {r["gateway"]: r["weight"] for r in routes}
        assert derived_map == desired_map

    @pytest.mark.require_user("root")
    def test_rtm_multipath4_getroute(self):
        """Tests RTM_GET with exact prefix lookup on the multipath group"""
        gws = ["192.0.2.10", "192.0.2.11", "192.0.2.13"]
        return self._test_rtm_multipath_getroute(gws)

    @pytest.mark.require_user("root")
    def test_rtm_multipath6_getroute(self):
        """Tests RTM_GET with exact prefix lookup on the multipath group"""
        gws = ["2001:db8::10", "2001:db8::11", "2001:db8::13"]
        return self._test_rtm_multipath_getroute(gws)

    def _test_rtm_multipath_getroute(self, gws):
        valid_gws = []
        for gw in gws:
            msg = self.rtsock.new_rtm_add(self.PREFIX, gw)
            msg.rtm_flags = RtConst.RTF_GATEWAY
            self.rtsock.write_message(msg)

            desired_sa = {
                RtConst.RTA_DST: msg.get_sa(RtConst.RTA_DST),
                RtConst.RTA_NETMASK: msg.get_sa(RtConst.RTA_NETMASK),
            }
            valid_gws.append(msg.get_sa(RtConst.RTA_GATEWAY))

            msg_get = RtsockRtMessage(
                RtConst.RTM_GET,
                self.rtsock.get_seq(),
                msg.get_sa(RtConst.RTA_DST),
                msg.get_sa(RtConst.RTA_NETMASK),
            )
            self.rtsock.write_message(msg_get)

            data = self.rtsock.read_data(msg_get.rtm_seq)
            msg_in = RtsockRtMessage.from_bytes(data)
            msg_in.print_in_message()
            msg_in.verify(RtConst.RTM_GET, desired_sa)

            # Additionally, check that the gateway is among the valid
            # gateways
            gw_found = False
            gw_in = msg_in.get_sa(RtConst.RTA_GATEWAY)
            for valid_gw in valid_gws:
                try:
                    assert valid_gw == gw_in
                    gw_found = True
                    break
                except AssertionError:
                    pass
            assert gw_found is True
