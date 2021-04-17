# Host class
# Copyright (c) 2016, Qualcomm Atheros, Inc.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
import subprocess
import threading
import tempfile
import os
import traceback
import select

logger = logging.getLogger()

def remote_compatible(func):
    func.remote_compatible = True
    return func

def execute_thread(command, reply):
    cmd = ' '.join(command)
    logger.debug("thread run: " + cmd)
    err = tempfile.TemporaryFile()
    try:
        status = 0
        buf = subprocess.check_output(command, stderr=err).decode()
    except subprocess.CalledProcessError as e:
        status = e.returncode
        err.seek(0)
        buf = err.read()
    err.close()

    logger.debug("thread cmd: " + cmd)
    logger.debug("thread exit status: " + str(status))
    logger.debug("thread exit buf: " + str(buf))
    reply.append(status)
    reply.append(buf)

def gen_reaper_file(conf):
    fd, filename = tempfile.mkstemp(dir='/tmp', prefix=conf + '-')
    f = os.fdopen(fd, 'w')

    f.write("#!/bin/sh\n")
    f.write("name=\"$(basename $0)\"\n")
    f.write("echo $$ > /tmp/$name.pid\n")
    f.write("exec \"$@\"\n");

    return filename;

class Host():
    def __init__(self, host=None, ifname=None, port=None, name="", user="root"):
        self.host = host
        self.name = name
        self.user = user
        self.monitors = []
        self.monitor_thread = None
        self.logs = []
        self.ifname = ifname
        self.port = port
        self.dev = None
        self.monitor_params = []
        if self.name == "" and host != None:
            self.name = host

    def local_execute(self, command):
        logger.debug("execute: " + str(command))
        err = tempfile.TemporaryFile()
        try:
            status = 0
            buf = subprocess.check_output(command, stderr=err)
        except subprocess.CalledProcessError as e:
            status = e.returncode
            err.seek(0)
            buf = err.read()
        err.close()

        logger.debug("status: " + str(status))
        logger.debug("buf: " + str(buf))
        return status, buf.decode()

    def execute(self, command):
        if self.host is None:
            return self.local_execute(command)

        cmd = ["ssh", self.user + "@" + self.host, ' '.join(command)]
        _cmd = self.name + " execute: " + ' '.join(cmd)
        logger.debug(_cmd)
        err = tempfile.TemporaryFile()
        try:
            status = 0
            buf = subprocess.check_output(cmd, stderr=err)
        except subprocess.CalledProcessError as e:
            status = e.returncode
            err.seek(0)
            buf = err.read()
        err.close()

        logger.debug(self.name + " status: " + str(status))
        logger.debug(self.name + " buf: " + str(buf))
        return status, buf.decode()

    # async execute
    def thread_run(self, command, res, use_reaper=True):
        if use_reaper:
            filename = gen_reaper_file("reaper")
            self.send_file(filename, filename)
            self.execute(["chmod", "755", filename])
            _command = [filename] + command
        else:
            filename = ""
            _command = command

        if self.host is None:
            cmd = _command
        else:
            cmd = ["ssh", self.user + "@" + self.host, ' '.join(_command)]
        _cmd = self.name + " thread_run: " + ' '.join(cmd)
        logger.debug(_cmd)
        t = threading.Thread(target=execute_thread, name=filename, args=(cmd, res))
        t.start()
        return t

    def thread_stop(self, t):
        if t.name.find("reaper") == -1:
            raise Exception("use_reaper required")

        pid_file = t.name + ".pid"

        if t.is_alive():
            cmd = ["kill `cat " + pid_file + "`"]
            self.execute(cmd)

        # try again
        self.thread_wait(t, 5)
        if t.is_alive():
            cmd = ["kill `cat " + pid_file + "`"]
            self.execute(cmd)

        # try with -9
        self.thread_wait(t, 5)
        if t.is_alive():
            cmd = ["kill -9 `cat " + pid_file + "`"]
            self.execute(cmd)

        self.thread_wait(t, 5)
        if t.is_alive():
            raise Exception("thread still alive")

        self.execute(["rm", pid_file])
        self.execute(["rm", t.name])
        self.local_execute(["rm", t.name])

    def thread_wait(self, t, wait=None):
        if wait == None:
            wait_str = "infinite"
        else:
            wait_str = str(wait) + "s"

        logger.debug(self.name + " thread_wait(" + wait_str + "): ")
        if t.is_alive():
            t.join(wait)

    def pending(self, s, timeout=0):
        [r, w, e] = select.select([s], [], [], timeout)
        if r:
            return True
        return False

    def proc_run(self, command):
        filename = gen_reaper_file("reaper")
        self.send_file(filename, filename)
        self.execute(["chmod", "755", filename])
        _command = [filename] + command

        if self.host:
            cmd = ["ssh", self.user + "@" + self.host, ' '.join(_command)]
        else:
            cmd = _command

        _cmd = self.name + " proc_run: " + ' '.join(cmd)
        logger.debug(_cmd)
        err = tempfile.TemporaryFile()
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=err)
        proc.reaper_file = filename
        return proc

    def proc_wait_event(self, proc, events, timeout=10):
        if not isinstance(events, list):
            raise Exception("proc_wait_event() events not a list")

        logger.debug(self.name + " proc_wait_event: " + ' '.join(events) + " timeout: " + str(timeout))
        start = os.times()[4]
        try:
            while True:
                while self.pending(proc.stdout):
                    line = proc.stdout.readline()
                    if not line:
                        return None
                    line = line.decode()
                    logger.debug(line.strip('\n'))
                    for event in events:
                        if event in line:
                            return line
                now = os.times()[4]
                remaining = start + timeout - now
                if remaining <= 0:
                    break
                if not self.pending(proc.stdout, timeout=remaining):
                    break
        except:
            logger.debug(traceback.format_exc())
            pass
        return None

    def proc_stop(self, proc):
        if not proc:
            return

        self.execute(["kill `cat " + proc.reaper_file + ".pid`"])
        self.execute(["rm", proc.reaper_file + ".pid"])
        self.execute(["rm", proc.reaper_file])
        self.local_execute(["rm", proc.reaper_file])
        proc.kill()

    def proc_dump(self, proc):
        if not proc:
            return ""
        return proc.stdout.read()

    def execute_and_wait_event(self, command, events, timeout=10):
        proc = None
        ev = None

        try:
            proc = self.proc_run(command)
            ev = self.proc_wait_event(proc, events, timeout)
        except:
            pass

        self.proc_stop(proc)
        return ev

    def add_log(self, log_file):
        self.logs.append(log_file)

    def get_logs(self, local_log_dir=None):
        for log in self.logs:
            if local_log_dir:
                self.local_execute(["scp", self.user + "@[" + self.host + "]:" + log, local_log_dir])
            self.execute(["rm", log])
        del self.logs[:]

    def send_file(self, src, dst):
        if self.host is None:
            return
        self.local_execute(["scp", src,
                            self.user + "@[" + self.host + "]:" + dst])
