from collections import defaultdict
import logging
from trinamic_3d_printer.Machine import Machine

__author__ = 'marcus'
_logger = logging.getLogger(__name__)


class Printer():
    def __init__(self):
        self.ready = False
        self.config = None
        self.x_axis_motor = None
        self.y_axis_motor = None
        self.x_axis_scale = None
        self.y_axis_scale = None
        self.x_pos = None
        self.y_pos = None
        self.x_axis_max_speed = None
        self.x_axis_max_acceleration = None
        self.y_axis_max_speed = None
        self.y_axis_max_acceleration = None
        self.current_speed = 0

        #finally create and conect the machine
        self.machine = Machine()
        self.machine.connect()

    def configure(self, config):
        if not config:
            raise PrinterError("No printer config given!")

        x_axis_config = config["x-axis"]
        self.x_axis_motor = x_axis_config["motor"]
        self.configure_axis_motor(x_axis_config)
        self.x_axis_scale = x_axis_config["steps-per-mm"]
        self.x_axis_max_speed = x_axis_config["max-speed"]
        self.x_axis_max_acceleration = x_axis_config["max-acceleration"]

        y_axis_config = config["y-axis"]
        self.y_axis_motor = y_axis_config["motor"]
        self.configure_axis_motor(y_axis_config)
        self.y_axis_scale = y_axis_config["steps-per-mm"]
        self.y_axis_max_speed = y_axis_config["max-speed"]
        self.y_axis_max_acceleration = y_axis_config["max-acceleration"]

        self.config = config

        #todo in thery we should have homed here
        self.x_pos = 0
        self.y_pos = 0

    def start_print(self):
        self.machine.batch_mode=True

    def stop_print(self):
        pass

    # tuple with x/y/e coordinates - if left out no change is intenden
    def move_to(self, position):
        #extract and convert values
        if 'x' in position:
            x_move = position['x']
        else:
            x_move = None
        if 'y' in position:
            y_move = position['y']
        else:
            y_move = None
        if 'f' in position:
            target_speed = position['f']
            move_speed = target_speed
            #todo convert to steps??
        else:
            target_speed = None
            move_speed = self.current_speed
            #next store new current positions
        #todo or is there any advantage in storing real world values??
        delta_x = None
        if x_move:
            x_step = _convert_mm_to_steps(x_move, self.x_axis_scale)
            delta_x = x_step - self.x_pos
            self.x_pos = x_step
        delta_y = None
        if y_move:
            y_step = _convert_mm_to_steps(y_move, self.y_axis_scale)
            delta_y = y_step - self.y_pos
            self.y_pos = y_step
            #now we can decide which axis to move
        if delta_x and not delta_y: #silly, but simpler to understand
            #move x motor
            _logger.debug("Moving X axis to " + str(x_step))
            self.machine.move_to(self.x_axis_motor, x_step, move_speed)
        elif delta_y and not delta_x: # still silly, but stil easier to understand
            #move y motor to position
            _logger.debug("Moving Y axis to " + str(y_step))
            self.machine.move_to(self.y_axis_motor, y_step, move_speed)
        elif delta_x and delta_y:
            #ok we have to see which axis has bigger movement
            if (delta_x > delta_y):
                y_gearing = float(delta_y) / float(delta_x)
                _logger.debug("Moving X axis to " + str(x_step) + " gearing y by " + str(y_gearing))
                self.machine.move_to(self.x_axis_motor, x_step, move_speed, [{
                    'motor': self.y_axis_motor,
                    'gearing': y_gearing
                }])
                #move
            else:
                x_gearing = float(delta_x) / float(delta_y)
                _logger.debug("Moving Y axis to " + str(y_step) + " gearing y by " + str(x_gearing))
                self.machine.move_to(self.x_axis_motor, x_step, move_speed, [{
                    'motor': self.x_axis_motor,
                    'gearing': x_gearing
                }])
                #move
        if not target_speed == None:
            self.current_speed = target_speed

    def configure_axis_motor(self, axis_config):
        motor = axis_config["motor"]
        current = axis_config["current"]
        self.machine.set_current(motor, current)

        max_acceleration = axis_config["max-acceleration"]
        axis_scale = axis_config["steps-per-mm"]

        self.machine.set_acceleration_settings(motor, max_acceleration*axis_scale)

def _convert_mm_to_steps(millimeters, conversion_factor):
    if millimeters is None:
        return None
    return int(millimeters * conversion_factor)


class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg

