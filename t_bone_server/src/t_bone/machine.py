from Queue import Queue, Empty
import logging
import re
from threading import Thread
import serial
import threading
import time
import Adafruit_BBIO.GPIO as GPIO

__author__ = 'marcus'

_default_timeout = 120
_commandEndMatcher = re.compile(";")  #needed to search for command ends
_min_command_buffer_free_space = 5  # how much arduino buffer to preserve
_initial_buffer_length = 20  #how much buffer do we need befoer starting to print
_buffer_empyting_wait_time = 0.1
_buffer_warn_waittime = 10
clock_frequency = 16000000

_logger = logging.getLogger(__name__)


class Machine():
    def __init__(self, serial_port, reset_pin):
        #preapre the reset pin
        self.reset_pin = reset_pin
        _logger.debug("Defining ports & pins")
        GPIO.setup(self.reset_pin, GPIO.OUT)
        GPIO.output(self.reset_pin, GPIO.HIGH)
        self.serial_port = serial_port
        self.remaining_buffer = ""
        self.machine_connection = None
        self.command_queue = Queue()
        self.batch_mode = False

    def connect(self):
        _logger.info("resetting arduino at %s", self.serial_port)
        GPIO.output(self.reset_pin, GPIO.LOW)
        #reset the arduino
        time.sleep(1)
        GPIO.output(self.reset_pin, GPIO.HIGH)
        time.sleep(15)
        _logger.info("waiting for arduino")
        if not self.machine_connection:
            machineSerial = serial.Serial(self.serial_port, 115200, timeout=_default_timeout)
            self.machine_connection = _MachineConnection(machineSerial)
        init_command = MachineCommand()
        init_command.command_number = 9
        reply = self.machine_connection.send_command(init_command)
        if reply.command_number != 0:
            _logger.fatal("Unable to start, received %s which is not OK", reply)
            raise MachineError("Unable to start")

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
            _logger.fatal("Unable to set motor current:%s", reply)
            raise MachineError("Unable to set motor current", reply)

    def invert_motor(self, motor=None, inverted=False):
        command = MachineCommand()
        command.command_number = 4
        if inverted:
            invert_value = -1
        else:
            invert_value = 1
        command.arguments = (
            int(motor),
            invert_value
        )
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 0:
            _logger.fatal("Unable to set motor current:%s", reply)
            raise MachineError("Unable to invert motor", reply)

    def configure_endstop(self, motor, position, end_stop_config):
        command = MachineCommand()
        command.command_number = 3

        type = end_stop_config['type']
        position = position
        if position == 'left':
            position_number = -1
        else:
            position_number = 1

        if type == 'real':
            polarity = end_stop_config['polarity']
            if polarity in ('negative', '-', 'neg'):
                polarity_token = -1
            else:
                polarity_token = 1

            command.arguments = (
                int(motor),
                position_number,
                1,  #1 is a real endstop
                polarity_token
            )
        else:
            command.arguments = (
                int(motor),
                position_number,
                0,  #1 is a virtual endstop
                int(end_stop_config['position'])
            )

        #send the command
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 0:
            _logger.fatal("Unable to configure end stops :%s", reply)
            raise MachineError("Unable to configure end stops", reply)

    def home(self, home_config, timeout):
        command = MachineCommand()
        command.command_number = 12
        if 'jerk' in home_config:
            #TODO why so complicated? can't it be integrated with the addition code later on ?
            jerk = int(home_config['jerk'])
        else:
            jerk = None
        if 'followers' in home_config:
            #we lazily add the motor in the printer - since it is quite easy to remove it here
            following = [motor for motor in home_config['followers'] if motor != home_config['motor']]
        else:
            following = None

        command.arguments = [
            int(home_config['motor']),
            int(home_config['timeout']),
            float(home_config['home_speed']),
            float(home_config['home_slow_speed']),
            int(home_config['home_retract']),
            float(home_config['acceleration']),
        ]
        if jerk:
            command.arguments.append(jerk)
        if following:
            for motor in following:
                command.arguments.append(int(motor))
        if 'homing_right_position' in home_config:
            command.arguments.append(int(home_config['homing_right_position']))

        command.arguments.append(int(0))
        reply = self.machine_connection.send_command(command, timeout)
        if not reply or reply.command_number != 0:
            _logger.fatal("Unable to home axis %s: %s", home_config, reply)
            raise MachineError("Unable to home axis " + str(home_config), reply)

    def set_pos(self, motor, pos):
        command = MachineCommand()
        command.command_number = 13
        command.arguments = [
            int(motor),
            int(pos)
        ]
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 0:
            _logger.fatal("Unable set axis %s to %s", motor, pos)
            raise MachineError("Unable to set axis " + str(motor) + " to " + str(pos), reply)

    def start_motion(self):
        _logger.info("Starting movement")
        start_command = MachineCommand()
        start_command.command_number = 11
        start_command.arguments = [1, _initial_buffer_length]
        reply = self.machine_connection.send_command(start_command)
        if not reply or not reply.command_number == 0:
            _logger.error("Unable to start motion", reply)
            raise MachineError("Unable to start motion")
        self.batch_mode = True


    def finish_motion(self):
        _logger.info("Finishing movement")
        stop_command = MachineCommand()
        stop_command.command_number = 11
        stop_command.arguments = [-1]
        reply = self.machine_connection.send_command(stop_command)
        if not reply or not reply.command_number == 0:
            _logger.error("Unable to stop motion", reply)
            raise MachineError("Unable to stop motion")
        self.batch_mode = False


    def move_to(self, motors):
        if not motors:
            logging.warn("no motor to move??")
            return
        command = MachineCommand()
        command.command_number = 10
        command.arguments = []
        _logger.info("Adding Move:")
        for motor in motors:
            command.arguments.append(int(motor['motor']))
            command.arguments.append(int(motor['target']))
            if motor['type'] == 'stop':
                command.arguments.append(ord('s'))
            else:
                command.arguments.append(ord('w'))
            command.arguments.append(float(motor['speed']))
            command.arguments.append(float(motor['acceleration']))
            command.arguments.append(int(motor['startBow']))
            _logger.debug("Motor %s to %s as %s with %s", int(motor['motor']), int(motor['target']), motor['type'],
                         motor['speed'])

        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 0:
            _logger.error("Unable to move Motor: %s -> %s", command, reply)
            raise MachineError("Unable to add motor move", reply)
        if self.batch_mode:
            command_buffer_length = int(reply.arguments[0])
            command_max_buffer_length = int(reply.arguments[1])
            command_buffer_free = command_max_buffer_length - command_buffer_length
            command_queue_running = int(reply.arguments[2]) > 0
            _logger.debug("Arduino command Buffer at %s of %s", command_buffer_length, command_max_buffer_length)
            if command_queue_running and command_buffer_free <= _min_command_buffer_free_space:
                buffer_free = False
                wait_time = 0
                while not buffer_free:
                    #sleep a bit
                    time.sleep(_buffer_empyting_wait_time)
                    wait_time += _buffer_empyting_wait_time
                    info_command = MachineCommand()
                    info_command.command_number = 31
                    reply = self.machine_connection.send_command(info_command)
                    if reply:
                        command_buffer_length = int(reply.arguments[0])
                        command_max_buffer_length = int(reply.arguments[1])
                        command_buffer_free = command_max_buffer_length - command_buffer_length
                        buffer_free = (command_buffer_free > _min_command_buffer_free_space)
                        if (wait_time > _buffer_warn_waittime):
                            _logger.warning(
                                "Waiting for free arduino command buffer: %s free of % s total, waiting for %s free",
                                command_buffer_free, command_buffer_length, _min_command_buffer_free_space)
                            wait_time = 0
                        else:
                            _logger.debug("waiting for free buffer")
                    else:
                        _logger.warn("Waiting for a free command timed out!")
        else:
            #while self.machine_connection.internal_queue_length > 0:
            pass  # just wait TODO timeout??

    def read_positon(self, motor):
        command = MachineCommand()
        command.command_number = 30
        command.arguments = [
            int(motor)
        ]
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 30:
            _logger.error("Unable read motor position: %s", reply)
            raise MachineError("Unable read motor position", reply)
        return int(reply.arguments[0])

    def read_current(self, input):
        command = MachineCommand()
        command.command_number = 41
        command.arguments = [
            int(input)
        ]
        reply = self.machine_connection.send_command(command)
        if not reply or reply.command_number != 41:
            _logger.error("Unable read current: %s", reply)
            raise MachineError("Unable read current", reply)
        return int(reply.arguments[1])


