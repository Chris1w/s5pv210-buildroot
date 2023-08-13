#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <libusb-1.0/libusb.h>

static void dump_hex(const char *func, unsigned char *buf, int size)
{
	int i;
	
	printf("%s: size=%d, [", func, size);
	for (i=0; i<size; i++) {
		printf("%02x", buf[i]);
		if (i < (size - 1)) {
			printf(", ");
		}
	}
	printf("]\n");
}

static int test_lb(libusb_device_handle *handle, uint8_t endpoint_in, uint8_t endpoint_out)
{
	int ret;
	int i, size;
	unsigned char buf_i[64];
	unsigned char buf_o[512];
	
	#if 1
	for (i=0; i<sizeof(buf_i); i++)
		buf_i[i] = i;
	
	ret = libusb_bulk_transfer(handle, endpoint_out, buf_i, sizeof(buf_i), &size, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printf("libusb_bulk_transfer out error ret=%d\n", ret);
		return -1;
	}
	#endif
	
	#if 1
	size = 0;
	for (i=0; i<sizeof(buf_o); i++)
		buf_o[i] = 0;
	
	ret = libusb_bulk_transfer(handle, endpoint_in, buf_o, sizeof(buf_o), &size, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printf("libusb_bulk_transfer in error ret=%d\n", ret);
		return -1;
	}
	
	printf("   received %d bytes\n", size);
	
	dump_hex(__func__, buf_o, size);
	#endif
	
	return 0;
}

static int test_device(uint16_t vid, uint16_t pid)
{
	int ret;
	libusb_device_handle *handle;
	libusb_device *dev;
	struct libusb_config_descriptor *conf_desc;
	uint8_t endpoint_in = 0, endpoint_out = 0;	// default IN and OUT endpoints
	
	printf("Opening device %04X:%04X...\n", vid, pid);
	handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if (handle == NULL) {
		printf("libusb_open_device_with_vid_pid error\n");
		return -1;
	}
	
	dev = libusb_get_device(handle);
	
	libusb_get_config_descriptor(dev, 0, &conf_desc);
	printf("             bNumInterfaces: %d\n", conf_desc->bNumInterfaces);
	printf("             num_altsetting: %d\n", conf_desc->interface[0].num_altsetting);
	printf("             bNumEndpoints: %d\n", conf_desc->interface[0].altsetting[0].bNumEndpoints);
	
	endpoint_in = conf_desc->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
	endpoint_out = conf_desc->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
	printf("             endpoint_in: 0x%02x\n", endpoint_in);
	printf("             endpoint_out: 0x%02x\n", endpoint_out);
	
	libusb_set_auto_detach_kernel_driver(handle, 1);
	
	ret = libusb_claim_interface(handle, 0);
	if (ret != LIBUSB_SUCCESS) {
		printf("libusb_claim_interface error ret=%d\n", ret);
		return -1;
	}
	
	test_lb(handle, endpoint_in, endpoint_out);
	
	libusb_release_interface(handle, 0);
	
	libusb_close(handle);
	
	return 0;
}

int main(int argc, char *argv[])  
{
	const struct libusb_version *ver;
	int ret;
	
	ver = libusb_get_version();
	printf("%d.%d.%d.%d, %s, %s\n", ver->major, ver->minor, ver->micro, ver->nano, ver->rc, ver->describe);
	
	ret = libusb_init(NULL);
	if (ret < 0) {
		printf("libusb_init: ret=%d\n", ret);
		return -1;
	}
	
	test_device(0x0525, 0xf0f1);
	
	libusb_exit(NULL);
	
	return 0;
}
