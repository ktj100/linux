#!/usr/bin/env python

# This file provides supporting classes/functions to all the BARSM Python scripts.

import subprocess

barsm_loc = "~/Documents/linux/barsm/"
apps_loc = "~/Documents/linux/barsm/dummies/"

# clear out all app directories	
def remove():
    subprocess.call("rm /opt/rc360/system/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/TPA/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/TPA/*", shell=True)


def monitor_launch():
    # Get BARSM PID
    barsm_info = proc.findproc("valgrind.bin")
    barsm_pid = (barsm_info[0]['pid'])
    print(barsm_info[0]['pid'])

    run_time = 0
    child_amount = 0
    start_tries = 0


    while (7 * child_amount) + 30 >= run_time and barsm_info:
        all_child = proc.findchild(barsm_pid)
        # prev_amount = child_amount
        child_amount = len(all_child)

        time.sleep(0.1)
        run_time += 0.1
        barsm_info = proc.findproc("valgrind.bin")

    if not barsm_info and 0 == child_amount:
        # return 0 if aacm never launched and BARSM exited
        return 0

    elif 4 == child_amount:
        # return 1 if the correct number of apps launched
        return 1

    else:
        # return 2 if another module never launched
        return 2



# import log

# if __name__ == '__main__':

#     if len(sys.argv) >= 2:
#         proc = sys.argv[1]
#     else:
#         proc = None

#     follow = log.logfollower()

#     # subprocess.call("cd ~/Documents/linux/barsm/", shell=True)
#     subprocess.call("gcc ~/Documents/linux/barsm/barsm.c -o barsm", shell=True)
#     # subprocess.call("~/Documents/linux/barsm/barsm", shell=True)
#     test_output = open('test.out', 'w')
#     subprocess.Popen("valgrind ~/Documents/linux/barsm/barsm -v --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --malloc-fill=B5 --free-fill=4A", stdout=test_output, stderr=test_output, shell=True)

#     while True:
#         #print(follow.read(proc))  # we can specify a process ID on the command
#         logs = follow.read(proc)
#         errors = 0
#         # for "'ERROR:" in logs
#         #     errors = errors + 1
#         search = "ERROR"
#         for l in logs:
#             errors += l['message'].count(search)

#         print(logs)
#         print(errors)

#         time.sleep(1)

# if __name__ == '__main__':
    
#     # clear out all app directories
#     subprocess.call("rm /opt/rc360/system/*", shell=True)
#     subprocess.call("rm /opt/rc360/modules/GE/*", shell=True)
#     subprocess.call("rm /opt/rc360/modules/TPA/*", shell=True)
#     subprocess.call("rm /opt/rc360/apps/GE/*", shell=True)
#     subprocess.call("rm /opt/rc360/apps/TPA/*", shell=True)

#     # compile needed applications and place in required directories
#     subprocess.call('gcc '+apps_loc+'zero_sec.c -o /opt/rc360/system/zero_sec_aacm', shell=True)

#     # start BARSM
#     test_output = open('test.out', 'w')

#     subprocess.call('gcc '+barsm_loc+'barsm.c -o '+barsm_loc+'barsm', shell=True)
#     subprocess.Popen('valgrind '+barsm_loc+'barsm -v --read-var-info=yes --leak-check=full --track-origins=yes ' \
#         'show-reachable=yes --malloc-fill=B5 --free-fill=4A', stdout=test_output, stderr=test_output, shell=True)

#     # monitor SYSLOG

#     # THE FOLLOWING IS FUNTIONAL TEST CODE TO MAKE SURE BARSM QUITS WITHIN 30 SECONDS OF STARTING
#     # # barsm_info stores the list of the one directory containing BARSM's info
#     # barsm_info = proc.findproc("valgrind.bin")l
#     # # '0' selects the one and only directory in the list, and 'pid' selects the item in the directory.
#     # barsm_pid = int(barsm_info[0]['pid'])
#     # print(barsm_info[0]['pid'])

#     # # BARSM should quit within about 10 seconds. 
#     # # This will quit when BARSM quits or 30 seconds pass.
#     # run_time = 0
#     # while barsm_info and run_time <= 30:
#     #     time.sleep(1)
#     #     run_time += 1
#     #     barsm_info = proc.findproc("valgrind.bin")

#     # # If BARSM is still alive, kill BARSM
#     # if barsm_info:
#     #     # print(barsm_pid)
#     #     os.kill(barsm_pid, signal.SIGKILL)
    
#     # Ensure that BARSM tries to start aacm exactly five times./
#     barsm_info = proc.findproc("valgrind.bin")
#     # '0' selects the one and only directory in the list, and 'pid' selects the item in the directory.
#     barsm_pid = (barsm_info[0]['pid'])
#     print(barsm_info[0]['pid'])

#     run_time = 0
#     child_amount = 0
#     start_tries = 0
#     while 30 >= run_time and barsm_info:
#         # print(follow.read())
#         all_child = proc.findchild(barsm_pid)
#         print(all_child)

#         # update number of children
#         # if there were none last time
#         if 0 == child_amount:
#             # check if there is one now
#             if 1 <= len(all_child):
#                 child_amount = 1
#                 child_pid = int(all_child[0]['pid'])
#                 start_tries += 1

#         # if there was one last time
#         else:
#             # check if there is one now
#             if 1 > len(all_child):
#                 child_amount = 0

#             elif int(all_child[0]['pid']) != child_pid:
#                 start_tries += 1
#                 child_pid = int(all_child[0]['pid'])

#         time.sleep(0.1)
#         run_time += 0.1
#         barsm_info = proc.findproc("valgrind.bin")

#     # check logs to see if five errors were logged for each launch failure
#     logs = follow.read()
#     launch_errors = sum(1 for d in logs if "File launch failed!" in d.get('message') and "/opt/rc360/system/" in d.get('message'))

#     if 5 == start_tries and 0 == child_amount and 5 == launch_errors:
#         print("SUCCESS")
#     else:
#         print("FAIL")

#     print("SCRIPT ENDED")
