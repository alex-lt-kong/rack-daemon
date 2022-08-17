#!/usr/bin/python
# -*- coding: utf-8 -*-

import RPi.GPIO as GPIO
import time
import typing

GPIO.setmode(GPIO.BCM)

CLOCK = 11 # G16
DATA = 17 # G4
OUTPUT_ENABLE = 18 # G24

GPIO.setwarnings(False)
GPIO.setup([CLOCK, DATA, OUTPUT_ENABLE], GPIO.OUT, initial=GPIO.LOW)

def clock_in_data(bits):
	#print "clock_in_data",len(bits),bits
	for i in range(0, len(bits)):
		#bit = not bits[i] # active-low
		bit = bits[i]
		#print bit
		GPIO.output(DATA, bit)

		# rising transition on '595 -> clocks in data
		GPIO.output(CLOCK, False)
		time.sleep(0.00001)
		#time.sleep(1)
		GPIO.output(CLOCK, True)

segment_positions = [
	# 7 0 4 5  6 1 2 3 8 letter#
	# h a e f  g b c d i
	# 0 1 2 3  4 5 6 7 8 bit#

	1,5,6,7,2,3,4,0,8
]

def segments_to_bits(segments):
	assert len(segments) == 9, "incomplete segments: %s" % (segments,)
	bits = [1]*9
	for i in range(0, len(segments)):
		position = segment_positions[i]
		bits[position] = not segments[i]
	return bits


symbols = {
	# /h\
	#   _d_
	# e|   |c
	#   _a_
	# f|   |b
	#   _g_
	# \i/
	#    a,b,c,d,e,f,g,h,i
	" ":[0,0,0,0,0,0,0,0,0],

	"0":[0,1,1,1,1,1,1,0,0],
	"1":[0,1,1,0,0,0,0,0,0],
	"2":[1,0,1,1,0,1,1,0,0],
	"3":[1,1,1,1,0,0,1,0,0],
	"4":[1,1,1,0,1,0,0,0,0],
	"5":[1,1,0,1,1,0,1,0,0],
	"6":[1,1,0,1,1,1,1,0,0],
	"7":[0,1,1,1,0,0,0,0,0],
	"8":[1,1,1,1,1,1,1,0,0],
	"9":[1,1,1,1,1,0,1,0,0],

	"'":[0,0,0,0,0,0,0,1,0],
	",":[0,0,0,0,0,0,0,0,1],

	# hex from https://en.wikipedia.org/wiki/Seven-segment_display#Displaying_letters
	"A":[1,1,1,1,1,1,0,0,0],
	"b":[1,1,0,0,1,1,1,0,0],
	"C":[0,0,0,1,1,1,1,0,0],
	"d":[1,1,1,0,0,1,1,0,0],
	"E":[1,0,0,1,1,1,1,0,0],
	"F":[1,0,0,1,1,1,0,0,0],

	# misc
	"-":[1,0,0,0,0,0,0,0,0],
	"_":[0,0,0,0,0,0,1,0,0],
	u"‾":[0,0,0,1,0,0,0,0,0], # U+203E OVERLINE
	"=":[1,0,0,0,0,0,1,0,0],
	u"≡":[1,0,0,1,0,0,1,0,0], # U+2661 IDENTICAL TO
	"u":[0,1,0,0,0,1,1,0,0],
	"o":[1,1,0,0,0,1,1,0,0],
	"|":[0,0,0,0,1,1,0,0,0],
	"y":[1,1,1,0,1,0,1,0,0],
	"P":[1,0,1,1,1,1,0,0,0],
	"H":[1,1,1,0,1,1,0,0,0],
	"h":[1,1,0,0,1,1,0,0,0],
	"r":[1,0,0,0,0,1,0,0,0],

	# only for the middle group
	".":[0,0,0,0,0,1,0,0,0],
	":":[0,0,0,0,1,1,0,0,0],
	u"·":[0,0,0,0,1,0,0,0,0],
}

# Set a group's segments to a symbol
def set_group(group, symbol, red_led=0):
	GPIO.output(OUTPUT_ENABLE, True) # turn off output while shifting in
	group_bits = [0,0,0,0,0]
	#group_bits = [1,1,1,1,1]
	# Note it is possible to set more than one group simultaneously, all to the same
	# segment bits, but here we only set exactly one and use persistence of vision
	group_bits[4 - group] = 1

	ignored = 0 # not connected
	#red_led = 0 # an extra 0603 chip LED I wired up for fun

	if isinstance(symbol, typing.List):
		segment_bits = segments_to_bits(symbol)
	elif type(symbol) == int:
		segment_bits = segments_to_bits(symbols[str(symbol)])
	elif type(symbol) == str:
		segment_bits = segments_to_bits(symbols[symbol])
	else:
		assert False, "invalid symbol type: %s" % (symbol,)

	bits = [ignored] + group_bits + segment_bits + [red_led]

	clock_in_data(bits)
	GPIO.output(OUTPUT_ENABLE, False)

#set_group(1, "F")
#raise SystemExit

#clock_in_data([0,1,1,0,1,0] + segments_to_bits(symbols["3"]) + [0])
def set_display(text):
	middle = None
	red_led = False
	if "!" in text:
		# special case: exclamation turns on the red LED, not part of the segment display
		text = text.replace("!", "")
		red_led = True	

	if len(text) == 5:
		middle = text[2]
		text = text[0:2] + text[3:5]
	if len(text) < 4: text += " " * (4 - len(text))
	assert len(text) == 4, "set_display bad text length: %s (%d)" % (text, len(text))

	group = 1
	for char in text:
		set_group(group, char, red_led)
		group += 1
		time.sleep(0.001)

	if middle:
		set_group(0, middle, red_led)
		time.sleep(0.001)


while True:
	#set_display("bE.EF")
	#continue
	#set_display("88888")
	#set_group(0, [0,0,0,0,1,1,0,0,0])
	#set_display(u"12·34")
	#set_display(u"12:34")

	t = time.localtime()
	hour = t.tm_hour
	minute = t.tm_min
	second = t.tm_sec

	if second % 2:
		colon = ':'
	else:
		colon = ' '

	if hour > 12: hour -= 12
	s = "%2d%s%.2d" % (hour, colon, minute)
	s = '1234'
	print(s)
	set_display(s)

	#set_group(1, "2")
	#time.sleep(0.01)
	#set_group(2, "3")
	#time.sleep(0.01)

raise SystemExit

#clock_in_data([0,0,0,0, 0,0,0,0,1])
#clock_in_data([1,1,1,1, 1,1,1,1,0])
#              5 4 3 2 1  e f g b  c d i a h  - R
#clock_in_data([0,0,0,0,0, 0,0,0,0, 0,0,0,0,0, 0,0])
#clock_in_data([1,0,0,0,0, 0,0,0,0, 0,0,0,0,0,0])
#              - 2 3 4 5 1  h a e f  g b c d i -
#clock_in_data([0,0,0,0,1,0, 1,0,0,1, 1,0,0,1,1,0])
#clock_in_data([0,0,1,0,1,0, 1,0,0,1, 1,0,0,1,1,0])
