from Adafruit_BBIO import PWM
import unittest

__author__ = 'marcus'


class PWMTests(unittest.TestCase):
    def test_multiple_PWM(self):
        try:
            PWM.start("P9_16")
            PWM.start("P9_14")
            PWM.start("P8_19")
            PWM.set_duty_cycle("P9_16", 0.1)
            PWM.set_duty_cycle("P9_16", 0)
            PWM.set_duty_cycle("P9_14", 0.1)
            PWM.set_duty_cycle("P9_14", 0)
        finally:
            PWM.cleanup()