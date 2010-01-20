#!/usr/local/bin/python

# Copyright (c) 2002-2003, Jeffrey Roberson <jeff@freebsd.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice unmodified, this list of conditions, and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

import sys
import re
from Tkinter import *

# To use:
# - Install the ports/x11-toolkits/py-tkinter package.
# - Add KTR_SCHED to KTR_COMPILE and KTR_MASK in your KERNCONF
# - It is encouraged to increase KTR_ENTRIES size to 32768 to gather
#    enough information for analysis.
# - Rebuild kernel with proper changes to KERNCONF.
# - Dump the trace to a file: 'ktrdump -ct > ktr.out'
# - Run the python script: 'python schedgraph.py ktr.out'
#
# To do:
# 1)  Add a per-thread summary display
# 2)  Add bounding box style zoom.
# 3)  Click to center.
# 4)  Implement some sorting mechanism.

ticksps = None
status = None
configtypes = []

def ticks2sec(ticks):
	ns = ticksps / 1000000000
	ticks /= ns
	if (ticks < 1000):
		return (str(ticks) + "ns")
	ticks /= 1000
	if (ticks < 1000):
		return (str(ticks) + "us")
	ticks /= 1000
	if (ticks < 1000):
		return (str(ticks) + "ms")
	ticks /= 1000
	return (str(ticks) + "s")

class Scaler(Frame):
	def __init__(self, master, target):
		Frame.__init__(self, master)
		self.scale = Scale(self, command=self.scaleset,
		    from_=1000, to_=1000000, orient=HORIZONTAL, resolution=1000)
		self.label = Label(self, text="Ticks per pixel")
		self.label.pack(side=LEFT)
		self.scale.pack(fill="both", expand=1)
		self.target = target
		self.scale.set(target.scaleget())
		self.initialized = 1

	def scaleset(self, value):
		self.target.scaleset(int(value))

	def set(self, value):
		self.scale.set(value)

class Status(Frame):
	def __init__(self, master):
		Frame.__init__(self, master)
		self.label = Label(self, bd=1, relief=SUNKEN, anchor=W)
		self.label.pack(fill="both", expand=1)
		self.clear()

	def set(self, str):
		self.label.config(text=str)

	def clear(self):
		self.label.config(text="")

	def startup(self, str):
		self.set(str)
		root.update()

class EventConf(Frame):
	def __init__(self, master, name, color, enabled):
		Frame.__init__(self, master)
		self.name = name
		self.color = StringVar()
		self.color_default = color
		self.color_current = color
		self.color.set(color)
		self.enabled = IntVar()
		self.enabled_default = enabled
		self.enabled_current = enabled
		self.enabled.set(enabled)
		self.draw()

	def draw(self):
		self.label = Label(self, text=self.name, anchor=W)
		self.sample = Canvas(self, width=24, height=24,
		    bg='grey')
		self.rect = self.sample.create_rectangle(0, 0, 24, 24,
		    fill=self.color.get())
		self.list = OptionMenu(self, self.color,
		    "dark red", "red", "pink",
		    "dark orange", "orange",
		    "yellow", "light yellow",
		    "dark green", "green", "light green",
		    "dark blue", "blue", "light blue",
		    "dark violet", "violet", "purple",
		    "dark grey", "light grey",
		    "white", "black",
		    command=self.setcolor)
		self.checkbox = Checkbutton(self, text="enabled",
		    variable=self.enabled)
		self.label.grid(row=0, column=0, sticky=E+W)
		self.sample.grid(row=0, column=1)
		self.list.grid(row=0, column=2, sticky=E+W)
		self.checkbox.grid(row=0, column=3)
		self.columnconfigure(0, weight=1)
		self.columnconfigure(2, minsize=110)

	def setcolor(self, color):
		self.color.set(color)
		self.sample.itemconfigure(self.rect, fill=color)

	def apply(self):
		cchange = 0
		echange = 0
		if (self.color_current != self.color.get()):
			cchange = 1
		if (self.enabled_current != self.enabled.get()):
			echange = 1
		self.color_current = self.color.get()
		self.enabled_current = self.enabled.get()
		if (echange != 0):
			if (self.enabled_current):
				graph.setcolor(self.name, self.color_current)
			else:
				graph.hide(self.name)
			return
		if (cchange != 0):
			graph.setcolor(self.name, self.color_current)

	def revert(self):
		self.setcolor(self.color_current)
		self.enabled.set(self.enabled_current)

	def default(self):
		self.setcolor(self.color_default)
		self.enabled.set(self.enabled_default)

