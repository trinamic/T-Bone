from Queue import Empty, Full
from hamcrest import assert_that, not_none, equal_to, close_to, less_than_or_equal_to, greater_than, less_than, \
    has_length
from math import sqrt
from threading import Thread
import unittest
from trinamic_3d_printer.printer import _calculate_relative_vector, find_shortest_vector, PrintQueue, Printer


class VectorTests(unittest.TestCase):
    def test_print_queue_blocking(self):
        default_timeout = 0.1
        axis_config = {
            'x': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1
            },
            'y': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=2, max_length=5)
        for i in range(5):
            position = {
                'x': i,
                'y': i,
                'f': 1
            }
            queue.add_movement(position, timeout=default_timeout)
        try:
            queue.add_movement({
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
                'bow': 1
            },
            'y': {
                'max_acceleration': 1,
                'max_speed': 1,
                'bow': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=2, max_length=5)

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
                'bow': 1
            },
            'y': {
                'max_acceleration': 0.5,
                'max_speed': max_speed_y,
                'bow': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=20, max_length=21)
        #TODO add a movement to check if it accelerates correctly
        #we do not need any real buffer
        for i in range(6):
            queue.add_movement({
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
                'x': i + 1,
                'y': i + 1,
            })
        last_movement = queue.previous_movement
        assert_that(last_movement['speed']['x'], close_to(max_speed_y, 0.01))  #becaus we go 1/1 each time
        assert_that(last_movement['speed']['y'], close_to(max_speed_y, 0.01))
        #and check if we can do a full stop
        queue.add_movement({
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
        result = _calculate_relative_vector(1, 1)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['l'], close_to(1.4, 0.1))

        result = _calculate_relative_vector(23, 23)
        assert_that(result, not_none())
        assert_that(result['x'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['y'], close_to(1 / sqrt(2), 0.0001))
        assert_that(result['l'], close_to(32.5, 0.1))

        result = _calculate_relative_vector(0, 0)
        assert_that(result, not_none())
        assert_that(result['x'], equal_to(0))
        assert_that(result['y'], equal_to(0))
        assert_that(result['l'], equal_to(0))

        result = _calculate_relative_vector(0, 20)
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
                'motor': 0
            },
            'y': {
                'max_acceleration': 0.5,
                'max_step_acceleration': 2,
                'max_speed': max_speed,
                'bow': 1,
                'bow_step': 11,
                'scale': 11,
                'motor': 1
            }
        }
        queue = PrintQueue(axis_config=axis_config, min_length=20, max_length=21)
        queue.default_target_speed = max_speed
        #add some circle
        queue.add_movement({
            'x': 1,
            'y': 0
            #movement 0
        })
        queue.add_movement({
            'x': 2,
            'y': 0
            #movement 1
        })
        queue.add_movement({
            'x': 3,
            'y': 0
            #movement 2
        })
        queue.add_movement({
            'x': 4,
            'y': 1
            #movement 3
        })
        queue.add_movement({
            'x': 4,
            'y': 2
            #movement 4
        })
        queue.add_movement({
            'x': 4,
            'y': 3
            #movement 5
        })
        queue.add_movement({
            'x': 3,
            'y': 4
            #movement 6
        })
        queue.add_movement({
            'x': 2,
            'y': 4
            #movement 7
        })
        queue.add_movement({
            'x': 0,
            'y': 4
            #movement 8
        })
        queue.add_movement({
            'x': 0,
            'y': 2
            #movement 9
        })
        queue.add_movement({
            'x': 0,
            'y': 1
            #movement 10
        })
        queue.add_movement({
            'x': 1,
            'y': 0
            #movement 11
        })
        queue.add_movement({
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
        printer.axis = axis_config

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
            delta_x, delta_y, move_vector, step_pos, step_speed_vector = printer._add_movement_calculations(movement)
            assert_that(step_pos['x'], equal_to(movement['x'] * 7))
            assert_that(step_pos['y'], equal_to(movement['y'] * 11))
            assert_that(step_speed_vector['x'], equal_to(int(movement['speed']['x'] * 7)))
            assert_that(step_speed_vector['y'], equal_to(int(movement['speed']['y'] * 11)))
            x_move_config, y_move_config = printer._generate_move_config(movement, step_pos, step_speed_vector)
            assert_that(x_move_config['motor'], equal_to(0))
            assert_that(y_move_config['motor'], equal_to(1))
            assert_that(x_move_config['acceleration'], equal_to(1))
            assert_that(y_move_config['acceleration'], equal_to(2))
            assert_that(x_move_config['deceleration'], equal_to(1))
            assert_that(y_move_config['deceleration'], equal_to(2))
            assert_that(x_move_config['startBow'], equal_to(7))
            assert_that(y_move_config['startBow'], equal_to(11))
            assert_that(x_move_config['endBow'], equal_to(7))
            assert_that(y_move_config['endBow'], equal_to(11))
            assert_that(x_move_config['target'], equal_to(movement['x'] * 7))
            assert_that(y_move_config['target'], equal_to(movement['y'] * 11))
            assert_that(x_move_config['speed'], equal_to(abs(int(movement['speed']['x'] * 7))))
            assert_that(y_move_config['speed'], equal_to(abs(int(movement['speed']['y'] * 11))))
            ##collect the move configs for later analyisis
            move_configs.append({
                'x': x_move_config,
                'y': y_move_config
            })
            #move a bit just like the real thing
            printer._move(delta_x, delta_y, move_vector, step_pos, x_move_config, y_move_config)

        assert_that(nr_of_commands, equal_to(12))
        assert_that(len(move_configs), equal_to(12))

        for i in range(0, 2):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
            assert_that(move_configs[i]['y']['type'], equal_to('stop'))
        assert_that(move_configs[3]['x']['type'], equal_to('stop'))
        for i in range(3, 5):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        assert_that(move_configs[5]['y']['type'], equal_to('way'))
        assert_that(move_configs[8]['x']['type'], equal_to('stop'))
        assert_that(move_configs[7]['x']['type'], equal_to('way'))
        for i in range(0, 3):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
        for i in range(0, 2):
            assert_that(move_configs[i]['y']['type'], equal_to('stop'))
        for i in range(3, 5):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        for i in range(4, 5):
            assert_that(move_configs[i]['x']['type'], equal_to('stop'))
        for i in range(6, 8):
            assert_that(move_configs[i]['x']['type'], equal_to('way'))
        for i in range(7, 8):
            assert_that(move_configs[i]['y']['type'], equal_to('stop'))
        for i in range(9, 10):
            assert_that(move_configs[i]['y']['type'], equal_to('way'))
        for i in range(9, 10):
            assert_that(move_configs[i]['x']['type'], equal_to('stop'))

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
            x_found = False
            y_found = False
            for machine_move in machine_move_config:
                if machine_move['motor'] == 0:
                    assert_that(x_found, equal_to(False))
                    x_found = True
                if machine_move['motor'] == 1:
                    assert_that(y_found, equal_to(False))
                    y_found = True
            if not x_found:
                assert_that(x_move_config['speed'] == 0)
            if not y_found:
                assert_that(y_move_config['speed'] == 0)
            assert_that(machine_move['acceleration'],equal_to(machine_move['deceleration']))
            assert_that(machine_move['startBow'],equal_to(machine_move['endBow']))
            assert_that(x_found or y_found, equal_to(True))

        for command_number, machine_command in enumerate(machine_move_list):
            compare_move_configs(machine_command, move_configs[command_number]['x'], move_configs[command_number]['y'])