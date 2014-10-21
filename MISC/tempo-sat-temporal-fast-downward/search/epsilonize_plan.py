#! /usr/bin/env python
# -*- coding: utf-8 -*-

from decimal import Decimal
import itertools
import re
import sys


class TimedAction(object):
    def __init__(self, timestamp, name, duration):
        if timestamp < 0:
            raise ValueError("bad timestamp: %r" % timestamp)
        if duration < 0:
            raise ValueError("bad duration: %r" % duration)
        self.timestamp = timestamp
        self.name = name
        self.duration = duration

    def end(self):
        return self.timestamp + self.duration

    def copy(self):
        return self.__class__(self.timestamp, self.name, self.duration)

    def dump(self):
        print "%s: (%s) [%s]" % (self.timestamp, self.name, self.duration)

    @classmethod
    def parse(cls, line):
        def bad():
            raise ValueError("cannot parse line: %r" % line)
        regex = re.compile(
            r"\s*(.*?):\s*\(\s*(.*?)\s*\)\s*(?:\[\s*(.*?)\s*\])?$")
        match = regex.match(line)
        if not match:
            bad()
        try:
            timestamp = Decimal(match.group(1))
            name = match.group(2)
            if match.group(3) is not None:
                duration = Decimal(match.group(3))
            else:
                duration = Decimal("0")
        except ValueError:
            raise#bad()
        return cls(timestamp, name, duration)


class Plan(object):
    def __init__(self, actions):
        self.actions = actions

    def copy(self):
        return self.__class__([action.copy() for action in self.actions])

    @classmethod
    def parse(cls, lines):
        actions = []
        for line in sys.stdin:
            line = line.partition(";")[0].strip()
            if line:
                actions.append(TimedAction.parse(line))
        return cls(actions)

    def dump(self):
        for action in self.actions:
            action.dump()

    def sort(self):
        self.actions.sort(key=lambda action: action.timestamp)

    def required_separation(self):
        duration_granularity = granularity(a.duration for a in self.actions)
        rounded_num_actions = next_power_of_ten(len(self.actions))
        return duration_granularity / rounded_num_actions / 100

    def add_separation(self):
        separation = self.required_separation()
        for index, action in enumerate(self.actions):
            action.timestamp += (index + 1) * separation

    def makespan(self):
        if not self.actions:
            return Decimal("0")
        else:
            return max(action.end() for action in self.actions)

    def remove_wasted_time(self):
        # Using last_happening_before() only works properly when all
        # happenings only concern a single action, so we add separation
        # first.
        self.add_separation()
        for action in sorted(self.actions, key=lambda a: a.timestamp):
            action.timestamp = self.last_happening_before(
                action.timestamp, action)

    def last_happening_before(self, time, excluded_action):
        last_happening = Decimal("0")
        for action in self.actions:
            if action is not excluded_action:
                if action.timestamp <= time:
                    last_happening = max(last_happening, action.timestamp)
                if action.end() <= time:
                    last_happening = max(last_happening, action.end())
        return last_happening

    def wasted_time(self):
        return self.makespan() - self.adjusted_makespan()

    def adjusted_makespan(self):
        plan = self.copy()
        plan.remove_wasted_time()
        return plan.makespan()


def granularity(numbers):
    numbers = list(numbers)
    for power in itertools.count():
        if all((number * 10 ** power) % 1 == 0
               for number in numbers):
            return Decimal("0.1") ** power


def next_power_of_ten(num):
    for power in itertools.count():
        if num <= 10 ** power:
            return 10 ** power


plan = Plan.parse(sys.stdin)
plan.add_separation()

print "; Makespan: %s" % plan.makespan()
print "; Wasted time: %s" % plan.wasted_time()
print "; Adjusted makespan: %s" % plan.adjusted_makespan()
print "; validate with -t %s" % (plan.required_separation() / 2)
plan.dump()

## NOTE: To verify that the "wasted time" adjustment works,
##       try validating the plan obtained with
##       plan.remove_wasted_time(); plan.add_separation().
