#!/usr/bin/env python3
#
# File: test_framework.py
# Copyright (c) 2015, DornerWorks, Ltd.
#
# Description:
#  This script provides the core test functionality for the RC360 application.  
#  This simulates the data to be read by the application, and provides methods 
#  for a test script to generate keep alive messages and validate the responses.
#

import argparse
import os
import subprocess
import socket
import struct
import itertools
import collections
import time
import random
import operator
import binascii
import copy

import numpy

class tests(object):
    MSG_TYPE_KEEP_ALIVE = 0x207E
    MSG_TYPE_DATA_REQ   = 0x217E

    # Valid hardware channels for sensors to be connected to
    HardwareChannels = range(16)
    FullScaleADC = 4095

    # Expected "Device" IDs for the sensors
    SensorTypes = [ 0, 1, 2 ]

    # a list of the sensor names
    SensorNames = [
        'Temperature Sensor 1',
        'Temperature Sensor 2',
        'Pressure Sensor',
    ]

    # a list of the sensor configuration file strings, so that the delays can be 
    # easily found using the sensor type ID (from the SensorTypes list)
    SensorPeriodConfigMapping = [
        'sensor_period__temp_1',
        'sensor_period__temp_2',
        'sensor_period__pressure',
    ]

    SensorChannelConfigMapping = [
        'sensor_channel__temp_1',
        'sensor_channel__temp_2',
        'sensor_channel__pressure',
    ]

    # A mapping of argument strings to configuration file strings.  Most of 
    # these aren't necessary, but all of them are included here for the sake of 
    # completeness.
    ConfigFileStrings = {
        'port':                       'port',
        'max_msg_size':               'max_msg_size',
        'sensor_samples_saved':       'sensor_samples_saved',
        'system_sample_period':       'system_sample_period',
        'fpga_buffer_size':           'fpga_buffer_size',
        'fpga_poll_period':           'fpga_poll_period',
        'sensor_period__temp_1':      'sensor_period[temp_1]',
        'sensor_period__temp_2':      'sensor_period[temp_2]',
        'sensor_period__pressure':    'sensor_period[pressure]',
        'sensor_channel__temp_1':     'sensor_channel[temp_1]',
        'sensor_channel__temp_2':     'sensor_channel[temp_2]',
        'sensor_channel__pressure':   'sensor_channel[pressure]',
    }

    # Define the common messages
    MsgHeaderFormat = '!HBL'
    MsgHeaderSize = 7
    MsgHeader = collections.namedtuple(
        'MsgHeader',
        [ 'command', 'version', 'length' ]
    )

    # Not all of the Keep Alive Request fields are used, so they are just marked 
    # as padding.
    KeepAliveReqFormat = '!B8x'
    KeepAliveReqSize = 9
    KeepAliveReq = collections.namedtuple(
        'KeepAliveReq',
        # The message format without the unused fields removed:
        # [ 'msgid', 'timestamp' ]
        [ 'msgid' ]
    )

    # Not all of the Keep Alive Response fields are used, so they are just 
    # marked as padding.
    KeepAliveRespFormat = '!BBBxQ21xH8xB'
    KeepAliveRespSize = 44
    KeepAliveResp = collections.namedtuple(
        'KeepAliveResp',
        # The message format without the unused fields removed:
        # [ 'respnum', 'nummsgs', 'msgid', 'errcode', 'timestamp', 'firmware',
        #   'serialnum', 'config', 'idlen', 'maxmsgsize', 'curindex',
        #   'maxlogsize', 'numdevices' ]
        [ 'respnum', 'nummsgs', 'msgid', 'timestamp', 'maxmsgsize', 'numdevices' ]
    )

    # Not all of the Sensor Data fields are used, so they are just marked as 
    # padding.
    SensorDataFormat = '!40xBL'
    SensorDataSize = 45
    SensorData = collections.namedtuple(
        'SensorData',
        # The message format without the unused fields removed:
        # [ 'state', 'pendfirmware', 'pendconfig', 'serialnum', 'firmware',
        #   'config', 'timestamp', 'devtype', 'rev', 'devidlen', 'devid',
        #   'datasize' ]
        [ 'devid', 'datasize' ]
    )

    # There is a CRC32 appended to every message
    MsgCRCFormat = '!L'
    MsgCRCSize = 4
    MsgCRC = collections.namedtuple(
        'MsgCRC',
        [ 'crc' ]
    )

    def __init__(self, path, valgrind=False, debug=False, **kwargs):
        # default sensor sample periods
        self.config = kwargs
        self.test_flags = {
            'valgrind': valgrind,
            'debug': debug,
        }
        self.path = path

        self.process = None
        self.socket = None

        # Placeholder for where generated data will be stored
        self.time_range = None
        self.last_time = 0
        self.adcvalues = None
        self.data = None
        self.last_time_checked = dict([(s, None) for s in self.SensorTypes])

    def __del__(self):
        self.stop()

    def start(self):
        if not self.test_flags['debug']:
            # Write a new config file
            f = open('rc360.conf', 'w')
            for (k, v) in self.config.items():
                if k in self.ConfigFileStrings:
                    f.write('{} = {}\n'.format(self.ConfigFileStrings[k], v))
            f.close()

            # Start the program to be tested in the background
            if self.test_flags['valgrind']:
                # check if valgrind exists on this platform
                assert subprocess.call(['valgrind', '--version']) == 0
                args = [
                    'valgrind',
                    #"--gen-suppressions=all",
                    "--suppressions=../valgrind.supp",
                    # Standard valgrind options
                    "--read-var-info=yes",
                    "--track-fds=yes",
                    # memcheck tool options
                    "--leak-check=full",
                    "--track-origins=yes",
                    "--show-reachable=yes",
                    "--malloc-fill=B5",
                    "--free-fill=4A",
                    self.path,
                ]
            else:
                args = [self.path]
            self.process = subprocess.Popen(args)
            assert self.process

    def stop(self):
        try:
            if self.socket:
                self.socket.shutdown(socket.SHUT_RDWR)
                self.socket.close()
        except OSError:
            pass

        if not self.test_flags['debug']:
            try:
                if self.process:
                    self.process.terminate()
                    self.process = None
            except OSError:
                pass

            try:
                os.remove('program.conf')
            except OSError:
                pass

    def _send(self, data):
        #print_packet(data, 'sending msg: ')
        sent = 0
        while sent < len(data):
            ret = self.socket.send(data[sent:])
            if not ret:
                errmsg = 'Lost connection attempting to send msg {}'.format(str(data))
                raise RuntimeError(errmsg)
            sent += ret

    def _recv(self, size, timeout=None):
        old_timeout = self.socket.gettimeout()
        self.socket.settimeout(timeout)

        data = self.socket.recv(size)
        while len(data) < size:
            data += self.socket.recv(size - len(data))

        self.socket.settimeout(old_timeout)

        #print_packet(data, 'received msg: ')

        return data

    def _save_test_data_samples(self):
        # The data file format will match the format used by the real hardware.
        # All values will be 16bit ADC/integer values, one for each sensor.
        #
        #   | s 1 | s 2 | .... | s n |
        #   | s 1 | s 2 | .... | s n |
        #   ...
        #
        # Each 16bit value consists of a 12 bit ADC reading in the upper 12
        # bits of the value, and the hardware channel is contained in the
        # lower 4 bits.

        # For each timestamp, grab the values for every sensor and pack them 
        # into 1 new sample.  Save the last value a sensor had in the case that 
        # sensors are generated with different data rates.
        default_value = dict([(s, self.config[self.SensorChannelConfigMapping[s]]) for s in self.SensorTypes])
        values = []
        for t in self.time_range:
            for s in self.SensorTypes:
                chan = self.config[self.SensorChannelConfigMapping[s]]
                try:
                    val = (self.adcvalues[s][t] << 4) | chan
                    default_value[s] = val
                except KeyError:
                    val = default_value[s]
                values.append(val)

        print('converting {} data values...'.format(len(values)))
        format_str = '=' + ('H' * len(values))
        data = struct.pack(format_str, *values)

        print('writing {} data values...'.format(len(values)))
        f = open('samples.bin', 'wb')
        f.write(data)
        f.flush()
        f.close()

    def _calc_sensor_value(self, sensor, raw):
        # Turn the raw value into a voltage:
        volts = float(raw) / self.FullScaleADC
        if sensor in [ 0, 1 ]:
            return (volts * 4.0) / 0.005
        elif sensor in [ 2 ]:
            return volts * 10.0
        else:
            return volts

    def get_sensor_sample_interval(self, sensor):
        return self.config[self.SensorPeriodConfigMapping[sensor]]

    def get_samples_per_second(self, sensor):
        return int(1e9 / self.config[self.SensorPeriodConfigMapping[sensor]])

    def estimate_num_msgs(self, samples):
        # Make a copy of the number of samples so we don't modify it
        samples_remaining = copy.deepcopy(samples)

        # TODO: it seems like there should be a better way to do this
        num_msgs = 0
        sensor = min(self.SensorTypes)
        while sensor in self.SensorTypes:
            num_msgs += 1
            avail_space = self.config['max_msg_size'] - (self.MsgHeaderSize + self.KeepAliveRespSize + self.MsgCRCSize)

            # Account for the space required by the device header fields
            while sensor in self.SensorTypes and avail_space > (self.SensorDataSize + 8):
                avail_space -= self.SensorDataSize;
                allowed_samples = avail_space // 8
                min_samples = min([ allowed_samples, samples_remaining[sensor] ])
                samples_remaining[sensor] -= min_samples

                avail_space -= min_samples * 8

                if not samples_remaining[sensor]:
                    sensor += 1

        return num_msgs

    def save_test_data(self, samples):
        # TODO: have this function accept floating point values and then 
        # determine ADC values from them?

        # Sanity checks to make sure that the incoming data has the proper 
        # format, ensure that there are entries in the samples dictionary for 
        # every sensor, and that each sensor's value is a timestamp/value 
        # dictionary.
        assert_verbose(isinstance, samples, dict)
        sample_keys = set(samples.keys())
        all_sensors = set(self.SensorTypes)
        assert_verbose(operator.eq, sample_keys, all_sensors)
        for (k, v) in samples.items():
            assert_verbose(isinstance, v, dict)

        # To generate the time range that is normally created when data is 
        # randomly generated, get the start/end times out of the list timestamp 
        # keys for every sensor and then get the min/max values. 
        all_times = list(itertools.chain.from_iterable([ v.keys() for v in samples.values() ]))
        start_time = min(all_times)
        end_time = max(all_times)
        self.time_range = range(start_time, end_time, self.config['system_sample_period'])

        # The samples can be saved as-is
        self.adcvalues = samples

        # Use the ADC values generated and determine the floating point value 
        # that should be reported by the software under test for that value.
        self.data = {}
        for s in self.adcvalues.keys():
            self.data[s] = dict([ (t, self._calc_sensor_value(s, v)) for (t, v) in self.adcvalues[s].items() ])

        # Write the samples out to disk
        self._save_test_data_samples()

    def generate_data(self, num_samples=10000, start_time=None):
        if not start_time:
            # The timestamp returned is a floating point value with microsecond 
            # resolution.  Multiply by 1e9 to get a nanosecond resolution 8-byte 
            # timestamp.
            end_time = int(time.time() * 1e9)

            # Figure out when the samples should have started
            start_time = end_time - (num_samples * self.config['system_sample_period'])
        else:
            # Figure out the end time
            end_time = start_time + (num_samples * self.config['system_sample_period'])

        # Generate random data
        self.time_range = range(start_time, end_time, self.config['system_sample_period'])
        self.adcvalues = {}
        # TODO: these sample rates are not quite right, we must account for the
        # way the sequencer samples one ADC at a time.
        print('generating {} samples...'.format(num_samples))
        for s in self.SensorTypes:
            # get a subset of the time samples for each sensor, that are based 
            # on the sample rate of the sensor
            skip_samples = int(self.config[self.SensorPeriodConfigMapping[s]] / len(self.SensorTypes) / self.config['system_sample_period'])
            self.adcvalues[s] = dict([(t, random.getrandbits(12)) for t in self.time_range[::skip_samples]])

        # Use the ADC values generated and determine the floating point value 
        # that should be reported by the software under test for that value.
        self.data = {}
        for s in self.adcvalues.keys():
            self.data[s] = dict([ (t, self._calc_sensor_value(s, v)) for (t, v) in self.adcvalues[s].items() ])

        self._save_test_data_samples()

    def connect(self, timeout=None):
        # Try to connect to the target application for 30 seconds, that should
        # be long enough even if it was launched through valgrind
        #
        print('waiting for application to start', end='')
        now = time.time()
        stop = now + 30.0
        while time.time() < stop:
            try:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.connect((self.config['address'], int(self.config['port'])))
                break
            except socket.error:
                time.sleep(1.0)
                print('.', end='')
        print('\nconnected')

    def send_keep_alive_req(self, msgid=None):
        if not msgid:
            msgid = random.randint(0, 255)

        # Keep Alive request structure:
        #   command     2 bytes
        #   version     1 byte
        #   bytes       4 bytes
        #   msg ID      1 byte
        hdr = self.MsgHeader(
            command = self.MSG_TYPE_KEEP_ALIVE,
            version = 0x00,
            length = self.KeepAliveReqSize + self.MsgCRCSize,
        )
        msg = struct.pack(self.MsgHeaderFormat, *hdr)

        req = self.KeepAliveReq(
            msgid = msgid,
        )
        msg += struct.pack(self.KeepAliveReqFormat, *req)

        # Now calculate the crc32 of the message and append it before sending
        crc = binascii.crc32(msg) & 0xFFFFFFFF
        msg += struct.pack(self.MsgCRCFormat, crc)

        self._send(msg)

        return (hdr, req)

    def recv_keep_alive_resp(self, msgid=0, expect_num_responses=0, expect_num_samples=None, timeout=None):
        # It may take multiple messages to receive the full response.
        responses_recv = 0

        # A list/struct to maintain received sensor data, it is necessary to 
        # collect the binary values and convert them after all messages have 
        # been received because the data for 1 sensor may be split into multiple 
        # messages.
        sensor_raw = {}
        sensor_data = {}

        # Save the first response we receive, the information it provides may be 
        # needed for further validation
        first_resp = None

        # Do the comparison internally since the number of responses to expect 
        # will be set by the first message received.
        expect_resp_num = 0
        while True:
            # Receive the first 7 bytes of a message, the last 4 bytes of that 
            # will determine how many remaining bytes need to be received.
            data = self._recv(self.MsgHeaderSize, timeout)
            hdr = self.MsgHeader(*struct.unpack(self.MsgHeaderFormat, data))

            # Start accumulating the CRC for the incoming message
            crc = binascii.crc32(data)

            assert_verbose(operator.eq, hdr.command, self.MSG_TYPE_KEEP_ALIVE)
            data = self._recv(hdr.length, timeout)

            # Verify that the received data is big enough, but not larger than 
            # the configured message
            assert_verbose(operator.ge, self.config['max_msg_size'], self.MsgHeaderSize + hdr.length)
            assert_verbose(operator.ge, len(data), self.KeepAliveRespSize)
            assert_verbose(operator.eq, len(data), hdr.length)

            # This should be the rest of the message, but it also contains a CRC 
            # at the end, so leave off the final 4 bytes
            crc = binascii.crc32(data[:-self.MsgCRCSize], crc) & 0xFFFFFFFF

            # Extract the CRC from the end of the message and compare with the 
            # calculated CRC
            msg_crc = struct.unpack(self.MsgCRCFormat, data[-self.MsgCRCSize:])[0]
            assert_verbose(operator.eq, crc, msg_crc)

            # remove the CRC from the end of the message
            data = data[:-self.MsgCRCSize]

            resp = self.KeepAliveResp(*struct.unpack(self.KeepAliveRespFormat, data[:self.KeepAliveRespSize]))
            data = data[self.KeepAliveRespSize:]

            if not first_resp:
                first_resp = resp

            # Validate that the message ID received was the one expected
            if msgid:
                assert_verbose(operator.eq, msgid, resp.msgid)

            # Validate that the message number matches what we expect
            assert_verbose(operator.eq, expect_resp_num, resp.respnum)
            expect_resp_num += 1

            if not expect_num_responses:
                expect_num_responses = resp.nummsgs
            else:
                assert_verbose(operator.eq, expect_num_responses, resp.nummsgs)

            # Validate that the max message size matches what we have configured
            assert_verbose(operator.eq, self.config['max_msg_size'], resp.maxmsgsize)

            # Validate that the number of devices maintained by the target 
            # software match the test configuration
            assert_verbose(operator.eq, len(self.SensorTypes), resp.numdevices)

            # Loop through the message and pull the data for each sensor out
            while data:
                # Get the sensor data header
                assert_verbose(operator.ge, len(data), self.SensorDataSize)
                sensor_hdr = self.SensorData(*struct.unpack(self.SensorDataFormat, data[:self.SensorDataSize]))
                data = data[self.SensorDataSize:]

                # Get the sensor data
                assert_verbose(operator.ge, len(data), sensor_hdr.datasize)
                raw = data[:sensor_hdr.datasize]
                data = data[sensor_hdr.datasize:]

                # Save the sensor data and we will convert it later
                if sensor_hdr.devid in sensor_raw:
                    sensor_raw[sensor_hdr.devid] += raw
                else:
                    sensor_raw[sensor_hdr.devid] = raw

            # Check if this was the last response message
            responses_recv += 1
            if responses_recv >= expect_num_responses:
                break

        for (k,v) in sensor_raw.items():
            data_len = len(v)
            assert_verbose(operator.eq, (data_len % 8), 0)

            vals = data_len // 8
            if expect_num_samples:
                assert_verbose(operator.eq, vals, expect_num_samples[k])

            # In theory '!' should mean standard network order, but for doubles 
            # the standard order is little endian, or rather, machine native
            sensor_data[k] = list(struct.unpack('<' + ('d' * int(vals)), v))

        return sensor_data, first_resp

    def expect_keep_alive_resp(self, keep_alive, expect_num_responses, expect_num_samples, timeout=None):
        (sensor_final, resp) = self.recv_keep_alive_resp(keep_alive.msgid, expect_num_responses, expect_num_samples, timeout=timeout)

        # extract the section of generated data that should be compared against 
        # the data just received from the program under test
        first_time = 0
        compare_data = {}
        for s in self.SensorTypes:
            all_times = sorted(self.data[s].keys())

            # Initialize the time stamp value
            if not self.last_time_checked[s]:
                self.last_time_checked[s] = all_times[0]

            # see if this sensor's timestamp should be saved as the sample 
            # timestamp
            if first_time < self.last_time_checked[s]:
                first_time = self.last_time_checked[s]

            # Now find the start
            start = all_times.index(self.last_time_checked[s])
            end = start + expect_num_samples[s]
            check_times = all_times[start : end]

            compare_data[s] = [self.data[s][t] for t in check_times]

            # Guard against reaching the end of the list, we don't want this to 
            # cause an exception now, but if this function is called again.
            if (end + 1) >= len(all_times):
                self.last_time_checked[s] = -1
            else:
                self.last_time_checked[s] = all_times[end + 1]

        # Validate the received data before returning
        compare_expect_recv_data(compare_data, sensor_final)

        # TODO: It isn't expected that the timestamp reported by the software 
        # will be exactly that of when the samples are taken (or in this case, 
        # generated) because it is timestamped by the software when the samples 
        # are read, so just check that the first timestamp we receive is greater 
        # than the first timestamp of the generated data.
        assert_verbose(operator.ge, first_time, resp.timestamp)

        # TODO: It should be at most "fpga_poll_period" nsec greater than when 
        # the samples were taken (generated).... this logic isn't right, because 
        # depending on how long it takes for the application to be launched the 
        # timestamp saved by the app can be off.
        #assert_verbose(operator.le, sensor_final['timestamp'], first_time + self.config['fpga_poll_period'])

        return sensor_final

##############################################################
# utility functions
##############################################################

def print_packet(msg, prefix=''):
    debug_msg = ''
    for b in msg:
        debug_msg += '{0:02x} '.format(b)
    print(prefix + debug_msg)

def compare_expect_recv_data(test_data, resp_data):
    # TODO: It'd be nice to be able to track more information such as key names 
    # and list positions for debugging purposes.  At the moment the best way to 
    # debug failures is to insert this at the point of failure:
    #   import pdb; pdb.set_trace()

    # Just use asserts to check for all conditions
    if isinstance(test_data, dict):
        assert_verbose(lambda x: isinstance(x, dict), resp_data)
        assert_verbose(operator.eq, len(test_data), len(resp_data))
        for k in test_data.keys():
            assert_verbose(operator.contains, resp_data, k)
            compare_expect_recv_data(test_data[k], resp_data[k])

    elif isinstance(test_data, list):
        assert_verbose(lambda x: isinstance(x, list), resp_data)
        assert_verbose(operator.eq, len(test_data), len(resp_data))
        for i in range(len(test_data)):
            compare_expect_recv_data(test_data[i], resp_data[i])

    elif isinstance(test_data, float):
        # Floating point values are likely never exactly the same, use numpy's 
        # isclose() function determine if the floating point values are close 
        # enough to be called "the same".
        assert_verbose(operator.eq, numpy.isclose(test_data, resp_data), True)

    else:
        # It isn't strictly necessary for the types to match, because the data 
        # generation and message unpacking could result in different types.  
        # If the comparison shows that the values are the same then the 
        # values are close enough to pass.
        assert_verbose(operator.eq, test_data, resp_data)

