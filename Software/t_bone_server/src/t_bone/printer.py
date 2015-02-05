# coding=utf-8
from Adafruit_BBIO import PWM
from Queue import Queue, Empty
from collections import deque
from copy import deepcopy
import logging
from math import copysign, sqrt
from threading import Thread

from numpy import sign
import time
import beagle_bone_pins
from heater import PwmHeater, Thermometer, PID, OnOffHeater

from machine import Machine, MAXIMUM_FREQUENCY_ACCELERATION, MAXIMUM_FREQUENCY_BOW
from helpers import convert_mm_to_steps, find_shortest_vector, calculate_relative_vector, \
    convert_velocity_clock_ref_to_realtime_ref, convert_acceleration_clock_ref_to_realtime_ref
from LEDS import LedManager

__author__ = 'marcus'
_logger = logging.getLogger(__name__)
_axis_config = {
    # maps axis name to config entry
    'x': 'x-axis',
    'y': 'y-axis',
    'z': 'z-axis',
    'e': 'extruder',
}
# order of the axis
_axis_names = ('x', 'y', 'z')


class Printer(Thread):
    def __init__(self, serial_port, reset_pin, print_queue_min_length=50, print_queue_max_length=100):
        Thread.__init__(self)
        self.ready = False
        self.printing = False
        self.config = None
        self.homed_axis = []

        self.heated_bed = None
        self.extruder_heater = None
        self.axis = {}

        self.axis_position = {}
        for axis_name in _axis_config:
            self.axis_position[axis_name] = 0

        self.printer_thread = None
        self._print_queue = None
        self.print_queue_min_length = print_queue_min_length
        self.print_queue_max_length = print_queue_max_length
        self._default_homing_retraction = None
        self._x_step_conversion = None
        self._y_step_conversion = None

        self._homing_timeout = 10000
        self._print_queue_wait_time = 0.1
        self.homed = False

        self.led_manager = LedManager()

        # todo why didn't this work as global constant?? - should be confugired anyway
        self._FAN_OUTPUT = beagle_bone_pins.pwm_config[2]['out']

        # finally create the machine
        self.machine = Machine(serial_port=serial_port, reset_pin=reset_pin)
        self.running = True
        self.start()

    def stop(self):
        if self.running:
            self.running = False
        if self.isAlive():
            self.join()
        self.machine.disconnect()

    def axis_names(self):
        return _axis_names

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
        self.default_speed = printer_config['default-speed']

        # todo this is the fan and should be configured
        PWM.start(self._FAN_OUTPUT, printer_config['fan-duty-cycle'], printer_config['fan-frequency'], 0)

        if 'heated-bed' in printer_config:
            bed_heater_config = printer_config['heated-bed']
            self.heated_bed = self._configure_heater(bed_heater_config)

        extruder_heater_config = config['extruder']['heater']
        # we do not care if it the extruder heate may not be given in the config
        # # - the whole point of additive printing is pretty dull w/o an heated extruder
        self.extruder_heater = self._configure_heater(extruder_heater_config)

        for axis_name, config_name in _axis_config.iteritems():
            _logger.info("Configuring axis \'%s\' according to conf \'%s\'", axis_name, config_name)
            axis = {'name': axis_name}
            self.axis[axis_name] = axis
            self._configure_axis(axis, config[config_name])
        self._postconfig()

    def connect(self):
        _logger.debug("Connecting printer")
        self.machine.connect()


    def start_print(self):
        self._print_queue = PrintQueue(axis_config=self.axis, min_length=self.print_queue_min_length,
                                       max_length=self.print_queue_max_length, default_target_speed=self.default_speed)
        self.machine.start_motion()
        self.printing = True
        self.led_manager.light(1, True)


    def finish_print(self):
        self._print_queue.finish()
        self.machine.finish_motion()
        self.printing = False
        self.led_manager.light(1, False)

    def read_motor_positons(self):
        positions = {}
        for axis_name in self.axis:
            axis_config = self.axis[axis_name]
            motor = axis_config['motor']
            position = self.machine.read_positon(motor)
            positions[axis_name] = position / axis_config['steps_per_mm']
        return positions

    def read_axis_status(self):
        status = {}
        for axis_name in self.axis:
            axis_config = self.axis[axis_name]
            motor = axis_config['motor']
            if motor:
                internal_status = self.machine.read_axis_status(motor)
                position = internal_status['position']
                position = position / axis_config['steps_per_mm']
                encoder_pos = internal_status['encoder_pos']
                encoder_pos = encoder_pos / axis_config['steps_per_mm']
                left_endstop_ = internal_status['left_endstop']
                right_endstop_ = internal_status['right_endstop']
            else:
                # todo implement
                position = 0
                encoder_pos = 0
                left_endstop_ = False
                right_endstop_ = False
            status[axis_name] = {
                "position": position,
                "encoder_pos": encoder_pos,
                "left_endstop": left_endstop_,
                "right_endstop": right_endstop_
            }
        return status


    def home(self, axis):
        for home_axis in axis:
            if not self.axis[home_axis]['end-stops'] or not self.axis[home_axis]['end-stops']['left']:
                _logger.debug("Axis %s does not have endstops - or an left end stop, cannot home it.", home_axis)
            else:
                _logger.info("Homing axis \'%s\' to zero", home_axis)
                # read the homing config for the axis
                home_speed = self.axis[home_axis]['home_speed']
                home_precision_speed = self.axis[home_axis]['home_precision_speed']
                home_acceleration = self.axis[home_axis]['home_acceleration']
                home_retract = self.axis[home_axis]['home_retract']
                # TODO we just enforce the existence of a left endstop - is there a simpler way?
                if self.axis[home_axis]['end-stops']['left']['type'] == 'virtual':
                    homing_right_position = convert_mm_to_steps(self.axis[home_axis]['end-stops']['left']['distance']
                                                                , self.axis[home_axis]['steps_per_mm'])
                else:
                    homing_right_position = 0
                # convert everything from mm to steps
                home_speed = convert_mm_to_steps(home_speed, self.axis[home_axis]['steps_per_mm'])
                home_precision_speed = convert_mm_to_steps(home_precision_speed, self.axis[home_axis]['steps_per_mm'])
                home_acceleration = convert_mm_to_steps(home_acceleration, self.axis[home_axis]['steps_per_mm'])
                if self.axis[home_axis]['clock-referenced']:
                    home_speed = convert_velocity_clock_ref_to_realtime_ref(home_speed)
                    home_precision_speed = convert_velocity_clock_ref_to_realtime_ref(home_precision_speed)
                    home_acceleration = convert_acceleration_clock_ref_to_realtime_ref(home_acceleration)
                    # no home jerk - since this only applies to 5041 axis w/o jerk control
                home_retract = convert_mm_to_steps(home_retract, self.axis[home_axis]['steps_per_mm'])
                # make a config out of it
                if self.axis[home_axis]['motor']:
                    homing_config = {
                        'motor': self.axis[home_axis]['motor'],
                        'timeout': self._homing_timeout,
                        'home_speed': home_speed,
                        'home_slow_speed': home_precision_speed,
                        'home_retract': home_retract,
                        'acceleration': home_acceleration,
                        'homing_right_position': homing_right_position,
                    }
                    #if self.axis[home_axis]['bow_step']:
                    #    homing_config['jerk'] = self.axis[home_axis]['bow_step']
                else:
                    # todo we should check if there is a motor for the left endstop??
                    homing_config = {
                        'motor': self.axis[home_axis]['end-stops']['left']['motor'],
                        'followers': self.axis[home_axis]['motors'],
                        'timeout': self._homing_timeout,
                        'home_speed': home_speed,
                        'home_slow_speed': home_precision_speed,
                        'home_retract': home_retract,
                        'acceleration': home_acceleration,
                        'homing_right_position': homing_right_position,
                    }
                    #if self.axis[home_axis]['bow_step']:
                    #    homing_config['bow'] = self.axis[home_axis]['bow_step']

                # and do the homing
                self.machine.home(homing_config, timeout=self._homing_timeout)
                # better but still not good - we should have a better concept of 'axis'
                self.axis[home_axis]['homed'] = True
                self.axis_position[home_axis] = 0

    def set_position(self, positions):
        if positions:
            positions['type'] = 'set_position'
            # todo and what if there is no movement??
            self._print_queue.plan_new_movement(positions)#old: self._print_queue.add_movement(positions)

    def relative_move_to(self, position):
        movement = {}
        for axis_name, pos in self.axis_position.iteritems():
            new_pos = pos
            if axis_name in position:
                new_pos += position[axis_name]
            movement[axis_name] = new_pos
        self.move_to(movement)

    # tuple with x/y/e coordinates - left out if no change is intended
    def move_to(self, position):
        if self.printing:
            position['type'] = 'move'
            self._print_queue.plan_new_movement(position)#old: self._print_queue.add_movement(position)
        else:
            self.start_print()
            position['type'] = 'move'
            position['target_speed'] = self.default_speed;
            self._print_queue.plan_new_movement(position)#old: self._print_queue.add_movement(position)
            self.finish_print()

    def execute_movement(self, movement):
        if movement['type'] == 'move':
            _logger.debug("Execute: Entered to type 'move'")
            step_pos, step_speed_vector = self._add_movement_calculations(movement)
            x_move_config, y_move_config, z_move_config, e_move_config = self._generate_move_config(movement,
                                                                                        step_pos,step_speed_vector)
            self._move(movement, step_pos, x_move_config, y_move_config, z_move_config, e_move_config)
        elif movement['type'] == 'set_position':
            _logger.debug("Execute: Entered to type 'set_position'")
            for axis_name in self.axis:
                set_pos_name = "s%s" % axis_name
                if set_pos_name in movement:
                    position = movement[set_pos_name]
                    axis = self.axis[axis_name]
                    step_position = convert_mm_to_steps(position, axis['steps_per_mm'])
                    if 'motor' in axis and axis['motor']:
                        # todo one of the above should be enough
                        motor = axis['motor']
                        self.machine.set_pos(motor, step_position)
                    elif 'motors' in axis and axis['motors']:
                        for motor in axis['motors']:
                            self.machine.set_pos(motor, step_position)


    def set_fan(self, value):
        if value < 0:
            value = 0
        elif value > 1:
            value = 1
        PWM.set_duty_cycle(self._FAN_OUTPUT, value * 100.0)

    def run(self):
        self.led_manager.light(0, True)
        while self.running:
            if self.printing:
                try:
                    # get the next movement from stack
                    movement = self._print_queue.next_movement_to_execute(self._print_queue_wait_time)#old: movement
                    #  = self._print_queue.next_movement(self._print_queue_wait_time)
                    self.execute_movement(movement)
                except Empty:
                    _logger.debug("Print Queue did not return a value - this can be pretty normal. Queue: %s Ex: %s",
                                  len(self._print_queue.planning_queue),self._print_queue.execution_queue.qsize())
            else:
                time.sleep(0.1)
        self.led_manager.light(0, False)

    def _configure_axis(self, axis, config):
        axis_name = axis['name']
        # let's see if we got one or more motors
        if 'motor' in config:
            axis['motor'] = config['motor']
        elif 'motors' in config:
            axis['motor'] = None
            axis['motors'] = config['motors']
        else:
            raise PrinterError("you must configure one ('motor') or more 'motors' in the axis configuration")

        axis['steps_per_mm'] = config['steps-per-mm']
        if 'step-scaling-correction' in config:
            _logger.debug("Scaling axis %s stepping %s by %s", axis_name, axis['steps_per_mm'],
                          config['step-scaling-correction'])
            step_scaling_correction = float(config['step-scaling-correction'])
            axis['steps_per_mm'] *= step_scaling_correction
        if 'time-reference' in config and config['time-reference'] == 'clock signal':
            axis['clock-referenced'] = True
        else:
            axis['clock-referenced'] = False

        axis['max_speed'] = config['max-speed']
        # todo - this can be clock signal referenced - convert acc. to  axis['clock-referenced']
        axis['max_speed_step'] = convert_mm_to_steps(config['max-speed'], config['steps-per-mm'])
        axis['max_acceleration'] = config['max-acceleration']
        axis['max_step_acceleration'] = convert_mm_to_steps(config['max-acceleration'], config['steps-per-mm'])
        if axis['max_step_acceleration'] > MAXIMUM_FREQUENCY_ACCELERATION:
            _logger.error("Acceleration of %s is higher than %s for axis %s!", axis['max_step_acceleration'],
                          MAXIMUM_FREQUENCY_ACCELERATION, axis_name)
            raise PrinterError("Acceleration for axis " + axis_name + " too high")
        if 'bow-acceleration' in config:
            axis['bow'] = config['bow-acceleration']
            axis['bow_step'] = convert_mm_to_steps(config['bow-acceleration'], config['steps-per-mm'])
            if axis['bow_step'] > MAXIMUM_FREQUENCY_BOW:
                _logger.error("Bow of %s is higher than %s for axis %s!", axis['bow_step'], MAXIMUM_FREQUENCY_BOW,
                              axis_name)
                raise PrinterError("Bow for axis " + axis_name + " too high")
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
            if convert_mm_to_steps(config['home-acceleration'],
                                   config['steps-per-mm']) > MAXIMUM_FREQUENCY_ACCELERATION:
                _logger.error("Homing acceleration of %s is higher than %s for axis %s!",
                              convert_mm_to_steps(config['home-acceleration'], config['steps-per-mm']),
                              MAXIMUM_FREQUENCY_ACCELERATION, axis_name)
            raise PrinterError("Acceleration for axis " + axis_name + " too high")
        else:
            axis['home_acceleration'] = config['max-acceleration']
        if 'home-retract' in config:
            axis['home_retract'] = config['home-retract']
        else:
            axis['home_retract'] = self._default_homing_retraction

        axis['end-stops'] = {}
        if 'end-stops' in config:
            _logger.debug("Configuring endstops for axis %s", axis_name)
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
                        # left endstop get's 0 posiotn - makes sense and distance for homing use
                        if end_stop_pos == 'left':
                            axis['end-stops'][end_stop_pos]['distance'] = axis['end-stops'][end_stop_pos]['position']
                            axis['end-stops'][end_stop_pos]['position'] = 0

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
                        # endstop config is a bit more complicated for multiple motors
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
            _logger.debug("No endstops for axis %s", axis_name)

        if 'encoder' in config:
            # read out the encoder config
            encoder_config = config['encoder']
            increments = int(encoder_config['increments-per-revolution'])
            if 'differential' in encoder_config and encoder_config['differential']:
                differential = True
            else:
                differential = False
            if 'inverted' in encoder_config and encoder_config['inverted']:
                inverted = True
            else:
                inverted = False
            axis['encoder'] = {
                'steps-per-rev': config['steps-per-revolution'],
                'increments-per-rev': increments,
                'differential': differential,
                'inverted': inverted
            }
            self.machine.configure_encoder(axis['motor'], deepcopy(axis['encoder']))

        current = config["current"]
        if axis["motor"]:
            self.machine.set_current(axis["motor"], current)
        else:
            for motor in axis['motors']:
                self.machine.set_current(motor, current)

        # let's see if there are any inverted motors
        if 'motor' in config:
            if "inverted" in config and config["inverted"]:
                axis['inverted'] = True
            else:
                axis['inverted'] = False
            self.machine.invert_motor(axis["motor"], axis['inverted'])
        else:
            # todo this is ok - but no perfect structure
            if "inverted" in config:
                axis['inverted'] = config["inverted"]
                for motor in axis['motors']:
                    if str(motor) in config['inverted'] and config['inverted'][str(motor)]:
                        self.machine.invert_motor(motor, True)

    def _configure_heater(self, heater_config):
        output_number = heater_config['output'] - 1
        if output_number < 0 or output_number >= len(beagle_bone_pins.pwm_config):
            raise PrinterError("PWM pins can only be between 1 and %s" % len(beagle_bone_pins.pwm_config))
        output = beagle_bone_pins.pwm_config[output_number]['out']
        thermometer = Thermometer(themistor_type=heater_config['sensor-type'],
                                  analog_input=beagle_bone_pins.pwm_config[output_number]['temp'])
        if 'current_input' in beagle_bone_pins.pwm_config[output_number]:
            current_pin = beagle_bone_pins.pwm_config[output_number]['current_input']
        else:
            current_pin = None
        type = heater_config['type']
        if type == 'PID':
            # do we have a maximum duty cycle??
            max_duty_cycle = None
            if 'max-duty-cycle' in heater_config:
                max_duty_cycle = heater_config['max-duty-cycle']
            pid_controller = PID(P=heater_config['pid-config']['Kp'],
                                 I=heater_config['pid-config']['Ki'],
                                 D=heater_config['pid-config']['Kd'],
                                 Integrator_max=heater_config['max-duty-cycle'])
            heater = PwmHeater(thermometer=thermometer, pid_controller=pid_controller,
                               output=output, maximum_duty_cycle=max_duty_cycle,
                               current_measurement=current_pin, machine=self.machine)
        elif type == "2 Point":
            hysteresis = heater_config['hysteresis']
            heater = OnOffHeater(thermometer=thermometer, output=output, active_high=True,
                                 hysteresis=hysteresis,
                                 current_measurement=current_pin, machine=self.machine)
        else:
            raise PrinterError("Unkown heater type %s" % type)
        return heater

    def _postconfig(self):
        # we need the stepping rations for variuos calclutaions later
        self._x_step_conversion = float(self.axis['x']['steps_per_mm']) / float(self.axis['y']['steps_per_mm'])
        self._y_step_conversion = float(self.axis['y']['steps_per_mm']) / float(self.axis['x']['steps_per_mm'])
        self._e_x_step_conversion = float(self.axis['e']['steps_per_mm']) / float(self.axis['x']['steps_per_mm'])
        self._e_y_step_conversion = float(self.axis['e']['steps_per_mm']) / float(self.axis['y']['steps_per_mm'])

        self._extract_homing_information()

        self.ready = True

    def _extract_homing_information(self):
        for axis_name, axis in self.axis.iteritems():
            axis['homeable'] = False
            if 'end-stops' in axis:
                for position in ['left', 'right']:
                    if position in axis['end-stops'] and not 'virtual' == axis['end-stops'][position]:
                        axis['homeable'] = True
                        axis['homed'] = False
                        break


    def _add_movement_calculations(self, movement):
        step_pos = {
            'x': convert_mm_to_steps(movement['x'], self.axis['x']['steps_per_mm']),
            'y': convert_mm_to_steps(movement['y'], self.axis['y']['steps_per_mm']),
            'z': convert_mm_to_steps(movement['z'], self.axis['z']['steps_per_mm']),
            'e': convert_mm_to_steps(movement['e'], self.axis['e']['steps_per_mm'])
        }
        movement['entry_speed'] = sqrt(movement['entry_speed_sqr'])
        movement['nominal_speed'] = sqrt(movement['nominal_speed_sqr'])
        movement['exit_speed'] = sqrt(movement['exit_speed_sqr'])
        _logger.debug("Execute - add calculations: entry(%s) nominal(%s) exit(%s)",movement['entry_speed'],
                      movement['nominal_speed'],movement['exit_speed'])
        for axis in _axis_config:
            if ('delta_' + axis) in movement:
                _logger.debug("Execute - add calculations: delta %s %s", axis, movement['delta_' + axis])
        relative_move_vector = movement['relative_move_vector']
        z_speed = min(abs(relative_move_vector['v'] * relative_move_vector['z']), self.axis['z']['max_speed'])
        e_speed = min(abs(relative_move_vector['v'] * relative_move_vector['e']), self.axis['e']['max_speed'])
        step_speed_vector = {
            # todo - this can be clock signal referenced - convert acc. to  axis['clock-referenced']
            'nominal_speed_x':max(convert_mm_to_steps(abs(movement['nominal_speed']),self.axis['x']['steps_per_mm']),1),
            'nominal_speed_y':max(convert_mm_to_steps(abs(movement['nominal_speed']),self.axis['y']['steps_per_mm']),1),
            'nominal_speed_z':max(convert_mm_to_steps(abs(movement['nominal_speed']),self.axis['z']['steps_per_mm']),1),#todo
            'nominal_speed_e':max(convert_mm_to_steps(abs(movement['nominal_speed']),self.axis['e']['steps_per_mm']),1),
            'entry_speed_x':max(convert_mm_to_steps(abs(movement['entry_speed']),self.axis['x']['steps_per_mm']),1),
            'entry_speed_y':max(convert_mm_to_steps(abs(movement['entry_speed']),self.axis['y']['steps_per_mm']),1),
            'entry_speed_z':max(convert_mm_to_steps(abs(movement['entry_speed']), self.axis['z']['steps_per_mm']),1),#todo
            'entry_speed_e':max(convert_mm_to_steps(abs(movement['entry_speed']), self.axis['e']['steps_per_mm']),1),
            'exit_speed_x':max(convert_mm_to_steps(abs(movement['exit_speed']),self.axis['x']['steps_per_mm']),1),
            'exit_speed_y':max(convert_mm_to_steps(abs(movement['exit_speed']),self.axis['y']['steps_per_mm']),1),
            'exit_speed_z':max(convert_mm_to_steps(abs(movement['exit_speed']), self.axis['z']['steps_per_mm']),1),#todo
            'exit_speed_e':max(convert_mm_to_steps(abs(movement['exit_speed']), self.axis['e']['steps_per_mm']),1),
            'acceleration_x':max(convert_mm_to_steps(abs(movement['acceleration']),self.axis['x']['steps_per_mm']),1),
            'acceleration_y':max(convert_mm_to_steps(abs(movement['acceleration']),self.axis['y']['steps_per_mm']),1),
            'acceleration_z':max(convert_mm_to_steps(abs(movement['acceleration']), self.axis['z']['steps_per_mm']),1),
            'acceleration_e':max(convert_mm_to_steps(abs(movement['acceleration']), self.axis['e']['steps_per_mm']),1)
        }
        for axis in _axis_config:
            step_speed_vector['entry_speed_'+axis] = min(step_speed_vector['entry_speed_'+axis],
                                                         step_speed_vector['nominal_speed_'+axis])
            step_speed_vector['exit_speed_'+axis] = min(step_speed_vector['exit_speed_'+axis],
                                                         step_speed_vector['nominal_speed_'+axis])
        return step_pos, step_speed_vector

    def _generate_move_config(self, movement, step_pos, step_speed_vector):
        def _axis_movement_template(axis):
            return {
                'motor': axis['motor'],
                'acceleration': axis['max_step_acceleration'],
            }
        debug_axis = "Execute - generate: "
        if movement['delta_x']:
            x_move_config = _axis_movement_template(self.axis['x'])
            x_move_config['target'] = step_pos['x']
            x_move_config['entry_speed'] = step_speed_vector['entry_speed_x']
            x_move_config['nominal_speed'] = step_speed_vector['nominal_speed_x']
            x_move_config['exit_speed'] = step_speed_vector['exit_speed_x']
            x_move_config['acceleration'] = step_speed_vector['acceleration_x']
            if 'x_stop' in movement:
                x_move_config['type'] = 'stop'
            else:
                x_move_config['type'] = 'way'
            debug_axis += "x_axis "
        else:
            x_move_config = None

        if movement['delta_y']:
            y_move_config = _axis_movement_template(self.axis['y'])
            y_move_config['target'] = step_pos['y']
            y_move_config['entry_speed'] = step_speed_vector['entry_speed_y']
            y_move_config['nominal_speed'] = step_speed_vector['nominal_speed_y']
            y_move_config['exit_speed'] = step_speed_vector['exit_speed_y']
            y_move_config['acceleration'] = step_speed_vector['acceleration_y']
            if 'y_stop' in movement:
                y_move_config['type'] = 'stop'
            else:
                y_move_config['type'] = 'way'
            debug_axis += "y_axis "
        else:
            y_move_config = None

        if movement['delta_z']:
            z_move_config = [
                {
                    'motor': self.axis['z']['motors'][0],
                    'target': step_pos['z'],
                    'acceleration': step_speed_vector['acceleration_z'],#self.axis['z']['max_step_acceleration'],
                    'entry_speed': step_speed_vector['entry_speed_z'],
                    'exit_speed': step_speed_vector['exit_speed_z'],
                    'nominal_speed': step_speed_vector['nominal_speed_z'],
                    'type': 'stop'
                },
                {
                    'motor': self.axis['z']['motors'][1],
                    'target': step_pos['z'],
                    'acceleration': step_speed_vector['acceleration_z'],#self.axis['z']['max_step_acceleration'],
                    'entry_speed': step_speed_vector['entry_speed_z'],
                    'exit_speed': step_speed_vector['exit_speed_z'],
                    'nominal_speed': step_speed_vector['nominal_speed_z'],
                    'type': 'stop'
                }
            ]
            debug_axis += "z_axis "
        else:
            z_move_config = None

        if movement['delta_e']:
            e_move_config = _axis_movement_template(self.axis['e'])
            e_move_config['target'] = step_pos['e']
            e_move_config['entry_speed'] = abs(step_speed_vector['entry_speed_e'])
            e_move_config['nominal_speed'] = abs(step_speed_vector['nominal_speed_e'])
            e_move_config['exit_speed'] = abs(step_speed_vector['exit_speed_e'])
            if 'e_stop' in movement:
                e_move_config['type'] = 'stop'
            else:
                e_move_config['type'] = 'way'
            debug_axis += "e_axis "
        else:
            e_move_config = None
        _logger.debug(debug_axis)
        return x_move_config, y_move_config, z_move_config, e_move_config

    def _move(self, movement, step_pos, x_move_config, y_move_config, z_move_config, e_move_config):
        move_vector = movement['relative_move_vector']
        move_commands = []

        if x_move_config and not y_move_config and not z_move_config:
            # move x motor
            _logger.debug("Execute - move: X axis to %s", step_pos['x'])

            move_commands = [
                x_move_config
            ]

        elif y_move_config and not x_move_config and not z_move_config:
            # move y motor to position
            _logger.debug("Execute - move: Y axis to %s", step_pos['y'])

            move_commands = [
                y_move_config
            ]

        elif z_move_config and not x_move_config and not y_move_config:
            # move y motor to position
            _logger.debug("Execute - move: Z axis to %s", step_pos['z'])

            move_commands = [
                z_move_config
            ]

        elif x_move_config and y_move_config:
            # ok we have to see which axis has bigger movement
            if abs(movement['delta_x']) > abs(movement['delta_y']):
                y_factor = abs(move_vector['y'] / move_vector['x'] * self._y_step_conversion)
                _logger.debug(
                    "Execute - move: X axis to %s gearing Y by %s to %s"
                    , step_pos['x'], y_factor, step_pos['y'])

                y_move_config['entry_speed'] = x_move_config['entry_speed'] * y_factor
                y_move_config['nominal_speed'] = x_move_config['nominal_speed'] * y_factor
                y_move_config['exit_speed'] = x_move_config['exit_speed'] * y_factor
                y_move_config['acceleration'] = x_move_config['acceleration'] * y_factor
                # move
                move_commands = [
                    x_move_config,
                    y_move_config
                ]

            else:
                x_factor = abs(move_vector['x'] / move_vector['y'] * self._x_step_conversion)
                _logger.debug(
                    "Execute - move: Y axis to %s gearing X by %s  to %s"
                    , step_pos['x'], x_factor, step_pos['y'])

                x_move_config['entry_speed'] = y_move_config['entry_speed'] * x_factor
                x_move_config['nominal_speed'] = y_move_config['nominal_speed'] * x_factor
                x_move_config['exit_speed'] = y_move_config['exit_speed'] * x_factor
                x_move_config['acceleration'] = y_move_config['acceleration'] * x_factor
                move_commands = [
                    y_move_config,
                    x_move_config
                ]

        if e_move_config:
            if x_move_config and not (y_move_config and abs(move_vector['x']) < abs(move_vector['y'])):
                factor = abs(move_vector['e'] / move_vector['x'] * self._e_x_step_conversion)
                e_move_config['entry_speed'] = x_move_config['entry_speed'] * factor
                e_move_config['nominal_speed'] = x_move_config['nominal_speed'] * factor
                e_move_config['exit_speed'] = x_move_config['exit_speed'] * factor
                e_move_config['acceleration'] = factor * x_move_config['acceleration']
            elif y_move_config:
                factor = abs(move_vector['e'] / move_vector['y'] * self._e_y_step_conversion)
                e_move_config['entry_speed'] = y_move_config['entry_speed'] * factor
                e_move_config['nominal_speed'] = y_move_config['nominal_speed'] * factor
                e_move_config['exit_speed'] = y_move_config['exit_speed'] * factor
                e_move_config['acceleration'] = factor * y_move_config['acceleration']
            move_commands.append(e_move_config)
            _logger.debug("Execute - move: also e")

        # we update our position
        # todo isn't there a speedier way
        for axis_name in self.axis:
            self.axis_position[axis_name] = movement[axis_name]

        if move_commands:
            # we move only if there is something to move …
            self.machine.move_to(move_commands)


