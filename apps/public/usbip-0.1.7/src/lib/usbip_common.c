/*
 * Copyright (C) 2005-2006 Takahiro Hirofuchi
 */

#include "usbip.h"

int usbip_use_syslog = 0;
int usbip_use_stderr = 0;
int usbip_use_debug  = 0;

struct speed_string {
	int num;
	char *speed;
	char *desc;
};

static const struct speed_string speed_strings[] = {
	{ USB_SPEED_UNKNOWN, "unknown", "Unknown Speed"},
	{ USB_SPEED_LOW,  "1.5", "Low Speed(1.5Mbps)"  },
	{ USB_SPEED_FULL, "12",  "Full Speed(12Mbps)" },
	{ USB_SPEED_HIGH, "480", "High Speed(480Mbps)" },
	{ 0, NULL, NULL }
};

struct portst_string {
	int num;
	char *desc;
};

static struct portst_string portst_strings[] = {
	{ SDEV_ST_AVAILABLE,	"Device Available" },
	{ SDEV_ST_USED,		"Device in Use" },
	{ SDEV_ST_ERROR,	"Device Error"},
	{ VDEV_ST_NULL,		"Port Available"},
	{ VDEV_ST_NOTASSIGNED,	"Port Initializing"},
	{ VDEV_ST_USED,		"Port in Use"},
	{ VDEV_ST_ERROR,	"Port Error"},
	{ 0, NULL}
};
//delete by chz
#if 0 
/* added by tf, 110513, print log to webserver */
void log_to_web(int priority, char *fmt)
{
	char buf[255];
	strcpy(buf, fmt);
	msglogd(priority, LOGTYPE_NAS, "%s", fmt);
	return;
}
#endif
//end delete

const char *usbip_status_string(int32_t status)
{
	for(int i=0; portst_strings[i].desc != NULL; i++) 
		if(portst_strings[i].num == status)
			return portst_strings[i].desc;

	return "Unknown Status";
}

const char *usbip_speed_string(int num)
{
	for(int i=0; speed_strings[i].speed != NULL; i++) 
		if(speed_strings[i].num == num)
			return speed_strings[i].desc;

	return "Unknown Speed";
}

void pack_uint32_t(int pack, uint32_t *num)
{
	uint32_t i;

	if(pack)
		i = htonl(*num);
	else
		i = ntohl(*num);

	*num = i;
}

void pack_uint16_t(int pack, uint16_t *num)
{
	uint16_t i;

	if(pack)
		i = htons(*num);
	else
		i = ntohs(*num);

	*num = i;
}

void pack_usb_device(int pack, struct usb_device *udev)
{
	pack_uint32_t(pack, &udev->busnum);
	pack_uint32_t(pack, &udev->devnum);
	pack_uint32_t(pack, &udev->speed );

	pack_uint16_t(pack, &udev->idVendor );
	pack_uint16_t(pack, &udev->idProduct);
	pack_uint16_t(pack, &udev->bcdDevice);
}

void pack_usb_interface(int pack, struct usb_interface *udev)
{
	/* uint8_t members need nothing */
}


#define DBG_UDEV_INTEGER(name)\
	dbg("%-20s = %x", to_string(name), (int) udev->name)

#define DBG_UINF_INTEGER(name)\
	dbg("%-20s = %x", to_string(name), (int) uinf->name)

void dump_usb_interface(struct usb_interface *uinf)
{
	char buff[100];
	usbip_class_name(buff, sizeof(buff),
			uinf->bInterfaceClass,
			uinf->bInterfaceSubClass,
			uinf->bInterfaceProtocol);
	dbg("%-20s = %s", "Interface(C/SC/P)", buff);
}

void dump_usb_device(struct usb_device *udev)
{
	char buff[100];


	dbg("%-20s = %s", "path",  udev->path);
	dbg("%-20s = %s", "busid", udev->busid);

	usbip_class_name(buff, sizeof(buff),
			udev->bDeviceClass,
			udev->bDeviceSubClass,
			udev->bDeviceProtocol);
	dbg("%-20s = %s", "Device(C/SC/P)", buff);

	DBG_UDEV_INTEGER(bcdDevice);

	usbip_product_name(buff, sizeof(buff),
			udev->idVendor,
			udev->idProduct);
	dbg("%-20s = %s", "Vendor/Product", buff);

	DBG_UDEV_INTEGER(bNumConfigurations);
	DBG_UDEV_INTEGER(bNumInterfaces);

	dbg("%-20s = %s", "speed", 
			usbip_speed_string(udev->speed));

	DBG_UDEV_INTEGER(busnum);
	DBG_UDEV_INTEGER(devnum);
}


int read_attr_value(struct sysfs_device *dev, const char *name, const char *format)
{
	char attrpath[SYSFS_PATH_MAX];
	struct sysfs_attribute *attr;
	int num = 0;
	int ret;

	snprintf(attrpath, sizeof(attrpath), "%s/%s", dev->path, name);

	attr = sysfs_open_attribute(attrpath);
	if(!attr) {
		err("open attr %s", attrpath);
		return 0;
	}

	ret = sysfs_read_attribute(attr);
	if(ret < 0) {
		err("read attr");
		goto err;
	}

	ret = sscanf(attr->value, format, &num);
	if(ret < 1) {
		err("sscanf");
		goto err;
	}

err:
	sysfs_close_attribute(attr);

	return num;
}


