from trinamic_3d_printer.gcode import GCode, decode_gcode_line
from hamcrest import *

__author__ = 'marcus'
import unittest

class GCodeTest(unittest.TestCase):

    def testGCodeDecoding(self):
        line="M107"
        result = decode_gcode_line(line)
        assert_that(result.code,equal_to("M107"))


