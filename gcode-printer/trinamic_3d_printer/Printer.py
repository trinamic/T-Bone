# coding=utf-8
from collections import defaultdict
import logging
from math import sqrt
from trinamic_3d_printer.Machine import Machine

__author__ = 'marcus'
_logger = logging.getLogger(__name__)


class Printer():
    def __init__(self, print_queue_length=100):
        self.ready = False
        self.config = None
        self.axis = {'x': {}, 'y': {}}
        self.axis['x']['motor'] = None
        self.axis['y']['motor'] = None
        self.axis['x']['scale'] = None
        self.axis['y']['scale'] = None
        self.axis['x']['max_speed'] = None
        self.axis['y']['max_speed'] = None
        self.axis['x']['max_acceleration'] = None
        self.axis['y']['max_acceleration'] = None
        self.axis['x']['bow'] = None
        self.axis['y']['bow'] = None

        #this will be removed
        self.x_pos = None
        self.x_pos_step = None
        self.y_pos = None
        self.y_pos_step = None
        self.current_speed = 0



        #finally create and conect the machine
        self.machine = Machine()
        self.machine.connect()

    def configure(self, config):
        if not config:
            raise PrinterError("No printer config given!")

        self._configure_axis(self.axis['x'], config["x-axis"])
        self._configure_axis(self.axis['y'], config["y-axis"])

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
            x_step = _convert_mm_to_steps(x_move, self.axis['x']['scale'])
            delta_x = x_move - self.x_pos
        else:
            x_move = None
            x_step = None
            delta_x = 0
        if 'y' in position:
            y_move = position['y']
            y_step = _convert_mm_to_steps(y_move, self.axis['y']['scale'])
            delta_y = y_move - self.y_pos
        else:
            y_step = None
            y_move = None
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
                'x': self.axis['x']['max_speed'],
                'y': self.axis['x']['max_speed'] * move_vector['y'] / move_vector['x']
            })
        if move_vector['y'] != 0:
            speed_vectors.append({
                #what would the maximum speed vector for y movement look like
                'x': self.axis['x']['max_speed'] * move_vector['x'] / move_vector['y'],
                'y': self.axis['y']['max_speed']
            })
        speed_vector = find_shortest_vector(speed_vectors)
        #and finally find the shortest speed vector …
        step_speed_vector = {
            'x': abs(_convert_mm_to_steps(speed_vector['x'], self.axis['x']['scale'])),
            'y': abs(_convert_mm_to_steps(speed_vector['y'], self.axis['y']['scale']))
        }

        def _axis_movement_template(axis):
            return {
                'motor': axis['motor'],
                'acceleration': axis['max_step_acceleration'],
                'deceleration': axis['max_step_acceleration'],
                'startBow': axis['bow'],
                'endBow': axis['bow']
            }

        x_move_config = _axis_movement_template(self.axis['x'])
        x_move_config['target'] = x_step
        x_move_config['speed'] = step_speed_vector['x']
        y_move_config = _axis_movement_template(self.axis['y'])
        y_move_config['target'] = x_step
        y_move_config['speed'] = step_speed_vector['y']

        if delta_x and not delta_y: #silly, but simpler to understand
            #move x motor
            _logger.debug("Moving X axis to " + str(x_step))

            self.machine.move_to([
                x_move_config
            ])

        elif delta_y and not delta_x: # still silly, but stil easier to understand
            #move y motor to position
            _logger.debug("Moving Y axis to " + str(y_step))

            self.machine.move_to([
                y_move_config
            ])
        elif delta_x and delta_y:
            #ok we have to see which axis has bigger movement
            if abs(delta_x) > abs(delta_y):
                y_factor = abs(move_vector['y'] / move_vector['x'])
                _logger.info(
                    "Moving X axis to " + str(x_step) + " gearing Y by " + str(y_factor) + " to " + str(y_step))

                y_move_config['acceleration'] *= y_factor
                y_move_config['deceleration'] *= y_factor
                self.machine.move_to([
                    x_move_config,
                    y_move_config
                ])
                #move
            else:
                x_factor = abs(move_vector['x'] / move_vector['y'])
                _logger.info("Moving Y axis to " + str(y_step) + " gearing X by " + str(x_factor))
                x_move_config['acceleration'] *= x_factor
                x_move_config['deceleration'] *= x_factor
                self.machine.move_to([
                    x_move_config,
                    y_move_config
                ])
                #move
        if not target_speed == None:
            #finally update the state
            self.current_speed = target_speed
        if x_move:
            self.x_pos = x_move
            self.x_pos_step = x_step
        if y_move:
            self.y_pos = y_move
            self.y_pos_step = y_step

    def _configure_axis(self, axis, config):
        axis['motor'] = config['motor']
        axis['scale'] = config['steps-per-mm']
        axis['max_speed'] = config['max-speed']
        axis['max_acceleration'] = config['max-acceleration']
        axis['bow'] = config['bow-acceleration']

        motor = config["motor"]
        current = config["current"]
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


'''
class PrintQueue():
    def __init__(self, queue_size = 100):
'''


class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg

