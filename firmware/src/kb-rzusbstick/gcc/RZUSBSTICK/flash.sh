#!/bin/sh
sudo avrdude -P usb -c dragon_jtag -p usb1287 -B 10 -U flash:w:RZUSBSTICK.hex
