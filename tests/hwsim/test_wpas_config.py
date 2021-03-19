# wpa_supplicant config file
# Copyright (c) 2014, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import os

from wpasupplicant import WpaSupplicant
import hostapd
from utils import *

config_checks = [("ap_scan", "0"),
                 ("update_config", "1"),
                 ("device_name", "name"),
                 ("eapol_version", "2"),
                 ("wps_priority", "5"),
                 ("ip_addr_go", "192.168.1.1"),
                 ("ip_addr_mask", "255.255.255.0"),
                 ("ip_addr_start", "192.168.1.10"),
                 ("ip_addr_end", "192.168.1.20"),
                 ("disable_scan_offload", "1"),
                 ("fast_reauth", "0"),
                 ("uuid", "6aeae5e3-c1fc-4e76-8293-7346e1d1459d"),
                 ("manufacturer", "MANUF"),
                 ("model_name", "MODEL"),
                 ("model_number", "MODEL NUM"),
                 ("serial_number", "123qwerty"),
                 ("device_type", "1234-0050F204-4321"),
                 ("os_version", "01020304"),
                 ("config_methods", "label push_button"),
                 ("wps_cred_processing", "1"),
                 ("wps_vendor_ext_m1", "000137100100020001"),
                 ("p2p_listen_reg_class", "81"),
                 ("p2p_listen_channel", "6"),
                 ("p2p_oper_reg_class", "82"),
                 ("p2p_oper_channel", "14"),
                 ("p2p_go_intent", "14"),
                 ("p2p_ssid_postfix", "foobar"),
                 ("persistent_reconnect", "1"),
                 ("p2p_intra_bss", "0"),
                 ("p2p_group_idle", "2"),
                 ("p2p_passphrase_len", "63"),
                 ("p2p_pref_chan", "81:1,82:14,81:11"),
                 ("p2p_no_go_freq", "2412-2432,2462,5000-6000"),
                 ("p2p_add_cli_chan", "1"),
                 ("p2p_optimize_listen_chan", "1"),
                 ("p2p_go_ht40", "1"),
                 ("p2p_go_vht", "1"),
                 ("p2p_go_ctwindow", "1"),
                 ("p2p_disabled", "1"),
                 ("p2p_no_group_iface", "1"),
                 ("p2p_ignore_shared_freq", "1"),
                 ("p2p_cli_probe", "1"),
                 ("p2p_go_freq_change_policy", "0"),
                 ("country", "FI"),
                 ("bss_max_count", "123"),
                 ("bss_expiration_age", "45"),
                 ("bss_expiration_scan_count", "17"),
                 ("filter_ssids", "1"),
                 ("filter_rssi", "-10"),
                 ("max_num_sta", "3"),
                 ("disassoc_low_ack", "1"),
                 ("hs20", "1"),
                 ("interworking", "1"),
                 ("hessid", "02:03:04:05:06:07"),
                 ("access_network_type", "7"),
                 ("pbc_in_m1", "1"),
                 ("wps_nfc_dev_pw_id", "12345"),
                 ("wps_nfc_dh_pubkey", "1234567890ABCDEF"),
                 ("wps_nfc_dh_privkey", "FF1234567890ABCDEFFF"),
                 ("ext_password_backend", "test"),
                 ("p2p_go_max_inactivity", "9"),
                 ("auto_interworking", "1"),
                 ("okc", "1"),
                 ("pmf", "1"),
                 ("dtim_period", "3"),
                 ("beacon_int", "102"),
                 ("sae_groups", "5 19"),
                 ("ap_vendor_elements", "dd0411223301"),
                 ("ignore_old_scan_res", "1"),
                 ("freq_list", "2412 2437"),
                 ("scan_cur_freq", "1"),
                 ("sched_scan_interval", "13"),
                 ("external_sim", "1"),
                 ("tdls_external_control", "1"),
                 ("wowlan_triggers", "any"),
                 ("bgscan", '"simple:30:-45:300"'),
                 ("p2p_search_delay", "123"),
                 ("mac_addr", "2"),
                 ("rand_addr_lifetime", "123456789"),
                 ("preassoc_mac_addr", "1"),
                 ("gas_rand_addr_lifetime", "567"),
                 ("gas_rand_mac_addr", "2"),
                 ("key_mgmt_offload", "0"),
                 ("user_mpm", "0"),
                 ("max_peer_links", "17"),
                 ("cert_in_cb", "0"),
                 ("mesh_max_inactivity", "31"),
                 ("dot11RSNASAERetransPeriod", "19"),
                 ("passive_scan", "1"),
                 ("reassoc_same_bss_optim", "1"),
                 ("wpa_rsc_relaxation", "0"),
                 ("sched_scan_plans", "10:100 20:200 30"),
                 ("non_pref_chan", "81:5:10:2 81:1:0:2 81:9:0:2"),
                 ("mbo_cell_capa", "1"),
                 ("gas_address3", "1"),
                 ("ftm_responder", "1"),
                 ("ftm_initiator", "1"),
                 ("pcsc_reader", "foo"),
                 ("pcsc_pin", "1234"),
                 ("driver_param", "testing"),
                 ("dot11RSNAConfigPMKLifetime", "43201"),
                 ("dot11RSNAConfigPMKReauthThreshold", "71"),
                 ("dot11RSNAConfigSATimeout", "61"),
                 ("sec_device_type", "12345-0050F204-54321"),
                 ("autoscan", "exponential:3:300"),
                 ("osu_dir", "/tmp/osu"),
                 ("fst_group_id", "bond0"),
                 ("fst_priority", "5"),
                 ("fst_llt", "7"),
                 ("go_interworking", "1"),
                 ("go_access_network_type", "2"),
                 ("go_internet", "1"),
                 ("go_venue_group", "3"),
                 ("go_venue_type", "4"),
                 ("p2p_device_random_mac_addr", "1"),
                 ("p2p_device_persistent_mac_addr", "02:12:34:56:78:9a"),
                 ("p2p_interface_random_mac_addr", "1"),
                 ("openssl_ciphers", "DEFAULT")]

