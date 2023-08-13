/*
 * f_gadgetraw.c - USB peripheral gadgetraw configuration driver
 *
 * Copyright (C) 2022 by gaoqiang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/usb/composite.h>

#include "u_f.h"

struct f_opts {
	struct usb_function_instance func_inst;
	unsigned bulk_buflen;
	unsigned qlen;

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};

struct f_data {
	struct usb_function	function;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;

	unsigned                qlen;
	unsigned                buflen;
};

static inline struct f_data *func_to_drv_data(struct usb_function *f)
{
	return container_of(f, struct f_data, function);
}

/*-------------------------------------------------------------------------*/

static struct usb_interface_descriptor intf_ = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_descs[] = {
	(struct usb_descriptor_header *) &intf_,
	(struct usb_descriptor_header *) &fs_sink_desc,
	(struct usb_descriptor_header *) &fs_source_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *hs_descs[] = {
	(struct usb_descriptor_header *) &intf_,
	(struct usb_descriptor_header *) &hs_source_desc,
	(struct usb_descriptor_header *) &hs_sink_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor ss_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_source_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_endpoint_descriptor ss_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_sink_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_descriptor_header *ss_descs[] = {
	(struct usb_descriptor_header *) &intf_,
	(struct usb_descriptor_header *) &ss_source_desc,
	(struct usb_descriptor_header *) &ss_source_comp_desc,
	(struct usb_descriptor_header *) &ss_sink_desc,
	(struct usb_descriptor_header *) &ss_sink_comp_desc,
	NULL,
};

/* function-specific strings: */

static struct usb_string usb_strings[] = {
	[0].s = "gadgetraw read and write",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_ = {
	.language	= 0x0409,	/* en-us */
	.strings	= usb_strings,
};

static struct usb_gadget_strings *strings_[] = {
	&stringtab_,
	NULL,
};

/*-------------------------------------------------------------------------*/

static inline struct usb_request *_alloc_ep_req(struct usb_ep *ep, int len)
{
	return alloc_ep_req(ep, len);
}

/* Frees a usb_request previously allocated by alloc_ep_req() */
static inline void _free_ep_req(struct usb_ep *ep, struct usb_request *req)
{
	WARN_ON(req->buf == NULL);
	kfree(req->buf);
	req->buf = NULL;
	usb_ep_free_request(ep, req);
}

static void disable_ep(struct usb_composite_dev *cdev, struct usb_ep *ep)
{
	int			value;

	value = usb_ep_disable(ep);
	if (value < 0)
		DBG(cdev, "disable %s --> %d\n", ep->name, value);
}

static void disable_endpoints(struct usb_composite_dev *cdev,
		struct usb_ep *in, struct usb_ep *out)
{
	disable_ep(cdev, in);
	disable_ep(cdev, out);
}

static int _bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_data	*drv_data = func_to_drv_data(f);
	int			id;
	int ret;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	intf_.bInterfaceNumber = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	usb_strings[0].id = id;
	intf_.iInterface = id;

	/* allocate bulk endpoints */
	drv_data->in_ep = usb_ep_autoconfig(cdev->gadget, &fs_source_desc);
	if (!drv_data->in_ep) {
autoconf_fail:
		ERROR(cdev, "%s: can't autoconfigure on %s\n",
			f->name, cdev->gadget->name);
		return -ENODEV;
	}

	drv_data->out_ep = usb_ep_autoconfig(cdev->gadget, &fs_sink_desc);
	if (!drv_data->out_ep)
		goto autoconf_fail;

	/* support high speed hardware */
	hs_source_desc.bEndpointAddress = fs_source_desc.bEndpointAddress;
	hs_sink_desc.bEndpointAddress = fs_sink_desc.bEndpointAddress;

	/* support super speed hardware */
	ss_source_desc.bEndpointAddress = fs_source_desc.bEndpointAddress;
	ss_sink_desc.bEndpointAddress = fs_sink_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, fs_descs, hs_descs, ss_descs, ss_descs);
	if (ret)
		return ret;

	INFO(cdev, "%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, drv_data->in_ep->name, drv_data->out_ep->name);
	return 0;
}

static void _free_func(struct usb_function *f)
{
	struct f_opts *opts;

	opts = container_of(f->fi, struct f_opts, func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);

	usb_free_all_descriptors(f);
	kfree(func_to_drv_data(f));
}

static void dump_hex(const char *func, unsigned char *buf, int size)
{
	unsigned char *src;
	int block;
	char str[64];
	int i,j;
	
	printk("%s: size=%d, [\n", func, size);
	src = buf;
	for (j=0; j<size; j+=16) {
		if (j + 16 < size)
			block = 16;
		else
			block = size - j;
		for (i=0; i<block; i++) {
			sprintf(str + 3 * i, "%02x ", *src++);
		}
		printk("%s\n", str);
	}
	printk("]\n");
}

static void _complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_data	*drv_data = ep->driver_data;
	struct usb_composite_dev *cdev;
	int			status = req->status;
	unsigned char *data;
	static unsigned char val_ = 1;

	/* driver_data will be null if ep has been disabled */
	if (!drv_data)
		return;
	
	cdev = drv_data->function.config->cdev;

	INFO(cdev, "status=%d\n", status);

	switch (status) {
	case 0:				/* normal completion? */
		if (ep == drv_data->out_ep) {
			INFO(cdev, "out1 %s: %d-%d\n",
					  ep->name, req->actual, req->length);

			data = (unsigned char *)req->buf;
			INFO(cdev, "out2 %d-%d\n",
					  data[0], data[req->actual-1]);
			dump_hex("out3", data, req->actual);
		} else {
			INFO(cdev, "in1 %s: %d-%d\n",
					  ep->name, req->actual, req->length);

			memset(req->buf, val_, req->length);
			val_++;

			data = (unsigned char *)req->buf;
			INFO(cdev, "in2 %d-%d\n",
					  data[0], data[req->actual-1]);
		}

		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status == 0) {
			return;
		} else {
			ERROR(cdev, "Unable to queue buffer to %s: %d\n",
			      ep->name, status);
			goto free_req;
		}

	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
free_req:
		_free_ep_req(ep, req);
		ERROR(cdev, "_free_ep_req\n");
		return;
	case -EOVERFLOW:		/* buffer overrun on read means that
					 * we didn't provide a big enough
					 * buffer.
					 */
	case -EREMOTEIO:		/* short read */
	default:
		ERROR(cdev, "%s complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		break;
	}
}

static int alloc_requests(struct usb_composite_dev *cdev,
			  struct f_data *drv_data, bool is_in)
{
	struct usb_ep		*ep;
	struct usb_request	*req;
	int i;
	int result = 0;