class EventConfigure(Toplevel):
	def __init__(self):
		Toplevel.__init__(self)
		self.resizable(0, 0)
		self.title("Event Configuration")
		self.items = LabelFrame(self, text="Event Type")
		self.buttons = Frame(self)
		self.drawbuttons()
		self.items.grid(row=0, column=0, sticky=E+W)
		self.columnconfigure(0, weight=1)
		self.buttons.grid(row=1, column=0, sticky=E+W)
		self.types = []
		self.irow = 0
		for type in configtypes:
			self.additem(type.name, type.color, type.enabled)

	def additem(self, name, color, enabled=1):
		item = EventConf(self.items, name, color, enabled)
		self.types.append(item)
		item.grid(row=self.irow, column=0, sticky=E+W)
		self.irow += 1

	def drawbuttons(self):
		self.apply = Button(self.buttons, text="Apply",
		    command=self.apress)
		self.revert = Button(self.buttons, text="Revert",
		    command=self.rpress)
		self.default = Button(self.buttons, text="Default",
		    command=self.dpress)
		self.apply.grid(row=0, column=0, sticky=E+W)
		self.revert.grid(row=0, column=1, sticky=E+W)
		self.default.grid(row=0, column=2, sticky=E+W)
		self.buttons.columnconfigure(0, weight=1)
		self.buttons.columnconfigure(1, weight=1)
		self.buttons.columnconfigure(2, weight=1)

	def apress(self):
		for item in self.types:
			item.apply()

	def rpress(self):
		for item in self.types:
			item.revert()

	def dpress(self):
		for item in self.types:
			item.default()

class EventView(Toplevel):
	def __init__(self, event, canvas):
		Toplevel.__init__(self)
		self.resizable(0, 0)
		self.title("Event")
		self.event = event
		self.frame = Frame(self)
		self.frame.grid(row=0, column=0, sticky=N+S+E+W)
		self.buttons = Frame(self)
		self.buttons.grid(row=1, column=0, sticky=E+W)
		self.canvas = canvas
		self.drawlabels()
		self.drawbuttons()
		event.displayref(canvas)
		self.bind("<Destroy>", self.destroycb)

	def destroycb(self, event):
		self.unbind("<Destroy>")
		if (self.event != None):
			self.event.displayunref(self.canvas)
			self.event = None
		self.destroy()

	def clearlabels(self):
		for label in self.frame.grid_slaves():
			label.grid_remove()

	def drawlabels(self):
		ypos = 0
		labels = self.event.labels()
		while (len(labels) < 7):
			labels.append(("", "", 0))
		for label in labels:
			name, value, linked = label
			l = Label(self.frame, text=name, bd=1, width=15,
			    relief=SUNKEN, anchor=W)
			if (linked):
				fgcolor = "blue"
			else:
				fgcolor = "black"
			r = Label(self.frame, text=value, bd=1,
			    relief=SUNKEN, anchor=W, fg=fgcolor)
			l.grid(row=ypos, column=0, sticky=E+W)
			r.grid(row=ypos, column=1, sticky=E+W)
			if (linked):
				r.bind("<Button-1>", self.linkpress)
			ypos += 1
		self.frame.columnconfigure(1, minsize=80)

	def drawbuttons(self):
		self.back = Button(self.buttons, text="<", command=self.bpress)
		self.forw = Button(self.buttons, text=">", command=self.fpress)
		self.new = Button(self.buttons, text="new", command=self.npress)
		self.back.grid(row=0, column=0, sticky=E+W)
		self.forw.grid(row=0, column=1, sticky=E+W)
		self.new.grid(row=0, column=2, sticky=E+W)
		self.buttons.columnconfigure(2, weight=1)

	def newevent(self, event):
		self.event.displayunref(self.canvas)
		self.clearlabels()
		self.event = event
		self.event.displayref(self.canvas)
		self.drawlabels()

	def npress(self):
		EventView(self.event, self.canvas)

	def bpress(self):
		prev = self.event.prev()
		if (prev == None):
			return
		while (prev.real == 0):
			prev = prev.prev()
			if (prev == None):
				return
		self.newevent(prev)

	def fpress(self):
		next = self.event.next()
		if (next == None):
			return
		while (next.real == 0):
			next = next.next()
			if (next == None):
				return
		self.newevent(next)

	def linkpress(self, wevent):
		event = self.event.getlinked()
		if (event != None):
			self.newevent(event)

