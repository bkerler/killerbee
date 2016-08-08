#!/usr/bin/env python

# ZBWarDrive
# rmspeers 2010-13
# ZigBee/802.15.4 WarDriving Platform


import logging
import os
import re
from subprocess import call
from time import sleep
from usb import USBError

from killerbee import KillerBee, kbutils
from scanning import doScan

floatRE = '([-+]?[0-9]*\.?[0-9]+)'
locPattern = re.compile('^' + floatRE + '\|' + floatRE + '\|' + floatRE + ';$')

def goodLat(lat):
    return -180.00000005 < lat < 180.00000005

def goodLng(lng):
    return goodLat(lng)

def goodAlt(alt):
    return -180000.00005 < alt < 180000.00005

# GPS Poller
def gpsdPoller(currentGPS):
    '''
    @type currentGPS multiprocessing.Manager dict manager
    @arg currentGPS store relavent pieces of up-to-date GPS info
    '''
    FIFOPATH = '/tmp/gpsfifo'
    while True:
        try:
            with open(FIFOPATH, 'r') as f:
                match = locPattern.match(f.read())
                if match:
                    #TODO timeout lat/lng/alt values if too old...?
                    currentGPS['lng'] = float(match.group(1))
                    currentGPS['lat'] = float(match.group(2))
                    currentGPS['alt'] = float(match.group(3))
                    print "location updated: {}".format(currentGPS)
                else:
                    print "invalid location format"
                sleep(1)
        except KeyboardInterrupt:
            log_message = "Got KeyboardInterrupt in gpsdPoller, returning."
            print log_message
            logging.debug(log_message)
            return
        except IOError:
            log_message = "gps poll file not found, sleeping 5 seconds"
            print log_message
            logging.debug(log_message)
            sleep(5)

# startScan
# Detects attached interfaces
# Initiates scanning using doScan()
def startScan(currentGPS, verbose=False, include=[],
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

    log_message = "gps: {}".format(ignore)
    if verbose:
        print log_message
    logging.info(log_message)

    devices = kbutils.devlist(gps=ignore, include=include)

    for kbdev in devices:
        log_message = 'Found device at %s: \'%s\'' % (kbdev[0], kbdev[1])
        logging.info(log_message)
        if verbose:
            print log_message

    log_message = "Sending output to {}".format(output)
    if verbose:
        print log_message
    logging.info(log_message)

    kb.close()
    doScan(
        devices, currentGPS, verbose=verbose,
        output=output, scanning_time=scanning_time,
        capture_time=capture_time)
    return True
