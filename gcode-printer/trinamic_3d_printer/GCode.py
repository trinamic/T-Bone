import logging
import re

__author__ = 'marcus'

_text_and_number_pattern = re.compile('([a-zA-Z]+)(\d+(.\d+)?)')


def read_gcode_to_print(input, printer):
    for line in input:
        gcode = decode_gcode_line(line)
        #handling the negative case first is silly but gives us more flexibility in the elif struct
        if not gcode:
            #nothing to do fine!
            pass
        elif "G0" is gcode.code or "G1" is gcode.code: #TODO G1 & G0 is different
            #we simply interpret the arguments as positions
            positions = {}
            for argument in gcode.options:
                position = decode_text_and_number(argument)
                if position:
                    positions[position[0].lower()] = position[1]
                else:
                    logging.warn("Unable to interpret position " + argument + " in " + line)
            printer.move_to(positions)
        else:
            logging.warn("Unknown GCODE " + gcode + " ignored")


class GCode:
    def __init__(self, code, options=None):
        self.code = code
        self.options = options


# decode a line of text to gcode.
def decode_gcode_line(line):
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
        result = GCode(relevant_parts[0])
    if len(relevant_parts) > 1:
        result.options = relevant_parts[1:]

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
