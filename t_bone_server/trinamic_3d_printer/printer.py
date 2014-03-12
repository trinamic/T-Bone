# coding=utf-8
from Queue import Queue, Empty
from copy import deepcopy
import logging
from math import sqrt, copysign
from numpy import sign
from threading import Thread
from trinamic_3d_printer.machine import Machine
from trinamic_3d_printer.helpers import convert_mm_to_steps, find_shortest_vector, calculate_relative_vector, \
    convert_velocity_clock_ref_to_realtime_ref, convert_acceleration_clock_ref_to_realtime_ref

__author__ = 'marcus'
_logger = logging.getLogger(__name__)
_axis_config = {
    #maps axis name to config entry
    'x': 'x-axis',
    'y': 'x-axis',
    'z': 'x-axis',
    'e': 'extruder',
}


class Printer(Thread):
    def __init__(self, serial_port, reset_pin, print_queue_min_length=50, print_queue_max_length=100):
        Thread.__init__(self)
        self.ready = False
        self.printing = False
        self.config = None
        self.homed_axis = []

        self.printer_thread = None
        self._print_queue = None
        self.print_queue_min_length = print_queue_min_length
        self.print_queue_max_length = print_queue_max_length
        self._default_homing_retraction = None
        self._x_step_conversion = None
        self._y_step_conversion = None

        self._homing_timeout = 10
        self._print_queue_wait_time = 0.1

        #finally create the machine
        self.machine = Machine(serial_port=serial_port, reset_pin=reset_pin)

    def axis_names(self):
        names = list(self.axis.keys())
        names.sort()
        return names

    def configure(self, config):
        if not config:
            raise PrinterError("No printer config given!")

        self.config = config

        printer_config = config['printer']
        print_queue_config = printer_config["print-queue"]
        self.print_queue_min_length = print_queue_config['min-length']
        self.print_queue_max_length = print_queue_config['max-length']
        self._homing_timeout = printer_config['homing-timeout']
        self._default_homing_retraction = printer_config['home-retract']

        self.axis = {}
        for axis_name, config_name in _axis_config.iteritems():
            _logger.info("Configuring axis \'%s\' according to conf \'%s\'", axis_name, config_name)
            axis = {}
            axis['name'] = axis_name
            self.axis[axis_name] = axis
            self._configure_axis(axis, config[config_name])
        self._postconfig()

    def _postconfig(self):
        #we need the stepping rations for variuos calclutaions later
        self._x_step_conversion = float(self.axis['x']['steps_per_mm']) / float(self.axis['y']['steps_per_mm'])
        self._y_step_conversion = float(self.axis['y']['steps_per_mm']) / float(self.axis['x']['steps_per_mm'])

        self._extract_homing_information()

        self.ready = True

    def connect(self):
        _logger.debug("Connecting printer")
        self.machine.connect()


    def start_print(self):
        self._print_queue = PrintQueue(axis_config=self.axis, min_length=self.print_queue_min_length,
                                       max_length=self.print_queue_max_length)
        self.machine.start_motion()
        self.printing = True
        self.start()

    def finish_print(self):
        self._print_queue.finish()
        self.machine.finish_motion()
        self.printing = False
        pass

    def read_motor_positons(self):
        positions = {}
        for axis_name in self.axis:
            axis_config = self.axis[axis_name]
            motor = axis_config['motor']
            position = self.machine.read_positon(motor)
            positions[axis_name] = position * axis_config['steps_per_mm']
        return positions

    def _extract_homing_information(self):
        for axis_name, axis in self.axis.iteritems():
            axis['homeable'] = False
            if 'end-stops' in axis:
                for position in ['left', 'right']:
                    if position in axis['end-stops'] and not 'virtual' == axis['end-stops'][position]:
                        axis['homeable'] = True
                        axis['homed'] = False
                        break

    def home(self, axis):
        for home_axis in axis:
            _logger.info("Homing axis \'%s\' to zero", home_axis)
            #read the homing config for the axis
            home_speed = self.axis[home_axis]['home_speed']
            home_precision_speed = self.axis[home_axis]['home_precision_speed']
            home_acceleration = self.axis[home_axis]['home_acceleration']
            home_retract = self.axis[home_axis]['home_retract']
            #convert everything from mm to steps
            home_speed = convert_mm_to_steps(home_speed, self.axis[home_axis]['steps_per_mm'])
            home_precision_speed = convert_mm_to_steps(home_precision_speed, self.axis[home_axis]['steps_per_mm'])
            home_acceleration = convert_mm_to_steps(home_acceleration, self.axis[home_axis]['steps_per_mm'])
            if self.axis[home_axis]['clock-referenced']:
                home_speed = convert_velocity_clock_ref_to_realtime_ref(home_speed)
                home_precision_speed = convert_velocity_clock_ref_to_realtime_ref(home_precision_speed)
                home_acceleration = convert_acceleration_clock_ref_to_realtime_ref(home_acceleration)
                #no home jerk - since this only applies to 5041 axis w/o jerk control
            home_retract = convert_mm_to_steps(home_retract, self.axis[home_axis]['steps_per_mm'])
            #make a config out of it
            if self.axis[home_axis]['motor']:
                homing_config = {
                    'motor': self.axis[home_axis]['motor'],
                    'timeout': 0,
                    'home_speed': home_speed,
                    'home_slow_speed': home_precision_speed,
                    'home_retract': home_retract,
                    'acceleration': home_acceleration,
                }
                if self.axis[home_axis]['bow_step']:
                    homing_config['jerk'] = self.axis[home_axis]['bow_step']
            else:
                #todo we should check if there is a motor for the left endstop??
                homing_config = {
                    'motor': self.axis[home_axis]['end-stops']['left']['motor'],
                    'followers': self.axis[home_axis]['motors'],
                    'timeout': 0,
                    'home_speed': home_speed,
                    'home_slow_speed': home_precision_speed,
                    'home_retract': home_retract,
                    'acceleration': home_acceleration,
                }
                if self.axis[home_axis]['bow_step']:
                    homing_config['bow'] = self.axis[home_axis]['bow_step']

            #and do the homing
            self.machine.home(homing_config, timeout=self._homing_timeout)
            #better but still not good - we should have a better concept of 'axis'
            self.axis[home_axis]['homed'] = True
        self.x_pos = 0
        self.y_pos = 0
        self.z_pos = 0


    # tuple with x/y/e coordinates - if left out no change is intended
    def move_to(self, position):
        self._print_queue.add_movement(position)

    def run(self):
        while self.printing:
            try:
                #get the next movement from stack
                movement = self._print_queue.next_movement(self._print_queue_wait_time)
                step_pos, step_speed_vector = self._add_movement_calculations(movement)
                x_move_config, y_move_config, z_move_config = self._generate_move_config(movement, step_pos,
                                                                                         step_speed_vector)
                self._move(movement, step_pos, x_move_config, y_move_config, z_move_config)
            except Empty:
                _logger.debug("Print Queue did not return a value - this can be pretty normal")

    def _configure_axis(self, axis, config):
        #let's see if we got one or more motors
        if 'motor' in config:
            axis['motor'] = config['motor']
        elif 'motors' in config:
            axis['motor'] = None
            axis['motors'] = config['motors']
        else:
            raise PrinterError("you must configure one ('motor') or more 'motors' in the axis configuration")

        axis['steps_per_mm'] = config['steps-per-mm']
        if 'time-reference' in config and config['time-reference'] == 'clock signal':
            axis['clock-referenced'] = True
        else:
            axis['clock-referenced'] = False

        axis['max_speed'] = config['max-speed']
        #todo - this can be clock signal referenced - convert acc. to  axis['clock-referenced']
        axis['max_speed_step'] = convert_mm_to_steps(config['max-speed'], config['steps-per-mm'])
        axis['max_acceleration'] = config['max-acceleration']
        axis['max_step_acceleration'] = convert_mm_to_steps(config['max-acceleration'], config['steps-per-mm'])
        if 'bow-acceleration' in config:
            axis['bow'] = config['bow-acceleration']
            axis['bow_step'] = convert_mm_to_steps(config['bow-acceleration'], config['steps-per-mm'])
        else:
            axis['bow'] = None
            axis['bow_step'] = None

        if 'home-speed' in config:
            axis['home_speed'] = config['home-speed']
        else:
            axis['home_speed'] = config['max-speed']
        if 'home-precision-speed' in config:
            axis['home_precision_speed'] = config['home-precision-speed']
        else:
            axis['home_precision_speed'] = config['max-speed'] / 10
        if 'home_acceleration' in config:
            axis['home_acceleration'] = config['home-acceleration']
        else:
            axis['home_acceleration'] = config['max-acceleration']
        if 'home-retract' in config:
            axis['home_retract'] = config['home-retract']
        else:
            axis['home_retract'] = self._default_homing_retraction

        axis['end-stops'] = {}
        if 'end-stops' in config:
            _logger.debug("Configuring endstops for axis %s", axis['name'])
            end_stops_config = config['end-stops']
            for end_stop_pos in ('left', 'right'):
                if end_stop_pos in end_stops_config:
                    _logger.debug("Configuring %s endstops", end_stop_pos)
                    end_stop_config = end_stops_config[end_stop_pos]
                    polarity = end_stop_config['polarity']
                    if 'virtual' == polarity:
                        position = float(end_stop_config['position'])
                        _logger.debug(" %s endstop is virtual at %s", end_stop_pos, position)
                        axis['end-stops'][end_stop_pos] = {
                            'type': 'virtual',
                            'position': position
                        }
                    elif polarity in ('positive', 'negative'):
                        _logger.debug(" %s endstop is real with %s polarity", end_stop_pos, polarity)
                        axis['end-stops'][end_stop_pos] = {
                            'type': 'real',
                            'polarity': polarity
                        }
                        if 'motor' in end_stop_config:
                            motor_ = end_stop_config['motor']
                            _logger.debug(" %s endstops applies to motor %s", end_stop_pos, motor_)
                            axis['end-stops'][end_stop_pos]['motor'] = motor_
                    else:
                        raise PrinterError("Unknown end stop type " + polarity)
                    end_stop = deepcopy(axis['end-stops'][end_stop_pos])
                    if 'position' in end_stop:
                        end_stop['position'] = convert_mm_to_steps(end_stop['position'], axis['steps_per_mm'])
                    if axis['motor']:
                        self.machine.configure_endstop(motor=axis['motor'], position=end_stop_pos,
                                                       end_stop_config=end_stop)
                    else:
                        #endstop config is a bit more complicated for multiple motors
                        if end_stop_config['polarity'] == 'virtual':
                            for motor in axis['motors']:
                                self.machine.configure_endstop(motor=motor, position=end_stop_pos,
                                                               end_stop_config=end_stop)
                        else:
                            if 'motor' in end_stop_config:
                                motor = end_stop_config['motor']
                            else:
                                motor = axis['motors'][0]
                            self.machine.configure_endstop(motor=motor, position=end_stop_pos, end_stop_config=end_stop)
        else:
            _logger.debug("No endstops for axis %s", axis['name'])

        current = config["current"]
        if axis["motor"]:
            self.machine.set_current(axis["motor"], current)
        else:
            for motor in axis['motors']:
                self.machine.set_current(motor, current)

        #let's see if there are any inverted motors
        if 'motor' in config:
            if "inverted" in config and config["inverted"]:
                axis['inverted'] = True
            else:
                axis['inverted'] = False
            self.machine.invert_motor(axis["motor"], axis['inverted'])
        else:
            #todo this is ok - but no perfect structure
            if "inverted" in config:
                axis['inverted'] = config["inverted"]
                for motor in axis['motors']:
                    if str(motor) in config['inverted'] and config['inverted'][str(motor)]:
                        self.machine.invert_motor(motor, True)


    def _add_movement_calculations(self, movement):
        step_pos = {
            'x': convert_mm_to_steps(movement['x'], self.axis['x']['steps_per_mm']),
            'y': convert_mm_to_steps(movement['y'], self.axis['y']['steps_per_mm']),
            'z': convert_mm_to_steps(movement['z'], self.axis['z']['steps_per_mm'])
        }
        max_z_speed = min(movement['target_speed'], self.axis['z']['max_speed'])
        step_speed_vector = {
            #todo - this can be clock signal referenced - convert acc. to  axis['clock-referenced']
            'x': convert_mm_to_steps(movement['speed']['x'], self.axis['x']['steps_per_mm']),
            'y': convert_mm_to_steps(movement['speed']['y'], self.axis['y']['steps_per_mm']),
            'z': convert_mm_to_steps(max_z_speed, self.axis['z']['steps_per_mm'])
        }
        return step_pos, step_speed_vector

    def _generate_move_config(self, movement, step_pos, step_speed_vector):
        #todo can't we null movements if there is no delta??
        def _axis_movement_template(axis):
            return {
                'motor': axis['motor'],
                'acceleration': axis['max_step_acceleration'],
                'startBow': axis['bow_step'],
            }

        if movement['delta_x']:
            x_move_config = _axis_movement_template(self.axis['x'])
            x_move_config['target'] = step_pos['x']
            x_move_config['speed'] = abs(step_speed_vector['x'])
            if 'x_stop' in movement:
                x_move_config['type'] = 'stop'
            else:
                x_move_config['type'] = 'way'
        else:
            x_move_config = None

        if movement['delta_y']:
            y_move_config = _axis_movement_template(self.axis['y'])
            y_move_config['target'] = step_pos['y']
            y_move_config['speed'] = abs(step_speed_vector['y'])
            if 'y_stop' in movement:
                y_move_config['type'] = 'stop'
            else:
                y_move_config['type'] = 'way'
        else:
            y_move_config = None

        if movement['delta_z']:
            z_move_config = [
                {
                    'motor': self.axis['z']['motors'][0],
                    'target': step_pos['z'],
                    'acceleration': self.axis['z']['max_step_acceleration'],
                    'speed': abs(step_speed_vector['z']),
                    'type': 'stop',
                    'startBow': 0
                },
                {
                    'motor': self.axis['z']['motors'][1],
                    'target': step_pos['z'],
                    'acceleration': self.axis['z']['max_step_acceleration'],
                    'speed': abs(step_speed_vector['z']),
                    'type': 'stop',
                    'startBow': 0
                }
            ]
        else:
            z_move_config = None

        return x_move_config, y_move_config, z_move_config

    def _move(self, movement, step_pos, x_move_config, y_move_config, z_move_config):
        move_vector = movement['relative_move_vector']
        move_commands = []
        if x_move_config and not y_move_config:  #silly, but simpler to understand
            #move x motor
            _logger.debug("Moving X axis to %s", step_pos['x'])

            move_commands = [
                x_move_config
            ]

        elif y_move_config and not x_move_config:  # still silly, but stil easier to understand
            #move y motor to position
            _logger.debug("Moving Y axis to %s", step_pos['y'])

            move_commands = [
                y_move_config
            ]
        elif x_move_config and y_move_config:
            #ok we have to see which axis has bigger movement
            if abs(movement['delta_x']) > abs(movement['delta_y']):
                y_factor = abs(move_vector['y'] / move_vector['x'] * self._y_step_conversion)
                _logger.debug(
                    "Moving X axis to %s gearing Y by %s to %s"
                    , step_pos['x'], y_factor, step_pos['y'])

                y_move_config['acceleration'] = x_move_config[
                                                    'acceleration'] * y_factor  # todo or the max of the config/scaeld??
                y_move_config['startBow'] = x_move_config['startBow'] * y_factor

            else:
                x_factor = abs(move_vector['x'] / move_vector['y'] * self._x_step_conversion)
                _logger.debug(
                    "Moving Y axis to %s gearing X by %s  to %s"
                    , step_pos['x'], x_factor, step_pos['y'])

                x_move_config['acceleration'] = y_move_config[
                                                    'acceleration'] * x_factor  # todo or the max of the config/scaeld??
                x_move_config['startBow'] = y_move_config['startBow'] * x_factor

            #move
            move_commands = [
                x_move_config,
                y_move_config
            ]
        if z_move_config:
            move_commands.extend(z_move_config)

        if move_commands:
            #we move only if ther eis something to move …
            self.machine.move_to(move_commands)