def supported_param(capa, field):
    mesh_params = ["user_mpm", "max_peer_links", "mesh_max_inactivity"]
    if field in mesh_params and not capa['mesh']:
        return False

    sae_params = ["dot11RSNASAERetransPeriod"]
    if field in sae_params and not capa['sae']:
        return False

    return True

def check_config(capa, config):
    with open(config, "r") as f:
        data = f.read()
    if "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=" not in data:
        raise Exception("Missing ctrl_interface")
    if "blob-base64-foo={" not in data:
        raise Exception("Missing blob")
    if "cred={" not in data:
        raise Exception("Missing cred")
    if "network={" not in data:
        raise Exception("Missing network")
    for field, value in config_checks:
        if supported_param(capa, field):
            if "\n" + field + "=" + value + "\n" not in data:
                raise Exception("Missing value: " + field)
    return data

def test_wpas_config_file(dev, apdev, params):
    """wpa_supplicant config file parsing/writing"""
    config = os.path.join(params['logdir'], 'wpas_config_file.conf')
    if os.path.exists(config):
        try:
            os.remove(config)
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    try:
        wpas.interface_add("wlan5", config=config)
        initialized = True
    except:
        initialized = False
    if initialized:
        raise Exception("Missing config file did not result in an error")

    try:
        with open(config, "w") as f:
            f.write("update_config=1 \t\r\n")
            f.write("# foo\n")
            f.write("\n")
            f.write(" \t\reapol_version=2")
            for i in range(0, 100):
                f.write("                    ")
            f.write("foo\n")
            f.write("device_name=name#foo\n")

        wpas.interface_add("wlan5", config=config)
        capa = {}
        capa['mesh'] = "MESH" in wpas.get_capability("modes")
        capa['sae'] = "SAE" in wpas.get_capability("auth_alg")

        id = wpas.add_network()
        wpas.set_network_quoted(id, "ssid", "foo")
        wpas.set_network_quoted(id, "psk", "12345678")
        wpas.set_network(id, "bssid", "00:11:22:33:44:55")
        wpas.set_network(id, "proto", "RSN")
        wpas.set_network(id, "key_mgmt", "WPA-PSK-SHA256")
        wpas.set_network(id, "pairwise", "CCMP")
        wpas.set_network(id, "group", "CCMP")
        wpas.set_network(id, "auth_alg", "OPEN")

        id = wpas.add_cred()
        wpas.set_cred(id, "priority", "3")
        wpas.set_cred(id, "sp_priority", "6")
        wpas.set_cred(id, "update_identifier", "4")
        wpas.set_cred(id, "ocsp", "1")
        wpas.set_cred(id, "eap", "TTLS")
        wpas.set_cred(id, "req_conn_capab", "6:1234")
        wpas.set_cred_quoted(id, "realm", "example.com")
        wpas.set_cred_quoted(id, "provisioning_sp", "example.com")
        wpas.set_cred_quoted(id, "domain", "example.com")
        wpas.set_cred_quoted(id, "domain_suffix_match", "example.com")
        wpas.set_cred(id, "roaming_consortium", "112233")
        wpas.set_cred(id, "required_roaming_consortium", "112233")
        wpas.set_cred_quoted(id, "roaming_consortiums",
                             "112233,aabbccddee,445566")
        wpas.set_cred_quoted(id, "roaming_partner",
                             "roaming.example.net,1,127,*")
        wpas.set_cred_quoted(id, "ca_cert", "/tmp/ca.pem")
        wpas.set_cred_quoted(id, "username", "user")
        wpas.set_cred_quoted(id, "password", "secret")
        ev = wpas.wait_event(["CRED-MODIFIED 0 password"])

        wpas.request("SET blob foo 12345678")

        for field, value in config_checks:
            if supported_param(capa, field):
                wpas.set(field, value)

        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")
        if "OK" not in wpas.global_request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")

        wpas.interface_remove("wlan5")
        data1 = check_config(capa, config)

        wpas.interface_add("wlan5", config=config)
        if len(wpas.list_networks()) != 1:
            raise Exception("Unexpected number of networks")
        if len(wpas.request("LIST_CREDS").splitlines()) != 2:
            raise Exception("Unexpected number of credentials")

        val = wpas.get_cred(0, "roaming_consortiums")
        if val != "112233,aabbccddee,445566":
            raise Exception("Unexpected roaming_consortiums value: " + val)

        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")
        data2 = check_config(capa, config)

        if data1 != data2:
            logger.debug(data1)
            logger.debug(data2)
            raise Exception("Unexpected configuration change")

        wpas.request("SET update_config 0")
        wpas.global_request("SET update_config 0")
        if "OK" in wpas.request("SAVE_CONFIG"):
            raise Exception("SAVE_CONFIG succeeded unexpectedly")
        if "OK" in wpas.global_request("SAVE_CONFIG"):
            raise Exception("SAVE_CONFIG (global) succeeded unexpectedly")

        # replace the config file with a directory to break writing/renaming
        os.remove(config)
        os.mkdir(config)
        wpas.request("SET update_config 1")
        wpas.global_request("SET update_config 1")
        if "OK" in wpas.request("SAVE_CONFIG"):
            raise Exception("SAVE_CONFIG succeeded unexpectedly")
        if "OK" in wpas.global_request("SAVE_CONFIG"):
            raise Exception("SAVE_CONFIG (global) succeeded unexpectedly")

    finally:
        try:
            os.rmdir(config)
        except:
            pass
        if not wpas.ifname:
            wpas.interface_add("wlan5")
        wpas.dump_monitor()
        wpas.request("SET country 00")
        wpas.wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=1)

