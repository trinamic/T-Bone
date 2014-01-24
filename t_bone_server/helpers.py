import os
from t_bone_server import _logger
from trinamic_3d_printer.printer import Printer

__author__ = 'marcus'

_reset_pin = "P8_26"
_default_serial_port = "/dev/ttyO1"
_create_serial_port_script = "echo BB-UART1 > /sys/devices/bone_capemgr.8/slots"


def check_for_serial_port():
    if not os.path.exists(_default_serial_port):
        _logger.info("No serial port %s, creating it", _default_serial_port)
        os.system(_create_serial_port_script)
    return _default_serial_port


def create_printer():
    serial_port_ = check_for_serial_port()
    printer = Printer(serial_port=serial_port_, reset_pin=_reset_pin)

    #basically the printer is just a bunch of stuff
    return printer