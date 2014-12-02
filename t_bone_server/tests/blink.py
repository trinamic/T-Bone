from Adafruit_BBIO import GPIO
import time

__author__ = 'marcus'
pin1="P9_16"
pin2="P9_14"

if __name__ == '__main__':
        GPIO.setup(pin1, GPIO.OUT)
        GPIO.setup(pin2, GPIO.OUT)
        while True:
            GPIO.output(pin1, GPIO.HIGH)
            GPIO.output(pin2, GPIO.LOW)
            time.sleep(0.5)
            GPIO.output(pin1, GPIO.LOW)
            GPIO.output(pin2, GPIO.HIGH)
            time.sleep(0.5)
