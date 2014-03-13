#!/bin/sh
opkg update
opkg install python-pip python-setuptools python-smbus
echo t-bone > /etc/hostname
