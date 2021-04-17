# Monitor support
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import remotehost
import config
import rutils
import monitor
import time
import os

import logging
logger = logging.getLogger()

def run_monitor(devices, setup_params, refs, duts, monitors, seconds=None):
    try:
        air_monitor = []
        output = "\n\tPCAP files:\n"
        # setup log dir
        local_log_dir = setup_params['local_log_dir']

        # add/run monitors if requested
        air_monitors = monitor.create(devices, setup_params, refs, duts,
                                      monitors)
        for air_monitor in air_monitors:
            monitor.setup(air_monitor)
            monitor.run(air_monitor, setup_params)
            logger.warning(air_monitor.name + " - monitor started ...")

        if seconds != None:
            time.sleep(int(seconds))
        else:
            input("\tPress Enter to end capturing...")

        # destroy monitor / get pcap
        monitor.destroy(devices, air_monitors)
        for air_monitor in air_monitors:
            for log in air_monitor.logs:
                head, tail = os.path.split(log)
                output = output + "\t" + local_log_dir + "/" + tail + "\n"
            air_monitor.get_logs(local_log_dir)
        return output
    except:
        for air_monitor in air_monitors:
            monitor.destroy(devices, air_monitors)
            air_monitor.get_logs(local_log_dir)
        raise

def test_run_monitor(devices, setup_params, refs, duts, monitors):
    """TC run standalone monitor"""
    return run_monitor(devices, setup_params, refs, duts, monitors)
