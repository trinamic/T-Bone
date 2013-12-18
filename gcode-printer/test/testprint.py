"""Module docstring.

This serves as a long usage message.
"""
from Adafruit_BBIO import GPIO
import json
import logging
import os
import sys
import getopt
from trinamic_3d_printer.Printer import Printer
from trinamic_3d_printer.gcode import read_gcode_to_printer

_config_file = "testprint-config.json"
_print_file = "../reference designs/test-model.gcode"
_default_serial_port = "/dev/ttyO1"
_reset_pin = "P8_26"
_create_serial_port_script = "echo BB-UART1 > /sys/devices/bone_capemgr.8/slots"
_logger = logging.getLogger(__name__)


class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg


def main(argv=None):
    try:
        #configure the overall logging
        logging.basicConfig(filename='print.log', level=logging.INFO,
                            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        logging.info('Started')

        if argv is None:
            argv = sys.argv
        try:
            try:
                opts, args = getopt.getopt(argv[1:], "p:h", ["port=", "help"])
            except getopt.error, msg:
                raise Usage(msg)
                # more code, unchanged
        except Usage, err:
            print >> sys.stderr, err.msg
            print >> sys.stderr, "for help use --help"
            return 2

        if not os.path.exists(_default_serial_port):
            _logger.info("No serial port %s, creating it", _default_serial_port)
            os.system(_create_serial_port_script)

        ##ok here we go
        _logger.info("connecting to printer at %s", _default_serial_port)
        printer = Printer(serial_port=_default_serial_port, reset_pin=_reset_pin)

        config = read_config()
        #configure the printer
        printer.configure(config)
        _logger.info("printing %s", _print_file)
        printer.home(axis={'x-axis', 'y-axis'})
        #test_file = open(_print_file)
        #printer.start_print()
        #read_gcode_to_printer(test_file, printer)
        #printer.stop_print()
        _logger.info("finished printing")
    finally:
        GPIO.cleanup()


def read_config():
    json_config_file = open(_config_file)
    data = json.load(json_config_file)
    json_config_file.close()
    return data


if __name__ == "__main__":
    sys.exit(main())