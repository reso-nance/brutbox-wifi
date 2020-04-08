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
from rtmidi.midiconstants import CONTROL_CHANGE
pdCommand = None # set the puredata command here if you want this script to launch pd ex:"pd -myoption1 -myoption2"
OSClistenPort = 8000 # must match the OSC clients port
listenToOSC = False # used to stop the OSCserver thread
midiout = None # rt-midi object
definitionFile = "OSCaddress.txt" # will be created if needed
oscDefinitions = {} # will contain {OSCpath : CCnumber} for every known OSC message
availableCCs = [i for i in range(128)] # used to auto assign undefined OSC messages
defaultDefinitions = """# this file contains the mapping of OSC addresses to CC number
# the expected OSC path can be written here follow by the corresponding
# CC number, separated by "->"
# ex : /myDevice/myOSC -> 12
"""

def OSCcallback(path, args, types, src):
    """called each time an OSC message is received."""
    global oscDefinitions, availableCCs
    value =int(args[0]*127) # the first (float) value is converted to 0~127
    if path in oscDefinitions : sendMidi(oscDefinitions[path], value)
    else : 
        newCCattributed = availableCCs.pop(0)
        oscDefinitions.update({path:newCCattributed})
        with open(definitionFile, "a") as f :
            f.write("\n#automatically added :\n%s -> %i" % (path, newCCattributed))
        print("received message %s not in definitions, attributed CC#%i" % (path, newCCattributed))

def listen():
    """blocking function listening to OSC messages.
    A callback is fired each time a message is received """
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
    """opens the alsa-midi port created by PureData, named "Pure Data Midi-In 1" """
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
    """ reads the definition file and returns a dict containing {oscPaths:CCvalue}
    the definition file is a simple text file using spaces as separator. Lines containing a # are ignored"""
    global availableCCs
    oscDict={}
    print("reading definitions file %s..."  % definitionFile)
    if not os.path.exists(definitionFile):
        print("  unable to find definition file %s, creating a new one" % definitionFile)
        with open(definitionFile, "wt") as defFile :
            defFile.write(defaultDefinitions) # write a header into the empty definitions file

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
                elif CCnumber not in availableCCs :
                    print("  ERROR : the CC number for %s has already been assigned" % oscAddress)
                    continue
                oscDict.update({oscAddress:CCnumber})
                availableCCs.remove(CCnumber)
                print("  added %s -> CC#%i" % (oscAddress, CCnumber))
            except (ValueError, StopIteration) as e :
                print("  ERROR reading line %s : " %line, e)
    return oscDict

def sendMidi(ccNumber, value):
    """sends the requested midi CC message to puredata using rt-midi"""
    global midiout
    # print("set CC#%i to %i" %(ccNumber, value))
    midiout.send_message([CONTROL_CHANGE, ccNumber, value])

if __name__ == '__main__':
    pidofPD = lambda: os.popen("pidof pd").read().replace("\n","")
    if pidofPD() : print ("Puredata is already running")
    elif pdCommand :
        pdCommand += ' -alsamidi  -midiaddindev "ALSA MIDI device #1" &'
        print("lauching Puredata : "+pdCommand)
        os.system(pdCommand)
        time.sleep(3)
    if not pidofPD() : raise SystemError("Puredata is not running. Launch it manually or set the pdCommand a the beginning of this script.")
    print("opening puredata MIDI port :")
    initAlsaMIDI()
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
            if pidofPD() and pdCommand : 
                print("closing Puredata ...")
                os.system("kill "+pidofPD())
            print("bye !")
            raise SystemExit