def test_wpas_config_file_wps(dev, apdev):
    """wpa_supplicant config file parsing/writing with WPS"""
    config = "/tmp/test_wpas_config_file.conf"
    if os.path.exists(config):
        os.remove(config)

    params = {"ssid": "test-wps", "eap_server": "1", "wps_state": "2",
              "skip_cred_build": "1", "extra_cred": "wps-ctrl-cred"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    try:
        with open(config, "w") as f:
            f.write("update_config=1\n")

        wpas.interface_add("wlan5", config=config)

        hapd.request("WPS_PIN any 12345670")
        wpas.scan_for_bss(apdev[0]['bssid'], freq="2412")
        wpas.request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        ev = wpas.wait_event(["WPS-FAIL"], timeout=10)
        if ev is None:
            raise Exception("WPS-FAIL event timed out")

        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)
            if "network=" in data:
                raise Exception("Unexpected network block in configuration data")

    finally:
        try:
            os.remove(config)
        except:
            pass
        try:
            os.remove(config + ".tmp")
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

def test_wpas_config_file_wps2(dev, apdev):
    """wpa_supplicant config file parsing/writing with WPS (2)"""
    config = "/tmp/test_wpas_config_file.conf"
    if os.path.exists(config):
        os.remove(config)

    params = {"ssid": "test-wps", "eap_server": "1", "wps_state": "2",
              "skip_cred_build": "1", "extra_cred": "wps-ctrl-cred2"}
    hapd = hostapd.add_ap(apdev[0]['ifname'], params)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    try:
        with open(config, "w") as f:
            f.write("update_config=1\n")

        wpas.interface_add("wlan5", config=config)

        hapd.request("WPS_PIN any 12345670")
        wpas.scan_for_bss(apdev[0]['bssid'], freq="2412")
        wpas.request("WPS_PIN " + apdev[0]['bssid'] + " 12345670")
        ev = wpas.wait_event(["WPS-SUCCESS"], timeout=10)
        if ev is None:
            raise Exception("WPS-SUCCESS event timed out")

        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)

            with open(config, "r") as f:
                data = f.read()
                if "network=" not in data:
                    raise Exception("Missing network block in configuration data")
                if "ssid=410a420d430044" not in data:
                    raise Exception("Unexpected ssid parameter value")

    finally:
        try:
            os.remove(config)
        except:
            pass
        try:
            os.remove(config + ".tmp")
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

