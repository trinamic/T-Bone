import serial

__author__ = 'marcus'

__default_serial_port__ = "/dev/ttyO1"


class Machine():
    def __init__(self, serialport = None):
        if serialport is None:
            serialport = __default_serial_port__
        ser = serial.Serial(serialport, 115200, timeout=1)
        line = ser.readline()   # read a '\n' terminated line
