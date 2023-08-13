#!/bin/sh

gr_start()
{
	mount -t configfs none /sys/kernel/config
	
	cd /sys/kernel/config/usb_gadget/
	
	mkdir g1
	cd g1
	#echo 0x0510 > bcdDevice
	echo 0x0200 > bcdUSB	# usb2.0
	echo 0x0525 > idVendor
	echo 0xf0f1 > idProduct
	
	mkdir strings/0x409
	echo "0123456789ABCDEF" > strings/0x409/serialnumber
	echo "linux" > strings/0x409/manufacturer
	echo "gr" > strings/0x409/product
	
	mkdir configs/c.1
	#echo 500 > configs/c.1/MaxPower

	mkdir configs/c.1/strings/0x409
	echo GadgetRaw > configs/c.1/strings/0x409/configuration
	
	mkdir functions/GadgetRaw.usb0
	cd functions/GadgetRaw.usb0
	
	#echo 4096 > bulk_buflen
	#echo 32 > qlen
	
	cd ../../
	ln -s functions/GadgetRaw.usb0 configs/c.1

	udc_node=`ls /sys/class/udc/`
	echo ${udc_node} > UDC
}

gr_stop()
{
	cd /sys/kernel/config/usb_gadget/g1/
	
	echo "" > UDC
	
	rm configs/c.1/GadgetRaw.usb0
	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1
	rmdir functions/GadgetRaw.usb0
	rmdir strings/0x409
	cd ..
	rmdir g1
}

case "$1" in
start)
	gr_start
	;;
stop)
	gr_stop
	;;
restart)
	gr_stop
	sleep 1
	gr_start
	;;
*)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