class PrintQueue():
    def __init__(self, axis_config, min_length, max_length, default_target_speed=None, led_manager=None):
        self.axis = axis_config
        self.planning_queue = deque()
        self.queue_size = min_length - 1  # since we got one extra
        self.execution_queue = Queue(maxsize=(max_length - min_length))
        self.last_planned = 0
        self.planner = Planner()
        self.default_target_speed = default_target_speed
        self.led_manager = led_manager
        #todo move these parameters to configuration file
        self.DEFAULT_JUNCTION_DEVIATION = 0.02 # mm
        self.MINIMUM_JUNCTION_SPEED = 0.0 #mm/min
        self.MINIMUM_FEED_RATE = 0.1666 #mm/s #1.0 #mm/min

    def add_movement_to_planning_queue(self, new_movement):
        self.planning_queue.append(new_movement)

    def get_movement_from_planning_queue(self):
        movement = self.planning_queue.popleft()
        if len(self.planning_queue) > 0:
            movement['exit_speed_sqr'] = max(self.planning_queue[0]['entry_speed_sqr'],movement['entry_speed_sqr'])
        else:
            movement['exit_speed_sqr'] = movement['entry_speed_sqr']
        if movement['exit_speed_sqr'] < movement['entry_speed_sqr']:
            _logger.warning("Push - get: Vstop smaller than Vstart")
        if self.last_planned > 0:
            self.last_planned -= 1
        return movement

    def plan_new_movement(self, target_position, timeout=None):
        if 'type' in target_position:
            _logger.debug("Planner: Begin movement (%s)", target_position['type'])
        else:
            _logger.error("Planner: Movement with no type")
        movement = self._extract_movement_values(target_position)
        #Compute speed, unit vector and other parameters of the movement
        unit_vec = movement['relative_move_vector']
        if target_position['type'] == 'move':
            _logger.debug("Planner: Plan movement. Type: Move")
            if movement['distance_event_count'] == 0.0:
                return
            if movement['target_speed'] < self.MINIMUM_FEED_RATE:#todo or could exist a movement with 0 feed_rate?
                movement['target_speed'] = self.MINIMUM_FEED_RATE
                _logger.debug("Planner: target speed set to minimum")
            self.planner.set_previous_feed_rate(movement['target_speed'])
            for axis in _axis_names:
                planner_previous_unit_vec = self.planner.get_previous_unit_vec()
                if unit_vec[axis] == 0:
                    inverted_unit_vec_axis = float("inf")
                else:
                    inverted_unit_vec_axis = 1.0 / abs(unit_vec[axis])
                movement['target_speed'] = min(movement['target_speed'],self.axis[axis]['max_speed']\
                                               *inverted_unit_vec_axis)
                movement['acceleration'] = min(movement['acceleration'],
                                               self.axis[axis]['max_acceleration']*inverted_unit_vec_axis)
                movement['junction_cos_theta'] -= planner_previous_unit_vec[axis] * unit_vec[axis]
            #todo acceleration will be infinity if movement is also in e axis
            if movement['acceleration'] == float("inf"):
                if unit_vec['e'] == 0:
                    inverted_unit_vec_axis = float("inf")
                else:
                    inverted_unit_vec_axis = 1.0 / abs(unit_vec['e'])
                movement['acceleration'] = min(movement['acceleration'],
                                               self.axis['e']['max_acceleration']*inverted_unit_vec_axis)
            _logger.debug("Planner: target_speed(%s) acceleration(%s) junction_cos (%s)",movement['target_speed'],
                          movement['acceleration'],movement['junction_cos_theta'])
            if self.is_planning_queue_empty():
                movement['max_junction_speed_sqr'] = 0.0
            else:
                sin_theta_d2 = sqrt(0.5*(1.0-movement['junction_cos_theta']))
                if (1.0 - sin_theta_d2) == 0:
                    _logger.error("Planner: Zero division error")
                movement['max_junction_speed_sqr'] = max(self.MINIMUM_JUNCTION_SPEED*self.MINIMUM_JUNCTION_SPEED,
                             (movement['acceleration']*self.DEFAULT_JUNCTION_DEVIATION*sin_theta_d2)/(1.0-sin_theta_d2))
            movement['nominal_speed_sqr']=movement['target_speed']*movement['target_speed']
            movement['max_entry_speed_sqr'] = min(movement['max_junction_speed_sqr'],min(movement['nominal_speed_sqr'],
                                                  self.planner.get_previous_nominal_speed_sqr()))
            _logger.debug("Planner: max entry speed (%s) max junction speed (%s) and nominal speed (%s) calculated",
                          movement['max_entry_speed_sqr'],movement['max_junction_speed_sqr'],
                          movement['nominal_speed_sqr'])
            unit_vec['v']=movement['target_speed']/movement['millimeters']#only computed in T-Bone, not grbl
            movement['relative_move_vector'] = unit_vec
            #Save movement data in Planner -> will be previous movement info for the next one
            self.planner.set_previous_unit_vec(unit_vec)
            self.planner.set_previous_nominal_speed_sqr(movement['nominal_speed_sqr'])
            position = {}
            for axis in _axis_config.keys():
                position[axis] = movement[axis]
            self.planner.set_position(position)
        elif target_position['type'] == 'set_position':
            _logger.debug("Planner: Plan movement. Type: Set Position")
            movement['target_speed'] = 0.0
            movement['acceleration'] = 0.0
            movement['entry_speed_sqr'] = 0.0
            movement['nominal_speed_sqr'] = 0.0
            movement['max_junction_speed_sqr'] = 0.0
            movement['max_entry_speed_sqr'] = 0.0

        #If there are enough planned movements, move one to execution queue
        if len(self.planning_queue) > self.queue_size:
            self._push_from_planning_to_execution(timeout)
            _logger.debug("Planner: planning queue big enough. push to execution")
        #Add new movement to planning queue
        self.add_movement_to_planning_queue(movement)
        _logger.debug("Planner: movement added to planning queue (%s) (%s in execution)",len(self.planning_queue),
                      self.execution_queue.qsize())
        #Recalculate the plan using the new movement
        self._recalculate_move_speeds()

    def is_planning_queue_empty(self):
        if len(self.planning_queue) == 0:
            return True
        else:
            return False

    def next_movement_to_execute(self, timeout=None):
        return self.execution_queue.get(timeout=timeout)

    def finish(self, timeout=None):
        if not self.is_planning_queue_empty():
            self.planning_queue[-1]['x_stop'] = True
            self.planning_queue[-1]['y_stop'] = True
            self.planning_queue[-1]['e_stop'] = True
            _logger.debug("Finish: adding axis stop")
        while len(self.planning_queue) > 0:
            self._push_from_planning_to_execution(timeout)
        while not self.execution_queue.empty():
            pass

    def _push_from_planning_to_execution(self, timeout):
        executed_move = self.get_movement_from_planning_queue()
        #todo calculate parameters of the old interface from the new one

        self.execution_queue.put(executed_move, timeout=timeout)

    def _extract_movement_values(self, target_position):
        movement = {'type':target_position['type'], 'millimeters': 0.0, 'acceleration': float("inf"),
                    'junction_cos_theta': 0.0, 'distance_event_count':0.0, 'entry_speed_sqr':0.0}
        #test: reduced speed
        reduction_factor = 1
        if 'target_speed' in target_position:
            movement['target_speed'] = target_position['target_speed'] / reduction_factor
            _logger.debug("Planner - Extract Movement: Target Speed 1 %s", movement['target_speed'])
        else:
            movement['target_speed'] = self.planner.get_previous_feed_rate()
            _logger.debug("Planner - Extract Movement: Target Speed 2 %s", movement['target_speed'])
        delta = {}
        unit_vec = {}
        planner_position = self.planner.get_position()
        if target_position['type'] == 'move':
            for axis_i in _axis_config.keys():
                if axis_i in target_position:
                    movement[axis_i] = target_position[axis_i]
                    delta[axis_i] = target_position[axis_i] - planner_position[axis_i]
                    if axis_i in ["x","y"]:
                        _logger.debug("Planner - Extract Movement: %s target(%s) last(%s) difference(%s)",
                                      axis_i,target_position[axis_i],planner_position[axis_i],delta[axis_i])
                else:
                    movement[axis_i] = planner_position[axis_i]
                    delta[axis_i] = 0.0
                _logger.debug("Planner - Extract Movement: axis %s target %s, delta %s",axis_i,
                              movement[axis_i],delta[axis_i])
                movement['distance_event_count'] = max(movement['distance_event_count'],movement[axis_i])
                unit_vec[axis_i] = delta[axis_i]
                movement['delta_'+axis_i] = delta[axis_i]
                movement['millimeters'] += delta[axis_i]*delta[axis_i]
            movement['millimeters'] = sqrt(movement['millimeters'])
            _logger.debug("Planner - Extract Movement: total mm (%s)",movement['millimeters'])
            movement['relative_move_vector'] = unit_vec
            for axis_i in _axis_config.keys():
                unit_vec[axis_i] /= movement['millimeters']#make unitary: divide by total length
            return movement
        elif target_position['type'] == 'set_position':
            for axis_i in _axis_config.keys():
                movement[axis_i] = planner_position[axis_i]
                delta[axis_i] = 0.0
                movement['delta_'+axis_i] = delta[axis_i]
                unit_vec[axis_i]=0.0
            movement['millimeters'] = 0.0
            movement['relative_move_vector'] = unit_vec
            for axis_name, value in target_position.iteritems():
                if not axis_name == 'type':
                    movement['s%s' % axis_name] = value
                    movement[axis_name] = value
                    _logger.debug("Planner - Extract Movement: set axis(%s) to (%s)", axis_name,value)
            return movement

    def _recalculate_move_speeds(self):
        if self.led_manager:
            self.led_manager.light(2, True)

        current_id = len(self.planning_queue) - 1
        if current_id == self.last_planned:
            return
        _logger.debug("Planner - Recalculate: At least 2 movements in queue. Current id (%s), last planned (%s)",
                      current_id, self.last_planned)
        self.planning_queue[current_id]['entry_speed_sqr'] = min(self.planning_queue[current_id]['max_entry_speed_sqr'],
                    2*self.planning_queue[current_id]['acceleration']*self.planning_queue[current_id]['millimeters'])
        _logger.debug("Planner - Recalculate: entry_speed_sqr[%s] is (%s)",current_id,
                      self.planning_queue[current_id]['entry_speed_sqr'])
        #Reverse order calculation
        current_id -= 1
        while current_id != self.last_planned:
            next_id = current_id + 1
            _logger.debug("Planner - Recalculate: Reverse order between current(%s) and next(%s)",current_id,next_id)
            if self.planning_queue[current_id]['entry_speed_sqr'] != \
                    self.planning_queue[current_id]['max_entry_speed_sqr']:
                entry_speed_sqr = self.planning_queue[next_id]['entry_speed_sqr'] + \
                                  2*self.planning_queue[current_id]['acceleration']*\
                                  self.planning_queue[current_id]['millimeters']
                if entry_speed_sqr < self.planning_queue[current_id]['max_entry_speed_sqr']:
                    self.planning_queue[current_id]['entry_speed_sqr'] = entry_speed_sqr
                else:
                    self.planning_queue[current_id]['entry_speed_sqr'] = \
                        self.planning_queue[current_id]['max_entry_speed_sqr']
            _logger.debug("Planner - Recalculate: Reverse order, entry_speed of current (%s)",
                          self.planning_queue[current_id]['entry_speed_sqr'])
            current_id -= 1
        #Forward order calculation
        next_id = self.last_planned
        while next_id != (len(self.planning_queue)-1):
            current_id = next_id
            next_id += 1
            #If next movement doesn't have displacement in one axis, previous one has "stop" parameter
            factor = {}
            if self.planning_queue[next_id]["delta_y"] == 0:
                _logger.debug("Planner - Recalculate: Forward. factor y fixed")
                factor["y"] = 5000.0
            else:
                _logger.debug("Planner - Recalculate: Forward. factor x calc")
                factor["y"] = abs(self.planning_queue[next_id]["delta_x"] / self.planning_queue[next_id]["delta_y"])
                _logger.debug("Planner - Recalculate: Forward. factor y %s",factor["y"])
            if self.planning_queue[next_id]["delta_x"] == 0:
                factor["x"] = 5000.0
                _logger.debug("Planner - Recalculate: Forward. factor x fixed")
            else:
                _logger.debug("Planner - Recalculate: Forward. factor x calc")
                factor["x"] = abs(self.planning_queue[next_id]["delta_y"] / self.planning_queue[next_id]["delta_x"])
                _logger.debug("Planner - Recalculate: Forward. factor x %s",factor["x"])
            for axis in ["x", "y"]:
                #if factor[axis] > 500.0:
                self.planning_queue[current_id][axis+"_stop"] = True
                _logger.debug("Planner - Recalculate: Forward. Adding stop to axis %s",axis)
            #If x and y will stop, e will as well
            if "x_stop" in self.planning_queue[current_id] and "y_stop" in self.planning_queue[current_id]:
                self.planning_queue[current_id]["e_stop"] = True
                _logger.debug("Planner - Recalculate: Forward. Adding stop to axis e")
            _logger.debug("Planner - Recalculate: Forward order between current(%s) and next(%s)",current_id,next_id)
            if self.planning_queue[current_id]['entry_speed_sqr'] < self.planning_queue[next_id]['entry_speed_sqr']:
                entry_speed_sqr = self.planning_queue[current_id]['entry_speed_sqr'] + \
                                      2*self.planning_queue[current_id]['acceleration']*\
                                      self.planning_queue[current_id]['millimeters']
                if entry_speed_sqr < self.planning_queue[next_id]['max_entry_speed_sqr']:
                    self.planning_queue[next_id]['entry_speed_sqr'] = entry_speed_sqr
                    self.last_planned = next_id
            if self.planning_queue[next_id]['entry_speed_sqr'] == self.planning_queue[next_id]['max_entry_speed_sqr']:
                self.last_planned = next_id
            _logger.debug("Planner - Recalculate: Forward order, entry_speed of next (%s)",
                          self.planning_queue[next_id]['entry_speed_sqr'])
        if self.led_manager:
            self.led_manager.light(2, False)

