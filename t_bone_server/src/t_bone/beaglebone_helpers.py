from flask import logging
import os

from printer import Printer

__author__ = 'marcus'
_logger = logging.getLogger(__name__)

_reset_pin = "P9_12"
_default_serial_port = "/dev/ttyO1"
_serial_port_config_file = "/sys/devices/bone_capemgr.%s/slots"
_create_serial_port_script = "echo BB-UART1 > %s"

ALLOWED_EXTENSIONS = {'gcode'}


def check_for_serial_port():
    serial_port_config_file = "/sys/devices/bone_capemgr.9/slots"
    for i in range(8, 9):
        if os.path.exists(_serial_port_config_file % i):
            serial_port_config_file = _serial_port_config_file % i
            break
    if not os.path.exists(_default_serial_port):
        _logger.info("No serial port %s, creating it", _default_serial_port)
        os.system(_create_serial_port_script % serial_port_config_file)
    return _default_serial_port


def create_printer():
    serial_port_ = check_for_serial_port()
    printer = Printer(serial_port=serial_port_, reset_pin=_reset_pin)

    #basically the printer is just a bunch of stuff
    return printer


def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS
