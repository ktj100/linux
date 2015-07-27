#!/usr/bin/env python
#
# File: periodic_timer.py
# Copyright (c) 2015, DornerWorks, Ltd.
#
# Description:
#  This script creates a thread that calls a function periodically
#

import threading
import time

class PeriodicTimer(threading.Thread):
    """
    Periodic timer class that spawns a thread that will execute an action
    periodically.
    """
    def __init__(self, action, delay, args=()):
        """
        Initialization function for the periodic timer class.  When the start()
        function is called it will start the thread that will cause the
        specified action to be called after the delay seconds have elapsed until
        the stop() function is called.

        The action function can optionally return a new delay value if the
        periodic timer needs to be able to adjust itself.
        """
        self.action = action
        self.delay = delay
        super(PeriodicTimer, self).__init__(target=self._thread, args=args)
        self.stop_event = threading.Event()

    def _thread(self, *args):
        """
        The main execution thread.
        This thread will loop forever (until stop() is called) and perform the
        specified periodic action after the specified delay has elapsed.  When
        stop is called this thread will exit immediately.
        """
        while True:
            if self.stop_event.isSet():
                return
            # get the current time
            before = time.time()

            # Optionally the action function can return a new value for the
            # delay timer
            ret = self.action(*args)

            if ret:
                if ret > 0:
                    self.delay = float(ret)
                else:
                    # If a negative timeout is returned, exit.
                    return

            # Now check and see how much time has elapsed, and take that into 
            # account before delaying
            after = time.time()

            # If the difference is greater than the set delay, just delay the 
            # normal amount, otherwise take the difference into account to 
            # ensure that the periodic action does not wander.
            diff = after - before
            delay = self.delay
            if diff > delay:
                delay = delay - diff
            self.stop_event.wait(delay)

    def stop(self, wait=True, timeout=None):
        """
        This function will set the event that causes the periodic thread to
        stop executing and exit.  If the "wait" parameter is set, this function
        will wait until the periodic thread exits before returning.  Optionally
        a timeout can be provided to specify how long this function should wait
        before raising an exception if the thread has not yet exited.
        """
        self.stop_event.set()
        if wait:
            # A timeout parameter of 'None' would cause join() to immediately
            # exit regardless of if the thread has been stopped or not, so only
            # provide a timeout parameter to the join() function if it is not
            # 'None'.
            if timeout:
                self.join(timeout)
            else:
                self.join()
            assert self.isAlive() == False