def test_wpas_config_file_set_psk(dev):
    """wpa_supplicant config file parsing/writing with arbitrary PSK value"""
    config = "/tmp/test_wpas_config_file.conf"
    if os.path.exists(config):
        os.remove(config)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    try:
        with open(config, "w") as f:
            f.write("update_config=1\n")

        wpas.interface_add("wlan5", config=config)

        id = wpas.add_network()
        wpas.set_network_quoted(id, "ssid", "foo")
        if "OK" in wpas.request('SET_NETWORK %d psk "12345678"\n}\nmodel_name=foobar\nnetwork={\n#\"' % id):
            raise Exception("Invalid psk value accepted")

        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")

        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)
            if "model_name" in data:
                raise Exception("Unexpected parameter added to configuration")

        wpas.interface_remove("wlan5")
        wpas.interface_add("wlan5", config=config)

    finally:
        try:
            os.remove(config)
        except:
            pass
        try:
            os.remove(config + ".tmp")
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

def test_wpas_config_file_set_cred(dev):
    """wpa_supplicant config file parsing/writing with arbitrary cred values"""
    config = "/tmp/test_wpas_config_file.conf"
    if os.path.exists(config):
        os.remove(config)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    try:
        with open(config, "w") as f:
            f.write("update_config=1\n")

        wpas.interface_add("wlan5", config=config)

        id = wpas.add_cred()
        wpas.set_cred_quoted(id, "username", "hello")
        fields = ["username", "milenage", "imsi", "password", "realm",
                  "phase1", "phase2", "provisioning_sp"]
        for field in fields:
            if "FAIL" not in wpas.request('SET_CRED %d %s "hello"\n}\nmodel_name=foobar\ncred={\n#\"' % (id, field)):
                raise Exception("Invalid %s value accepted" % field)

        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")

        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)
            if "model_name" in data:
                raise Exception("Unexpected parameter added to configuration")

        wpas.interface_remove("wlan5")
        wpas.interface_add("wlan5", config=config)

    finally:
        try:
            os.remove(config)
        except:
            pass
        try:
            os.remove(config + ".tmp")
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

