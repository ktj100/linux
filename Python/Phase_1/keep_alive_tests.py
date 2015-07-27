#!/usr/bin/env python3
#
# File: keep_alive_tests.py
# Copyright (c) 2015, DornerWorks, Ltd.
#
# Description:
#  This script provides an integration-type test for the keep alive behavior of 
#  the RC360 application.
#

import time
import random

import test_framework

def test_keep_alive_single_response(**kwargs):
    test_instance = test_framework.tests(**kwargs)

    # Only 1 response is being tested, but it will take at least 60 * 1000 
    # sample file reads before 1 buffer is ready to be sent.  Double the amount 
    # of data we need just to be safe, and to allow for the software to 
    # accidentally send too much data if it is not working properly.
    total_samples = int((1000000000 / kwargs['system_sample_period']) * 120)
    test_instance.generate_data(total_samples)
    test_instance.start()
    test_instance.connect()

    # the number of sensor samples expected for each sensor
    samples = {
        test_instance.SensorTypes[0]: 600,
        test_instance.SensorTypes[1]: 600,
        test_instance.SensorTypes[2]: 600,
    }

    msgs = test_instance.estimate_num_msgs(samples)

    print('expecting {} messages'.format(msgs))

    # The software starts retrieving data as soon as it starts, so wait 1.5x the 
    # expected request period (1 minute) before sending the Keep Alive request.  
    # Time must be given for 1 buffer to be filled completely.
    print('waiting 90.0 seconds for 1 full message buffer to be collected')
    time.sleep(90.0)
    (hdr, req) = test_instance.send_keep_alive_req()

    test_instance.expect_keep_alive_resp(req, expect_num_responses=msgs, expect_num_samples=samples, timeout=5)

#def test_keep_alive_multiple_responses(**kwargs):

if __name__ == '__main__':
    #tests = [ test_keep_alive_single_response, test_keep_alive_multiple_responses ]
    tests = [ test_keep_alive_single_response ]

    # Pick random unique channels for the sensors, valid channels are 0-15
    channels = random.sample(test_framework.tests.HardwareChannels, len(test_framework.tests.SensorTypes))

    # The default configuration values match the default values used by the 
    # software itself.  For functional testing however, values that result in 
    # smaller data sets are more useful.
    defaults = {
        # To make things easier set fastest sensor that data is collected for to 
        # result in 10 samples every second, so the total samples for each 
        # sensor for 1 minute would be 600 samples.
        'sensor_samples_saved':       600,

        # Set the system sample rate to be 100x the fastest poll rate (every 
        # 1 msec) so that the sample skipping is tested.
        'system_sample_period':       1000000,

        # Set the software to poll the "FPGA" buffer every 1.5 seconds.
        'fpga_poll_period':           1500000000,

        # The FPGA buffer will simulate a sample collected every msec, and it 
        # the data buffer will be read by software every 500 msec.  This plus 
        # the fpga_poll_period are critical to getting the sensor read rate 
        # simulated properly:
        # 1500 * 2 bytes = 3000
        'fpga_buffer_size':           3000,

        # Ensure that the sensors are sampled 10 times a second
        'sensor_period__temp_1':      100000000,
        'sensor_period__temp_2':      100000000,
        'sensor_period__pressure':    100000000,

        # The randomly selected sensor channels:
        'sensor_channel__temp_1':     channels[0],
        'sensor_channel__temp_2':     channels[1],
        'sensor_channel__pressure':   channels[2],
    }

    # TODO: The following numbers have found a few different errors in the
    # message packing, make a test for them, in this case the keep alive is
    # sent every second, and we should run for at least a minute ensuring that
    # the target software continues to operate properly:
    #
    #   'sensor_samples_saved':       100,
    #   'sensor_period__temp_1':      10000000,
    #   'sensor_period__temp_2':      10000000,
    #   'sensor_period__pressure':    10000000,
    #   'sensor_period__vibration_y': 10000000,
    #   'sensor_period__vibration_x': 10000000,
    #   'sensor_period__vibration_z': 10000000,
    #
    # default values for everything else

    # TODO: configuration error tests
    # TODO: data request test

    test_framework.run(tests, **defaults)