class Event:
	name = "none"
	color = "grey"
	def __init__(self, source, cpu, timestamp, last=0):
		self.source = source
		self.cpu = cpu
		self.timestamp = int(timestamp)
		self.entries = []
		self.real = 1
		self.idx = None
		self.state = 0
		self.item = None
		self.dispcnt = 0
		self.linked = None
		if (last):
			source.lastevent(self)
		else:
			source.event(self)

	def status(self):
		statstr = self.name + " " + self.source.name
		statstr += " on: cpu" + str(self.cpu)
		statstr += " at: " + str(self.timestamp)
		statstr += self.stattxt()
		status.set(statstr)

	def stattxt(self):
		return ""

	def textadd(self, tuple):
		pass
		self.entries.append(tuple)

	def labels(self):
		return [("Source:", self.source.name, 0),
				("Event:", self.name, 0),
				("CPU:", self.cpu, 0),
				("Timestamp:", self.timestamp, 0)] + self.entries
	def mouseenter(self, canvas, item):
		self.displayref(canvas)
		self.status()

	def mouseexit(self, canvas, item):
		self.displayunref(canvas)
		status.clear()

	def mousepress(self, canvas, item):
		EventView(self, canvas)

	def next(self):
		return self.source.eventat(self.idx + 1)

	def prev(self):
		return self.source.eventat(self.idx - 1)

	def displayref(self, canvas):
		if (self.dispcnt == 0):
			canvas.itemconfigure(self.item, width=2)
		self.dispcnt += 1

	def displayunref(self, canvas):
		self.dispcnt -= 1
		if (self.dispcnt == 0):
			canvas.itemconfigure(self.item, width=0)
			canvas.tag_raise("point", "state")

	def getlinked(self):
		return self.linked.findevent(self.timestamp)

class PointEvent(Event):
	def __init__(self, thread, cpu, timestamp, last=0):
		Event.__init__(self, thread, cpu, timestamp, last)

	def draw(self, canvas, xpos, ypos):
		l = canvas.create_oval(xpos - 6, ypos + 1, xpos + 6, ypos - 11,
		    fill=self.color, tags=("all", "point", "event")
		    + (self.name,), width=0)
		canvas.events[l] = self
		self.item = l
		if (self.enabled == 0):
			canvas.itemconfigure(l, state="hidden")

		return (xpos)

class StateEvent(Event):
	def __init__(self, thread, cpu, timestamp, last=0):
		Event.__init__(self, thread, cpu, timestamp, last)
		self.duration = 0
		self.skipnext = 0
		self.skipself = 0
		self.state = 1

	def draw(self, canvas, xpos, ypos):
		next = self.nextstate()
		if (self.skipself == 1 or next == None):
			return (xpos)
		while (self.skipnext):
			skipped = next
			next.skipself = 1
			next.real = 0
			next = next.nextstate()
			if (next == None):
				next = skipped
			self.skipnext -= 1
		self.duration = next.timestamp - self.timestamp
		delta = self.duration / canvas.ratio
		l = canvas.create_rectangle(xpos, ypos,
		    xpos + delta, ypos - 10, fill=self.color, width=0,
		    tags=("all", "state", "event") + (self.name,))
		canvas.events[l] = self
		self.item = l
		if (self.enabled == 0):
			canvas.itemconfigure(l, state="hidden")

		return (xpos + delta)

	def stattxt(self):
		return " duration: " + ticks2sec(self.duration)

	def nextstate(self):
		next = self.next()
		while (next != None and next.state == 0):
			next = next.next()
		return (next)

	def labels(self):
		return [("Source:", self.source.name, 0),
				("Event:", self.name, 0),
				("Timestamp:", self.timestamp, 0),
				("CPU:", self.cpu, 0),
				("Duration:", ticks2sec(self.duration), 0)] \
				 + self.entries