class PrintQueue():
    def __init__(self, axis_config, min_length, max_length, default_target_speed=None):
        self.axis = axis_config
        self.planning_list = list()
        self.queue_size = min_length - 1  #since we got one extra
        self.queue = Queue(maxsize=(max_length - min_length))
        self.previous_movement = None
        #we will use the last_movement as special case since it may not fully configured
        self.default_target_speed = default_target_speed

    def add_movement(self, target_position, timeout=None):
        move = {}
        #calculate the target
        self._extract_movement_values(move, target_position)
        #and see how fast we can allowable go
        maximum_achievable_speed = self._maximum_achievable_speed(move)
        move['max_achievable_speed_vector'] = maximum_achievable_speed
        #and since we do not know it better the first guess is that the final speed is the max speed
        move['speed'] = maximum_achievable_speed
        #now we can push the previous move to the queue and recalculate the whole queue
        if self.previous_movement:
            self.planning_list.append(self.previous_movement)
            #if the list is long enough we can give it to the queue so that readers can get it
        if len(self.planning_list) > self.queue_size:
            self._push_from_planning_to_execution(timeout)
        self.previous_movement = move
        #and recalculate the maximum allowed speed
        self._recalculate_move_speeds(self.previous_movement)

    def next_movement(self, timeout=None):
        return self.queue.get(timeout=timeout)

    def finish(self, timeout=None):
        while len(self.planning_list) > 0:
            self._push_from_planning_to_execution(timeout)
        while not self.queue.empty():
            pass

    def _push_from_planning_to_execution(self, timeout):
        executed_move = self.planning_list.pop(0)
        self.queue.put(executed_move, timeout=timeout)
        _logger.debug("adding to execution queue, now at %s/%s entries", len(self.planning_list), self.queue.qsize())

    def _extract_movement_values(self, move, target_position):
        #extract values
        if 'x' in target_position:
            move['x'] = target_position['x']
        else:
            if self.previous_movement:
                move['x'] = self.previous_movement['x']
            else:
                move['x'] = 0
        if 'y' in target_position:
            move['y'] = target_position['y']
        else:
            if self.previous_movement:
                move['y'] = self.previous_movement['y']
            else:
                move['y'] = 0
        if 'z' in target_position:
            move['z'] = target_position['z']
        else:
            if self.previous_movement:
                move['z'] = self.previous_movement['z']
            else:
                move['z'] = 0
        if 'f' in target_position:
            move['target_speed'] = target_position['target_speed']
        elif self.previous_movement:
            move['target_speed'] = self.previous_movement['target_speed']
        elif self.default_target_speed:
            move['target_speed'] = self.default_target_speed
        else:
            raise PrinterError("movement w/o a set speed and no default speed is set!")
        if self.previous_movement:
            last_x = self.previous_movement['x']
            last_y = self.previous_movement['y']
            last_z = self.previous_movement['z']
        else:
            last_x = 0
            last_y = 0
            last_z = 0
            #logg this move
        _logger.debug("moving to: X:%s, Y:%s, Z:%s", move['x'], move['z'], move['z'])

        move['delta_x'] = move['x'] - last_x
        move['delta_y'] = move['y'] - last_y
        move['delta_z'] = move['z'] - last_z
        move_vector = calculate_relative_vector(move['delta_x'], move['delta_y'])
        #save the move vector for later use …
        move['relative_move_vector'] = move_vector

    def _maximum_achievable_speed(self, move):
        if self.previous_movement:
            last_x_speed = self.previous_movement['speed']['x']
            last_y_speed = self.previous_movement['speed']['y']
        else:
            last_x_speed = 0
            last_y_speed = 0
        delta_x = move['delta_x']
        delta_y = move['delta_y']
        normalized_move_vector = move['relative_move_vector']
        #derrive the various speed vectors from the movement … for desired head and maximum axis speed
        speed_vectors = [
            {
                # add the desired speed vector as initial value
                'x': move['target_speed'] * normalized_move_vector['x'],
                'y': move['target_speed'] * normalized_move_vector['y']
            }
        ]
        if delta_x != 0:
            scaled_y = normalized_move_vector['y'] / normalized_move_vector['x']
            speed_vectors.append({
                #what would the speed vector for max x speed look like
                'x': copysign(self.axis['x']['max_speed'], normalized_move_vector['x']),
                'y': self.axis['x']['max_speed'] * copysign(scaled_y, normalized_move_vector['y'])
            })
            if not self.previous_movement or sign(delta_x) == sign(self.previous_movement['delta_x']):
                #ww can accelerate further
                max_speed_x = get_target_velocity(last_x_speed, delta_x,
                                                  self.axis['x']['bow'])  # TODO huh, no max acceleration??
            else:
                #we HAVE to turn around!
                if self.previous_movement:
                    self.previous_movement['x_stop'] = True
                max_speed_x = get_target_velocity(0, delta_x, self.axis['x']['bow'])
            speed_vectors.append({
                #how fast can we accelerate in X direction anyway
                'x': max_speed_x,
                'y': max_speed_x * scaled_y
            })
        else:
            #we HAVE to turn around!
            if self.previous_movement:
                self.previous_movement['x_stop'] = True

        if delta_y != 0:
            scaled_x = normalized_move_vector['x'] / normalized_move_vector['y']
            speed_vectors.append({
                #what would the maximum speed vector for y movement look like
                'x': self.axis['y']['max_speed'] * scaled_x,
                'y': copysign(self.axis['y']['max_speed'], normalized_move_vector['y'])
            })
            if not self.previous_movement or sign(delta_y) == sign(self.previous_movement['delta_y']):
                #ww can accelerate further
                max_speed_y = get_target_velocity(last_y_speed, delta_y, self.axis['y']['bow'])
            else:
                #we HAVE to turn around!
                if self.previous_movement:
                    self.previous_movement['y_stop'] = True
                max_speed_y = get_target_velocity(0, delta_y, self.axis['y']['bow'])
            speed_vectors.append({
                #how fast can we accelerate in X direction anyway
                'x': max_speed_y * scaled_x,
                'y': max_speed_y
            })
        else:
            #we HAVE to turn around!
            if self.previous_movement:
                self.previous_movement['y_stop'] = True

        max_local_speed_vector = find_shortest_vector(speed_vectors)
        #the minimum achievable speed is the minimum of all those local vectors

        return max_local_speed_vector


    def _recalculate_move_speeds(self, move):
        x_bow_ = self.axis['x']['bow']
        y_bow_ = self.axis['y']['bow']

        max_speed = move['speed']
        for movement in reversed(self.planning_list):
            #todo in theory we can stop somewhere …
            delta_x = movement['delta_x']
            if sign(delta_x) == sign(max_speed['x']):
                max_speed_x = get_target_velocity(max_speed['x'], delta_x, x_bow_)
            else:
                max_speed_x = get_target_velocity(0, delta_x, x_bow_)
            delta_y = movement['delta_y']
            if sign(delta_y) == sign(max_speed['y']):
                max_speed_y = get_target_velocity(max_speed['y'], delta_y, y_bow_)
            else:
                max_speed_y = get_target_velocity(0, delta_y, y_bow_)
            speed_vectors = [
                movement['speed']
            ]
            move_vector = movement['relative_move_vector']
            if move_vector['x'] != 0:
                speed_vectors.append({
                    #what would the speed vector for max x speed look like
                    'x': max_speed_x,
                    'y': max_speed_x * move_vector['y'] / move_vector['x']
                })
            if move_vector['y'] != 0:
                speed_vectors.append({
                    #what would the speed vector for max x speed look like
                    'x': max_speed_y * move_vector['x'] / move_vector['y'],
                    'y': max_speed_y
                })
            movement['speed'] = find_shortest_vector(speed_vectors)
            max_speed = movement['speed']


#from https://github.com/synthetos/TinyG/blob/master/firmware/tinyg/plan_line.c#L579
def get_target_velocity(start_velocity, length, jerk):
    target_velocity = pow(abs(length), 0.666666666666) * jerk
    target_velocity = copysign(target_velocity, length) + start_velocity
    return target_velocity


class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg

