from Adafruit_BBIO import ADC, PWM, GPIO
from flask import logging
from threading import Thread
import time
import thermistors

__author__ = 'marcus'
_logger = logging.getLogger(__name__)

ADC.setup()


class Heater(Thread):
    def __init__(self, thermometer, output, maximum_duty_cycle=None, current_measurement=None, machine=None, pwm_frequency=None):
        super(Heater, self).__init__()
        self._thermometer = thermometer
        self._output = output
        if maximum_duty_cycle:
            self._maximum_duty_cycle = float(maximum_duty_cycle)
        else:
            self._maximum_duty_cycle = 1.0
        self._current_measurement = current_measurement
        self._machine = machine

        self.active = False
        self.set_temperature = 0.0
        self.temperature = 0.0
        self.current_consumption = 0.0
        self.duty_cycle = 0.0
        if not pwm_frequency:
            self.pwm_frequency = 1000
        else:
            self.pwm_frequency = pwm_frequency

        self.readout_delay = 1

        self.start()

    def stop(self):
        self.active = False

    def run(self):
        PWM.start(self._output, 0.0, self.pwm_frequency, 0)
        self.active = True
        while self.active:
            self.temperature = self._thermometer.read()
            self._apply_duty_cycle()
            time.sleep(self.readout_delay)

    def _apply_duty_cycle(self):
        #todo this is a hack because the current reading si onyl avail on arduino
        if self._current_measurement is not None:
            try:
                PWM.set_duty_cycle(self._output, 100.0)
                self.current_consumption = self._machine.read_current(self._current_measurement)
            finally:
                PWM.set_duty_cycle(self._output, min(self.duty_cycle, self._maximum_duty_cycle)*100.0)


class Thermometer(object):
    def __init__(self, themistor_type, analog_input):
        self._thermistor_type = themistor_type
        self._input = analog_input

    def read(self):
        raw_value = ADC.read_raw(self._input)  #adafruit says it is a bug http://learn.adafruit.com/setting-up-io-python-library-on-beaglebone-black/adc
        raw_value = ADC.read_raw(self._input)  #read up to 4096
        return thermistors.get_thermistor_reading(self._thermistor_type, raw_value)

#from https://github.com/steve71/RasPiBrew
#tehre used as
#pid = PIDController.pidpy(cycle_time, k_param, i_param, d_param) #init pid
#duty_cycle = pid.calcPID_reg4(temp_ma, set_point, True)
class pidpy(object):
    ek_1 = 0.0  # e[k-1] = SP[k-1] - PV[k-1] = Tset_hlt[k-1] - Thlt[k-1]
    ek_2 = 0.0  # e[k-2] = SP[k-2] - PV[k-2] = Tset_hlt[k-2] - Thlt[k-2]
    xk_1 = 0.0  # PV[k-1] = Thlt[k-1]
    xk_2 = 0.0  # PV[k-2] = Thlt[k-1]
    yk_1 = 0.0  # y[k-1] = Gamma[k-1]
    yk_2 = 0.0  # y[k-2] = Gamma[k-1]
    lpf_1 = 0.0  # lpf[k-1] = LPF output[k-1]
    lpf_2 = 0.0  # lpf[k-2] = LPF output[k-2]

    yk = 0.0  # output

    GMA_HLIM = 100.0
    GMA_LLIM = 0.0

    def __init__(self, ts, kc, ti, td):
        self.kc = kc
        self.ti = ti
        self.td = td
        self.ts = ts
        self.k_lpf = 0.0
        self.k0 = 0.0
        self.k1 = 0.0
        self.k2 = 0.0
        self.k3 = 0.0
        self.lpf1 = 0.0
        self.lpf2 = 0.0
        self.ts_ticks = 0
        self.pid_model = 3
        self.pp = 0.0
        self.pi = 0.0
        self.pd = 0.0
        if self.ti == 0.0:
            self.k0 = 0.0
        else:
            self.k0 = self.kc * self.ts / self.ti
        self.k1 = self.kc * self.td / self.ts
        self.lpf1 = (2.0 * self.k_lpf - self.ts) / (2.0 * self.k_lpf + self.ts)
        self.lpf2 = self.ts / (2.0 * self.k_lpf + self.ts)

    def calcPID_reg3(self, xk, tset, enable):
        ek = 0.0
        lpf = 0.0
        ek = tset - xk  # calculate e[k] = SP[k] - PV[k]
        #--------------------------------------
        # Calculate Lowpass Filter for D-term
        #--------------------------------------
        lpf = self.lpf1 * pidpy.lpf_1 + self.lpf2 * (ek + pidpy.ek_1);

        if (enable):
            #-----------------------------------------------------------
            # Calculate PID controller:
            # y[k] = y[k-1] + kc*(e[k] - e[k-1] +
            # Ts*e[k]/Ti +
            # Td/Ts*(lpf[k] - 2*lpf[k-1] + lpf[k-2]))
            #-----------------------------------------------------------
            self.pp = self.kc * (ek - pidpy.ek_1)  # y[k] = y[k-1] + Kc*(PV[k-1] - PV[k])
            self.pi = self.k0 * ek  # + Kc*Ts/Ti * e[k]
            self.pd = self.k1 * (lpf - 2.0 * pidpy.lpf_1 + pidpy.lpf_2)
            pidpy.yk += self.pp + self.pi + self.pd
        else:
            pidpy.yk = 0.0
            self.pp = 0.0
            self.pi = 0.0
            self.pd = 0.0

        pidpy.ek_1 = ek  # e[k-1] = e[k]
        pidpy.lpf_2 = pidpy.lpf_1  # update stores for LPF
        pidpy.lpf_1 = lpf

        # limit y[k] to GMA_HLIM and GMA_LLIM
        if (pidpy.yk > pidpy.GMA_HLIM):
            pidpy.yk = pidpy.GMA_HLIM
        if (pidpy.yk < pidpy.GMA_LLIM):
            pidpy.yk = pidpy.GMA_LLIM

        return pidpy.yk

    def calcPID_reg4(self, xk, tset, enable):
        ek = 0.0
        ek = tset - xk  # calculate e[k] = SP[k] - PV[k]

        if (enable):
            #-----------------------------------------------------------
            # Calculate PID controller:
            # y[k] = y[k-1] + kc*(PV[k-1] - PV[k] +
            # Ts*e[k]/Ti +
            # Td/Ts*(2*PV[k-1] - PV[k] - PV[k-2]))
            #-----------------------------------------------------------
            self.pp = self.kc * (pidpy.xk_1 - xk)  # y[k] = y[k-1] + Kc*(PV[k-1] - PV[k])
            self.pi = self.k0 * ek  # + Kc*Ts/Ti * e[k]
            self.pd = self.k1 * (2.0 * pidpy.xk_1 - xk - pidpy.xk_2)
            pidpy.yk += self.pp + self.pi + self.pd
        else:
            pidpy.yk = 0.0
            self.pp = 0.0
            self.pi = 0.0
            self.pd = 0.0

        pidpy.xk_2 = pidpy.xk_1  # PV[k-2] = PV[k-1]
        pidpy.xk_1 = xk  # PV[k-1] = PV[k]

        # limit y[k] to GMA_HLIM and GMA_LLIM
        if (pidpy.yk > pidpy.GMA_HLIM):
            pidpy.yk = pidpy.GMA_HLIM
        if (pidpy.yk < pidpy.GMA_LLIM):
            pidpy.yk = pidpy.GMA_LLIM

        return pidpy.yk
