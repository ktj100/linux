# place one 170 sec, one 50 sec, one 0 sec, one non executable, and one infinite app/module in each directory
# start barsm 
# watch for creation of a child PID
# check that only AACM has started
# make sure that all six of the next modules starting up are from the modules directories
# make sure that both the 0 sec and non executables try to start 5 times each
# make sure errors are logged for each startup try
# make sure that six more start up, and they match the six applications
# make sure that both the 0 sec and non executables try to start 5 times each
# make sure errors are logged for each startup try
# make sure that barsm replaces all ended processes, and only ended processes
# 1
# 6
# 7
# 8
# 9
# 10
# 11
# 12
# 13
# 14
# 15

# place a non executable file in the AACM directory
# start barsm
# watch for five log errors while attempting to start AACM
# watch for end of AACM
# 2
# 3

# place a 0 sec file in AACM directory
# start barsm
# watch for five log errors while attempting to start AACM
# watch for end of AACM
# 4
# 5

