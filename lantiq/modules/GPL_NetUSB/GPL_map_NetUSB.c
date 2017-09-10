#include "GPL_map_NetUSB.h"
//#include "hcd.h"	//some board need


int gpl_usb_clear_halt(struct usb_device *dev, int pipe) {
	return usb_clear_halt(dev, pipe);
}
EXPORT_SYMBOL(gpl_usb_clear_halt);

int gpl_usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request,
		    __u8 requesttype, __u16 value, __u16 index, void *data,
		    __u16 size, int timeout)
{
	return usb_control_msg(dev, pipe, request, requesttype, value, index, data, size, timeout);
}
EXPORT_SYMBOL(gpl_usb_control_msg);

int gpl_usb_set_interface(struct usb_device *dev, int interface, int alternate)
{
	return usb_set_interface(dev, interface, alternate);
}
EXPORT_SYMBOL(gpl_usb_set_interface);

int gpl_usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	return usb_string(dev, index, buf, size);
}
EXPORT_SYMBOL(gpl_usb_string);

struct urb *gpl_usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
	return usb_alloc_urb(iso_packets, mem_flags);
}
EXPORT_SYMBOL(gpl_usb_alloc_urb);

void gpl_usb_free_urb(struct urb *urb)
{
	return usb_free_urb(urb);
}
EXPORT_SYMBOL(gpl_usb_free_urb);

int gpl_usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
	return usb_submit_urb(urb, mem_flags);
}
EXPORT_SYMBOL(gpl_usb_submit_urb);

int gpl_usb_unlink_urb(struct urb *urb)
{
	return usb_unlink_urb(urb);
}
EXPORT_SYMBOL(gpl_usb_unlink_urb);

//	John: in some board no usb_get_current_frame_number
/*
#ifdef ALIAN00
int usb_hcd_get_frame_number (struct usb_device *udev)
{
	struct usb_hcd	*hcd = bus_to_hcd(udev->bus);

	if (!HC_IS_RUNNING (hcd->state))
		return -ESHUTDOWN;
	return hcd->driver->get_frame_number (hcd);
}
#endif //#ifdef ALIAN00
*/

int gpl_usb_get_current_frame_number(struct usb_device *dev)
{
	return usb_get_current_frame_number(dev);
/*
#ifdef ALIAN00
	return usb_hcd_get_frame_number(dev);			//	John: in some board no usb_get_current_frame_number
#endif	//#ifdef ALIAN00
*/
}
EXPORT_SYMBOL(gpl_usb_get_current_frame_number);

int gpl_usb_register(struct usb_driver *driver)
{
	return usb_register_driver(driver, THIS_MODULE, KBUILD_MODNAME);
}
EXPORT_SYMBOL(gpl_usb_register);

//	John: in some board no usb_deregister
/*
#ifdef ALIAN01
const char *usbcore_name = "usbcore";

void usb_remove_newid_file(struct usb_driver *usb_drv)
{
	if (usb_drv->no_dynamic_id)
		return;

	if (usb_drv->probe != NULL)
		driver_remove_file(&usb_drv->drvwrap.driver,
				   NULL);//&driver_attr_new_id);
}

inline void usb_free_dynids(struct usb_driver *usb_drv)
{
}
#endif	//#ifdef ALIAN01
*/

void gpl_usb_deregister(struct usb_driver *driver)
{
	usb_deregister(driver);

//	John: in some board no usb_deregister
/*
#ifdef ALIAN01
	pr_info("%s: deregistering interface driver %s\n",
			usbcore_name, driver->name);

	usb_remove_newid_file(driver);
	usb_free_dynids(driver);
	driver_unregister(&driver->drvwrap.driver);
#endif	//#ifdef ALIAN01
*/
//	usbfs_update_special();
}
EXPORT_SYMBOL(gpl_usb_deregister);

struct usb_host_interface *gpl_usb_altnum_to_altsetting(const struct usb_interface *intf,
                            unsigned int altnum)
{
    return usb_altnum_to_altsetting(intf, altnum);
}
EXPORT_SYMBOL(gpl_usb_altnum_to_altsetting);

struct usb_device *gpl_usb_get_dev(struct usb_device *dev)
{
    return usb_get_dev(dev);
}
EXPORT_SYMBOL(gpl_usb_get_dev);

void gpl_usb_put_dev(struct usb_device *dev)
{
    usb_put_dev(dev);
}
EXPORT_SYMBOL(gpl_usb_put_dev);

struct usb_interface *gpl_usb_ifnum_to_if(const struct usb_device *dev,
                      unsigned ifnum)
{
    return usb_ifnum_to_if(dev, ifnum);
}
EXPORT_SYMBOL(gpl_usb_ifnum_to_if);

