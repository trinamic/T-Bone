from Adafruit_BBIO import ADC, PWM, GPIO
from flask import logging
from threading import Thread
import threading
import time
import thermistors

__author__ = 'marcus'
_logger = logging.getLogger(__name__)
_DEFAULT_READOUT_DELAY = 0.1
_DEFAULT_CURRENT_READOUT_DELAY = 60
_PWM_LOCK = threading.Lock()
_DEFAULT_MAX_TEMPERATURE = 250
ADC.setup()


class Heater(Thread):
    def __init__(self, thermometer, pid_controller, output, maximum_duty_cycle=None, current_measurement=None,
                 machine=None,
                 pwm_frequency=None, max_temperature=None):
        super(Heater, self).__init__()
        if max_temperature:
            self.max_temperature = max_temperature
        else:
            self.max_temperature = _DEFAULT_MAX_TEMPERATURE
        self._thermometer = thermometer
        self._pid_controller = pid_controller
        self._output = output
        if maximum_duty_cycle:
            self._maximum_duty_cycle = float(maximum_duty_cycle)
        else:
            self._maximum_duty_cycle = 1.0
        self._current_measurement = current_measurement
        self._machine = machine

        self.active = False
        self._set_temperature = 0.0
        self.temperature = 0.0
        self.current_consumption = 0.0
        self.duty_cycle = 0.0
        if not pwm_frequency:
            self.pwm_frequency = 1000
        else:
            self.pwm_frequency = pwm_frequency

        self.readout_delay = _DEFAULT_READOUT_DELAY
        self.current_readout_delay = _DEFAULT_CURRENT_READOUT_DELAY
        self._wait_for_current_readout = 0

        with _PWM_LOCK:
            PWM.start(self._output, 0.0, self.pwm_frequency, 0)
        self.start()

    def stop(self):
        self.active = False

    def set_temperature(self, temperature):
        if not self.max_temperature or temperature < self.max_temperature:
            self._set_temperature = temperature
            self._pid_controller.setPoint(temperature)
        else:
            _logger.warn("Temperature %s", temperature)

    def get_set_temperature(self):
        return self._set_temperature


    def run(self):
        self._wait_for_current_readout = self.current_readout_delay + self.readout_delay
        self.active = True
        try:
            while self.active:
                self.temperature = self._thermometer.read()
                self.duty_cycle = self._pid_controller.update(self.temperature)
                self._apply_duty_cycle()
                time.sleep(self.readout_delay)
        except Exception as e:
            _logger.error("Heater thread crashed %s", e)
        finally:
            PWM.stop(self._output)

    def _apply_duty_cycle(self):
        #todo this is a hack because the current reading si only avail on arduino
        try:
            if self._current_measurement is not None and self._wait_for_current_readout > self.current_readout_delay:
                self._wait_for_current_readout = 0
                with _PWM_LOCK:
                    PWM.set_duty_cycle(self._output, 100.0)
                self.current_consumption = self._machine.read_current(self._current_measurement) \
                                           / 1024.0 * 1.8 * 121.0 / 10.0
            else:
                self._wait_for_current_readout += self.readout_delay
        finally:
            self.duty_cycle = max(self.duty_cycle, 0.0)
            self.duty_cycle = min(self.duty_cycle, 100.0)
            if self.temperature >= self.max_temperature:
                self.duty_cycle = 0.0
            with _PWM_LOCK:
                PWM.set_duty_cycle(self._output, min(self.duty_cycle, self._maximum_duty_cycle))

    def __del__(self):
        try:
            PWM.set_duty_cycle(self._output, 0)
        finally:
            PWM.stop(self._output)


class Thermometer(object):
    def __init__(self, themistor_type, analog_input):
        self._thermistor_type = themistor_type
        self._input = analog_input

    def read(self):
        #adafruit says it is a bug http://learn.adafruit.com/setting-up-io-python-library-on-beaglebone-black/adc
        ADC.read(self._input)
        value = ADC.read(self._input)  #read 0 to 1
        return thermistors.get_thermistor_reading(self._thermistor_type, value)


#from http://code.activestate.com/recipes/577231-discrete-pid-controller/
#The recipe gives simple implementation of a Discrete Proportional-Integral-Derivative (PID) controller.
# PID controller gives output value for error between desired reference input and measurement feedback to minimize
# error value.
#More information: http://en.wikipedia.org/wiki/PID_controller
#
#cnr437@gmail.com
#
#######	Example	#########
#
#p=PID(3.0,0.4,1.2)
#p.setPoint(5.0)
#while True:
#     pid = p.update(measurement_value)
#
#
class PID:
    """
    Discrete PID control
    """

    def __init__(self, P=2.0, I=0.0, D=1.0, Derivator=0, Integrator=0, Integrator_min=0.0, Integrator_max=100.0):
        self.Kp = float(P)
        self.Ki = float(I)
        self.Kd = float(D)
        self.Derivator = float(Derivator)
        self.Integrator = float(Integrator)
        self.Integrator_max = Integrator_max / I
        self.Integrator_min = Integrator_min / I

        self.set_point = 0.0
        self.error = 0.0
        self.pid_reset = False

        self._pid_max = 100.0
        self._pid_min = 0.0


    def update(self, current_value):
        """
        Calculate PID output value for given reference input and feedback
        """

        self.error = self.set_point - current_value

        if self.error > 10.0:
            PID = self._pid_max
            self.pid_reset = True
        elif self.error < -10.0:
            PID = self._pid_min
            self.pid_reset = True
        else:
            if self.pid_reset:
                self.I_value = 0.0
                self.pid_reset = False

            self.P_value = self.Kp * self.error
            self.D_value = self.Kd * (self.error - self.Derivator)
            self.Derivator = self.error

            self.Integrator += self.error

            if self.Integrator > self.Integrator_max:
                self.Integrator = float(self.Integrator_max)
            elif self.Integrator < self.Integrator_min:
                self.Integrator = float(self.Integrator_min)

            self.I_value = self.Integrator * self.Ki

            PID = self.P_value + self.I_value - self.D_value

        _logger.debug("PID is %s (%s of %s)", PID, current_value, self.set_point)
        #todo this is in reality just a debug

        if PID < 0.0:
            return 0.0
        elif PID > 100.0:
            return 100.0

        return PID

    def setPoint(self, set_point):
        """
        Initilize the setpoint of PID
        """
        self.set_point = float(set_point)
        self.Integrator = 0.0
        self.Derivator = 0.0

    def setIntegrator(self, Integrator):
        self.Integrator = Integrator

    def setDerivator(self, Derivator):
        self.Derivator = Derivator

    def setKp(self, P):
        self.Kp = P

    def setKi(self, I):
        self.Ki = I

    def setKd(self, D):
        self.Kd = D

    def getPoint(self):
        return self.set_point

    def getError(self):
        return self.error

    def getIntegrator(self):
        return self.Integrator

    def getDerivator(self):
        return self.Derivator