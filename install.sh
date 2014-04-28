#!/bin/sh
#install basic requirements
opkg update
opkg install python-pip python-setuptools python-smbus

#download the tbone source
curl -o t-bone.zip https://github.com/interactive-matter/trinamic3d/archive/master.zip
unzip t-bone.zip

#disabling x according to http://curiouslynerdy.com/2013/09/disable-gui-on-the-beaglebone-black/
systemctl enable multi-user.target
#tailoring more services according to http://www.element14.com/community/community/knode/single-board_computers/blog/2014/02/27/beaglebone-black-radio-challenge--part-3-user-interface
systemctl disable bonescript.service  
systemctl disable bonescript-autorun.service
rm '/etc/systemd/system/multi-user.target.wants/bonescript-autorun.service'  
systemctl disable cloud9.service  
rm '/etc/systemd/system/multi-user.target.wants/cloud9.service'  
systemctl disable bonescript.socket  
rm '/etc/systemd/system/sockets.target.wants/bonescript.socket'
#finally update the hostname
echo t-bone > /etc/hostname
echo "Hostname changed, it is a good idea to reboot, new hostname is:"
cat /etc/hostname
