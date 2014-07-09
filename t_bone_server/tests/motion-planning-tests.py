from Queue import Empty, Full
from math import sqrt
from threading import Thread
import unittest

from hamcrest import assert_that, not_none, equal_to, close_to, less_than_or_equal_to, greater_than, less_than, \
    has_length, none
from t_bone.printer import calculate_relative_vector, find_shortest_vector, PrintQueue, Printer, get_target_velocity


class VectorTests(unittest.TestCase):
    def test_print_queue_blocking(self):
        default_timeout = 0.1
        axis_config = {
            'x': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1,
                'steps_per_mm': 1
            },
            'y': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1,
                'steps_per_mm': 1
            },
            'z': {
                'steps_per_mm': 1
            },
            'e': {
                'steps_per_mm': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=2, max_length=5)
        queue.default_target_speed = 1

        for i in range(5):
            position = {
                'type': 'move',
                'x': i,
                'y': i,
                'f': 1
            }
            queue.add_movement(position, timeout=default_timeout)
        try:
            queue.add_movement({
                                   'type': 'move',
                                   'x': 0,
                                   'y': 1
                               }, timeout=default_timeout)
            exception_thrown = False
        except Full:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(True))
        for i in range(3):
            try:
                queue.next_movement(timeout=default_timeout)
                exception_thrown = False
            except Empty:
                exception_thrown = True
            assert_that(exception_thrown, equal_to(False))
        try:
            queue.next_movement(timeout=default_timeout)
            exception_thrown = False
        except Empty:
            exception_thrown = True
        assert_that(exception_thrown, equal_to(True))

    def test_print_queue_emptying(self):
        default_timeout = 0.1
        axis_config = {
            'x': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1,
                'steps_per_mm': 1
            },
            'y': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1,
                'steps_per_mm': 1
            },
            'z': {
                'steps_per_mm': 1
            },
            'e': {
                'steps_per_mm': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=2, max_length=5)
        queue.default_target_speed = 1

        class QueueEmptyThread(Thread):
            def __init__(self, queue):
                super(QueueEmptyThread, self).__init__()
                self.running = True
                self.queue = queue

            def run(self):
                while self.running:
                    self.queue.next_movement()
                    #and throw away

        emptyerThread = QueueEmptyThread(queue=queue, )
        emptyerThread.start()

        try:
            for i in range(25):
                position = {
                    'type': 'move',
                    'x': i,
                    'y': i,
                    'f': 1
                }
                queue.add_movement(position, timeout=default_timeout)
            queue.finish(timeout=default_timeout)
            assert_that(queue.queue.empty(), equal_to(True))
            assert_that(queue.planning_list, has_length(0))
        finally:
            emptyerThread.running = False
            queue.add_movement(
                {
                    'type': 'move',
                    'x': 0,
                    'y': 0
                })
            queue.finish(timeout=default_timeout)
            emptyerThread.join()

    def test_print_queue_calculations(self):
        default_timeout = 0.1
        max_speed_x = 3
        max_speed_y = 2
        axis_config = {
            'x': {
                'max_acceleration': 0.5,
                'max_speed': max_speed_x,
                'bow': 1,
                'steps_per_mm': 1
            },
            'y': {
                'max_acceleration': 0.5,
                'max_speed': max_speed_y,
                'bow': 1,
                'steps_per_mm': 1
            },
            'z': {
                'steps_per_mm': 1
            },
            'e': {
                'steps_per_mm': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=20, max_length=21)
        queue.default_target_speed = 5

        #TODO add a movement to check if it accelerates correctly
        #we do not need any real buffer
        for i in range(6):
            queue.add_movement({
                'type': 'move',
                'x': i + 1,
                'y': i + 1,
                'f': 10
            })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed'], not_none())
        assert_that(last_movement['speed']['x'], not_none())
        assert_that(last_movement['speed']['x'], less_than_or_equal_to(max_speed_x))
        assert_that(last_movement['speed']['y'], not_none())
        assert_that(last_movement['speed']['y'], less_than_or_equal_to(max_speed_y))
        assert_that(last_movement['speed']['x'], close_to(max_speed_y, 0.01))  #becaus we go 1/1 each time
        assert_that(last_movement['speed']['y'], close_to(max_speed_y, 0.01))
        queue.add_movement({
            'type': 'move',
            'x': 7,
            'y': 6
        })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed']['x'], less_than_or_equal_to(max_speed_x))
        assert_that(last_movement['speed']['x'], greater_than(0))
        assert_that(last_movement['speed']['y'], equal_to(0))
        previous_movement = queue.planning_list[-1]
        assert_that(previous_movement['speed']['x'], less_than(max_speed_x))
        assert_that(previous_movement['speed']['y'], less_than(max_speed_y))
        assert_that(previous_movement['y_stop'], equal_to(True))
        #we still go on in x - so in thery we can speed up to desired target speed
        assert_that(last_movement['speed']['x'], greater_than(previous_movement['speed']['x']))
        previous_movement = queue.planning_list[-3]
        assert_that(previous_movement['speed']['x'], close_to(max_speed_y, 0.5))  #becaus we go 1/1 each time
        assert_that(previous_movement['speed']['y'], close_to(max_speed_y, 0.5))
        queue.add_movement({
            'type': 'move',
            'x': 7,
            'y': 7
        })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed']['x'], equal_to(0))
        assert_that(last_movement['speed']['y'], greater_than(0))
        assert_that(last_movement['speed']['y'], less_than(max_speed_y))
        previous_movement = queue.planning_list[-1]
        assert_that(previous_movement['speed']['x'], less_than(max_speed_x))
        assert_that(previous_movement['x_stop'], equal_to(True))
        assert_that(previous_movement['speed']['y'], less_than(max_speed_y))
        assert_that(previous_movement['speed']['y'], equal_to(0))
        previous_movement = queue.planning_list[-4]
        assert_that(previous_movement['speed']['x'], close_to(max_speed_y, 0.5))  #becaus we go 1/1 each time
        assert_that(previous_movement['speed']['y'], close_to(max_speed_y, 0.5))
        # let's go back to zero to begin a new test
        queue.add_movement({
            'type': 'move',
            'x': 0,
            'y': 7
        })
        last_movement = queue.previous_movement
        #it is a long go - so we should be able to speed up to full steam
        assert_that(last_movement['speed']['x'], close_to(-max_speed_x, 0.5))
        assert_that(last_movement['speed']['y'], equal_to(0))
        previous_movement = queue.planning_list[-1]
        assert_that(previous_movement['y_stop'], equal_to(True))
        queue.add_movement({
            'type': 'move',
            'x': 0,
            'y': 0
        })
        last_movement = queue.previous_movement
        #it is a long go - so we should be able to speed up to full steam
        assert_that(last_movement['speed']['x'], equal_to(0))
        assert_that(last_movement['speed']['y'], close_to(-max_speed_y, 0.5))
        previous_movement = queue.planning_list[-1]
        assert_that(previous_movement['x_stop'], equal_to(True))
        #speed up
        for i in range(4):
            queue.add_movement({
                'type': 'move',
                'x': i + 1,
                'y': i + 1,
            })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed']['x'], close_to(max_speed_y, 0.01))  #becaus we go 1/1 each time
        assert_that(last_movement['speed']['y'], close_to(max_speed_y, 0.01))
        #and check if we can do a full stop
        queue.add_movement({
            'type': 'move',
            'x': 3,
            'y': 3
        })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed']['x'], less_than(0))
        assert_that(last_movement['speed']['x'], greater_than(-max_speed_x))
        assert_that(last_movement['speed']['y'], less_than(0))
        assert_that(last_movement['speed']['y'], greater_than(-max_speed_y))
        previous_movement = queue.planning_list[-1]
        assert_that(previous_movement['speed']['x'], less_than(max_speed_x))
        assert_that(previous_movement['speed']['y'], less_than(max_speed_y))
        assert_that(previous_movement['y_stop'], equal_to(True))
        assert_that(previous_movement['x_stop'], equal_to(True))
        another_previous_movement = queue.planning_list[-3]
        assert_that(another_previous_movement['speed']['x'],
                    greater_than(previous_movement['speed']['x']))  #becaus we stopped to turn around
        assert_that(another_previous_movement['speed']['y'], greater_than(previous_movement['speed']['x']))


    def test_vector_math(self):
        result = calculate_relative_vector(1, 1, 0, 1)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(3), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(3), 0.0001))
        assert_that(result['l'], close_to(1.7, 0.1))

        result = calculate_relative_vector(23, 23, 0, 0)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['l'], close_to(32.5, 0.1))

        result = calculate_relative_vector(0, 0, 0, 12)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(0))
        assert_that(result['l'], equal_to(12))

        result = calculate_relative_vector(0, 20, 0, 0)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(1))
        assert_that(result['l'], equal_to(20))

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
        testvectors = [
            {
                'x': 0,
                'y': 1
            },
            {
                'x': 0,
                'y': 0.5
            },
            {
                'x': 0,
                'y': 2.3
            }
        ]
        result = find_shortest_vector(testvectors)
        assert_that(result['y'], equal_to(0.5))

    def test_motion_planning(self):
        max_speed = 2
        axis_config = {
            'x': {
                'max_acceleration': 0.5,
                'max_step_acceleration': 1,
                'max_speed': max_speed,
                'bow': 1,
                'bow_step': 7,
                'scale': 7,
                'motor': 0,
                'steps_per_mm': 7
            },
            'y': {
                'max_acceleration': 0.5,
                'max_step_acceleration': 2,
                'max_speed': max_speed,
                'bow': 1,
                'bow_step': 11,
                'scale': 11,
                'motor': 1,
                'steps_per_mm': 11
            },
            'z': {
                'steps_per_mm': 1,
                'max_speed': 10
            },
            'e': {
                'steps_per_mm': 1,
                'max_speed': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=20, max_length=21)
        queue.default_target_speed = max_speed
        #add some circle
        queue.add_movement({
            'type': 'move',
            'x': 1,
            'y': 0
            #movement 0
        })
        queue.add_movement({
            'type': 'move',
            'x': 2,
            'y': 0
            #movement 1
        })
        queue.add_movement({
            'type': 'move',
            'x': 3,
            'y': 0
            #movement 2
        })
        queue.add_movement({
            'type': 'move',
            'x': 4,
            'y': 1
            #movement 3
        })
        queue.add_movement({
            'type': 'move',
            'x': 4,
            'y': 2
            #movement 4
        })
        queue.add_movement({
            'type': 'move',
            'x': 4,
            'y': 3
            #movement 5
        })
        queue.add_movement({
            'type': 'move',
            'x': 3,
            'y': 4
            #movement 6
        })
        queue.add_movement({
            'type': 'move',
            'x': 2,
            'y': 4
            #movement 7
        })
        queue.add_movement({
            'type': 'move',
            'x': 0,
            'y': 4
            #movement 8
        })
        queue.add_movement({
            'type': 'move',
            'x': 0,
            'y': 2
            #movement 9
        })
        queue.add_movement({
            'type': 'move',
            'x': 0,
            'y': 1
            #movement 10
        })
        queue.add_movement({
            'type': 'move',
            'x': 1,
            'y': 0
            #movement 11
        })
        queue.add_movement({
            'type': 'move',
            'x': 2,
            'y': 0
            #movement 12
        })
        assert_that(queue.previous_movement, not_none())
        assert_that(queue.planning_list, has_length(12))
        planned_list = queue.planning_list

        assert_that(planned_list[1]['speed']['x'], equal_to(max_speed))
        assert_that(planned_list[0]['speed']['x'], greater_than(0))
        assert_that(planned_list[0]['speed']['x'], less_than(planned_list[1]['speed']['x']))
        assert_that(planned_list[3]['speed']['x'], less_than(planned_list[1]['speed']['x']))
        assert_that(planned_list[3]['speed']['x'], equal_to(planned_list[0]['speed']['x']))
        assert_that(planned_list[3]['speed']['x'], greater_than(0))
        for i in range(0, 3):
            assert_that(planned_list[i]['delta_y'], equal_to(0))
            assert_that(planned_list[i]['speed']['y'], equal_to(0))
            assert_that(planned_list[i]['speed']['x'], greater_than(0))

        try:
            assert_that(planned_list[2]['x_stop'], not (equal_to(True)))
        except KeyError:
            pass  #ok, this is enough - it should just not be true
        assert_that(planned_list[3]['x_stop'], equal_to(True))

        assert_that(planned_list[4]['speed']['y'], equal_to(max_speed))
        assert_that(planned_list[3]['speed']['y'], greater_than(0))
        assert_that(planned_list[3]['speed']['y'], less_than(planned_list[4]['speed']['y']))
        assert_that(planned_list[6]['speed']['y'], less_than(planned_list[4]['speed']['y']))
        assert_that(planned_list[6]['speed']['y'], greater_than(0))
        assert_that(planned_list[6]['speed']['y'], equal_to(planned_list[3]['speed']['y']))
        assert_that(planned_list[3]['delta_x'], equal_to(1))

        for i in range(4, 5):
            assert_that(planned_list[i]['delta_x'], equal_to(0))
            assert_that(planned_list[i]['speed']['x'], equal_to(0))
            assert_that(planned_list[i]['speed']['y'], greater_than(0))

        try:
            assert_that(planned_list[5]['y_stop'], not (equal_to(True)))
        except KeyError:
            pass  #ok, this is enough - it should just not be true
        assert_that(planned_list[6]['y_stop'], equal_to(True))

        assert_that(planned_list[7]['speed']['x'], equal_to(-max_speed))
        assert_that(planned_list[6]['speed']['x'], less_than(0))
        assert_that(planned_list[6]['speed']['x'], greater_than(planned_list[7]['speed']['x']))
        assert_that(planned_list[8]['speed']['x'], greater_than(planned_list[7]['speed']['x']))
        assert_that(planned_list[8]['speed']['x'], less_than(0))
        assert_that(planned_list[8]['delta_x'], equal_to(-2))
        assert_that(planned_list[8]['x_stop'], equal_to(True))

        for i in range(7, 8):
            assert_that(planned_list[i]['delta_y'], equal_to(0))
            assert_that(planned_list[i]['speed']['y'], equal_to(0))

        try:
            assert_that(planned_list[7]['x_stop'], not (equal_to(True)))
        except KeyError:
            pass  #ok, this is enough - it should just not be true
        assert_that(planned_list[8]['x_stop'], equal_to(True))

        for i in range(9, 10):
            assert_that(planned_list[i]['delta_x'], equal_to(0))
            assert_that(planned_list[i]['speed']['x'], equal_to(0))
            assert_that(planned_list[i]['speed']['y'], less_than(0))

        try:
            assert_that(planned_list[10]['y_stop'], not (equal_to(True)))
        except KeyError:
            pass  #ok, this is enough - it should just not be true
        assert_that(planned_list[11]['y_stop'], equal_to(True))

        #ok so far so good - but let's see if this is correctly converted to a motion
        printer = Printer(serial_port="none", reset_pin="X")
        printer.default_speed = 1
        printer.axis = axis_config
        printer._postconfig()

        class DummyMaychine:
            move_list = []

            def move_to(self, move_config):
                self.move_list.append(move_config)

        printer.machine = DummyMaychine()

        #easy to detect
        nr_of_commands = 0
        move_configs = []
        for movement in planned_list:
            nr_of_commands += 1
            step_pos, step_speed_vector = printer._add_movement_calculations(movement)
            assert_that(step_pos['x'], equal_to(movement['x'] * 7))
            assert_that(step_pos['y'], equal_to(movement['y'] * 11))
            assert_that(step_speed_vector['x'], equal_to(int(movement['speed']['x'] * 7)))
            assert_that(step_speed_vector['y'], equal_to(int(movement['speed']['y'] * 11)))
            x_move_config, y_move_config, z_move_config, e_move_config = printer._generate_move_config(movement,
                                                                                                       step_pos,
                                                                                                       step_speed_vector)
            if x_move_config:
                assert_that(x_move_config['motor'], equal_to(0))
                assert_that(x_move_config['acceleration'], equal_to(1))
                assert_that(x_move_config['startBow'], equal_to(7))
                assert_that(x_move_config['target'], equal_to(movement['x'] * 7))
                assert_that(x_move_config['speed'], equal_to(abs(int(movement['speed']['x'] * 7))))

            if y_move_config:
                assert_that(y_move_config['motor'], equal_to(1))
                assert_that(y_move_config['acceleration'], equal_to(2))
                assert_that(y_move_config['startBow'], equal_to(11))
                assert_that(y_move_config['target'], equal_to(movement['y'] * 11))
                assert_that(y_move_config['speed'], equal_to(abs(int(movement['speed']['y'] * 11))))
            ##collect the move configs for later analyisis
            move_configs.append({
                'x': x_move_config,
                'y': y_move_config
            })
            #move a bit just like the real thing
            printer._move(movement, step_pos, x_move_config, y_move_config, z_move_config, e_move_config)

        assert_that(nr_of_commands, equal_to(12))
        assert_that(len(move_configs), equal_to(12))

        for i in range(0, 2):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
            assert_that(move_configs[i]['y'], equal_to(None))
        assert_that(move_configs[3]['x']['type'], equal_to('stop'))
        for i in range(3, 5):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        assert_that(move_configs[5]['y']['type'], equal_to('way'))
        assert_that(move_configs[8]['x']['type'], equal_to('stop'))
        assert_that(move_configs[7]['x']['type'], equal_to('way'))
        for i in range(0, 3):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
        for i in range(0, 2):
            assert_that(move_configs[i]['y'], none())
        for i in range(3, 5):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        assert_that(move_configs[3]['x']['type'], equal_to('stop'))
        for i in range(4, 5):
            assert_that(move_configs[i]['x'], none())
        for i in range(6, 8):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
        assert_that(move_configs[6]['y']['type'], equal_to('stop'))
        for i in range(7, 8):
            assert_that(move_configs[i]['y'], none())
        for i in range(9, 10):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        assert_that(move_configs[8]['x']['type'], equal_to('stop'))
        for i in range(9, 10):
            assert_that(move_configs[i]['x'], none())

        #ok what do we got in our machine move list??
        machine_move_list = printer.machine.move_list

        assert_that(machine_move_list, has_length(12))
        for i in range(0, 2):
            assert_that(machine_move_list[i], has_length(1))
            assert_that(machine_move_list[i][0]['motor'], equal_to(0))
        assert_that(machine_move_list[3], has_length(2))
        for i in range(4, 5):
            assert_that(machine_move_list[i], has_length(1))
            assert_that(machine_move_list[i][0]['motor'], equal_to(1))
        assert_that(machine_move_list[6], has_length(2))
        for i in range(7, 8):
            assert_that(machine_move_list[i], has_length(1))
            assert_that(machine_move_list[i][0]['motor'], equal_to(0))
        for i in range(9, 10):
            assert_that(machine_move_list[i], has_length(1))
            assert_that(machine_move_list[i][0]['motor'], equal_to(1))
        assert_that(machine_move_list[11], has_length(2))

        def compare_move_configs(machine_move_config, x_move_config, y_move_config):
            x_move = None
            y_move = None
            for machine_move in machine_move_config:
                if machine_move['motor'] == 0:
                    assert_that(x_move, equal_to(None))
                    x_move = machine_move
                if machine_move['motor'] == 1:
                    assert_that(y_move, equal_to(None))
                    y_move = machine_move
            if not x_move:
                assert_that(x_move_config, none())
            if not y_move:
                assert_that(y_move_config, none())
            if x_move and y_move:
                ratio = float(x_move['speed']) / float(y_move['speed'])
                assert_that(float(x_move['acceleration']) / float(y_move['acceleration']), close_to(ratio, 0.0001))
                assert_that(float(x_move['startBow']) / float(y_move['startBow']), close_to(ratio, 0.0001))
            assert_that(y_move or x_move, not (equal_to(None)))

        for command_number, machine_command in enumerate(machine_move_list):
            compare_move_configs(machine_command, move_configs[command_number]['x'], move_configs[command_number]['y'])


    def test_acceleration(self):
        # we use prime numbers in the hope to get more distinguishable numbers
        jerk = 3.0
        max_acceleration = 5.0
        for v0 in (0.0, 7.0):
            time_to_max_acceleration = constant_jerk_time_to_acceleration(j=jerk, a0=0, a=max_acceleration)
            # first of all we test an acceleration below the max acceleration
            test_time = time_to_max_acceleration*2.0 - 1.0
            # for the acceleration_phase
            acceleration_time = test_time / 2.0
            target_acceleration = constant_jerk_acceleration(jerk, acceleration_time)
            target_velocity_1 = constant_jerk_speed(jerk, acceleration_time, v0=v0)
            target_distance_1 = constant_jerk_displacement(jerk, acceleration_time, v0=v0)
            # for the deceleration phase
            target_velocity_2 = constant_jerk_speed(-jerk, acceleration_time,
                                                    a0=target_acceleration, v0=target_velocity_1)
            target_distance_2 = constant_jerk_displacement(-jerk, acceleration_time,
                                                           a0=target_acceleration, v0=target_velocity_1,
                                                           x0=target_distance_1)
            # and now test the s curve acceleration
            calculated_target_velocity = get_target_velocity(start_velocity=0,
                                                                          length=target_distance_2,
                                                                          max_acceleration=max_acceleration,
                                                                          jerk=jerk)
            assert_that(calculated_target_velocity, less_than(target_velocity_2))
            # and now for some constant acceleration phase
            constant_acceleration_time = 1.0
            acceleration_time = time_to_max_acceleration
            test_time = time_to_max_acceleration + constant_acceleration_time
            target_acceleration = constant_jerk_acceleration(jerk, acceleration_time)
            target_velocity_1 = constant_jerk_speed(jerk, acceleration_time, v0=v0)
            target_distance_1 = constant_jerk_displacement(jerk, acceleration_time, v0=v0)
            target_velocity_2 = constant_acceleration_speed(target_acceleration, constant_acceleration_time,
                                                            v0=target_velocity_1)
            target_distance_2 = constant_acceleration_displacement(target_acceleration, constant_acceleration_time,
                                                                   v0=target_velocity_1)
            target_velocity_3 = constant_jerk_speed(-jerk, acceleration_time,
                                                    a0=target_acceleration, v0=target_velocity_2)
            target_distance_3 = constant_jerk_displacement(-jerk, acceleration_time,
                                                           a0=target_acceleration, v0=target_velocity_2)
            calculated_target_velocity = get_target_velocity(start_velocity=0,
                                                                          length=target_distance_1 + target_distance_2 + target_distance_3,
                                                                          max_acceleration=max_acceleration,
                                                                          jerk=jerk)
            assert_that(calculated_target_velocity, less_than(target_velocity_3))

        pass


def constant_acceleration_speed(a, t, v0=0):
    return v0 + a * t


def constant_acceleration_displacement(a, t, v0=0, x0=0):
    return x0 + v0 * t + 0.5 * a * t ** 2


def constant_jerk_acceleration(j, t, a0=0):
    return a0 + j * t


def constant_jerk_time_to_acceleration(j, a, a0=0):
    return (a - a0) / j


def constant_jerk_speed(j, t, a0=0, v0=0):
    return v0 + a0 * t + 0.5 * j * t ** 2


def constant_jerk_displacement(j, t, a0=0, v0=0, x0=0):
    return x0 + v0 * t + 0.