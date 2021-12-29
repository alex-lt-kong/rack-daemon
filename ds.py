#!/usr/bin/python3

import argparse
import datetime as dt
import json
import logging
import os
import requests
import RPi.GPIO as GPIO
import smtplib
import threading
import time

def main_loop():

    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)

    GPIO_pin_positive = 19
    GPIO.setup(GPIO_pin_positive, GPIO.OUT)
    GPIO.output(GPIO_pin_positive, GPIO.HIGH)

    GPIO_pin_negative = 16
    GPIO.setup(GPIO_pin_negative, GPIO.IN, pull_up_down=GPIO.PUD_UP)
# When a GPIO pin is in input mode and not connected to 3.3 volts or ground, 
# the pin is said to be floating, meaning that it has no fixed voltage level.
# the pin will randomly float between HIGH and LOW.

    last_status = 1    
    open_count = 0

    while True:
        
        current_status = GPIO.input(GPIO_pin_negative)
        if current_status != last_status:

            if current_status == 1:
                print('door is closed')
                open_count = 0
            else:
                print('door is opened')

            last_status = current_status

        if current_status == 0:
            open_count += 1

               

        time.sleep(1)

    GPIO.cleanup()


def main():

    main_loop()




if __name__ == '__main__':

    main()
