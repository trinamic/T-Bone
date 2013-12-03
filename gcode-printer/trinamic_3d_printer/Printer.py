# coding=utf-8
from collections import defaultdict
import logging
from math import sqrt
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
        self.machine.batch_mode = True

    def stop_print(self):
        pass

    # tuple with x/y/e coordinates - if left out no change is intenden
    def move_to(self, position):
        #extract and convert values
        if 'x' in position:
            x_move = position['x']
            x_step = _convert_mm_to_steps(x_move, self.x_axis_scale)
            delta_x = x_move - self.x_pos
        else:
            x_step = None
            delta_x = 0
        if 'y' in position:
            y_move = position['y']
            y_step = _convert_mm_to_steps(y_move, self.y_axis_scale)
            delta_y = y_move - self.y_pos
        else:
            y_step = None
            delta_y = 0
        if 'f' in position:
            target_speed = position['f']
            move_speed = target_speed
        else:
            target_speed = None
            move_speed = self.current_speed
            #next store new current positions

        move_vector = calculate_relative_vector(delta_x, delta_y)
        #derrive the various speed vectors from the movement … for desired head and maximum axis speed
        speed_vectors = [
            {
                # add the desired speed vector as initial value
                'x': move_speed * move_vector['x'],
                'y': move_speed * move_vector['y']
            }
        ]
        if move_vector['x'] != 0:
            speed_vectors.append({
                #what would the speed vector for max x speed look like
                'x': self.x_axis_max_speed,
                'y': self.x_axis_max_speed * move_vector['y'] / move_vector['x']
            })
        if move_vector['y'] != 0:
            speed_vectors.append({
                #what would the maximum speed vector for y movement look like
                'x': self.y_axis_max_speed * move_vector['x'] / move_vector['y'],
                'y': self.y_axis_max_speed
            })

        speed_vector = find_shortest_vector(speed_vectors)
        #and finally find the shortest speed vector …
        step_speed_vector = {
            'x': _convert_mm_to_steps(speed_vector['x'], self.x_axis_scale),
            'y': _convert_mm_to_steps(speed_vector['y'], self.y_axis_scale)
        }

        if delta_x and not delta_y: #silly, but simpler to understand
            #move x motor
            _logger.debug("Moving X axis to " + str(x_step))
            self.machine.move_to((
                {
                    'motor': self.x_axis_motor,
                    'target:': x_step,
                    'speed': step_speed_vector['x'],
                    'acceleration': self.x_axis_max_step_acceleration,
                    'deceleration': self.x_axis_max_step_acceleration,
                    'startBow': self.x_axis_bow,
                    'endBow': self.x_axis_bow
                }
            ))
        elif delta_y and not delta_x: # still silly, but stil easier to understand
            #move y motor to position
            _logger.debug("Moving Y axis to " + str(y_step))
            self.machine.move_to((
                {
                    'motor': self.y_axis_motor,
                    'target:': y_step,
                    'speed': step_speed_vector['y'],
                    'acceleration': self.y_axis_max_step_acceleration,
                    'deceleration': self.y_axis_max_step_acceleration,
                    'startBow': self.y_axis_bow,
                    'endBow': self.y_axis_bow
                }
            ))
        elif delta_x and delta_y:
            #ok we have to see which axis has bigger movement
            if abs(delta_x) > abs(delta_y):
                y_factor = move_vector['y'] / move_vector['x']
                _logger.info("Moving X axis to " + str(x_step) + " gearing Y by " + str(y_factor))
                self.machine.move_to((
                    {
                        'motor': self.x_axis_motor,
                        'target:': x_step,
                        'speed': step_speed_vector['x'],
                        'acceleration': self.x_axis_max_step_acceleration,
                        'deceleration': self.x_axis_max_step_acceleration,
                        'startBow': self.x_axis_bow,
                        'endBow': self.x_axis_bow
                    }, {
                        'motor': self.y_axis_motor,
                        'target:': y_step,
                        'speed': step_speed_vector['y'],
                        'acceleration': self.y_axis_max_step_acceleration * y_factor,
                        'deceleration': self.y_axis_max_step_acceleration * y_factor,
                        'startBow': self.y_axis_bow * y_factor,
                        'endBow': self.y_axis_bow * y_factor
                    }
                ))
                #move
            else:
                x_factor = move_vector['x'] / move_vector['y']
                _logger.info("Moving Y axis to " + str(y_step) + " gearing X by " + str(x_factor))
                self.machine.move_to((
                    {
                        'motor': self.x_axis_motor,
                        'target:': x_step,
                        'speed': step_speed_vector['x'],
                        'acceleration': self.x_axis_max_step_acceleration * x_factor,
                        'deceleration': self.x_axis_max_step_acceleration * x_factor,
                        'startBow': self.x_axis_bow * x_factor,
                        'endBow': self.x_axis_bow * x_factor
                    }, {
                        'motor': self.y_axis_motor,
                        'target:': y_step,
                        'speed': step_speed_vector['y'],
                        'acceleration': self.y_axis_max_step_acceleration,
                        'deceleration': self.y_axis_max_step_acceleration,
                        'startBow': self.y_axis_bow,
                        'endBow': self.y_axis_bow
                    }
                ))
                #move
        if not target_speed == None:
            self.current_speed = target_speed

    def configure_axis_motor(self, axis_config):
        motor = axis_config["motor"]
        current = axis_config["current"]
        self.machine.set_current(motor, current)


def _convert_mm_to_steps(millimeters, conversion_factor):
    if millimeters is None:
        return None
    return int(millimeters * conversion_factor)


def calculate_relative_vector(delta_x, delta_y):
    length = sqrt(delta_x ** 2 + delta_y ** 2)
    if length == 0:
        return {
            'x': 0.0,
            'y': 0.0,
            'l': 0.0
        }
    return {
        'x': float(delta_x) / length,
        'y': float(delta_y) / length,
        'l': 1.0
    }


def find_shortest_vector(vector_list):
    find_list = list(vector_list)
    #ensure it is a list
    shortest_vector = 0
    #and wildly guess the shortest vector
    for number, vector in enumerate(find_list):
        if vector['x'] < find_list[shortest_vector]['x']:
            shortest_vector = number
    return find_list[shortest_vector]


class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg

