#!/usr/bin/env python3

import datetime
import os
import re

class logfollower(object):
    # The standard syslog timestamp format is:
    #   MON DATE 24HR:MIN:SEC
    TIMESTAMP_FORMAT = '%b %d %H:%M:%S'
    LOG_LINE_REGEX = r'^(?P<timestamp>\w{3} +\d{1,2} \d{2}:\d{2}:\d{2}) (?P<host>\S+) (?P<process>\S+)(?:\[(?P<pid>\d+)])?: (?P<message>.*)$'
    # regular expressions

    def __init__(self, path='/var/log/syslog'):
        # Initialize the standard log regular expression
        self.pattern = re.compile(self.LOG_LINE_REGEX)

        self.open(path)
        self.start()

    def __del__(self):
        self.close()

    def _match_line(self, line):
        result = self.pattern.match(line)
        if result:
            item = result.groupdict()
            now = datetime.datetime.now()
            log_time = datetime.datetime.strptime(item['timestamp'], self.TIMESTAMP_FORMAT)
            item['timestamp'] = datetime.datetime.combine(now.date(), log_time.time())
            if item['pid']:
                item['pid'] = int(item['pid'])
            return item

    def _filter_log_lines(self, lines):
        return [self._match_line(l) for l in lines]  # list comprehension
        # _match_line creates dictionary, this creates a list of dictionaries

        # result = []
        # for l in lines:
        #     val = self._match_line(l)
        #     result.append(val)
        # return result

    def start(self):
        # Seek to the end of the file, so that only new log entries will be 
        # parsed
        self.log.seek(0, os.SEEK_END)

    def open(self, path=None):
        if path:
            self.path = path
        self.log = open(self.path, 'r')

    def close(self):
        self.log.close()

    def read(self, proc_filter=None):
        # Read to the end of the log file, capture the output and parse it
        lines = self._filter_log_lines(self.log.readlines())
        if proc_filter:
            return [l for l in lines if l['process'] == proc_filter]
        else:
            return lines

if __name__ == '__main__':
    import sys
    import time

    if len(sys.argv) >= 2:
        proc = sys.argv[1]
    else:
        proc = None

    follow = logfollower()
    while True:
        print(follow.read(proc))  # we can specify a process ID on the command
        time.sleep(5)

