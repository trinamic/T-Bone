"""Module docstring.

This serves as a long usage message.
"""
import sys
import getopt
from logconfig import define_loggers


class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg


def main(argv=None):
    define_loggers()
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


if __name__ == "__main__":
    sys.exit(main())