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
_commandEndMatcher = re.compile(";")    #needed to search for command ends

_logger = logging.getLogger(__name__)

def matcher(buff):
    pass


class MachineError(Exception):
    def __init__(self, msg, additional_info):
        self.msg = msg
        self.additional_info = additional_info


class Machine():
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
        init_command = MachineCommand()
        init_command.command_number = 9
        reply = self.machine_connection.send_command(init_command)
        if reply.command_number != 0:
            raise MachineCommand("Unable to start")

    def disconnect(self):
        if self.machine_connection:
            self.machine_connection.run_on = False

    def set_current(self, motor=None, current=None):
        command = MachineCommand()
        command.command_number = 1
        command.arguments = (
            int(motor),
            int(current * 1000)
        )
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 0:
            raise MachineError("Unable to set motor current", reply)


class _MachineConnection:
    def __init__(self, machine_serial):
        self.listening_thread = Thread(target=self)
        self.machine_serial = machine_serial
        self.remaining_buffer = ""
        self.response_queue = Queue()
        #let's suck empty the serial connection by reading everything with an extremely short timeout
        dummy_read = "dummy"
        while machine_serial.inWaiting():
            machine_serial.read()
            #after we have started let's see if the connection is alive
        command = self._read_next_command()
        if not command or command.command_number != -128:
            raise MachineError("Machine does not seem to be ready")
            #ok and if everything is nice we can start a nwe heartbeat thread
        self.last_heartbeat = time.clock()
        self.run_on = True
        self.listening_thread.start()
        self.internal_queue_length = 0
        self.internal_queue_max_length = 1
        self.internal_free_ram = 0

    def send_command(self, command):
        _logger.info("sending command " + str(command))
        #empty the queue?? shouldn't it be empty??
        self.response_queue.empty()
        self.machine_serial.write(str(command.command_number))
        if command.arguments:
            self.machine_serial.write(",")
            for param in command.arguments[:-1]:
                self.machine_serial.write(str(param))
                self.machine_serial.write(",")
            self.machine_serial.write(str(command.arguments[-1]))
        self.machine_serial.write(";\n")
        self.machine_serial.flush()
        try:
            response = self.response_queue.get(timeout=_default_timeout)
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
                if command.command_number == -128:
                    self.last_heartbeat = time.clock()
                    if command.arguments:
                        self.internal_queue_length = command.arguments[0]
                        self.internal_queue_max_length = command.arguments[1]
                        self.internal_free_ram = command.arguments[2]
                else:
                    #we add it to the response queue
                    self.response_queue.put(command)
                    _logger.info("received command " + str(command))

    def _read_next_command(self):
        line = self._doRead()   # read a ';' terminated line
        if not line or not line.strip():
            return None
        line = line.strip()
        _logger.debug("machine said:\'" + line + "\'")
        command = MachineCommand(line)
        return command

    def _doRead(self):
        buff = self.remaining_buffer
        tic = time.time()
        buff += self.machine_serial.read()

        # you can use if not ('\n' in buff) too if you don't like re
        while ((time.time() - tic) < _default_timeout) and (not _commandEndMatcher.search(buff)):
            buff += self.machine_serial.read()

        if _commandEndMatcher.search(buff):
            split_result = buff.split(';', 1)
            self.remaining_buffer = split_result[1]
            return split_result[0]
        else:
            return ''


class MachineCommand():
    def __init__(self, input_line=None):
        self.command_number = None
        self.arguments = None
        if input_line:
            parts = input_line.strip().split(",")
            if len(parts) > 1:
                try:
                    self.command_number = int(parts[0])
                    if len(parts) > 1:
                        self.arguments = parts[1:]
                except ValueError:
                    _logger.warn("unable to decode command:" + input_line)

    def __repr__(self):
        if self.command_number == 0:
            result = "Acknowledgement "
        if self.command_number < 0:
            if self.command_number > -5:
                result = "Info "
            elif self.command_number > -9:
                result = "Warning "
            elif self.command_number == -9:
                result = "Error "
            elif self.command_number == -128:
                result = "Keep Alive Ping "
        else:
            result = "Command "
        if self.command_number is not None:
            result += str(self.command_number)
        if self.arguments:
            result += ": "
            result += str(self.arguments)
        return result