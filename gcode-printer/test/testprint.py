"""Module docstring.

This serves as a long usage message.
"""
import json
import logging
from pprint import pprint
import sys
import getopt
from trinamic_3d_printer.Machine import Machine
from trinamic_3d_printer.Printer import Printer

_config_file = "config.json"

class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg


def main(argv=None):
    #configure the overall logging
    logging.basicConfig(filename='print.log', level=logging.INFO)
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

    ##ok here we go
    printer = Printer()
    config = read_config()
    printer.configure(config)

def read_config():
    json_config_file=open(_config_file)
    data = json.load(json_config_file)
    pprint(data)
    json_config_file.close()
    return data


if __name__ == "__main__":
    sys.exit(main())