__author__ = 'marcus'


class GCode:
    def __init__(self, code, options=None):
        self.code = code
        self.options = options

'''
    decode a line of text to gcode.
'''
def decode_gcode_line(line):
    #we nee a result
    result = None
    #and prepare the line
    line = line.strip()
    # does it contain a comment??
    if ";" in line:
        line = line.split(";",1)[0]
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