class Planner():
    def __init__(self, position = None, previous_unit_vec = None, previous_nominal_speed_sqr = None,
                 previous_feed_rate = None):
        if position:
            self.position = position
        else:
            self.position = {'x': 0.0, 'y': 0.0, 'z': 0.0, 'e': 0.0}

        if previous_unit_vec:
            self.previous_unit_vec = previous_unit_vec
        else:
            self.previous_unit_vec = {'x': 0.0, 'y': 0.0, 'z': 0.0, 'e': 0.0}

        if previous_nominal_speed_sqr:
            self.previous_nominal_speed_sqr = previous_nominal_speed_sqr
        else:
            self.previous_nominal_speed_sqr = 0.0

        if previous_feed_rate:
            self.previous_feed_rate = previous_feed_rate
        else:
            self.previous_feed_rate = 0.0

    def get_position(self):
        return self.position

    def set_position(self,position):
        self.position = position

    def get_previous_unit_vec(self):
        return self.previous_unit_vec

    def set_previous_unit_vec(self,previous_unit_vec):
        self.previous_unit_vec = previous_unit_vec

    def get_previous_nominal_speed_sqr(self):
        return self.previous_nominal_speed_sqr

    def set_previous_nominal_speed_sqr(self,previous_nominal_speed_sqr):
        self.previous_nominal_speed_sqr = previous_nominal_speed_sqr

    def get_previous_feed_rate(self):
        return self.previous_feed_rate

    def set_previous_feed_rate(self, new_previous_feed_rate):
        self.previous_feed_rate = new_previous_feed_rate

# from http://www.physics.rutgers.edu/~masud/computing/WPark_recipes_in_python.html
def cbrt(x):
    from math import pow

    if x >= 0:
        return pow(x, 1.0 / 3.0)
    else:
        return -pow(abs(x), 1.0 / 3.0)


class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg
