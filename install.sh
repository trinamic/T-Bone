#!/bin/sh
#install basic requirements
opkg update
opkg install python-pip python-setuptools python-smbus

#download the tbone source
curl -o t-bone.zip https://github.com/interactive-matter/trinamic3d/archive/master.zip
unzip t-bone.zip

#disabling x according to http://curiouslynerdy.com/2013/09/disable-gui-on-the-beaglebone-black/
systemctl enable multi-user.target

#finally update the hostname
echo t-bone > /etc/hostname
echo "Hostname changed, it is a good idea to reboot, new hostname is:"
cat /etc/hostname
