#!/bin/sh

rndis_start()
{
	mount -t configfs none /sys/kernel/config
	
	cd /sys/kernel/config/usb_gadget/
	
	mkdir g1
	cd g1
	#echo 0x0510 > bcdDevice
	echo 0x0200 > bcdUSB	# usb2.0
	echo 0x0525 > idVendor
	echo 0xa4a2 > idProduct
	
	mkdir strings/0x409
	echo "0123456789ABCDEF" > strings/0x409/serialnumber
	echo "linux" > strings/0x409/manufacturer
	echo "rndis" > strings/0x409/product
	
	mkdir configs/c.1
	#echo 500 > configs/c.1/MaxPower

	mkdir configs/c.1/strings/0x409
	echo rndis > configs/c.1/strings/0x409/configuration
	
	mkdir functions/rndis.usb0
	cd functions/rndis.usb0
	
	echo "1a:fd:0d:a8:f1:3f" > dev_addr		#self
	echo "26:3a:ee:df:6f:c0" > host_addr	#pc
	
	cd ../../
	ln -s functions/rndis.usb0 configs/c.1

	udc_node=`ls /sys/class/udc/`
	echo ${udc_node} > UDC
}

rndis_stop()
{
	cd /sys/kernel/config/usb_gadget/g1/
	
	echo "" > UDC
	
	rm configs/c.1/rndis.usb0
	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1
	rmdir functions/rndis.usb0
	rmdir strings/0x409
	cd ..
	rmdir g1
}

case "$1" in
start)
	rndis_start
	;;
stop)
	rndis_stop
	;;
restart)
	rndis_stop
	sleep 1
	rndis_start
	;;
*)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
