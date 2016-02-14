#! /bin/sh


# fails on the busybox shell, wrong number of \\ ?
#descriptor=05010906a101050719e029e71500250175019508810295017508810395057501050819012905910295017503910395067508150025650507190029658100c0
#echo -n $descriptor | sed 's/\([0-9A-F]\{2\}\)/\\\\\\x\1/gI' | xargs printf > my_report_desc
set -e

mydir=$(pwd)

modprobe libcomposite
mkdir cfg
mount none cfg -t configfs
mkdir cfg/usb_gadget/g1
cd cfg/usb_gadget/g1
mkdir configs/c.1
mkdir functions/hid.usb0
echo 1 > functions/hid.usb0/protocol
echo 1 > functions/hid.usb0/subclass
echo 8 > functions/hid.usb0/report_length
cat $mydir/my_report_desc > functions/hid.usb0/report_desc
mkdir strings/0x409
mkdir configs/c.1/strings/0x409
echo 0xa4ac > idProduct
echo 0x0525 > idVendor
echo serial > strings/0x409/serialnumber
echo manufacturer > strings/0x409/manufacturer
echo HID Gadget > strings/0x409/product
echo "Conf 1" > configs/c.1/strings/0x409/configuration
echo 120 > configs/c.1/MaxPower
ln -s functions/hid.usb0 configs/c.1
echo 0000:00:14.2 > UDC