	for (i = 0; i < drv_data->qlen && result == 0; i++) {
		result = -ENOMEM;
		
		ep = is_in ? drv_data->in_ep : drv_data->out_ep;
		req = _alloc_ep_req(ep, drv_data->buflen);
		if (!req)
			goto fail;

		req->complete = _complete;
		
		if (is_in)
			memset(req->buf, 0x00, req->length);
		else
			memset(req->buf, 0x55, req->length);
		
		INFO(cdev, "%s[%d]: %d-%d\n",
				  ep->name, i, req->actual, req->length);

		result = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (result) {
			ERROR(cdev, "%s queue req --> %d\n",
					ep->name, result);
			goto fail_free;
		}
	}

	return 0;

fail_free:
	_free_ep_req(ep, req);
fail:
	return result;
}

static void _disable_endpoints(struct f_data *drv_data)
{
	struct usb_composite_dev	*cdev;

	cdev = drv_data->function.config->cdev;
	disable_endpoints(cdev, drv_data->in_ep, drv_data->out_ep);
	DBG(cdev, "%s disabled\n", drv_data->function.name);
}

static int enable_endpoint(struct usb_composite_dev *cdev,
			   struct f_data *drv_data, struct usb_ep *ep)
{
	int					result;

	result = config_ep_by_speed(cdev->gadget, &(drv_data->function), ep);
	if (result)
		goto out;

