from xntp import *
from datetime import date
import time
import serial
import sys
import argparse
import syslog


parser = argparse.ArgumentParser()

parser.add_argument('-t', '--tty', help='use TTY to communicate with arduino', default='/dev/ttyUSB0')
parser.add_argument('-s', '--speed', help='serial line speed', default=57600)

args = parser.parse_args()

syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
syslog.syslog(syslog.LOG_INFO, "Arduino DCF77 interface starting")

dcf = xntpshm(3)

syslog.syslog(syslog.LOG_INFO, 'talking to ' + args.tty + ' with ' + str(args.speed) + ' baud')

protocolMajor = 1
protocolMinor = 0

# configure the serial connections (the parameters differs on the devic
# e you are connecting to)
ser = serial.Serial(args.tty,args.speed)

willLeave = 0

receivedFullTime=0

line = ""
newcommand=1
currentcommand=''

utime=0
seconds=0

while 1 :
        if ser.inWaiting() > 0:
                c = ser.read(1)
                if c == '\n' :
                        if currentcommand == 'I':
                                syslog.syslog(syslog.LOG_INFO, line)
                        elif currentcommand == 'M':
                                if int(line[:1]) > protocolMajor:
                                        syslog.syslog(syslog.LOG_ERR,"ABORT: Arduino uses major protocol " + line[:1] + " I can only support " + str(protocolMajor))
                                        exit(1)
                        elif currentcommand == 'm':
                                if int(line[:1]) > protocolMajor:
                                        syslog.syslog(syslog.LOG_WARNING,"WARN: Arduino uses minor protocol " + line[:1] + " I only use " + str(protocolMinor))
                        elif currentcommand == '0':
                                syslog.syslog(syslog.LOG_DEBUG, line)
                        elif currentcommand == 'S':
                                s=int(line[:2])
                                print("second " + str(s))
                                if receivedFullTime == 1:
                                        print(time.asctime(time.localtime(utime + s)))
                                        dcf.settime(time.localtime(utime + s))
                        elif currentcommand == 'D':
                                #     0123456789012345678901234
                                #D -- 09 03 18;5; 12 31 30;tt1.
                                #     dd mm yy w  hh mm ss   d
                                #              d             s
                                #              y             t
                                timestr= line[:2] + " " + line[3:5] + " 20" + line[6:8] + " " + line[12:14] + " " + line[15:17]            
                                print(timestr)
                                time_struct = time.strptime(timestr,"%d %m %Y %H %M")
                                utime = time.mktime(time_struct)
                                tz=int(line[23:24])
#                                if tz > 2 or tz < 1:
#                                        syslog.syslog(syslog.LOG__ERR,"Illegal timezone " + str(tz))
#                                else:
#                                        utime -= (tz * 3600)
                                seconds = int(line[18:20])
                                receivedFullTime=1
                                print(time.asctime(time.localtime(utime + seconds)))
                                dcf.settime(time.localtime(utime + seconds))
                                print(tz)
                        else:
                                syslog.syslog(syslog.LOG_ERR, 'UNKNOWN CMD ' + currentcommand + ' -- ' + line)
                        line = ""
                        newcommand=1
                elif c == '\r' :
                        c = ''
                else:
                        if newcommand == 1:
                                newcommand = 0
                                currentcommand = c
                        else:
                                line = line + str(c)
ser.close()