class Count(Event):
	name = "Count"
	color = "red"
	enabled = 1
	def __init__(self, source, cpu, timestamp, count):
		self.count = int(count)
		Event.__init__(self, source, cpu, timestamp)
		self.duration = 0
		self.textadd(("count:", self.count, 0))

	def draw(self, canvas, xpos, ypos):
		next = self.next()
		self.duration = next.timestamp - self.timestamp
		delta = self.duration / canvas.ratio
		yhight = self.source.yscale() * self.count
		l = canvas.create_rectangle(xpos, ypos - yhight,
		    xpos + delta, ypos, fill=self.color, width=0,
		    tags=("all", "count", "event") + (self.name,))
		canvas.events[l] = self
		self.item = l
		if (self.enabled == 0):
			canvas.itemconfigure(l, state="hidden")
		return (xpos + delta)

	def stattxt(self):
		return " count: " + str(self.count)

configtypes.append(Count)

class Running(StateEvent):
	name = "running"
	color = "green"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Running)

class Idle(StateEvent):
	name = "idle"
	color = "grey"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Idle)

class Yielding(StateEvent):
	name = "yielding"
	color = "yellow"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.skipnext = 2
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Yielding)

class Swapped(StateEvent):
	name = "swapped"
	color = "violet"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Swapped)

class Suspended(StateEvent):
	name = "suspended"
	color = "purple"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Suspended)

class Iwait(StateEvent):
	name = "iwait"
	color = "grey"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Iwait)

class Preempted(StateEvent):
	name = "preempted"
	color = "red"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio, bythread):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.skipnext = 2
		self.prio = prio
		self.linked = bythread
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("by thread:", self.linked.name, 1))

configtypes.append(Preempted)

class Sleep(StateEvent):
	name = "sleep"
	color = "blue"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio, wmesg):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.wmesg = wmesg
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("wmesg:", self.wmesg, 0))

	def stattxt(self):
		statstr = StateEvent.stattxt(self)
		statstr += " sleeping on: " + self.wmesg
		return (statstr)

configtypes.append(Sleep)

class Blocked(StateEvent):
	name = "blocked"
	color = "dark red"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio, lock):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.lock = lock
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("lock:", self.lock, 0))

	def stattxt(self):
		statstr = StateEvent.stattxt(self)
		statstr += " blocked on: " + self.lock
		return (statstr)

configtypes.append(Blocked)

class KsegrpRunq(StateEvent):
	name = "KsegrpRunq"
	color = "orange"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio, bythread):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.linked = bythread
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("by thread:", self.linked.name, 1))

configtypes.append(KsegrpRunq)

class Runq(StateEvent):
	name = "Runq"
	color = "yellow"
	enabled = 1
	def __init__(self, thread, cpu, timestamp, prio, bythread):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.linked = bythread
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("by thread:", self.linked.name, 1))

configtypes.append(Runq)

class Sched_exit(StateEvent):
	name = "exit"
	color = "grey"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio):
		StateEvent.__init__(self, thread, cpu, timestamp)
		self.name = "sched_exit"
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Sched_exit)

class Padevent(StateEvent):
	def __init__(self, thread, cpu, timestamp, last=0):
		StateEvent.__init__(self, thread, cpu, timestamp, last)
		self.name = "pad"
		self.real = 0

	def draw(self, canvas, xpos, ypos):
		next = self.next()
		if (next == None):
			return (xpos)
		self.duration = next.timestamp - self.timestamp
		delta = self.duration / canvas.ratio
		return (xpos + delta)

class Tick(PointEvent):
	name = "tick"
	color = "black"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio, stathz):
		PointEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.textadd(("prio:", self.prio, 0))

configtypes.append(Tick)

class Prio(PointEvent):
	name = "prio"
	color = "black"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio, newprio, bythread):
		PointEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.newprio = newprio
		self.linked = bythread
		self.textadd(("new prio:", self.newprio, 0))
		self.textadd(("prio:", self.prio, 0))
		if (self.linked != self.source):
			self.textadd(("by thread:", self.linked.name, 1))
		else:
			self.textadd(("by thread:", self.linked.name, 0))

configtypes.append(Prio)

class Lend(PointEvent):
	name = "lend"
	color = "black"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, prio, tothread):
		PointEvent.__init__(self, thread, cpu, timestamp)
		self.prio = prio
		self.linked = tothread
		self.textadd(("prio:", self.prio, 0))
		self.textadd(("to thread:", self.linked.name, 1))

configtypes.append(Lend)

class Wokeup(PointEvent):
	name = "wokeup"
	color = "black"
	enabled = 0
	def __init__(self, thread, cpu, timestamp, ranthread):
		PointEvent.__init__(self, thread, cpu, timestamp)
		self.linked = ranthread
		self.textadd(("ran thread:", self.linked.name, 1))

configtypes.append(Wokeup)

class EventSource:
	def __init__(self, name):
		self.name = name
		self.events = []
		self.cpu = 0
		self.cpux = 0

	def fixup(self):
		pass

	def event(self, event):
		self.events.insert(0, event)

	def remove(self, event):
		self.events.remove(event)

	def lastevent(self, event):
		self.events.append(event)

	def draw(self, canvas, ypos):
		xpos = 10
		self.cpux = 10
		self.cpu = self.events[1].cpu
		for i in range(0, len(self.events)):
			self.events[i].idx = i
		for event in self.events:
			if (event.cpu != self.cpu and event.cpu != -1):
				self.drawcpu(canvas, xpos, ypos)
				self.cpux = xpos
				self.cpu = event.cpu
			xpos = event.draw(canvas, xpos, ypos)
		self.drawcpu(canvas, xpos, ypos)

	def drawname(self, canvas, ypos):
		ypos = ypos - (self.ysize() / 2)
		canvas.create_text(10, ypos, anchor="w", text=self.name)

	def drawcpu(self, canvas, xpos, ypos):
		cpu = int(self.cpu)
		if (cpu == 0):
			color = 'light grey'
		elif (cpu == 1):
			color = 'dark grey'
		elif (cpu == 2):
			color = 'light blue'
		elif (cpu == 3):
			color == 'light green'
		else:
			color == "white"
		l = canvas.create_rectangle(self.cpux,
		    ypos - self.ysize() - canvas.bdheight,
		    xpos, ypos + canvas.bdheight, fill=color, width=0,
		    tags=("all", "cpuinfo"))

	def ysize(self):
		return (None)

	def eventat(self, i):
		if (i >= len(self.events)):
			return (None)
		event = self.events[i]
		return (event)

	def findevent(self, timestamp):
		for event in self.events:
			if (event.timestamp >= timestamp and event.real):
				return (event)
		return (None)

class Thread(EventSource):
	names = {}
	def __init__(self, td, pcomm):
		EventSource.__init__(self, pcomm)
		self.str = td
		try:
			cnt = Thread.names[pcomm]
		except:
			Thread.names[pcomm] = 0
			return
		Thread.names[pcomm] = cnt + 1

	def fixup(self):
		cnt = Thread.names[self.name]
		if (cnt == 0):
			return
		cnt -= 1
		Thread.names[self.name] = cnt
		self.name += " td" + str(cnt)

	def ysize(self):
		return (10)

class Counter(EventSource):
	max = 0
	def __init__(self, name):
		EventSource.__init__(self, name)

	def event(self, event):
		EventSource.event(self, event)
		try:
			count = event.count
		except:
			return
		count = int(count)
		if (count > Counter.max):
			Counter.max = count

	def ysize(self):
		return (80)

	def yscale(self):
		return (self.ysize() / Counter.max)


