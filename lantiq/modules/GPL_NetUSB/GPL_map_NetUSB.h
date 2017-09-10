#ifndef 	GPL_MAP_NETUSB_H
#define GPL_MAP_NETUSB_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/usb.h>

extern int gpl_usb_clear_halt(struct usb_device *dev, int pipe);
extern int gpl_usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request,
		    __u8 requesttype, __u16 value, __u16 index, void *data,
		    __u16 size, int timeout);
extern int gpl_usb_set_interface(struct usb_device *dev, int interface, int alternate);
extern int gpl_usb_string(struct usb_device *dev, int index, char *buf, size_t size);
extern struct urb *gpl_usb_alloc_urb(int iso_packets, gfp_t mem_flags);
extern void gpl_usb_free_urb(struct urb *urb);
extern int gpl_usb_submit_urb(struct urb *urb, gfp_t mem_flags);
extern int gpl_usb_unlink_urb(struct urb *urb);
extern int gpl_usb_get_current_frame_number(struct usb_device *dev);
extern int gpl_usb_register(struct usb_driver *driver);
extern void gpl_usb_deregister(struct usb_driver *driver);
extern struct usb_host_interface *gpl_usb_altnum_to_altsetting(const struct usb_interface *intf,
                            unsigned int altnum);
extern struct usb_device *gpl_usb_get_dev(struct usb_device *dev);
extern void gpl_usb_put_dev(struct usb_device *dev);
extern struct usb_interface *gpl_usb_ifnum_to_if(const struct usb_device *dev,
                      unsigned ifnum);
extern int gpl_dev_dbg(const struct device *dev, const char *fmt, ...);
extern int gpl_dev_set_name(struct device *dev, const char *fmt, ...);

extern void gpl_klist_remove(struct klist_node * n);
extern void gpl_klist_add_head(struct klist_node * n, struct klist * k);

extern int gpl_usb_get_descriptor(struct usb_device *dev, unsigned char type,
                                    unsigned char index, void *buf, int size);

extern int gpl_usb_reset_device(struct usb_device *udev);

extern void gpl_usb_kill_urb(struct urb *urb);

extern int gpl_usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
                        void *data, int len, int *actual_length, int timeout);

extern void *gpl_usb_buffer_alloc(struct usb_device *dev, size_t size, 
                                            gfp_t mem_flags, dma_addr_t *dma);
extern void gpl_usb_buffer_free(struct usb_device *dev, size_t size, 
                                                  void *addr, dma_addr_t dma);

extern int gpl_usb_register_dev(struct usb_interface *intf,
                                        struct usb_class_driver *class_driver);
extern void gpl_usb_deregister_dev(struct usb_interface *intf,
                                        struct usb_class_driver *class_driver);

extern struct usb_interface *gpl_usb_find_interface(struct usb_driver *drv, 
                                                                    int minor);

extern int gpl_usb_autopm_get_interface(struct usb_interface *intf);
extern void gpl_usb_autopm_put_interface(struct usb_interface *intf);

extern int gpl_usb_driver_claim_interface(struct usb_driver *driver,
                                      struct usb_interface *iface, void *priv);

#endif 		//#ifdef 	GPL_MAP_NETUSB_H
