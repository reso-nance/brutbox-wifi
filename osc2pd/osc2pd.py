#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#  Copyright 2019 Reso-nance Numérique <laurent@reso-nance.org>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

"""
This script receives the OSC messages coming from the brutboxes
and send it back to Puredata using UDP via "pdsend" in order to
overcome OSC unpacking issues such as latency and system instability.
pdsend must be in the $PATH of the user invoking this script
"""

import liblo, subprocess, threading, time
OSClistenPort = 8000
UDPoutputPort = 10000
listenToOSC = False

def OSCcallback(path, args, types, src):
    """called each time an OSC message is received."""
    args = [str(a) for a in args]
    message = path + " " + " ".join(args)
    # print("message : ", message)
    cmd = "echo '%s' | pdsend %i localhost udp" % (message, UDPoutputPort)
    subprocess.Popen(cmd, shell=True)

def listen():
    """blocking function listening to OSC messages.
    A callback is fired each time a message os received """
    try:
        server = liblo.Server(OSClistenPort)
        print("listening to incoming OSC on port %i" % OSClistenPort)
    except liblo.ServerError as e:
        print(e)
        raise SystemError

    server.add_method(None, None, OSCcallback)

    while listenToOSC : 
        server.recv(1)

if __name__ == '__main__':
    print("starting the OSC server thread ...")
    listenToOSC = True
    threading.Thread(target=listen).start()
    while True :
        try : time.sleep(1)
        except KeyboardInterrupt :
            print("shutting down the OSC server thread...")
            listenToOSC = False
            print("bye !")
            raise SystemExit
