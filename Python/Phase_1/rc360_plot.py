#!/usr/bin/env python3
#
# File: rc360_plot.py
# Copyright (c) 2015, DornerWorks, Ltd.
#
# Description:
#  This script provides a utility that graphs the data received from the RC360 
#  application.  The configuration values specified when the test_framework is 
#  started must match the values used on the real target.
#

# TODO: It would be nice to have this show some debug information on the same 
# window as the graphs:
#   sensor data depth
#   last sensor value shown
#   current target time
#   newest time (in queue)
#   newest value (in queue)

import time
import datetime
import collections
import bisect
import numpy
import matplotlib.pyplot
import matplotlib.animation

import periodic_timer
import test_framework

class plot_client(object):
    def __init__(self, **kwargs):
        self.target = test_framework.tests(**kwargs)

        self.fig = matplotlib.pyplot.figure(figsize=(16, 8))
        self.grid = matplotlib.gridspec.GridSpec(3, 1)

        # Connect a close event handler to the figure
        self.fig.canvas.mpl_connect('close_event', self.window_close)

        self.max_data_points = 100
        self.plot_pad_percentage = 0.05
        self.time_slice_max = 0.0

        subplot_map = [ 0, 1, 2 ]

        # Initialize sensor configuration and data
        self.data = {}
        for s in self.target.SensorTypes:
            # Elements required for plotting
            self.data[s] = {}
            self.data[s]['label'] = self.target.SensorNames[s]
            self.data[s]['subplot'] = self.fig.add_subplot(self.grid[subplot_map[s]])

            self.data[s]['times'] = numpy.zeros(0)
            self.data[s]['values'] = numpy.zeros(0)
            self.data[s]['line'], = self.data[s]['subplot'].plot(self.data[s]['times'], self.data[s]['values'], label=self.data[s]['label'])

            # Set the initial ylimits to be fairly large so the plot layout can 
            # handle a variety of y axis labels 
            if s in [0, 1]:
                initial_ylimits = (50.0, 100.0)
            else:
                initial_ylimits = (0.0, 1.0)

            initial_xlimits = (0, 30.0 * (self.max_data_points / self.target.get_samples_per_second(s)))
            print('sensor {} initial limits = {}'.format(s, initial_xlimits))

            self.data[s]['ypad'] = (initial_ylimits[1] - initial_ylimits[0]) * self.plot_pad_percentage
            self.data[s]['ylimits'] = ((initial_ylimits[0] - self.data[s]['ypad']), (initial_ylimits[1] + self.data[s]['ypad']))
            self.data[s]['xpad'] = (initial_xlimits[1] - initial_xlimits[0]) * self.plot_pad_percentage
            self.data[s]['xlimits'] = (initial_xlimits[0], (initial_xlimits[1] + self.data[s]['xpad']))
            self.data[s]['xwindow'] = self.data[s]['xlimits'][1] - self.data[s]['xlimits'][0]

            self.data[s]['line'].axes.set_title(self.data[s]['label'])
            self.data[s]['line'].axes.set_ylim(*self.data[s]['ylimits'])
            self.data[s]['line'].axes.set_xlim(*self.data[s]['xlimits'])

            # placeholders for the incoming data
            self.data[s]['new_values'] = numpy.zeros(0)
            self.data[s]['new_times'] = numpy.zeros(0)
            self.data[s]['target_time'] = numpy.zeros(0)
            self.data[s]['last_time'] = numpy.zeros(0)

        self.grid.tight_layout(self.fig)

        # determine the rate that we need to request data from the target
        self.keep_alive_interval = min([ self.target.config['sensor_samples_saved'] / self.target.get_samples_per_second(s) for s in self.target.SensorTypes ])

        # Create the keep alive polling thread
        self.timer = periodic_timer.PeriodicTimer(self.keep_alive, self.keep_alive_interval)
        print('starting target poll thread every {} seconds'.format(self.keep_alive_interval))

        # configure the plotting animation function (this ends up being more of 
        # a guideline, don't trust that the animation function will be called 
        # exactly this many seconds apart)
        self.animation_period = 0.05

        animation_interval = 1000 * self.animation_period
        print('animating every {} seconds'.format(self.animation_period))
        self.ani = matplotlib.animation.FuncAnimation(self.fig, self.animate, blit=False, interval=animation_interval, repeat=False)

    def run(self):
        # Because this is meant to be run for integration testing, we don't need 
        # to do the normal test startup of generating data, or starting the 
        # target application.  Just attempt to connect to the target, and then 
        # receive data.
        self.target.connect()

        # start the keep alive thread (after a short wait)
        time.sleep(1.0)
        self.timer.start()

        # show the plots
        matplotlib.pyplot.show()

        # don't let this thread exit
        try:
            while self.running:
                time.sleep(60.0)
        except KeyboardInterrupt:
            self.timer.stop()
            raise

    def window_close(self, evt):
        # Stop the keep_alive thread
        self.timer.stop()

        # Stop the main thread
        self.running = False

    def keep_alive(self):
        # The software starts retrieving data as soon as it starts, it is likely 
        # that there is a buffer ready, so just send a request, and if a buffer 
        # is not yet ready an empty response will be received.
        (hdr, req) = self.target.send_keep_alive_req()
        (new_data, resp) = self.target.recv_keep_alive_resp(timeout=60.0)

        # Check to see if new data was received, if not, wait a short time and 
        # try again
        if not new_data[0]:
            time.sleep(0.1)
            (hdr, req) = self.target.send_keep_alive_req()
            (new_data, resp) = self.target.recv_keep_alive_resp(timeout=60.0)

        # Parse the new data out for every sensor, which will then be slowly 
        # animated by the matplotlib animation function 
        for s in new_data.keys():
            if new_data[s]:
                num_samples = len(new_data[s])

                # Convert the timestamp to seconds, and mod by 3600 to limit the 
                # X-axis values to something easy to read
                time_start = (resp.timestamp / 1e9) % 3600
                sensor_interval = self.target.get_sensor_sample_interval(s) / 1e9
                time_end = time_start + (num_samples * sensor_interval)

                # TODO: because there are no timestamps for each sample at the 
                # moment, generate a list of timestamps based on the initial 
                # time
                times = numpy.linspace(time_start, time_end, num_samples, endpoint=False)

                # adjust the timestamps to be in seconds
                self.data[s]['new_times'] = numpy.concatenate([ self.data[s]['new_times'], times ])
                self.data[s]['new_values'] = numpy.concatenate([ self.data[s]['new_values'], new_data[s] ])

    def animate(self, x):
        now = datetime.datetime.now().strftime("%H:%M:%S.%f")
        times_sizes = [ v['new_times'].size for v in self.data.values() ]
        values_sizes = [ v['new_values'].size for v in self.data.values() ]

        # Save the time that this was called, best to use 1 "now" for all of the 
        # time calculations
        now = time.time()

        for s in self.data.keys():
            new_xlimits = None
            new_target = None
            new_times = numpy.zeros(0)

            # If the target time is still at 0, and there is some new data, set 
            # the xmin to the timestamp of the first new time value, the target 
            # time is now the animation period beyond the minimum time, and push 
            # xmax to maintain the display window
            if self.data[s]['new_times'].size:
                if not self.data[s]['target_time']:
                    xmin = self.data[s]['new_times'][0]
                    xmax = xmin + self.data[s]['xwindow']
                    new_xlimits = (xmin, xmax)
                    new_target = xmin + self.animation_period
                else:
                    # Once the target_time is set, the last_time should be set 
                    # as well, use that instead of the animation_period
                    time_diff = now - self.data[s]['last_time']

                    # The target time has been set, so just increment it, and 
                    # update the xmin/xmax once the target time + xpad is 
                    # greater than xmax
                    new_target = self.data[s]['target_time'] + time_diff
                    if (self.data[s]['target_time'] + self.data[s]['xpad']) > self.data[s]['xlimits'][1]:
                        xmin, xmax = self.data[s]['xlimits']
                        xmin += time_diff
                        xmax += time_diff
                        new_xlimits = (xmin, xmax)
            else:
                # If there are no new times (or values) pending, do nothing
                pass

            # If the target time has been updated, extract new values from the 
            # new_times and new_values lists
            if new_target:
                self.data[s]['target_time'] = new_target
                i = bisect.bisect_right(self.data[s]['new_times'], self.data[s]['target_time'])
                # If an index was found that is greater than 0
                if i > 0:
                    new_times = self.data[s]['new_times'][:i]
                    self.data[s]['new_times'] = self.data[s]['new_times'][i:]

                    # Now extract the same number of new values
                    new_values = self.data[s]['new_values'][:i]
                    self.data[s]['new_values'] = self.data[s]['new_values'][i:]

            # update the plot for this sensor if there were new values and 
            # timestamps retrieved
            if new_times.size:
                self.data[s]['values'] = numpy.concatenate([ self.data[s]['values'], new_values ])
                self.data[s]['times'] = numpy.concatenate([ self.data[s]['times'], new_times ])
                self.data[s]['line'].set_data(self.data[s]['times'], self.data[s]['values'])

                # If new data is added, we should re-evaluate the y axis
                ymax = numpy.amax(self.data[s]['values']) + self.data[s]['ypad']
                ymin = numpy.amin(self.data[s]['values']) - self.data[s]['ypad']

                if ymin < self.data[s]['ylimits'][0] or ymax > self.data[s]['ylimits'][1]:
                    self.data[s]['ylimits'] = (ymin, ymax)
                    self.data[s]['line'].axes.set_ylim(*self.data[s]['ylimits'])

            # Update the xlimits if necessary
            if new_xlimits:
                self.data[s]['xlimits'] = new_xlimits
                self.data[s]['line'].axes.set_xlim(*new_xlimits)

            # Save the time that this graph was updated so we can accurately 
            # increment the time intervals (because the animation function is 
            # not called precisely as configured).
            self.data[s]['last_time'] = now

        # The lines that were (possibly) modified need to be returned
        lines = [ i['line'] for i in self.data.values() ]
        return lines

def runner(**kwargs):
    client = plot_client(**kwargs)
    client.run()

if __name__ == '__main__':
    # Match the configuration values on the target
    defaults = {
        'sensor_samples_saved':       900,
        'sensor_period__temp_1':      1000000000,
        'sensor_period__temp_2':      1000000000,
        'sensor_period__pressure':    1000000000,
    }

    test_framework.run([runner], **defaults)

