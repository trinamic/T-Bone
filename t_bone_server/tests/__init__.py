
__author__ = 'marcus'
import unittest
import gcode_tests

def suite():
    suite = unittest.TestSuite()
    suite.addTest(gcode_tests.suite())
    return suite

if __name__ == '__main__':
    unittest.TextTestRunner(verbosity=2).run(suite())