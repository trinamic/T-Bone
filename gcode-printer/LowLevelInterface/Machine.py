from Queue import Queue, Empty
import sys
import logging
import re
from threading import Thread
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
    _commandEndMatcher = re.compile(";")    #needed to search for command ends

    def __init__(self, serialport=None):
        if serialport is None:
            serialport = _default_serial_port
        self.serialport = serialport
        self.remaining_buffer = ""
        self.machine_connection = None


    def connect(self):
        if not self.machine_connection:
            machineSerial = serial.Serial(self.serialport, 115200, timeout=_default_timeout)
            self.machine_connection = _MachineConnection(machineSerial)

    def disconnect(self):
        if self.machine_connection:
            self.machine_connection.run_on = False


class _MachineConnection:
    def __init__(self, machine_serial):
        self.machine_serial = machine_serial
        self.remaining_buffer = ""
        self.response_queue = Queue()
        #after we have started let's see if the connection is alive
        command = self._read_next_command()
        if not command or command.return_code != -128:
            raise MachineError("Machine does not seem to be ready")
            #ok and if everything is nice we can start a nwe heartbeat thread
        self.last_heartbeat = time.clock()
        self.run_on = True
        self.listening_thread = Thread(target=self)

    def send_command(self, command):
        #empty the queue?? shouldn't it be empty??
        self.response_queue.empty()
        self.machine_serial.send(command.return_code)
        if command.arguments:
            self.machine_serial.send(",")
            for param in command.arguments[:-1]:
                self.machine_serial.send(param)
                self.machine_serial.send(",")
            self.machine_serial.send(command.arguments[-1])
            self.machine_serial.send(";")
        try:
            response = self.response_queue.get(block=True, timeout=_default_timeout)
           #TODO logging
            return response
        except Empty:
            #disconnect in panic
            self.run_on = False
            raise MachineError("Machine does not listen!")


    def last_heart_beat(self):
        if self.last_heartbeat:
            return time.clock() - self.last_heartbeat
        else:
            return None

    def __call__(self, *args, **kwargs):
        while self.run_on:
            command = self._read_next_command()
            if command:
                # if it is just the heart beat we write down the time
                if command.return_code == -128:
                    self.last_heartbeat = time.clock()
                else:
                    #we add it to the response queue
                    self.response_queue.put(command)

    def _read_next_command(self):
        line = self._doRead()   # read a ';' terminated line
        if not line or not line.strip():
            return None
        logging.info("machine said:\'" + line + "\'")
        command = MachineCommand(line)
        return command

    def _doRead(self):
        buff = self.remaining_buffer
        tic = time.time()
        buff += self.machineSerial.read()

        # you can use if not ('\n' in buff) too if you don't like re
        while ((time.time() - tic) < _default_timeout) and (not self._commandEndMatcher.search(buff)):
            buff += self.machineSerial.read()

        if self._commandEndMatcher.search(buff):
            split_result = buff.split(';', 1)
            self.remaining_buffer = split_result[1]
            return split_result[0]
        else:
            return ''


class MachineCommand():
    def __init__(self, input_line):
        parts = input_line.strip().split(",")
        self.return_code = int(parts[0])
        self.arguments = parts[1:]