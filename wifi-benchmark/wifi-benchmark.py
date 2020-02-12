#!/usr/bin/python3
# -*- coding: utf-8 -*-
#  
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
#  
#  For testing purposes, this script send a random int
#  between 0 and 1024 followed by "/n" to the serial port
#  The esp8266 respond by sending an osc message back containing the same int.
#  The time between the serial.write() and the reception of the 
#  corresponding OSC message is store inside a np array (in ms)
#  Various stats can then be calculated and plotter to mesure the
#  distribution of the latence and it's 10% and 90% percentile.


import liblo, serial, time
from datetime import datetime, timedelta # because time.time and time.sleep are not accurate enough
from threading import Thread
from random import randint
import numpy as np
serialPort = "/dev/ttyUSB1"
incomingAddress = "/pression_E14A7A" # so we can ignore other incoming messages
sendEvery = 8 # serial sending rate in milliseconds
startTime = datetime.now()
sleepTime = timedelta(milliseconds=sendEvery)
awaitingResponse = {} # will be populated by {code:startTime}
messagesSent, messagesRecieved = 0,0
durations = np.array([], dtype="int") # will contain all the latencies measured
keepSending = True

def sendSerial() :
    global startTime, sleepTime, messagesSent, awaitingResponse
    code=randint(0,1023)
    with serial.Serial(serialPort, 115200, timeout=1) as s:
        # print("sending code %i" %code)
        s.write(b"%i\n"%code)
        s.flush()
    startTime = datetime.now()
    sleepTime = timedelta(milliseconds=sendEvery)
    awaitingResponse[code] = startTime
    messagesSent+=1

def receivedOSC(address, args, tags, sender):
    global messagesRecieved, awaitingResponse, durations
    code = args[0] # the code is sent back by the ESP8266
    if code in awaitingResponse : # we previously sent this code
        durationMs = 1000 * (datetime.now() - awaitingResponse[code]).total_seconds()
        messagesRecieved += 1
        durations = np.append(durations, durationMs)
        awaitingResponse.pop(code)
        # print ("received OSC, took %f ms" % durationMs)
    else : print("received message from outer space :", address, args) # we did not send this code ?

OSCserver = liblo.ServerThread(8000)
OSCserver.add_method(incomingAddress, None, receivedOSC)
OSCserver.start()

def drawGraph(durations, delay, totalSent, loss):
    import matplotlib.pyplot as plt
    plt.hist(durations, bins='auto')
    plt.suptitle("py->serial->esp->OSC->router->py toutes les %ims" % delay)
    plt.ylabel("messages OSC")
    plt.xlabel("délai (ms)")
    min90, max90 = np.percentile(durations, 10), np.percentile(durations, 90)
    plt.title("%i msg envoyés, perte: %.2fpc, 90pc latences %.01f~%.01fms, med: %.01fms" % (totalSent, loss, min90, max90, np.median(durations)))
    plt.show()

keepSending = True
while True :
    try :
        while  startTime + sleepTime > datetime.now() : pass
        sendSerial()
    except KeyboardInterrupt : 
        # OSCserver.stop()
        print("\n\n----------------------------------------------")
        print("\tTotal sent :", messagesSent)
        percentLost = 100*(messagesSent - messagesRecieved)/messagesSent #includes messages from outer space
        print("\tpercent lost :", percentLost)
        print("\t mean delay", sum(durations)/len(durations), "(max :", max(durations), "min :", min(durations),")")
        drawGraph(durations, sendEvery, messagesSent, percentLost)
        raise SystemExit