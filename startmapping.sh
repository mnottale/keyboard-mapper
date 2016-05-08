#! /bin/bash

export PATH=/sbin:$PATH
cd /home/pi/keyboard/
/home/pi/keyboard/mapping > /tmp/mapping.log 2>&1 &
disown

