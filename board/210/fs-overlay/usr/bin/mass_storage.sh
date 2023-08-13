#!/bin/sh

mass_storage_start()
{
	mount -t configfs none /sys/kernel/config
	
	cd /sys/kernel/config/usb_gadget/
	
	mkdir g1
	cd g1
	#echo 0x0510 > bcdDevice
	echo 0x0200 > bcdUSB	# usb2.0
	echo 0x0525 > idVendor
	echo 0xa4a5 > idProduct
	
	mkdir strings/0x409
	echo "0123456789ABCDEF" > strings/0x409/serialnumber
	echo "linux" > strings/0x409/manufacturer
	echo "mass_storage" > strings/0x409/product
	
	mkdir configs/c.1
	#echo 500 > configs/c.1/MaxPower

	mkdir configs/c.1/strings/0x409
	echo mass_storage > configs/c.1/strings/0x409/configuration
	
	mkdir functions/mass_storage.usb0
	cd functions/mass_storage.usb0
	
	#echo 0 > stall
	echo /dev/mmcblk1p2 > lun.0/file
	#echo 1 > lun.0/removable
	
	cd ../../
	ln -s functions/mass_storage.usb0 configs/c.1

	udc_node=`ls /sys/class/udc/`
	echo ${udc_node} > UDC
}

mass_storage_stop()
{
	cd /sys/kernel/config/usb_gadget/g1/
	
	echo "" > UDC
	
	rm configs/c.1/mass_storage.usb0
	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1
	rmdir functions/mass_storage.usb0
	rmdir strings/0x409
	cd ..
	rmdir g1
}

case "$1" in
start)
	mass_storage_start
	;;
stop)
	mass_storage_stop
	;;
restart)
	mass_storage_stop
	sleep 1
	mass_storage_start
	;;
*)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
