import logging
import re
from threading import Thread

from helpers import file_len
from printer import PrinterError


__author__ = 'marcus'

_text_and_number_pattern = re.compile('([a-zA-Z]+)(\d+(.\d+)?)')
_logger = logging.getLogger(__name__)


class GCodePrintThread(Thread):
    def __init__(self, file, printer, callback):
        #this constructor could be a bit more elegant
        super(GCodePrintThread, self).__init__()
        self.file = file
        self.printer = printer
        self.callback = callback
        #let's see how many lines the print file got and reset progress
        self.lines_to_print = file_len(file)
        self.lines_printed = 0
        self.printing = False

    def run(self):
        self.printing = True
        try:
            gcode_input = open(self.file)
            _logger.info("starting GCODE interptretation from %s to %s", input, self.printer)
            self.lines_printed = 0
            self.printer.start_print()
            for line in gcode_input:
                read_gcode_to_printer(line, self.printer)
                self.lines_printed += 1
            self.printer.finish_print()
            _logger.info("fininshed gcode reading to %s ", self.printer)
            #todo and here we need some more or less clever plan - since we cannot restart the print thread
        finally:
            self.printing = False
            if self.callback:
                self.callback()


def read_gcode_to_printer(line, printer):
    gcode = decode_gcode_line(line)
    #handling the negative case first is silly but gives us more flexibility in the elif struct
    if not gcode:
        #nothing to do fine!
        pass
    elif "G0" == gcode.code or "G1" == gcode.code:  #TODO G1 & G0 is different
        #we simply interpret the arguments as positions
        positions = _decode_positions(gcode, line)
        printer.move_to(positions)
    elif "G20" == gcode.code:
        #we cannot switch to inches - sorry folks
        raise PrinterError("Currently only metric units are supported!")
    elif "G21" == gcode.code:
        _logger.info("Using metric units according to the g code")
    elif "G28" == gcode.code:
        positions = _decode_positions(gcode, line)
        homing_axis = []
        if positions:
            for axis_name in positions:
                if axis_name in printer.axis:
                    if printer.axis[axis_name]['homeable']:
                        _logger.info("Configuring axis %s for homing", axis_name)
                        homing_axis.append(axis_name)
                    else:
                        _logger.warn("Ignoring not homeable axis %s for homing", axis_name)
                else:
                    _logger.warn("Ignoring unknown axis %s for homing", axis_name)
        else:
            for axis_name, axis in printer.axis.iteritems():
                if axis['homeable']:
                    homing_axis.append(axis_name)
        _logger.info("Homing axis %s", homing_axis)
        printer.home(homing_axis)
    elif "G90" == gcode.code:
        _logger.info("Using absolute positions")
        #todo we can support relative positions - if we are a bit careful
    elif "G91" == gcode.code:
        raise PrinterError("Currently only absolute positions are supported!")
    elif "G92" == gcode.code:
        #set XPOS
        positions = _decode_positions(gcode, line)
        printer.set_position(positions)
    elif "M82" == gcode.code:
        _logger.info("Using absolute positions")
        #todo we can support relative positions - if we are a bit careful
    elif "M83" == gcode.code:
        raise PrinterError("Currently only absolute positions are supported!")
    elif "M104" == gcode.code:
        if 's' in gcode.options:
            temperature = gcode.options['s']
            if not temperature>printer.extruder_heater.max_temperature:
                printer.extruder_heater.set_temperature(temperature)
            else:
                _logger.error("Setting be temperature to %s got ignored, too hot", temperature)
    elif "M106" == gcode.code:
        if 's' in gcode.options:
            fan_speed = gcode.options['s']/255.0
            printer.set_fan(fan_speed)
        else:
            _logger.info("No fan speed given in %",gcode)
    elif "M107" == gcode.code:
            printer.set_fan(0)
    elif "M109" == gcode.code:
        #Wait for bed temperature to reach target temp
        #Example: M190 S60"
        if 's' in gcode.options:
            temperature = gcode.options['s']
            if printer.extruder_heater.get_set_temperature() < temperature:
                _logger.warn("The set temperature of %s can never reach the target temperature of %s",
                             printer.heated_bed.set_temperature, temperature)
                return
            while printer.heated_bed.get_set_temperature() < temperature:
                #todo a timeout value would be great?
                pass
    elif "M140" == gcode.code:
        if 's' in gcode.options:
            temperature = gcode.options['s']
            if printer.heated_bed:
                printer.heated_bed.set_temperature(temperature)
                #todo can this go wrong??
            else:
                _logger.info("Setting be temperature to %s got ignored", temperature)
    elif "M143" == gcode.code:
        #Maximum hot-end temperature
        #Example: M143 S275"
        #todo this is useful to implement
        pass
    elif "M190" == gcode.code:
        #Wait for bed temperature to reach target temp
        #Example: M190 S60"
        if 's' in gcode.options:
            temperature = gcode.options['s']
            if printer.heated_bed:
                if printer.heated_bed.get_set_temperature() < temperature:
                    _logger.warn("The set temperature of %s can never reach the target temperature of %s",
                                 printer.heated_bed.set_temperature, temperature)
                    return
                while printer.heated_bed.get_set_temperature() < temperature:
                    #todo a timeout value would be great?
                    pass
    else:
        _logger.warn("Unknown GCODE %s ignored", gcode)


def _decode_positions(gcode, line):
    positions = {}
    if gcode.options:
        for argument in gcode.options:
            position = decode_text_and_number(argument)
            if position:
                positions[position[0].lower()] = position[1]
            else:
                _logger.warn("Unable to interpret position %s in %s", argument, line)
    if 'f' in positions:
        positions['target_speed'] = positions['f'] / 60.0
    return positions


class GCode:
    def __init__(self, code, options=None):
        self.code = code
        self.options = options

    def __repr__(self):
        result = self.code
        if self.options:
            for option in self.options:
                result += " " + str(option)
        return result


# decode a line of text to gcode.
def decode_gcode_line(line):
    _logger.debug("decoding line %s", line)
    #we nee a result
    result = None
    #and prepare the line
    line = line.strip()
    # does it contain a comment??
    if ";" in line:
        line = line.split(";", 1)[0]
    parts = line.split()  # split by space
    #filter only the relevant parts
    relevant_parts = []
    for part in parts:
        if part.strip():
            relevant_parts.append(part.strip())
    if len(relevant_parts) > 0:
        _logger.debug("read gcode %s", relevant_parts[0])
        result = GCode(relevant_parts[0])
    if len(relevant_parts) > 1:
        result.options = relevant_parts[1:]
        _logger.debug("found arguments: %s", result.options)

    return result


def decode_text_and_number(line):
    result = None
    line = line.strip()
    if line:
        match = _text_and_number_pattern.match(line)
        if match:
            text = match.group(1)
            number = match.group(2)
            if "." in number:
                result = [text, float(number)]
            else:
                result = [text, int(number)]
    return result
