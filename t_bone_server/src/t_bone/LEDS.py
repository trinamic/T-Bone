from Adafruit_BBIO import GPIO
import logging

__author__ = 'marcus'
_logger = logging.getLogger(__name__)

_led_pins = [
    "P8_11",
    "P8_12",
    "P8_14",
    "P8_16"
]


class LedManager:
    def __init__(self):
        for led in _led_pins:
            GPIO.setup(led, GPIO.OUT)
            GPIO.output(led, GPIO.LOW)

    def light(self, led, value):
        try:
            if 0 < led < len(_led_pins):
                if value:
                    GPIO.output(_led_pins[led], GPIO.HIGH)
                else:
                    GPIO.output(_led_pins[led], GPIO.LOW)
        except:
            _logger.warn("Unable to set LED %s to %s", led, value)