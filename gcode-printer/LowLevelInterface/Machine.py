import sys
import logging
import re
import serial
import time

__author__ = 'marcus'

_default_serial_port = "/dev/ttyO1"
_default_timeout = 5


def matcher(buff):
    pass

class MachineError(Exception):
    def __init__(self, msg):
        self.msg = msg


class Machine():
    _commandEndMatcher = re.compile(";")    #gives you the ability to search for anything

    def __init__(self, serialport=None):
        if serialport is None:
            serialport = _default_serial_port
        self.machineSerial = serial.Serial(serialport, 115200, timeout=_default_timeout)
        line = self.doRead()   # read a ';' terminated line
        logging.info(line)

    def doRead(self):
        buff = ""
        tic = time.clock()
	print >> sys.stdout, "read it"
        buff += self.machineSerial.read(128)
        # you can use if not ('\n' in buff) too if you don't like re
        while ((time.clock() - tic) < _default_timeout) and (not self._commandEndMatcher.search(buff)):
            print >> sys.stdout, (time.clock() - tic)*60
            buff += self.machineSerial.read(128)

        return buff
