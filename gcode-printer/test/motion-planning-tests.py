from Queue import Empty
from hamcrest import assert_that, not_none, equal_to, close_to
from math import sqrt
import unittest
from trinamic_3d_printer.Printer import _calculate_relative_vector, find_shortest_vector, PrintQueue


class VectorTests(unittest.TestCase):
    def test_print_queue_blocking(self):
        axis_config = {
            'x': {
                'max_acceleration': 1,
                'max_speed':1
            },
            'y': {
                'max_acceleration': 1,
                'max_speed':1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=2, max_length=5)
        for i in range(5):
            position = {
                'x': i,
                'y': i,
                'f': 1
            }
            queue.add_movement(position)
        try:
            queue.next_movment(timeout=0.01)
            exception_thrown = False
        except Empty:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(True))
        queue.add_movement({
            'x': 0,
            'y': 1
        })
        try:
            queue.next_movment(timeout=0.01)
            exception_thrown = False
        except Empty:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(True))
        queue.add_movement(position)
        try:
            queue.next_movment(timeout=0.01)
            exception_thrown = False
        except Empty:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(False))
        for i in range(2):
            queue.next_movment()
        try:
            queue.next_movment(timeout=0.01)
            exception_thrown = False
        except Empty:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(True))


    def test_vector_math(self):
        result = _calculate_relative_vector(1, 1)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['l'], close_to(1, 0.0001))

        result = _calculate_relative_vector(23, 23)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['l'], close_to(1, 0.0001))

        result = _calculate_relative_vector(0, 0)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(0))
        assert_that(result['l'], equal_to(0))

        result = _calculate_relative_vector(0, 20)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(1))
        assert_that(result['l'], equal_to(1))

    def test_vector_comparison(self):
        testvectors = [
            {
                'x': 1,
                'y': 1
            },
            {
                'x': 0.5,
                'y': 0.5
            },
            {
                'x': 2.3,
                'y': 2.3
            }
        ]
        result = find_shortest_vector(testvectors)
        assert_that(result['x'], equal_to(0.5))