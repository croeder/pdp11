"""python package for timing stuff"""
import sys
import time
import logging

class StopWatch():
    """One StopWatch object that tracks one method's duration."""
    def __init__(self, instance_id):
        self.instance_id = instance_id
        self.count = 0
        self.min = 1000000000000  # "uninitialized"
        self.max = -1  # "uninitialized"
        self.sum = 0
        self.mean = -1  # "uninitialized"
        self.start_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        self.duration = 0

    def get_id(self):
        """retrun this StopWatch's instance ID"""
        return self.instance_id

    def get_min(self):
        """return the minimum time recorded so far"""
        return self.min

    def get_mean(self):
        """return the mean time recorded so far"""
        return self.mean

    def get_max(self):
        """return the maximum time recorded so far"""
        return self.max

    def get_sum(self):
        """return the sum of all times recorded so far"""
        return self.sum

    def get_count(self):
        """return the number of times this interval has been timed so far"""
        return self.count

    def get_duration(self):
        """return the duration of the last start-stop pair"""
        return self.duration

    def restart_watch(self):
        """Restarts the clock. You better call stop first if you want to accumulate that time!"""
        self.start_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC)

    def stop_watch(self, time_stop):
        """Stops the clock and calculates min, max, and mean."""
        self.duration = time_stop - self.start_time
        self.min = min(self.duration, self.min)
        self.max = max(self.duration, self.max)
        self.sum = self.sum + self.duration
        self.count = self.count + 1
        self.mean = self.sum / self.count

    def to_string(self):
        """pretty-print instance_id"""
        pad = ""
        i = 0
        while i < 20-len(self.instance_id):
            pad = pad + " "
            i = i + 1
        reportmin = self.min
        if reportmin == sys.maxsize:
            reportmin = -1

        format_min = '{:12.0f}'.format(reportmin/1000)
        format_mean = '{:12.0f}'.format(self.mean/1000)
        format_max = '{:12.0f}'.format(self.max/1000)
        format_sum = '{:12.0f}'.format(self.sum/1000)
        format_count = '{:12.0f}'.format(self.count)

        return f'{self.instance_id}{pad}{format_min}{format_mean}' \
               f'{format_max}{format_sum}{format_count}'

class StopWatches():
    """Manages a dictionary of StopWatch objects.
    Lets you obtain timing information for methods in your project."""
    def __init__(self):
        self.stop_watch_dict = {}

    def reset(self):
        """Reset StopWatch list, clear all StopWatches."""
        self.stop_watch_dict = {}

    def start(self, instance_id):
        """Initialize one stop watch for a method.
        Call this before the part whose duration you want to measure.
        After the section, call stop()."""
        try:
            this_stop_watch = self.stop_watch_dict[instance_id]
        except KeyError:
            this_stop_watch = StopWatch(instance_id)
            self.stop_watch_dict[instance_id] = this_stop_watch
        this_stop_watch.restart_watch()

    def remove(self, instance_id):
        """remove this StopWatch from the list"""
        del self.stop_watch_dict[instance_id]

    def restart(self, instance_id):
        """Reset and start an existing StopWatch.
        Call this if the StopWatch already exists and you want a new reading.
        After the section, call stop(id).
        instance_id lets you have multiple StopWatches in one method"""
        self.remove(instance_id)
        self.start(instance_id)

    def stop(self, instance_id):
        """Call this right after the part whose duration you want to measure.
        If the id cannot be found, returns error message.
        id lets you have multiple StopWatches in one method"""
        stop_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
        try:
            this_stop_watch = self.stop_watch_dict[instance_id]
            this_stop_watch.stop_watch(stop_time)
        except KeyError:
            logging.info(f'WARN: StopWatches stop could not find id {instance_id}')

    def get_mean(self, instance_id):
        """Returns the mean value of the accumulated StopWatch.<br>
        If id cannot be found, returns -1.
        id lets you have multiple StopWatches in one method"""
        try:
            this_stop_watch = self.stop_watch_dict[instance_id]
            return this_stop_watch.get_mean()
        except KeyError:
            logging.info(f'WARN: StopWatches get_mean could not find id {instance_id}')
            return -1

    def to_string(self, instance_id):
        """Returns current state of StopWatch instance_id.
        If the instance_id cannot be found, returns error message.
        instance_id lets you have multiple StopWatches in one method"""
        try:
            this_stop_watch = self.stop_watch_dict[instance_id]
            return this_stop_watch.to_string()
        except KeyError:
            return f'WARN: StopWatches to_string could not find id {instance_id}'

    def report(self):
        """log text for every StopWatch in the dictionary"""
        logging.info("StopWatches Report (times in microseconds)")
        logging.info('id                      min        mean         max         sum       count')
        for instance_id in self.stop_watch_dict:
            this_stop_watch = self.stop_watch_dict[instance_id]
            logging.info(this_stop_watch.to_string())
        logging.info("StopWatches Report end")

    def get_watch(self, instance_id):
        '''get the specified watch options'''
        return self.stop_watch_dict[instance_id]

    def get_duration(self, instance_id):
        '''get the duration of the last stop-start pair'''
        this_stop_watch = self.stop_watch_dict[instance_id]
        return this_stop_watch.get_duration()