int read_attr_speed(struct sysfs_device *dev)
{
	char attrpath[SYSFS_PATH_MAX];
	struct sysfs_attribute *attr;
	char speed[100];
	int ret;

	snprintf(attrpath, sizeof(attrpath), "%s/%s", dev->path, "speed");

	attr = sysfs_open_attribute(attrpath);
	if(!attr) {
		err("open attr");
		return 0;
	}

	ret = sysfs_read_attribute(attr);
	if(ret < 0) {
		err("read attr");
		goto err;
	}

	ret = sscanf(attr->value, "%s\n", speed);
	if(ret < 1) {
		err("sscanf");
		goto err;
	}
err:
	sysfs_close_attribute(attr);

	for(int i=0; speed_strings[i].speed != NULL; i++) {
		if(!strcmp(speed, speed_strings[i].speed))
			return speed_strings[i].num;
	}

	return USB_SPEED_UNKNOWN;
}

#if CLIENT > 2
// added by zl 2011-2-12
int read_attr_string(struct sysfs_device *dev, char * string, const char * name)
{
    char attrpath[SYSFS_PATH_MAX];
    struct sysfs_attribute *attr;
    int ret;
    int len = STRING_MAXLEN;

    string[0] = 0;

    snprintf(attrpath, sizeof(attrpath), "%s/%s", dev->path, name);

    attr = sysfs_open_attribute(attrpath);
    if(!attr) {
        err("open attr");
        return 0;
    }

    ret = sysfs_read_attribute(attr);
    if(ret < 0) {
        err("read attr");
        goto err;
    }

    len = (len < attr->len? len: attr->len);
    if(len > 0)
    {
        memcpy(string, attr->value, len);
    }
    
err:
    sysfs_close_attribute(attr);

    return 0;
}
// end----added
#endif

#define READ_ATTR(object, type, dev, name, format)\
	do { (object)->name = (type) read_attr_value(dev, to_string(name), format); } while(0)


int read_usb_device(struct sysfs_device *sdev, struct usb_device *udev)
{
	uint32_t busnum, devnum;

	READ_ATTR(udev, uint8_t,  sdev, bDeviceClass,		"%02x\n");
	READ_ATTR(udev, uint8_t,  sdev, bDeviceSubClass,	"%02x\n");
	READ_ATTR(udev, uint8_t,  sdev, bDeviceProtocol,	"%02x\n");

	READ_ATTR(udev, uint16_t, sdev, idVendor,		"%04x\n");
	READ_ATTR(udev, uint16_t, sdev, idProduct,		"%04x\n");
	READ_ATTR(udev, uint16_t, sdev, bcdDevice,		"%04x\n");

	READ_ATTR(udev, uint8_t,  sdev, bConfigurationValue,	"%02x\n");
	READ_ATTR(udev, uint8_t,  sdev, bNumConfigurations,	"%02x\n");
	READ_ATTR(udev, uint8_t,  sdev, bNumInterfaces,		"%02x\n");

	READ_ATTR(udev, uint8_t,  sdev, devnum,			"%d\n");
	udev->speed = read_attr_speed(sdev);

#if CLIENT > 2	
	// added by zl 2011-2-12 
	read_attr_string(sdev, udev->product, to_string(product)); // product
	read_attr_string(sdev, udev->manufacturer, to_string(manufacturer)); // manufacturer
	read_attr_string(sdev, udev->serial, to_string(serial)); // serialNumber
	// end----added
#endif

	strncpy(udev->path,  sdev->path,  SYSFS_PATH_MAX);
	strncpy(udev->busid, sdev->name, SYSFS_BUS_ID_SIZE);

	sscanf(sdev->name, "%u-%u", &busnum, &devnum);
	udev->busnum = busnum;

	return 0;
}

int read_usb_interface(struct usb_device *udev, int i, struct usb_interface *uinf)
{
	char busid[SYSFS_BUS_ID_SIZE];
	struct sysfs_device *sif;

	sprintf(busid, "%s:%d.%d", udev->busid, udev->bConfigurationValue, i);

	sif = sysfs_open_device("usb", busid);
	if(!sif) {
		err("open sif of %s", busid);
		return -1;
	}

	READ_ATTR(uinf, uint8_t,  sif, bInterfaceClass,		"%02x\n");
	READ_ATTR(uinf, uint8_t,  sif, bInterfaceSubClass,	"%02x\n");
	READ_ATTR(uinf, uint8_t,  sif, bInterfaceProtocol,	"%02x\n");

	sysfs_close_device(sif);

	return 0;
}


void usbip_product_name(char *buff, size_t size, uint16_t vendor, uint16_t product)
{
	const char *prod, *vend;

	prod = names_product(vendor, product);
	if(!prod)
		prod = "unknown product";


	vend = names_vendor(vendor);
	if(!vend)
		vend = "unknown vendor";

	snprintf(buff, size, "%s : %s (%04x:%04x)", vend, prod, vendor, product);
}

void usbip_class_name(char *buff, size_t size, uint8_t class, uint8_t subclass, uint8_t protocol)
{
	const char *c, *s, *p;

	p = names_protocol(class, subclass, protocol);
	if(!p)
		p = "unknown protocol";

	s = names_subclass(class, subclass);
	if(!s)
		s = "unknown subclass";

	c = names_class(class);
	if(!c)
		c = "unknown class";

	snprintf(buff, size, "%s / %s / %s (%02x/%02x/%02x)", c, s, p, class, subclass, protocol);
}
