import logging
import re
from threading import Thread
from trinamic_3d_printer.helpers import file_len

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
    elif "G0" == gcode.code or "G1" == gcode.code: #TODO G1 & G0 is different
        #we simply interpret the arguments as positions
        positions = {}
        for argument in gcode.options:
            position = decode_text_and_number(argument)
            if position:
                positions[position[0].lower()] = position[1]
            else:
                _logger.warn("Unable to interpret position %s in %s", argument, line)
        printer.move_to(positions)
    else:
        _logger.warn("Unknown GCODE %s ignored", gcode)


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
    parts = line.split() # split by space
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
