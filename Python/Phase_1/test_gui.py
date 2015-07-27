#!/usr/bin/env python3
#
# File: test_gui.py
# Copyright (c) 2015, DornerWorks, Ltd.
#
# Description:
#  This implements a small self-test to validate that the rc360_plot script is 
#  working properly. 
#

import time
import collections
import numpy
import scipy.signal
import matplotlib

import rc360_plot
import test_framework

SensorPlot = collections.namedtuple(
    'SensorPlot',
    [ 'label', 'subplot', 'line', 'values', 'times', 'ylimits', 'xlimits', 'ypad', 'xpad' ]
)

SensorData = collections.namedtuple(
    'SensorData',
    [ 'new_times', 'new_values' ]
)

class plot_test(rc360_plot.plot_client):
    def __init__(self, **kwargs):
        # Initialize the test GUI and test framework
        super().__init__(**kwargs)

        # Determine the range of time for data to be generated.  Pick a point 10 
        # minutes in the past and use that as the start time.
        end_time = time.time()
        start_time = int((end_time - 600.0) * 1e9)
        end_time = int(end_time * 1e9)

        # generate a unique waveform for each sensor so we can ensure that the 
        # correct sensor data is being plotted
        waveform_gen = [
            lambda x: scipy.signal.square(x),
            lambda x: numpy.cos(x),
            lambda x: scipy.signal.sawtooth(x),
        ]

        # Generate 10 minutes of data for every sensor
        test_data = {}
        for s in self.target.SensorTypes:
            samples = self.target.get_samples_per_second(s) * 600
            times = numpy.linspace(start_time, end_time, samples, endpoint=False)

            # TODO: for now all values are read as integers and those values are 
            # maintained, so shift the generated values (with a range of 0-1) to 
            # be from 0 to 1000, this will make them rather jagged, but not much 
            # can be done about that for now:
            values = (waveform_gen[s](times) * 500.0) + 500.0

            test_data[s] = dict(zip(times.astype('uint64'), values.astype('uint16')))

        # Now that the data has been generated, save it and start the target, 
        # after the target is started the plotting gui should be able to run as 
        # normal.
        self.target.save_test_data(test_data)
        self.target.start()

def runner(**kwargs):
    client = plot_test(**kwargs)
    client.run()

if __name__ == '__main__':
    # In this particular case, we want to generate 10 minutes worth of data, and 
    # run the GUI to receive the data, but we want to do it on a closed system 
    # to test that the GUI is operating properly, and with a known set of data 
    # rather than random data.

    # The default configuration values match the default values used by the 
    # software itself.  For functional testing however, values that result in 
    # smaller data sets are more useful.
    defaults = {
        # To make things easier set fastest sensor that data is collected for to 
        # result in 10 samples every second, so the total samples for each 
        # sensor for 1 minute would be 600 samples. (the slow sensors can be 
        # left at the default 1Hz sample rate).
        'sensor_samples_saved':       600,

        # Set the system sample rate to be 100x the fastest poll rate (every 
        # 1 msec) so that the sample skipping is tested.
        'system_sample_period':       1000000,

        # Set the software to poll the "FPGA" buffer every half a second.
        'fpga_poll_period':           500000000,

        # The FPGA buffer size needs to accommodate 1 sample per sensor per msec 
        # collected over 500 msec, so it should have a size of:
        # 500 * 6 * 2 bytes = 6000
        'fpga_buffer_size':           6000,
    }

    test_framework.run([runner], **defaults)

