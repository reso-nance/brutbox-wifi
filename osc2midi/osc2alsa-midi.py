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

import liblo, threading, time, rtmidi, os
from rtmidi.midiconstants import CONTROL_CHANGE, NOTE_ON, NOTE_OFF
pdCommand = "pd"
OSClistenPort = 8000
listenToOSC = False
midiout = None
definitionFile = "OSCaddress.txt"
oscDefinitions = {}

def OSCcallback(path, args, types, src):
    """called each time an OSC message is received."""
    value =int(args[0]/1024*127) # the first value is converted to 0~127
    if path in oscDefinitions : sendMidi(oscDefinitions[path], value)
    else : print("received message %s not in definitions" % path)

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

def initAlsaMIDI():
    global midiout
    midiout = rtmidi.MidiOut()
    try : portIndex = next(i for i, p in enumerate(midiout.get_ports()) if "Pure Data Midi-In" in p)
    except StopIteration : 
        print("puredata MIDI port not found :")
        print("  is puredata running ?")
        print("  is ALSA-MIDI selected ?")
        print("  has at least one midi-input port being set ?")
        raise SystemExit
    midiout.open_port(portIndex)
    print("successfully opened Puredata MIDI port")

def readOSCdefinitions(definitionFile):
    """ this function reads the definition file and returns a dict containing {oscPaths:CCvalue}
    the definition file is a simple text file using spaces as separator. Lines containing a # are ignored"""
    oscDict={}
    print("reading definitions file %s..."  % definitionFile)
    if not os.path.exists(definitionFile):
        print("  ERROR : unable to find definition file %s" % definitionFile)
        raise SystemError
    with open(definitionFile, "rt") as defFile :
        for line in defFile.readlines():
            line = line.replace(" ","").replace("\n","") # strip down spaces and \n
            if not line or line.startswith("#") : continue # skip comments and empty lines
            try :
                oscAddress, CCnumber = [s for s in line.split("->") if s]
                CCnumber = int(CCnumber)
                if CCnumber not in range(128) : 
                    print( "  ERROR : invalid CC number for %s"% oscAddress)
                    continue
                oscDict.update({oscAddress:CCnumber})
                print("  added %s -> CC#%i" % (oscAddress, CCnumber))
            except (ValueError, StopIteration) as e :
                print("  ERROR reading line %s : " %line, e)
    return oscDict

def sendMidi(ccNumber, value):
    """sends the requested midi CC message to puredata"""
    global midiout
    print("set CC#%i to %i" %(ccNumber, value))
    midiout.send_message([CONTROL_CHANGE, ccNumber, value])

if __name__ == '__main__':
    print("opening puredata MIDI port :")
    initAlsaMIDI()
    # with midiout :
    #     midiout.send_message([NOTE_ON, 42, 100])
    #     time.sleep(1)
    #     midiout.send_message([NOTE_OFF, 42])
    #     time.sleep(1)
    #     midiout.send_message([CONTROL_CHANGE, 12, 100])
    print("reading the OSC definition file...")
    oscDefinitions = readOSCdefinitions(definitionFile)
    print("starting the OSC server thread ...")
    listenToOSC = True
    threading.Thread(target=listen).start()
    while True :
        try : time.sleep(1)
        except KeyboardInterrupt :
            print("shutting down the OSC server thread...")
            listenToOSC = False
            print("closing the midi output...")
            del midiout
            print("bye !")
            raise SystemExit