	result = usb_ep_enable(ep);
	if (result < 0)
		goto out;
	ep->driver_data = drv_data;
	result = 0;

out:
	return result;
}

static int _enable(struct usb_composite_dev *cdev, struct f_data *drv_data)
{
	int					result = 0;

	/* one bulk endpoint writes (sources) zeroes IN (to the host) */
	result = enable_endpoint(cdev, drv_data, drv_data->in_ep);
	if (result)
		goto out;

	result = alloc_requests(cdev, drv_data, true);
	if (result)
		goto disable_in;

	/* one bulk endpoint reads (sinks) anything OUT (from the host) */
	result = enable_endpoint(cdev, drv_data, drv_data->out_ep);
	if (result)
		goto disable_in;

	result = alloc_requests(cdev, drv_data, false);
	if (result)
		goto disable_out;

	DBG(cdev, "%s enabled\n", drv_data->function.name);
	return 0;

disable_out:
	usb_ep_disable(drv_data->out_ep);
disable_in:
	usb_ep_disable(drv_data->in_ep);
out:
	return result;
}

static int _set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct f_data	*drv_data = func_to_drv_data(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	INFO(cdev, "alt intf %d\n", alt);

	_disable_endpoints(drv_data);
	return _enable(cdev, drv_data);
}

static void _disable(struct usb_function *f)
{
	struct f_data	*drv_data = func_to_drv_data(f);

	_disable_endpoints(drv_data);
}

static struct usb_function *alloc_function(struct usb_function_instance *fi)
{
	struct f_data	*drv_data;
	struct f_opts	*opts;

	drv_data = kzalloc(sizeof *drv_data, GFP_KERNEL);
	if (!drv_data)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_opts, func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt++;
	mutex_unlock(&opts->lock);

	drv_data->buflen = opts->bulk_buflen;
	drv_data->qlen = opts->qlen;
	if (!drv_data->qlen)
		drv_data->qlen = 32;

	drv_data->function.name = "gadgetraw";
	drv_data->function.bind = _bind;
	drv_data->function.set_alt = _set_alt;
	drv_data->function.disable = _disable;
	drv_data->function.strings = strings_;

	drv_data->function.free_func = _free_func;

	return &drv_data->function;
}

static inline struct f_opts *to_f_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_opts,
			    func_inst.group);
}

static void _release(struct config_item *item)
{
	struct f_opts *opts = to_f_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations _item_ops = {
	.release		= _release,
};

static ssize_t f_opts_qlen_show(struct config_item *item, char *page)
{
	struct f_opts *opts = to_f_opts(item);
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", opts->qlen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_opts_qlen_store(struct config_item *item,
				    const char *page, size_t len)
{
	struct f_opts *opts = to_f_opts(item);
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->qlen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

CONFIGFS_ATTR(f_opts_, qlen);

static ssize_t f_opts_bulk_buflen_show(struct config_item *item, char *page)
{
	struct f_opts *opts = to_f_opts(item);
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", opts->bulk_buflen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_opts_bulk_buflen_store(struct config_item *item,
				    const char *page, size_t len)
{
	struct f_opts *opts = to_f_opts(item);
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->bulk_buflen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

CONFIGFS_ATTR(f_opts_, bulk_buflen);

static struct configfs_attribute *_attrs[] = {
	&f_opts_attr_qlen,
	&f_opts_attr_bulk_buflen,
	NULL,
};

static const struct config_item_type _func_type = {
	.ct_item_ops    = &_item_ops,
	.ct_attrs	= _attrs,
	.ct_owner       = THIS_MODULE,
};

static void _free_instance(struct usb_function_instance *fi)
{
	struct f_opts *opts;

	opts = container_of(fi, struct f_opts, func_inst);
	kfree(opts);
}

static struct usb_function_instance *alloc_instance(void)
{
	struct f_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = _free_instance;
	opts->bulk_buflen = 512;
	opts->qlen = 2;

	config_group_init_type_name(&opts->func_inst.group, "",
				    &_func_type);

	return  &opts->func_inst;
}
DECLARE_USB_FUNCTION(GadgetRaw, alloc_instance, alloc_function);

static int __init _modinit(void)
{
	int ret;

	ret = usb_function_register(&GadgetRawusb_func);

	return ret;
}
static void __exit _modexit(void)
{
	usb_function_unregister(&GadgetRawusb_func);
}
module_init(_modinit);
module_exit(_modexit);

MODULE_LICENSE("GPL");