def run(tests, **kwargs):
    # Parse any arguments that were supplied
    parser = argparse.ArgumentParser(description='Assimilator software tests')
    parser.add_argument('--path',     default='./program', help='the path to the application to run as the server software')
    parser.add_argument('--valgrind', action='store_true', help='run the application through valgrind')
    parser.add_argument('--debug',    action='store_true', help='indicates that the target program is being debugged, so this script won\'t attempt to start or stop the target')

    # Assimilator software configuration values
    parser.add_argument('--address',                    default='127.0.0.1',          help='port that the software under test should listen for connections on')
    parser.add_argument('--port',                       default=10000,      type=int, help='port that the software under test should listen for connections on')
    parser.add_argument('--max-msg-size',               default=1000,       type=int, help='maximum message size')
    parser.add_argument('--sensor-samples-saved',       default=500,        type=int, help='the number of sensor samples that the software should maintain in a buffer')
    parser.add_argument('--system-sample-period',       default=5000,       type=int, help='the time between sensor samples by the FPGA (nsec)')
    parser.add_argument('--fpga-buffer-size',           default=24000,      type=int, help='the size of the buffer that the FPGA maintains to store raw sensor values')
    parser.add_argument('--fpga-poll-period',           default=500000000,  type=int, help='the period between ADC samples by the FPGA (nsec)')
    parser.add_argument('--sensor-period--temp-1',      default=1000000000, type=int, help='the sample period for temperature sensor 1')
    parser.add_argument('--sensor-period--temp-2',      default=1000000000, type=int, help='the sample period for temperature sensor 2')
    parser.add_argument('--sensor-period--pressure',    default=1000000000, type=int, help='the sample period for the pressure sensor')
    parser.add_argument('--sensor-channel--temp-1',     default=2,          type=int, help='the hardware channel for temperature sensor 1')
    parser.add_argument('--sensor-channel--temp-2',     default=3,          type=int, help='the hardware channel for temperature sensor 2')
    parser.add_argument('--sensor-channel--pressure',   default=5,          type=int, help='the hardware channel for the pressure sensor')

    # It is allowed for the test module calling this to override the default 
    # values.
    parser.set_defaults(**kwargs)

    args = parser.parse_args()

    for t in tests:
        t(**vars(args))

    print('TESTS COMPLETE')

def assert_verbose(op, *args):
    if not op(*args):
        lambda_type = lambda: None
        if isinstance(op, type(lambda_type)):
            import inspect
            msg = 'assertion failed: {} <- {}'.format(inspect.getsource(op), args)
        else:
            msg = 'assertion failed: {} <- {}'.format(op, args)
        raise AssertionError(msg)

