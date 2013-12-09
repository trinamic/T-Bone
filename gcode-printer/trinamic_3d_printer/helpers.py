from numpy import sqrt

__author__ = 'marcus'

def _convert_mm_to_steps(millimeters, conversion_factor):
    if millimeters is None:
        return None
    return int(millimeters * conversion_factor)


def _calculate_relative_vector(delta_x, delta_y):
    length = sqrt(delta_x ** 2 + delta_y ** 2)
    if length == 0:
        return {
            'x': 0.0,
            'y': 0.0,
            'l': 0.0
        }
    return {
        'x': float(delta_x) / length,
        'y': float(delta_y) / length,
        'l': length
    }


def find_shortest_vector(vector_list):
    find_list = list(vector_list)
    #ensure it is a list
    shortest_vector = 0
    #and wildly guess the shortest vector
    for number, vector in enumerate(find_list):
        if (vector['x'] ** 2 + vector['y'] ** 2) < (
                    find_list[shortest_vector]['x'] ** 2 + find_list[shortest_vector]['y'] ** 2):
            shortest_vector = number
    return find_list[shortest_vector]

