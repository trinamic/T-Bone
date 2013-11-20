from hamcrest import assert_that, not_none, equal_to, close_to
from math import sqrt
import unittest
from trinamic_3d_printer.Printer import calculate_relative_vector, find_shortest_vector


class VectorTests(unittest.TestCase):
    def test_vector_math(self):
        result = calculate_relative_vector(1, 1)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1/sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1/sqrt(2), 0.0001))
        assert_that(result['l'], close_to(1, 0.0001))

        result = calculate_relative_vector(23, 23)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1/sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1/sqrt(2), 0.0001))
        assert_that(result['l'], close_to(1, 0.0001))

        result = calculate_relative_vector(0, 0)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(0))
        assert_that(result['l'], equal_to(0))

        result = calculate_relative_vector(0, 20)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(1))
        assert_that(result['l'], equal_to(1))

    def test_vector_comparison(self):
        testvectors= [
            {
                'x':1,
                'y':1
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