class _MachineConnection:
    def __init__(self, machine_serial):
        self.listening_thread = Thread(target=self)
        self.machine_serial = machine_serial
        self.remaining_buffer = ""
        self.response_queue = Queue()
        #let's suck empty the serial connection by reading everything with an extremely short timeout
        init_start = time.clock()
        last = ''
        while not last is ';' and time.clock() - init_start < _default_timeout:
            last = machine_serial.read()
            #after we have started let's see if the connection is alive
        command = None
        while (not command or command.command_number != -128) and time.clock() - init_start < _default_timeout:
            command = self._read_next_command()
        if not command or command.command_number != -128:
            raise MachineError("Machine does not seem to be ready")
            #ok and if everything is nice we can start a nwe heartbeat thread
        self.last_heartbeat = time.clock()
        self.run_on = True
        self.listening_thread.start()
        self.internal_queue_length = 0
        self.internal_queue_max_length = 1
        self.serial_lock = threading.Lock()

    def send_command(self, command, timeout=None):
        with self.serial_lock:
            _logger.debug("sending command %s", command)
            if not timeout:
                timeout = _default_timeout
                #empty the queue?? shouldn't it be empty??
            self.response_queue.empty()
            self.machine_serial.write(str(command.command_number))
            if command.arguments:
                self.machine_serial.write(",")
                for param in command.arguments[:-1]:
                    self.machine_serial.write(repr(param))
                    self.machine_serial.write(",")
                self.machine_serial.write(repr(command.arguments[-1]))
            self.machine_serial.write(";\n")
            self.machine_serial.flush()
            try:
                response = self.response_queue.get(timeout=timeout)
                _logger.debug("Received %s as response to %s", response, command)
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
                    if command.arguments and len(command.arguments) == 2:
                        self.internal_queue_length = command.arguments[0]
                        self.internal_queue_max_length = command.arguments[1]
                    else:
                        _logger.warn("did not understand status command %s", command)
                else:
                    #we add it to the response queue
                    self.response_queue.put(command)
                    _logger.debug("received command %s", command)

    def _read_next_command(self):
        line = self._doRead()  # read a ';' terminated line
        if not line or not line.strip():
            return None
        line = line.strip()
        _logger.debug("machine said:\'%s\'", line)
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
                    _logger.warn("unable to decode command: %s", input_line)

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
                result = "Unkown "
        else:
            result = "Command "
        if self.command_number is not None:
            result += str(self.command_number)
        if self.arguments:
            result += ": "
            result += str(self.arguments)
        return result


class MachineError(Exception):
    def __init__(self, msg, additional_info=None):
        self.msg = msg
        self.additional_info = additional_info

    def __str__(self):
        return super(MachineError, self).__str__() + "[" + str(self.additional_info) + "]"


