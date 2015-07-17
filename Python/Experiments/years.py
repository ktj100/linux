#!/usr/bin/env python
# Years till 100
import sys

###
#name = sys.argv[1]
#age = int(sys.argv[2])
#diff = 100 - age

#print 'Hello', name + ', you will be 100 in', diff, 'years!'
###

###
#if len(sys.argv) > 1:
#    name = sys.argv[1]
#else:
#    name = raw_input('Enter Name:')

#if len(sys.argv) > 2:
#    age = int(sys.argv[2])
#else:
#    age = int(raw_input('Enter Age:'))
###

###
import optparse

parser = optparse.OptionParser()
parser.add_option('-n', '--name', dest='name', help='Your Name')
parser.add_option('-a', '--age', dest='age', help='Your Age', type=int)

(options, args) = parser.parse_args()

if options.name is None:
    options.name = raw_input('Enter Name:')

if options.age is None:
    options.age = int(raw_input('Enter Age:'))
###

sayHello = 'Hello ' + name + ','

if age == 100:
    sayAge = 'You are already 100 years old!'
elif age < 100:
    sayAge = 'You will be 100 in ' + str(100 - age) + ' years!'
else:
    sayAge = 'You turned 100 ' + str(age - 100) + ' years ago!'

print sayHello, sayAge