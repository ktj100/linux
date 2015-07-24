#!/usr/bin/env python3

def pidlist():
    import os
    import os.path
    path = '/proc'
    return [int(d) for d in os.listdir('/proc') if d.isdigit() and os.path.isdir(os.path.join(path, d))]

def procinfo(pid):
    import re
    # Format is specified by "man proc"
    pat = r'^(?P<pid>\d+) \(\S+\) (?P<state>[RSDZTW]) (?P<ppid>\d+) .*$'

    import os.path
    if os.path.isdir("/proc/{}".format(pid)):
        with open('/proc/{}/cmdline'.format(pid), 'r') as cmdfile:
            name = os.path.basename(cmdfile.readline().split('\0')[0])

            with open('/proc/{}/stat'.format(pid), 'r') as statfile:
                matchobj = re.match(pat, statfile.readline())
                if matchobj:
                    result = matchobj.groupdict()
                    result['name'] = name
                    return result
    # When a process disappears after being found by pidlist, but before being 
    # analyzed by procinfo, the script would end. So this if/else addition 
    # keeps that from happening.
    else:
        with open('/proc/1/cmdline'.format(pid), 'r') as cmdfile:
            name = os.path.basename(cmdfile.readline().split('\0')[0])

            with open('/proc/1/stat'.format(pid), 'r') as statfile:
                matchobj = re.match(pat, statfile.readline())
                if matchobj:
                    result = matchobj.groupdict()
                    result['name'] = name
                    return result
    # returns the list of info for the given PID

def procname(pid):
    info = procinfo(pid)
    if info:
        return info['name']
    # returns the name for a given PID

def infolist():
    return list(map(procinfo, pidlist()))

def proclist():
    return list(map(procname, pidlist()))

def findproc(proc):
    return list(filter(lambda x: x.get('name') == proc, infolist()))
    # checks all instances of 'name' in infolist and sees if it matches 'proc' input.

def findchild(ppid):
    return list(filter(lambda x: x.get('ppid') == ppid, infolist()))
    # lists all the children of the given parent PID

if __name__ == '__main__':
    import sys
    action = sys.argv[1]
    if action == 'list':
        print(pidlist())
    elif action == 'procs':
        print(proclist())
    elif action == 'name':
        pid = sys.argv[2]
        print(procname(pid))
    elif action == 'info':
        pid = sys.argv[2]
        print(procinfo(pid))
    elif action == 'find':
        proc = sys.argv[2]
        print(findproc(proc))
    elif action == 'child':
        ppid = sys.argv[2]
        print(findchild(ppid))

