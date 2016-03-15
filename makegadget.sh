#! /bin/sh


# fails on the busybox shell, wrong number of \\ ?
#descriptor=05010906a101050719e029e71500250175019508810295017508810395057501050819012905910295017503910395067508150025650507190029658100c0
#echo -n $descriptor | sed 's/\([0-9A-F]\{2\}\)/\\\\\\x\1/gI' | xargs printf > my_report_desc
#set -e

# kill arduino stuff
kill $(ps |grep launcher |grep -v grep | cut '-d ' -f 3)
systemctl stop getty@ttyGS0.service
sleep 1
rmmod g_serial

mydir=$(pwd)

modprobe libcomposite
mkdir cfg
mount none cfg -t configfs
mkdir cfg/usb_gadget/g1
cd cfg/usb_gadget/g1

mkdir configs/c.1
#echo 0xa4a2 > idProduct # ether
#echo 0x0525 > idVendor # ether
#echo 0xa4ac > idProduct
#echo 0x0525 > idVendor
echo 0x1d6b > idVendor # Linux Foundation
echo 0x0104 > idProduct # Multifunction Composite Gadget
mkdir strings/0x409
mkdir configs/c.1/strings/0x409
echo serial > strings/0x409/serialnumber
echo manufacturer > strings/0x409/manufacturer
echo HID Gadget > strings/0x409/product
echo "Conf 1" > configs/c.1/strings/0x409/configuration
echo 120 > configs/c.1/MaxPower

# HID keyboard
mkdir functions/hid.usb0
echo 1 > functions/hid.usb0/protocol
echo 1 > functions/hid.usb0/subclass
echo 8 > functions/hid.usb0/report_length
cat $mydir/my_report_desc > functions/hid.usb0/report_desc
ln -s functions/hid.usb0 configs/c.1

# SERIAL
mkdir -p functions/acm.usb0
ln -s functions/acm.usb0 configs/c.1/

#ETHERNET
#mkdir -p functions/ecm.usb0
# first byte of address must be even
#HOST="48:6f:73:74:50:43" # "HostPC"
#SELF="42:61:64:55:53:42" # "BadUSB"
#echo "48:6f:73:74:50:43" > functions/ecm.usb0/host_addr
#echo "42:61:64:55:53:42" > functions/ecm.usb0/dev_addr
#ln -s functions/ecm.usb0 configs/c.1/

#MASS STORAGE
mkdir -p functions/mass_storage.usb0
echo 1 > functions/mass_storage.usb0/stall
echo 0 > functions/mass_storage.usb0/lun.0/cdrom
echo 0 > functions/mass_storage.usb0/lun.0/ro
echo 0 > functions/mass_storage.usb0/lun.0/nofua
echo /media/realroot/keyboard/massstorage > functions/mass_storage.usb0/lun.0/file
ln -s functions/mass_storage.usb0 configs/c.1/

# ENABLE
echo 0000:00:14.2 > UDC #/sys/class/udc

# hmm, can we do this now before connection?
systemctl enable getty@ttyGS0.service
systemctl start getty@ttyGS0.service


# Start
#./mapping.i586 /dev/input/by-id/*-kbd  target.xmodmap fr.xmodmap /dev/hidg0


# Info: mounting the massstorage
# mount -o loop,offset=4096 /media/realroot/keyboard/massstorage /media/hkrconfig