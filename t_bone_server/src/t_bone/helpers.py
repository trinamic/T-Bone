from numpy import sqrt

from src.trinamic_3d_printer import machine


__author__ = 'marcus'


def convert_mm_to_steps(millimeters, conversion_factor):
    if millimeters is None:
        return None
    return int(float(millimeters) * float(conversion_factor))


def convert_velocity_clock_ref_to_realtime_ref(velocity):
    #see datasheet 9.1: v[5031] * ( fCLK[Hz]/2 / 2^23 )
    return velocity * machine.clock_frequency / 2 / (2 ** 23)

def convert_acceleration_clock_ref_to_realtime_ref(acceleration):
    #see datasheet 9.1: a[5031] * fCLK[Hz]^2 / (512*256) / 2^24
    return acceleration * machine.clock_frequency**2 / (512*256) / (2**24)


def calculate_relative_vector(delta_x, delta_y, delta_z, delta_e):
    length = sqrt(delta_x ** 2 + delta_y ** 2 + delta_z ** 2 + delta_e**2)
    if length == 0:
        return {
            'x': 0.0,
            'y': 0.0,
            'z': 0.0,
            'e': 0.0,
            'l': 0.0
        }
    return {
        'x': float(delta_x) / length,
        'y': float(delta_y) / length,
        'z': float(delta_z) / length,
        'e': float(delta_e) / length,
        'l': length
    }


def find_shortest_vector(vector_list):
    find_list = list(vector_list)
    #ensure it is a list
    shortest_vector = 0
    x_square = find_list[shortest_vector]['x'] ** 2
    y_square = find_list[shortest_vector]['y'] ** 2
    #and wildly guess the shortest vector
    for number, vector in enumerate(find_list):
        if (vector['x'] ** 2 + vector['y'] ** 2) < (
                    x_square + y_square):
            shortest_vector = number
            x_square = find_list[shortest_vector]['x'] ** 2
            y_square = find_list[shortest_vector]['y'] ** 2
    return find_list[shortest_vector]


def file_len(fname):
    i = 0
    with open(fname) as f:
        for i, l in enumerate(f):
            pass
    return i + 1