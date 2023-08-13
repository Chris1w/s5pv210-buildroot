#!/bin/sh

configure_uvc_resolution_yuyv()
{
	W=$1
	H=$2
	
	mkdir streaming/uncompressed/u/${H}p
	cd streaming/uncompressed/u/${H}p
	
	echo $W > wWidth
	echo $H > wHeight
	echo 666666 > dwDefaultFrameInterval
	echo $((W*H*2*10)) > dwMinBitRate
	echo $((W*H*2*15)) > dwMaxBitRate
	echo $((W*H*2)) > dwMaxVideoFrameBufferSize
	# 15fps 10fps
	echo -e "666666\n1000000" > dwFrameInterval
	
	cd ../../../../
}

configure_uvc_resolution_mjpeg()
{
	W=$1
	H=$2
	
	mkdir streaming/mjpeg/m/${H}p
	cd streaming/mjpeg/m/${H}p
	
	echo $W > wWidth
	echo $H > wHeight
	echo 333333 > dwDefaultFrameInterval
	echo $((W*H*10)) > dwMinBitRate
	echo $((W*H*30)) > dwMaxBitRate
	echo $((W*H)) > dwMaxVideoFrameBufferSize
	# 30fps 15fps 10fps
	echo -e "333333\n666666\n1000000" > dwFrameInterval
	
	cd ../../../../
}

uvc_start()
{
	mount -t configfs none /sys/kernel/config
	
	cd /sys/kernel/config/usb_gadget/
	
	mkdir g1
	cd g1
	echo 0x0010 > bcdDevice
	echo 0x0200 > bcdUSB	# usb2.0
	echo 0x1d6b > idVendor
	echo 0x0102 > idProduct
	
	mkdir strings/0x409
	echo "0123456789ABCDEF" > strings/0x409/serialnumber
	echo "linux" > strings/0x409/manufacturer
	echo "uvc" > strings/0x409/product
	
	mkdir configs/c.1
	#echo 500 > configs/c.1/MaxPower

	mkdir configs/c.1/strings/0x409
	echo uvc > configs/c.1/strings/0x409/configuration
	
	mkdir functions/uvc.usb0
	cd functions/uvc.usb0
	
	# echo 3072 > streaming_maxpacket	#1109
	# echo 1024 > streaming_maxpacket		#40
	echo 512 > streaming_maxpacket		#210/3288
	# echo 2 > uvc_num_request	#1109
	
	mkdir control/header/h
	ln -s control/header/h control/class/fs/h
	ln -s control/header/h control/class/ss/h
	
	mkdir streaming/mjpeg/m
	configure_uvc_resolution_mjpeg 800 480
	configure_uvc_resolution_mjpeg 480 800
	configure_uvc_resolution_mjpeg 720 1280
	
	mkdir streaming/uncompressed/u
	configure_uvc_resolution_yuyv 640 360
	
	mkdir streaming/header/h
	ln -s streaming/mjpeg/m streaming/header/h/m
	ln -s streaming/uncompressed/u streaming/header/h/u
	ln -s streaming/header/h streaming/class/fs/h
	ln -s streaming/header/h streaming/class/hs/h
	ln -s streaming/header/h streaming/class/ss/h
	
	cd ../../
	ln -s functions/uvc.usb0 configs/c.1

	udc_node=`ls /sys/class/udc/`
	echo ${udc_node} > UDC
}

uvc_stop()
{
	cd /sys/kernel/config/usb_gadget/g1/
	
	echo "" > UDC
	
	rm configs/c.1/uvc.usb0
	rm functions/uvc.usb0/streaming/class/fs/h
	rm functions/uvc.usb0/streaming/class/hs/h
	rm functions/uvc.usb0/streaming/class/ss/h
	rm functions/uvc.usb0/streaming/header/h/u
	rm functions/uvc.usb0/streaming/header/h/m
	rmdir functions/uvc.usb0/streaming/header/h
	rmdir functions/uvc.usb0/streaming/uncompressed/u/360p
	rmdir functions/uvc.usb0/streaming/uncompressed/u
	rmdir functions/uvc.usb0/streaming/mjpeg/m/720p
	rmdir functions/uvc.usb0/streaming/mjpeg/m
	rm functions/uvc.usb0/control/class/fs/h
	rm functions/uvc.usb0/control/class/ss/h
	rmdir functions/uvc.usb0/control/header/h
	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1
	rmdir functions/uvc.usb0
	rmdir strings/0x409
	cd ..
	rmdir g1
}

case "$1" in
start)
	uvc_start
	;;
stop)
	uvc_stop
	;;
restart)
	uvc_stop
	sleep 1
	uvc_start
	;;
*)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