def test_wpas_config_file_set_global(dev):
    """wpa_supplicant config file parsing/writing with arbitrary global values"""
    config = "/tmp/test_wpas_config_file.conf"
    if os.path.exists(config):
        os.remove(config)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    try:
        with open(config, "w") as f:
            f.write("update_config=1\n")

        wpas.interface_add("wlan5", config=config)

        fields = ["model_name", "device_name", "ctrl_interface_group",
                  "opensc_engine_path", "pkcs11_engine_path",
                  "pkcs11_module_path", "openssl_ciphers", "pcsc_reader",
                  "pcsc_pin", "driver_param", "manufacturer", "model_name",
                  "model_number", "serial_number", "config_methods",
                  "p2p_ssid_postfix", "autoscan", "ext_password_backend",
                  "osu_dir", "wowlan_triggers", "fst_group_id",
                  "sched_scan_plans", "non_pref_chan"]
        for field in fields:
            if "FAIL" not in wpas.request('SET %s hello\nmodel_name=foobar' % field):
                raise Exception("Invalid %s value accepted" % field)

        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")

        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)
            if "model_name" in data:
                raise Exception("Unexpected parameter added to configuration")

        wpas.interface_remove("wlan5")
        wpas.interface_add("wlan5", config=config)

    finally:
        try:
            os.remove(config)
        except:
            pass
        try:
            os.remove(config + ".tmp")
        except:
            pass
        try:
            os.rmdir(config)
        except:
            pass