extern int gpl_dev_dbg(const struct device *dev, const char *fmt, ...)
{
  int ret = 0;
	va_list args;
  const int bufSize = 128;
  unsigned char buf[bufSize];

  va_start(args, fmt);
  ret = vsnprintf(buf, bufSize, fmt, args);
  va_end(args);

  if(ret >= bufSize)
  { 
    printk(KERN_ALERT"dev_dbg message may be cutted\n"); 
    buf[bufSize - 1] = '\0';
  }
  else if(ret < 0)
  { return ret; }     //exit

  // < 2.6.27 , use (struct device *)dev in argument 1 to avoid warning
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
  dev_dbg((struct device *)dev, "%s", buf);
#else
  dev_dbg(dev, "%s", buf);
#endif

  return ret;
}
EXPORT_SYMBOL(gpl_dev_dbg);


extern int gpl_dev_set_name(struct device *dev, const char *fmt, ...)
{
  int ret = 0;
	va_list args;
  const int bufSize = 128;
  unsigned char buf[bufSize];

  va_start(args, fmt);
  ret = vsnprintf(buf, bufSize, fmt, args);
  va_end(args);

  if(ret >= bufSize)
  { 
    printk(KERN_ALERT"dev_set_name message may be cutted\n"); 
    buf[bufSize - 1] = '\0';
  }
  else if(ret < 0)
  { return ret; }     //exit

  ret = dev_set_name(dev, "%s", buf);
  return ret;
}
EXPORT_SYMBOL(gpl_dev_set_name);


void gpl_klist_remove(struct klist_node * n)
{ klist_remove(n); }
EXPORT_SYMBOL(gpl_klist_remove);

void gpl_klist_add_head(struct klist_node * n, struct klist * k)
{ klist_add_head(n, k); }
EXPORT_SYMBOL(gpl_klist_add_head);

int gpl_usb_get_descriptor(struct usb_device *dev, unsigned char type,
                                    unsigned char index, void *buf, int size)
{
  return usb_get_descriptor(dev, type, index, buf, size);
}
EXPORT_SYMBOL(gpl_usb_get_descriptor);


int gpl_usb_reset_device(struct usb_device *udev)
{
  return usb_reset_device(udev);
}
EXPORT_SYMBOL(gpl_usb_reset_device);

void gpl_usb_kill_urb(struct urb *urb)
{
  usb_kill_urb(urb);
}
EXPORT_SYMBOL(gpl_usb_kill_urb);

int gpl_usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
                        void *data, int len, int *actual_length, int timeout)
{
  return usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout);
}
EXPORT_SYMBOL(gpl_usb_bulk_msg);

void *gpl_usb_buffer_alloc(struct usb_device *dev, size_t size, 
                                            gfp_t mem_flags, dma_addr_t *dma)
{
#ifdef NO_USB_BUFFER_ALLOC
  return usb_alloc_coherent(dev, size, mem_flags, dma);
#else
  return usb_buffer_alloc(dev, size, mem_flags, dma);
#endif  //  #ifdef NO_USB_BUFFER_ALLOC
}
EXPORT_SYMBOL(gpl_usb_buffer_alloc);

void gpl_usb_buffer_free(struct usb_device *dev, size_t size, void *addr,
                                                                dma_addr_t dma)
{
#ifdef NO_USB_BUFFER_ALLOC
  usb_free_coherent(dev, size, addr, dma);
#else
  usb_buffer_free(dev, size, addr, dma);
#endif  //  #ifdef NO_USB_BUFFER_ALLOC
}
EXPORT_SYMBOL(gpl_usb_buffer_free);

int gpl_usb_register_dev(struct usb_interface *intf,
                                        struct usb_class_driver *class_driver)
{
  return usb_register_dev(intf, class_driver);
}
EXPORT_SYMBOL(gpl_usb_register_dev);

void gpl_usb_deregister_dev(struct usb_interface *intf,
                                        struct usb_class_driver *class_driver)
{
  usb_deregister_dev(intf, class_driver);
}
EXPORT_SYMBOL(gpl_usb_deregister_dev);

struct usb_interface *gpl_usb_find_interface(struct usb_driver *drv, int minor)
{
  return usb_find_interface(drv, minor);
}
EXPORT_SYMBOL(gpl_usb_find_interface);

int gpl_usb_autopm_get_interface(struct usb_interface *intf)
{
  return usb_autopm_get_interface(intf);
}
EXPORT_SYMBOL(gpl_usb_autopm_get_interface);

void gpl_usb_autopm_put_interface(struct usb_interface *intf)
{
  usb_autopm_put_interface(intf);
}
EXPORT_SYMBOL(gpl_usb_autopm_put_interface);

int gpl_usb_driver_claim_interface(struct usb_driver *driver,
                                      struct usb_interface *iface, void *priv)
{
  return usb_driver_claim_interface(driver, iface, priv);
}
EXPORT_SYMBOL(gpl_usb_driver_claim_interface);

//------------------------------------------------------------------------------


static int GPL_NetUSB_init(void) {
  printk("<1> GPL NetUSB up!\n");
  return 0;
}

static void GPL_NetUSB_exit(void) {
  printk("<1> GPL NetUSB down!\n");
}

module_init(GPL_NetUSB_init);
module_exit(GPL_NetUSB_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NetUSB module for Linux 2.6 from KCodes.");
MODULE_AUTHOR("KCodes");

