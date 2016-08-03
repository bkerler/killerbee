#!/usr/bin/env python

# ZBWarDrive
# rmspeers 2010-13
# ZigBee/802.15.4 WarDriving Platform


import logging
from subprocess import call
from time import sleep
from usb import USBError

from killerbee import KillerBee, kbutils
from scanning import doScan
from os.path import abspath

def goodLat(lat):
    return lat > -180.00000005 and lat < 180.00000005

def goodLng(lng):
    return goodLat(lng)

def goodAlt(alt):
    alt > -180000.00005 and alt < 180000.00005

# startScan
# Detects attached interfaces
# Initiates scanning using doScan()
def startScan(verbose=True, include=[],
              ignore=None, output='.',
              scanning_time=5, capture_time=2):

    try:
        kb = KillerBee()
    except USBError, e:
        if e.args[0].find('Operation not permitted') >= 0:
            log_message = 'Error: Permissions error, try running using sudo.'
            logging.error(log_message)
            print log_message
        else:
            log_message = 'Error: USBError: {}'.format(e)
            logging.error(log_message)
            print log_message
        return False
    except Exception, e:
        log_message = 'Error: Issue starting KillerBee instance: {}'.format(e)
        logging.error(log_message)
        print log_message
        return False

    devices = kbutils.devlist(include=include)

    for kbdev in devices:
        log_message = 'Found device at %s: \'%s\'' % (kbdev[0], kbdev[1])
        logging.info(log_message)
        print log_message

    log_message = "Sending output to {}".format(abspath(output))
    print log_message
    logging.info(log_message)

    kb.close()
    doScan(
        devices, verbose=True,
        output=output, scanning_time=scanning_time,
        capture_time=capture_time)
    return True