class KTRFile:
	def __init__(self, file):
		self.timestamp_first = None
		self.timestamp_last = None
		self.lineno = -1
		self.threads = []
		self.sources = []
		self.ticks = {}
		self.load = {}

		self.parse(file)
		self.fixup()
		global ticksps
		ticksps = self.ticksps()

	def parse(self, file):
		try:
			ifp = open(file)
		except:
			print "Can't open", file
			sys.exit(1)

		ktrhdr = "\s+\d+\s+(\d+)\s+(\d+)\s+"
		tdname = "(\S+)\(([^)]*)\)"

		ktrstr = "mi_switch: " + tdname
		ktrstr += " prio (\d+) inhibit (\d+) wmesg (\S+) lock (\S+)"
		switchout_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "mi_switch: " + tdname + " prio (\d+) idle"
		idled_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "mi_switch: " + tdname + " prio (\d+) preempted by "
		ktrstr += tdname
		preempted_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "mi_switch: running " + tdname + " prio (\d+)"
		switchin_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "sched_add: " + tdname + " prio (\d+) by " + tdname
		sched_add_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "setrunqueue: " + tdname + " prio (\d+) by " + tdname
		setrunqueue_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "sched_rem: " + tdname + " prio (\d+) by " + tdname
		sched_rem_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "sched_exit_thread: " + tdname + " prio (\d+)"
		sched_exit_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "statclock: " + tdname + " prio (\d+)"
		ktrstr += " stathz (\d+)"
		sched_clock_re = re.compile(ktrhdr + ktrstr)

		ktrstr = "sched_prio: " + tdname + " prio (\d+)"
		ktrstr += " newprio (\d+) by " + tdname
		sched_prio_re = re.compile(ktrhdr + ktrstr)

		cpuload_re = re.compile(ktrhdr + "load: (\d+)")
		loadglobal_re = re.compile(ktrhdr + "global load: (\d+)")

		parsers = [[cpuload_re, self.cpuload],
			   [loadglobal_re, self.loadglobal],
			   [switchin_re, self.switchin],
			   [switchout_re, self.switchout],
			   [sched_add_re, self.sched_add],
			   [setrunqueue_re, self.sched_rem],
			   [sched_prio_re, self.sched_prio],
			   [preempted_re, self.preempted],
			   [sched_rem_re, self.sched_rem],
			   [sched_exit_re, self.sched_exit],
			   [sched_clock_re, self.sched_clock],
			   [idled_re, self.idled]]

		for line in ifp.readlines():
			self.lineno += 1
			if ((self.lineno % 1024) == 0):
				status.startup("Parsing line " +
				    str(self.lineno))
			for p in parsers:
				m = p[0].match(line)
				if (m != None):
					p[1](*m.groups())
					break
			# if (m == None):
			# 	print line,

	def checkstamp(self, timestamp):
		timestamp = int(timestamp)
		if (self.timestamp_first == None):
			self.timestamp_first = timestamp
		if (timestamp > self.timestamp_first):
			print "Bad timestamp on line ", self.lineno
			return (0)
		self.timestamp_last = timestamp
		return (1)

	def timespan(self):
		return (self.timestamp_first - self.timestamp_last);

	def ticksps(self):
		return (self.timespan() / self.ticks[0]) * int(self.stathz)

	def switchout(self, cpu, timestamp, td, pcomm, prio, inhibit, wmesg, lock):
		TDI_SUSPENDED = 0x0001
		TDI_SLEEPING = 0x0002
		TDI_SWAPPED = 0x0004
		TDI_LOCK = 0x0008
		TDI_IWAIT = 0x0010 

		if (self.checkstamp(timestamp) == 0):
			return
		inhibit = int(inhibit)
		thread = self.findtd(td, pcomm)
		if (inhibit & TDI_SWAPPED):
			Swapped(thread, cpu, timestamp, prio)
		elif (inhibit & TDI_SLEEPING):
			Sleep(thread, cpu, timestamp, prio, wmesg)
		elif (inhibit & TDI_LOCK):
			Blocked(thread, cpu, timestamp, prio, lock)
		elif (inhibit & TDI_IWAIT):
			Iwait(thread, cpu, timestamp, prio)
		elif (inhibit & TDI_SUSPENDED):
			Suspended(thread, cpu, timestamp, prio)
		elif (inhibit == 0):
			Yielding(thread, cpu, timestamp, prio)
		else:
			print "Unknown event", inhibit
			sys.exit(1)
		
	def idled(self, cpu, timestamp, td, pcomm, prio):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		Idle(thread, cpu, timestamp, prio)

	def preempted(self, cpu, timestamp, td, pcomm, prio, bytd, bypcomm):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		Preempted(thread, cpu, timestamp, prio,
		    self.findtd(bytd, bypcomm))

	def switchin(self, cpu, timestamp, td, pcomm, prio):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		Running(thread, cpu, timestamp, prio)

	def sched_add(self, cpu, timestamp, td, pcomm, prio, bytd, bypcomm):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		bythread = self.findtd(bytd, bypcomm)
		Runq(thread, cpu, timestamp, prio, bythread)
		Wokeup(bythread, cpu, timestamp, thread)

	def sched_rem(self, cpu, timestamp, td, pcomm, prio, bytd, bypcomm):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		KsegrpRunq(thread, cpu, timestamp, prio,
		    self.findtd(bytd, bypcomm))

	def sched_exit(self, cpu, timestamp, td, pcomm, prio):
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		Sched_exit(thread, cpu, timestamp, prio)

	def sched_clock(self, cpu, timestamp, td, pcomm, prio, stathz):
		if (self.checkstamp(timestamp) == 0):
			return
		self.stathz = stathz
		cpu = int(cpu)
		try:
			ticks = self.ticks[cpu]
		except:
			self.ticks[cpu] = 0
		self.ticks[cpu] += 1
		thread = self.findtd(td, pcomm)
		Tick(thread, cpu, timestamp, prio, stathz)

	def sched_prio(self, cpu, timestamp, td, pcomm, prio, newprio, bytd, bypcomm):
		if (prio == newprio):
			return
		if (self.checkstamp(timestamp) == 0):
			return
		thread = self.findtd(td, pcomm)
		bythread = self.findtd(bytd, bypcomm)
		Prio(thread, cpu, timestamp, prio, newprio, bythread)
		Lend(bythread, cpu, timestamp, newprio, thread)

	def cpuload(self, cpu, timestamp, count):
		if (self.checkstamp(timestamp) == 0):
			return
		cpu = int(cpu)
		try:
			load = self.load[cpu]
		except:
			load = Counter("cpu" + str(cpu) + " load")
			self.load[cpu] = load
			self.sources.insert(0, load)
		Count(load, cpu, timestamp, count)

	def loadglobal(self, cpu, timestamp, count):
		if (self.checkstamp(timestamp) == 0):
			return
		cpu = 0
		try:
			load = self.load[cpu]
		except:
			load = Counter("CPU load")
			self.load[cpu] = load
			self.sources.insert(0, load)
		Count(load, cpu, timestamp, count)

	def findtd(self, td, pcomm):
		for thread in self.threads:
			if (thread.str == td and thread.name == pcomm):
				return thread
		thread = Thread(td, pcomm)
		self.threads.append(thread)
		self.sources.append(thread)
		return (thread)

	def fixup(self):
		for source in self.sources:
			Padevent(source, -1, self.timestamp_last)
			Padevent(source, -1, self.timestamp_first, last=1)
			source.fixup()