def test_wpas_config_file_key_mgmt(dev, apdev, params):
    """wpa_supplicant config file writing and key_mgmt values"""
    config = os.path.join(params['logdir'],
                          'wpas_config_file_key_mgmt.conf')
    if os.path.exists(config):
        os.remove(config)

    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')

    with open(config, "w") as f:
        f.write("update_config=1\n")

    wpas.interface_add("wlan5", config=config)

    from test_dpp import params1_csign, params1_sta_connector, params1_sta_netaccesskey, check_dpp_capab

    check_dpp_capab(wpas)

    id = wpas.add_network()
    wpas.set_network_quoted(id, "ssid", "foo")
    wpas.set_network(id, "key_mgmt", "DPP")
    wpas.set_network(id, "ieee80211w", "2")
    wpas.set_network_quoted(id, "dpp_csign", params1_csign)
    wpas.set_network_quoted(id, "dpp_connector", params1_sta_connector)
    wpas.set_network_quoted(id, "dpp_netaccesskey", params1_sta_netaccesskey)
    if "OK" not in wpas.request("SAVE_CONFIG"):
        raise Exception("Failed to save configuration file")

    with open(config, "r") as f:
        data = f.read()
        logger.info("Configuration file contents: " + data)
        if "key_mgmt=DPP" not in data:
            raise Exception("Missing key_mgmt")
        if 'dpp_connector="' + params1_sta_connector + '"' not in data:
            raise Exception("Missing dpp_connector")
        if 'dpp_netaccesskey="' + params1_sta_netaccesskey + '"' not in data:
            raise Exception("Missing dpp_netaccesskey")
        if 'dpp_csign="' + params1_csign + '"' not in data:
            raise Exception("Missing dpp_csign")

    wpas.set_network(id, "dpp_csign", "NULL")
    wpas.set_network(id, "dpp_connector", "NULL")
    wpas.set_network(id, "dpp_netaccesskey", "NULL")
    wpas.set_network_quoted(id, "psk", "12345678")
    wpas.set_network(id, "ieee80211w", "0")

    tests = ["WPA-PSK", "WPA-EAP", "IEEE8021X", "NONE", "WPA-NONE", "FT-PSK",
             "FT-EAP", "FT-EAP-SHA384", "WPA-PSK-SHA256", "WPA-EAP-SHA256",
             "SAE", "FT-SAE", "OSEN", "WPA-EAP-SUITE-B",
             "WPA-EAP-SUITE-B-192", "FILS-SHA256", "FILS-SHA384",
             "FT-FILS-SHA256", "FT-FILS-SHA384", "OWE", "DPP"]
    supported_key_mgmts = dev[0].get_capability("key_mgmt")
    for key_mgmt in tests:
        if key_mgmt == "WPA-EAP-SUITE-B-192" and key_mgmt not in supported_key_mgmts:
            logger.info("Skip unsupported " + key_mgmt)
            continue
        wpas.set_network(id, "key_mgmt", key_mgmt)
        if "OK" not in wpas.request("SAVE_CONFIG"):
            raise Exception("Failed to save configuration file")
        with open(config, "r") as f:
            data = f.read()
            logger.info("Configuration file contents: " + data)
            if "key_mgmt=" + key_mgmt not in data:
                raise Exception("Missing key_mgmt " + key_mgmt)

    wpas.interface_remove("wlan5")
    wpas.interface_add("wlan5", config=config)

def check_network_config(config, network_expected, check=None):
    with open(config, "r") as f:
        data = f.read()
        logger.info("Configuration file contents:\n" + data.rstrip())
        if network_expected and "network=" not in data:
            raise Exception("Missing network block in configuration data")
        if not network_expected and "network=" in data:
            raise Exception("Unexpected network block in configuration data")
        if check and check not in data:
            raise Exception("Missing " + check)

def test_wpas_config_file_sae(dev, apdev, params):
    """wpa_supplicant config file writing with SAE"""
    config = os.path.join(params['logdir'], 'wpas_config_file_sae.conf')
    with open(config, "w") as f:
        f.write("update_config=1\n")
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5", config=config)
    check_sae_capab(wpas)

    # Valid SAE configuration with sae_password
    wpas.connect("test-sae", sae_password="sae-password", key_mgmt="SAE",
                 only_add_network=True)
    wpas.save_config()
    check_network_config(config, True, check="key_mgmt=SAE")

    wpas.request("REMOVE_NETWORK all")
    wpas.save_config()
    check_network_config(config, False)

    # Valid SAE configuration with psk
    wpas.connect("test-sae", psk="sae-password", key_mgmt="SAE",
                 only_add_network=True)
    wpas.save_config()
    check_network_config(config, True, check="key_mgmt=SAE")
    wpas.request("REMOVE_NETWORK all")

    # Invalid PSK configuration with sae_password
    wpas.connect("test-psk", sae_password="sae-password", key_mgmt="WPA-PSK",
                 only_add_network=True)
    wpas.save_config()
    check_network_config(config, False)

    # Invalid SAE configuration with raw_psk
    wpas.connect("test-sae", raw_psk=32*"00", key_mgmt="SAE",
                 only_add_network=True)
    wpas.save_config()
    check_network_config(config, False)

def test_wpas_config_update_without_file(dev, apdev):
    """wpa_supplicant SAVE_CONFIG without config file"""
    wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
    wpas.interface_add("wlan5")
    wpas.set("update_config", "1")
    if "FAIL" not in wpas.request("SAVE_CONFIG"):
        raise Exception("SAVE_CONFIG accepted unexpectedly")
