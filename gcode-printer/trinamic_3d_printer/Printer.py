from trinamic_3d_printer.Machine import Machine

__author__ = 'marcus'


class Printer():
    def __init__(self):
        self.ready = False
        self.config = None
        self.x_axis_motor = None
        self.y_axis_motor = None
        #finally create and conect the machine
        self.machine = Machine()
        self.machine.connect()

    def configure(self, config):
        if not config:
            raise PrinterError("No printer config given!")

        x_axis_config = config["x-axis"]
        self.x_axis_motor = x_axis_config["motor"]
        self.set_current(x_axis_config)

        y_axis_config = config["y-axis"]
        self.y_axis_motor = y_axis_config["motor"]
        self.set_current(y_axis_config)

        self.config = config

    def set_current(self, axis_config):
        motor = axis_config["motor"]
        current = axis_config["current"]
        self.machine.set_current(motor, current)



class PrinterError(Exception):
    def __init__(self, msg):
        self.msg = msg