class SchedDisplay(Canvas):
	def __init__(self, master):
		self.ratio = 1
		self.ktrfile = None
		self.sources = None
		self.bdheight = 10 
		self.events = {}

		Canvas.__init__(self, master, width=800, height=500, bg='grey',
		     scrollregion=(0, 0, 800, 500))

	def setfile(self, ktrfile):
		self.ktrfile = ktrfile
		self.sources = ktrfile.sources

	def draw(self):
		ypos = 0
		xsize = self.xsize()
		for source in self.sources:
			status.startup("Drawing " + source.name)
			self.create_line(0, ypos, xsize, ypos,
			    width=1, fill="black", tags=("all",))
			ypos += self.bdheight
			ypos += source.ysize()
			source.draw(self, ypos)
			ypos += self.bdheight
			try:
				self.tag_raise("point", "state")
				self.tag_lower("cpuinfo", "all")
			except:
				pass
		self.create_line(0, ypos, xsize, ypos,
		    width=1, fill="black", tags=("all",))
		self.tag_bind("event", "<Enter>", self.mouseenter)
		self.tag_bind("event", "<Leave>", self.mouseexit)
		self.tag_bind("event", "<Button-1>", self.mousepress)

	def mouseenter(self, event):
		item, = self.find_withtag(CURRENT)
		event = self.events[item]
		event.mouseenter(self, item)

	def mouseexit(self, event):
		item, = self.find_withtag(CURRENT)
		event = self.events[item]
		event.mouseexit(self, item)

	def mousepress(self, event):
		item, = self.find_withtag(CURRENT)
		event = self.events[item]
		event.mousepress(self, item)

	def drawnames(self, canvas):
		status.startup("Drawing names")
		ypos = 0
		canvas.configure(scrollregion=(0, 0,
		    canvas["width"], self.ysize()))
		for source in self.sources:
			canvas.create_line(0, ypos, canvas["width"], ypos,
			    width=1, fill="black", tags=("all",))
			ypos += self.bdheight
			ypos += source.ysize()
			source.drawname(canvas, ypos)
			ypos += self.bdheight
		canvas.create_line(0, ypos, canvas["width"], ypos,
		    width=1, fill="black", tags=("all",))

	def xsize(self):
		return ((self.ktrfile.timespan() / self.ratio) + 20)

	def ysize(self):
		ysize = 0
		for source in self.sources:
			ysize += source.ysize() + (self.bdheight * 2)
		return (ysize)

	def scaleset(self, ratio):
		if (self.ktrfile == None):
			return
		oldratio = self.ratio
		xstart, ystart = self.xview()
		length = (float(self["width"]) / self.xsize())
		middle = xstart + (length / 2)

		self.ratio = ratio
		self.configure(scrollregion=(0, 0, self.xsize(), self.ysize()))
		self.scale("all", 0, 0, float(oldratio) / ratio, 1)

		length = (float(self["width"]) / self.xsize())
		xstart = middle - (length / 2)
		self.xview_moveto(xstart)

	def scaleget(self):
		return self.ratio

	def setcolor(self, tag, color):
		self.itemconfigure(tag, state="normal", fill=color)

	def hide(self, tag):
		self.itemconfigure(tag, state="hidden")

class GraphMenu(Frame):
	def __init__(self, master):
		Frame.__init__(self, master, bd=2, relief=RAISED)
		self.view = Menubutton(self, text="Configure")
		self.viewmenu = Menu(self.view, tearoff=0)
		self.viewmenu.add_command(label="Events",
		    command=self.econf)
		self.view["menu"] = self.viewmenu
		self.view.pack(side=LEFT)

	def econf(self):
		EventConfigure()


class SchedGraph(Frame):
	def __init__(self, master):
		Frame.__init__(self, master)
		self.menu = None
		self.names = None
		self.display = None
		self.scale = None
		self.status = None
		self.pack(expand=1, fill="both")
		self.buildwidgets()
		self.layout()
		self.draw(sys.argv[1])

	def buildwidgets(self):
		global status
		self.menu = GraphMenu(self)
		self.display = SchedDisplay(self)
		self.names = Canvas(self,
		    width=100, height=self.display["height"],
		    bg='grey', scrollregion=(0, 0, 50, 100))
		self.scale = Scaler(self, self.display)
		status = self.status = Status(self)
		self.scrollY = Scrollbar(self, orient="vertical",
		    command=self.display_yview)
		self.display.scrollX = Scrollbar(self, orient="horizontal",
		    command=self.display.xview)
		self.display["xscrollcommand"] = self.display.scrollX.set
		self.display["yscrollcommand"] = self.scrollY.set
		self.names["yscrollcommand"] = self.scrollY.set

	def layout(self):
		self.columnconfigure(1, weight=1)
		self.rowconfigure(1, weight=1)
		self.menu.grid(row=0, column=0, columnspan=3, sticky=E+W)
		self.names.grid(row=1, column=0, sticky=N+S)
		self.display.grid(row=1, column=1, sticky=W+E+N+S)
		self.scrollY.grid(row=1, column=2, sticky=N+S)
		self.display.scrollX.grid(row=2, column=0, columnspan=2,
		    sticky=E+W)
		self.scale.grid(row=3, column=0, columnspan=3, sticky=E+W)
		self.status.grid(row=4, column=0, columnspan=3, sticky=E+W)

	def draw(self, file):
		self.master.update()
		ktrfile = KTRFile(file)
		self.display.setfile(ktrfile)
		self.display.drawnames(self.names)
		self.display.draw()
		self.scale.set(250000)
		self.display.xview_moveto(0)

	def display_yview(self, *args):
		self.names.yview(*args)
		self.display.yview(*args)

	def setcolor(self, tag, color):
		self.display.setcolor(tag, color)

	def hide(self, tag):
		self.display.hide(tag)

if (len(sys.argv) != 2):
	print "usage:", sys.argv[0], "<ktr file>"
	sys.exit(1)

root = Tk()
root.title("Scheduler Graph")
graph = SchedGraph(root)
root.mainloop()
