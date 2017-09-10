/******************************************************************************
**
** FILE NAME    : ifxmips_gpio.c
** PROJECT      : IFX UEIP
** MODULES      : GPIO
**
** DATE         : 21 Jun 2004
** AUTHOR       : btxu
** DESCRIPTION  : Global IFX GPIO driver source file
** COPYRIGHT    :       Copyright (c) 2006
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 21 Jun 2004   btxu            Generate from INCA-IP project
** 21 Jun 2005   Jin-Sze.Sow     Comments edited
** 01 Jan 2006   Huang Xiaogang  Modification & verification on Amazon_S chip
** 11 Feb 2008   Lei Chuanhua    Detect GPIO conflicts
** 28 May 2008   Lei Chuanhua    Added forth port support & cleanup source code
** 10 Nov 2008   Lei Chuanhua    Ported to VR9 and use new register defs
** 12 Mar 2009   Lei Chuanhua    Added GPIO/module display
** 22 May 2009   Xu Liang        UEIP
*******************************************************************************/



/*!
  \file ifxmips_gpio.c
  \ingroup IFX_GPIO
  \brief GPIO driver main source file.
*/



/*
 * ####################################
 *              Version No.
 * ####################################
 */

#define IFX_GPIO_VER_MAJOR              1
#define IFX_GPIO_VER_MID                2
#define IFX_GPIO_VER_MINOR              14

/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>

/*
 *  Chip Specific Head File
 */
#include <asm/ifx/ifx_types.h>
#include <asm/ifx/ifx_regs.h>
#include <asm/ifx/common_routines.h>
#include "ifxmips_gpio.h"

/*
 * ####################################
 *              Definition
 * ####################################
 */

#define GPIO_IOCTL_OPR(func_clear, func_set, ptr_parm, user_arg, ret)           do {    \
    ret = copy_from_user((void *)(ptr_parm), (void *)(user_arg), sizeof(*(ptr_parm)));  \
    if ( ret == IFX_SUCCESS ) {                                                         \
        (ptr_parm)->module |= GPIO_PIN_STATUS_APP_MASK;                                 \
        if ( (ptr_parm)->value == 0 )                                                   \
            ret = (func_clear)(IFX_GPIO_PIN_ID((ptr_parm)->port, (ptr_parm)->pin),      \
                             (ptr_parm)->module);                                       \
        else                                                                            \
            ret = (func_set)(IFX_GPIO_PIN_ID((ptr_parm)->port, (ptr_parm)->pin),        \
                           (ptr_parm)->module);                                         \
    }                                                                                   \
} while ( 0 )

#define GPIO_FUNC_PIN_ID_CHECK(pin)                     do {        \
    if ( pin < 0 || pin > g_max_pin_id ) {                          \
        err("Invalid port pin ID %d!", pin);                        \
        return IFX_ERROR;                                           \
    }                                                               \
} while ( 0 )

#define GPIO_FUNC_MODULE_ID_CHECK(module_id)            do {                        \
    if ( (module_id & GPIO_PIN_STATUS_MODULE_ID_MASK) <= IFX_GPIO_MODULE_MIN        \
        || (module_id & GPIO_PIN_STATUS_MODULE_ID_MASK) >= IFX_GPIO_MODULE_MAX ) {  \
        err("Invalid module ID %d!", module_id & GPIO_PIN_STATUS_MODULE_ID_MASK);   \
        return IFX_ERROR;                                                           \
    }                                                                               \
} while ( 0 )

#define GPIO_FUNC_PIN_ID_BREAKDOWN(pin_id, port, pin)   do {        \
    port = IFX_GPIO_PIN_ID_TO_PORT(pin_id);                         \
    pin = IFX_GPIO_PIN_ID_TO_PIN(pin_id);                           \
} while ( 0 )

#define GPIO_FUNC_OPR(x)                                do {        \
    int port;                                                       \
    unsigned long sys_flags;                                        \
                                                                    \
    if ( g_max_pin_id >= 0 ) {                                      \
        GPIO_FUNC_PIN_ID_CHECK(pin);                                \
        GPIO_FUNC_MODULE_ID_CHECK(module_id);                       \
        GPIO_FUNC_PIN_ID_BREAKDOWN(pin, port, pin);                 \
                                                                    \
        spin_lock_irqsave(&g_gpio_lock, sys_flags);                 \
        if ( (g_gpio_port_priv[port].pin_status[pin] & GPIO_PIN_STATUS_MODULE_ID_MASK) == (module_id & GPIO_PIN_STATUS_MODULE_ID_MASK) ) {  \
            { x }                                                   \
            spin_unlock_irqrestore(&g_gpio_lock, sys_flags);        \
            return IFX_SUCCESS;                                     \
        }                                                           \
        spin_unlock_irqrestore(&g_gpio_lock, sys_flags);            \
    }                                                               \
    else {                                                          \
        GPIO_FUNC_PIN_ID_BREAKDOWN(pin, port, pin);                 \
        { x }                                                       \
        return IFX_SUCCESS;                                         \
    }                                                               \
    return IFX_ERROR;                                               \
} while ( 0 )

/*
 * ####################################
 *             Declaration
 * ####################################
 */

/*
 *  File Operations
 */
static int ifx_gpio_open(struct inode *inode, struct file *filep);
static int ifx_gpio_release(struct inode *inode, struct file *filelp);
static int ifx_gpio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

/*
 *  Proc File Functions
 */
static inline void proc_file_create(void);
static inline void proc_file_delete(void);
static int proc_read_version(char *buf, char **start, off_t offset, int count, int *eof, void *data);
static int proc_read_dbg(char *buf, char **start, off_t offset, int count, int *eof, void *data);
static int proc_write_dbg(struct file *file, const char *buf, unsigned long count, void *data);
static int proc_read_setup(char *buf, char **start, off_t offset, int count, int *eof, void *data);
static int proc_read_module(char *buf, char **start, off_t offset, int count, int *eof, void *data);
static int proc_write_module(struct file *file, const char *buf, unsigned long count, void *data);
static int proc_read_board(char *buf, char **start, off_t offset, int count, int *eof, void *data);

/*
 *  Proc File help functions
 */
static inline int stricmp(const char *, const char *);
static inline int strincmp(const char *, const char *, int);
static inline int get_token(char **, char **, int *, int *);
static inline int print_board(char *, struct ifx_gpio_ioctl_pin_config *);

/*
 *  Init Help Functions
 */
static inline int ifx_gpio_version(char *);
static inline void ifx_gpio_early_register(void);

/*
 *  External Variable
 */
extern struct ifx_gpio_ioctl_pin_config g_board_gpio_pin_map[]; //  defined in board specific C file



/*
 * ####################################
 *            Local Variable
 * ####################################
 */

static spinlock_t g_gpio_lock;  //  use spinlock rather than semaphore or mutex
                                //  because most functions run in user context
                                //  and they do not take much time to finish operation

static IFX_GPIO_DECLARE_MODULE_NAME(g_gpio_module_name);

static int g_max_pin_id = -1;   //  console driver may need GPIO driver before it's init

static struct file_operations g_gpio_fops = {
    .open    = ifx_gpio_open,
    .release = ifx_gpio_release,
    .ioctl   = ifx_gpio_ioctl
};

static unsigned int g_dbg_enable = DBG_ENABLE_MASK_ERR;

static struct proc_dir_entry* g_gpio_dir = NULL;


/* added by yangxv for GPIO, 2012.4 */
#define TL_GPIO

#ifdef TL_GPIO

#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/reboot.h>
#include <asm/delay.h>
#include <linux/workqueue.h>

//#define TL_LED_DEBUG
#ifdef TL_LED_DEBUG
#define TL_LEDD(format, arg...) printk(format,## arg)
#else
#define TL_LEDD(format, arg...) do {} while (0)
#endif

/* assume HZ is 100 */
#define DEFAULT_TIME	20

#define	FAST_SEPPD	2	/* DEFAULT_TIME*2 */
#define	MED_SEPPD	5	/* DEFAULT_TIME*5 */
#define	SLOW_SEPPD	10	/* DEFAULT_TIME*10 */

/* For wps gpio INT */
#define WPS_IRQ_NUM	  (32*1+0)

typedef enum
{
	ONCE_BLINK = 0,
	REPEATED_BLINK,
}blink_type;

typedef struct
{
	int gpio;	/* gpio number */
	blink_type blinkt;	/* blink type */
	int onoff;	/* on or off flag */
	int blinking_set;	/* blinking set flag, 1 means set, 0 means not set */
	int blink_speed;
	int blink_count;	/* Internal use, with the blink_speed to control the blink speed */
}tl_gpio_config;

void (*wlan_button_on)(void *) = NULL;
void (*wlan_button_off)(void *) = NULL;
void (*wlan_wps)(void *) = NULL;
void *wlan_wps_dev = NULL;
void *wlan_radio_dev = NULL;


static int reset_gpio 	= 0;
static int wps_gpio		= 39;
static int wlan_gpio	= 9;
static int usb_gpio		= 33;

static int lan3_led	 	= 7;
static int lan4_led 	= 45;

static int usb1_led		= 19;
static int usb2_led		= 20;

static int adsl_led 	= 4;
static int internet_led = 5;
static int wps_led		= 37;


static int l_dsl_status = 0;

struct proc_dir_entry *g_tplink = NULL;
EXPORT_SYMBOL(g_tplink);


USB_LED_HANDLE g_usbLedTbl[TL_MAX_USB_NUM];
unsigned int g_usbLedNum = TL_MAX_USB_NUM;
unsigned int g_hubAddr[TL_MAX_USB_NUM];

EXPORT_SYMBOL(g_usbLedTbl);
EXPORT_SYMBOL(g_usbLedNum);
EXPORT_SYMBOL(g_hubAddr);


/* define in ifxmips_sflash.c */
extern int spiflash_ioctl_erase(u32 to, u32 len);
extern int spiflash_ioctl_write(u_char *buf, u32 to, u32 len);

static void reset_button_func(struct work_struct *work);
static void led_check(unsigned long val);
static irqreturn_t wps_handler(int irq, void *dev_id, struct pt_regs *regs);


static int led_internet_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data);
static int led_internet_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data);

static int led_wps_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data);
static int led_wps_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data);

static int usb_power_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data);
static int usb_power_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data);


static int l_reset_button_stats = 0;
static struct workqueue_struct *l_reset_button_workqueue;
struct delayed_work l_reset_button_work;


static struct timer_list l_led_timer;

static tl_gpio_config l_gconfig[TL_LED_MAX+1] = 
{
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
	{0, ONCE_BLINK, 0, 0, FAST_SEPPD, 0},
};

static void reset_button_func(struct work_struct *work)
{
	int reset_val = 0;
	int wlan_val = 0;
	static int s_last_wlan_val = 0;
	static int s_first_get = 0;
	char buf[128] = {0};

	ifx_gpio_input_get(reset_gpio, IFX_GPIO_MODULE_TEST, &reset_val);
	ifx_gpio_input_get(wlan_gpio, IFX_GPIO_MODULE_TEST, &wlan_val);

	if (0 == s_first_get)
	{
		s_last_wlan_val = wlan_val;
		s_first_get = 1;
	}

	/* reset button is pressed */
	if (0 == reset_val) 
	{		
		l_reset_button_stats++;

		if (l_reset_button_stats > 5)
		{
			/* load factory config when startup through cover with NULL data */
			spiflash_ioctl_erase(0x7c0000, 128);
			spiflash_ioctl_write(buf, 0x7c0000, 128);
			udelay(1000000);
			machine_restart(NULL);
		}
	} 
	else 
	{
		l_reset_button_stats = 0;
	}

	/* wlan button is setup */
	if (wlan_val != s_last_wlan_val)
	{
		if (1 == wlan_val)
		{
			if (wlan_button_on)
			{
				printk("press button on!\n");
				wlan_button_on(wlan_radio_dev);
			}
		}
		else
		{
			if (wlan_button_off)
			{
				printk("press button off!\n");
				wlan_button_off(wlan_radio_dev);
			}			
		}

		s_last_wlan_val = wlan_val;
	}

	queue_delayed_work(l_reset_button_workqueue, &l_reset_button_work, HZ);

	return;
}

static void led_check(unsigned long val)
{
	int i = 0;
	//TL_LEDD("led check \n\n");

	for (i = 0; i < TL_LED_MAX; i++)
	{
		if ((1 == l_gconfig[i].onoff) && (1 == l_gconfig[i].blinking_set))
		{
			TL_LEDD("conut is %d, speed is %d\n", l_gconfig[i].blink_count, l_gconfig[i].blink_speed);
			if (++l_gconfig[i].blink_count >= l_gconfig[i].blink_speed)
			{
				if ((ONCE_BLINK == l_gconfig[i].blinkt) 
						|| (REPEATED_BLINK== l_gconfig[i].blinkt))
				{
					if (l_gconfig[i].blink_count == l_gconfig[i].blink_speed)
					{
						ifx_gpio_output_clear(l_gconfig[i].gpio, IFX_GPIO_MODULE_TEST);
						TL_LEDD("Turn off led\n");
					}
					else if (l_gconfig[i].blink_count == l_gconfig[i].blink_speed + 1)
					{
						ifx_gpio_output_set(l_gconfig[i].gpio, IFX_GPIO_MODULE_TEST);

						if (ONCE_BLINK == l_gconfig[i].blinkt)
						{
							l_gconfig[i].blinking_set = 0;
						}
						l_gconfig[i].blink_count = 0;
						TL_LEDD("Turn on led\n");
					}
					else
					{
						printk("Error led check %d, %d, %d, %d\n", l_gconfig[i].onoff, 
							l_gconfig[i].blinking_set, l_gconfig[i].blink_count, l_gconfig[i].blink_speed);
					}
				}

			}
		}
	}
		
	mod_timer(&l_led_timer, jiffies + 10);

	return;
}


static irqreturn_t wps_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	(*IFX_ICU_EIU_EXIN_C) |= 0x00000000; //disable EXIN3

	printk("wps gpio irq handle\n");
	if (wlan_wps)
	{
		wlan_wps(wlan_wps_dev);
	}

	(*IFX_ICU_EIU_EXIN_C) |= 0x00002000; //set EXIN3 to level triggered	
	
	return IRQ_HANDLED;
}

static int led_internet_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	int len = 0;

	//len = sprintf(page, "%d %d\n", internet_led_on, internet_trying_led_on);

	len -= off;
	*start = page + off;

	if (len > count)
		len = count;
	else
		*eof = 1;

	if (len < 0)
		len = 0;

	return len;
}

static int led_internet_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	char val_string[8];
	int val1, val2;

	if (count > sizeof(val_string) - 1)
		return -EINVAL;

	if (copy_from_user(val_string, buffer, count))
		return -EFAULT;

	val_string[count] = '\0';
	
	if (sscanf(val_string, "%d %d", &val1, &val2) != 2) {

		printk("usage: <internet_led_onoff> <is_internet_trying_led>\n");
		return count;
	}

	if (val1 != 0)
	{
		tl_gpio_turnon(TL_LED_INTERNET);
	}
	else
	{
		tl_gpio_turnoff(TL_LED_INTERNET);
	}

	return count;
}

static int led_wps_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	int len = 0;

	//len = sprintf(page, "%d %d\n", internet_led_on, internet_trying_led_on);

	len -= off;
	*start = page + off;

	if (len > count)
		len = count;
	else
		*eof = 1;

	if (len < 0)
		len = 0;

	return len;
}

static int led_wps_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	char val_string[8] = {0};
	int val = 0;

	if (count > sizeof(val_string) - 1)
		return -EINVAL;

	if (copy_from_user(val_string, buffer, count))
		return -EFAULT;

	val_string[count] = '\0';
	
	if (sscanf(val_string, "%d", &val) != 1) {

		printk("usage: <wps_led_onoff>\n");
		return count;
	}
	
	switch(val)
	{
	case 0:
		tl_gpio_turnoff(TL_LED_WPS);
		break;
		
	case 1:
		tl_gpio_turnon(TL_LED_WPS);
		break;

	case 2:
		tl_gpio_turnon(TL_LED_WPS);
		tl_gpio_blink(TL_LED_WPS, FAST_SEPPD);
		break;
		
	default:
		printk("WPS LED %d unknow value\n", val);
	}

	return count;
}

static int wlan_status_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	int wlan_status = 0;

	ifx_gpio_input_get(wlan_gpio, IFX_GPIO_MODULE_TEST, &wlan_status);
	
	return sprintf(page, "%d\n", wlan_status);
}


/*wps status: 0.WPS_IDLE; 1. WPS_INPROGRESSE; 2.WPS_SUCCESS; 3.WPS_ERROR; 4.WPS_OVERLAP;*/
static int wps_status = 0;
static int wps_status_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{

	return sprintf(page, "%d\n", wps_status);
}

static int wps_status_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	u_int32_t val;	
	
	if (sscanf(buffer, "%d", &val) != 1)		
		return -EINVAL;

	wps_status = val;
	
	return count;
}


static int led_dsl_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(page, "%d\n", l_dsl_status);

	len -= off;
	*start = page + off;

	if (len > count)
		len = count;
	else
		*eof = 1;

	if (len < 0)
		len = 0;

	return len;
}

static int led_dsl_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	char val_string[8] = {0};
	int val = 0;

	if (count > sizeof(val_string) - 1)
		return -EINVAL;

	if (copy_from_user(val_string, buffer, count))
		return -EFAULT;

	val_string[count] = '\0';
	
	if (sscanf(val_string, "%d", &val) != 1) {

		printk("usage: <dsl_led_onoff>\n");
		return count;
	}

	switch(val)
	{
	case 0:
		tl_gpio_turnoff(TL_LED_ADSL);
		tl_gpio_turnoff(TL_LED_INTERNET);
		break;
		
	case 1:
		tl_gpio_turnon(TL_LED_ADSL);
		break;

	case 2:
		tl_gpio_turnon(TL_LED_ADSL);
		tl_gpio_blink(TL_LED_ADSL, MED_SEPPD);
		break;
		
	case 3:
		tl_gpio_turnon(TL_LED_ADSL);
		tl_gpio_blink(TL_LED_ADSL, FAST_SEPPD);
		break;
		
	default:
		printk("DSL LED %d unknow value\n", val);
	}

	l_dsl_status = val;

	return count;
}


void tl_gpio_turnon(int led_index)
{
	if (led_index < 0)
		return;

	if (TL_LED_INTERNET == led_index)
	{
		if (l_dsl_status != 1)
			return;
	}

	TL_LEDD("Turn on %d\n", led_index);
	l_gconfig[led_index].onoff = 1;
	l_gconfig[led_index].blinking_set = 0;
	l_gconfig[led_index].blink_count = 0;
	ifx_gpio_output_set(l_gconfig[led_index].gpio, IFX_GPIO_MODULE_TEST);

	return;
}

void tl_gpio_turnoff(int led_index)
{
	if (led_index < 0)
		return;

	TL_LEDD("Turn off %d\n", led_index);
	l_gconfig[led_index].onoff = 0;
	l_gconfig[led_index].blinking_set = 0;
	l_gconfig[led_index].blink_count = 0;
	ifx_gpio_output_clear(l_gconfig[led_index].gpio, IFX_GPIO_MODULE_TEST);

	return;
}

void tl_gpio_blinking(int led_index)
{
	if (led_index < 0)
		return;

	TL_LEDD("Turn blinking %d\n", led_index);
	l_gconfig[led_index].blinking_set = 1;
	l_gconfig[led_index].blink_speed = FAST_SEPPD;

	return;
}

/* for usb power control, 20120527 */
static int usb_power_read_proc(char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	return 0;
}

static int usb_power_write_proc(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	char val_string[8] = {0};
	int val = 0;

	if (count > sizeof(val_string) - 1)
		return -EINVAL;

	if (copy_from_user(val_string, buffer, count))
		return -EFAULT;

	val_string[count] = '\0';
	
	if (sscanf(val_string, "%d", &val) != 1) {

		printk("usage: 0-->power off, 1-->power on\n");
		return count;
	}

	if ((val < 0) || (val > 1))
	{
		printk("usage: 0-->power off, 1-->power on\n");
		return -EINVAL;
	}

	if (0 == val)
	{
		ifx_gpio_output_clear(usb_gpio, IFX_GPIO_MODULE_USB);
	}
	else
	{
		ifx_gpio_output_set(usb_gpio, IFX_GPIO_MODULE_USB);
	}
	
	return count;
}
/* Add end */

void tl_gpio_blink(int led_index, int blink_speed)
{
	if (led_index < 0)
		return;

	TL_LEDD("Turn blink %d, speed is %d\n", led_index, blink_speed);
	l_gconfig[led_index].blinking_set = 1;
	l_gconfig[led_index].blinkt = REPEATED_BLINK;
	l_gconfig[led_index].blink_speed = blink_speed;
	l_gconfig[led_index].blink_count = 0;

	return;
}
EXPORT_SYMBOL(tl_gpio_blinking);


void wlan_register_button_on(void (*pfun)(void*), void *dev)
{
	printk("Register wlan button on function\n");
	wlan_button_on = pfun;
	wlan_radio_dev = dev;

	return;
}

void wlan_register_button_off(void (*pfun)(void*))
{
	printk("Register wlan button off function\n");
	wlan_button_off = pfun;

	return;
}

void wlan_register_wps(void (*pfun)(void *), void *dev)
{
	printk("Register wlan WPS function, dev=0x%0x\n", (unsigned int)dev);
	
	wlan_wps = pfun;
	wlan_wps_dev = dev;
	return;
}

EXPORT_SYMBOL(wlan_register_button_on);
EXPORT_SYMBOL(wlan_register_button_off);
EXPORT_SYMBOL(wlan_register_wps);


#endif /* TL_GPIO */

/* end added */

/*
 * ####################################
 *            Local Function
 * ####################################
 */


static int ifx_gpio_open(struct inode *inode, struct file *filep)
{
    return IFX_SUCCESS;
}

static int ifx_gpio_release(struct inode *inode, struct file *filelp)
{
    return IFX_SUCCESS;
}

static int ifx_gpio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct ifx_gpio_ioctl_parm parm;

    //  check magic number
    if ( _IOC_TYPE(cmd) != IFX_GPIO_IOC_MAGIC )
        return -EINVAL;

    //  check read/write right
    if ( ((_IOC_DIR(cmd) & _IOC_WRITE) && !access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd)))
        || ((_IOC_DIR (cmd) & _IOC_READ) && !access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd))) )
        return -EACCES;

    switch ( cmd ) {
    case IFX_GPIO_IOC_OD:
        GPIO_IOCTL_OPR(ifx_gpio_open_drain_clear, ifx_gpio_open_drain_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_PUDSEL:
        GPIO_IOCTL_OPR(ifx_gpio_pudsel_clear, ifx_gpio_pudsel_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_PUDEN:
        GPIO_IOCTL_OPR(ifx_gpio_puden_clear, ifx_gpio_puden_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_STOFF:
        GPIO_IOCTL_OPR(ifx_gpio_stoff_clear, ifx_gpio_stoff_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_DIR:
        GPIO_IOCTL_OPR(ifx_gpio_dir_in_set, ifx_gpio_dir_out_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_OUTPUT:
        GPIO_IOCTL_OPR(ifx_gpio_output_clear, ifx_gpio_output_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_ALTSEL0:
        GPIO_IOCTL_OPR(ifx_gpio_altsel0_clear, ifx_gpio_altsel0_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_ALTSEL1:
        GPIO_IOCTL_OPR(ifx_gpio_altsel1_clear, ifx_gpio_altsel1_set, &parm, arg, ret);
        break;
    case IFX_GPIO_IOC_INPUT:
        ret = copy_from_user((void *)&parm, (void *)arg, sizeof(parm));
        if ( ret == IFX_SUCCESS ) {
            parm.module |= GPIO_PIN_STATUS_APP_MASK;
            ret = ifx_gpio_input_get(IFX_GPIO_PIN_ID(parm.port, parm.pin), parm.module, &parm.value);
            if ( ret == IFX_SUCCESS )
                ret = copy_to_user((void *)arg, (void *)&parm, sizeof(parm));
        }
        break;
    case IFX_GPIO_IOC_VERSION:
        {
            struct ifx_gpio_ioctl_version version = {
                .major = IFX_GPIO_VER_MAJOR,
                .mid   = IFX_GPIO_VER_MID,
                .minor = IFX_GPIO_VER_MINOR
            };
            ret = copy_to_user((void *)arg, (void *)&version, sizeof(version));
        }
        break;
    case IFX_GPIO_IOC_REGISTER:
        if ( (int)arg > IFX_GPIO_MODULE_MIN && (int)arg < IFX_GPIO_MODULE_MAX )
            ret = ifx_gpio_register((int)arg | GPIO_PIN_STATUS_APP_MASK);
        else
            ret = IFX_ERROR;
        break;
    case IFX_GPIO_IOC_DEREGISTER:
        if ( (int)arg > IFX_GPIO_MODULE_MIN && (int)arg < IFX_GPIO_MODULE_MAX )
            ret = ifx_gpio_deregister((int)arg | GPIO_PIN_STATUS_APP_MASK);
        else
            ret = IFX_ERROR;
        break;
    case IFX_GPIO_IOC_PIN_RESERVE:
    case IFX_GPIO_IOC_PIN_FREE:
        {
            struct ifx_gpio_ioctl_pin_reserve reserve;

            ret = copy_from_user((void *)&reserve, (void *)arg, sizeof(reserve));
            if ( ret == IFX_SUCCESS ) {
                reserve.module_id |= GPIO_PIN_STATUS_APP_MASK;
                if ( cmd == IFX_GPIO_IOC_PIN_RESERVE )
                    ret = ifx_gpio_pin_reserve(reserve.pin, reserve.module_id);
                else
                    ret = ifx_gpio_pin_free(reserve.pin, reserve.module_id);
            }
        }
        break;
    case IFX_GPIO_IOC_PIN_CONFIG:
        {
            int (*func[])(int, int) = {
                ifx_gpio_open_drain_set,    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET        (1 << 0)
                ifx_gpio_open_drain_clear,  //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_CLEAR      (1 << 1)
                ifx_gpio_pudsel_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET    (1 << 2)
                ifx_gpio_pudsel_clear,      //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_CLEAR  (1 << 3)
                ifx_gpio_puden_set,         //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET     (1 << 4)
                ifx_gpio_puden_clear,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_CLEAR   (1 << 5)
                ifx_gpio_stoff_set,         //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_SET     (1 << 6)
                ifx_gpio_stoff_clear,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_CLEAR   (1 << 7)
                ifx_gpio_dir_out_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT       (1 << 8)
                ifx_gpio_dir_in_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN        (1 << 9)
                ifx_gpio_output_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET    (1 << 10)
                ifx_gpio_output_clear,      //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_CLEAR  (1 << 11)
                ifx_gpio_altsel0_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   (1 << 12)
                ifx_gpio_altsel0_clear,     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR (1 << 13)
                ifx_gpio_altsel1_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET   (1 << 14)
                ifx_gpio_altsel1_clear,     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR (1 << 15)
            };
            struct ifx_gpio_ioctl_pin_config config;
            unsigned int bit;
            int i;

            ret = copy_from_user((void *)&config, (void *)arg, sizeof(config));
            if ( ret == IFX_SUCCESS ) {
                config.module_id |= GPIO_PIN_STATUS_APP_MASK;
                for ( bit = IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET, i = 0; bit <= IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR; bit <<= 1, i++ )
                    if ( (config.config & bit) ) {
                        ret = func[i](config.pin, config.module_id);
                        if ( ret != IFX_SUCCESS )
                            break;
                    }
            }
        }
        break;
    case IFX_GPIO_IOC_MODULE_QUERY:
        {
            struct ifx_gpio_ioctl_module_query query = {0};
            unsigned int i;

            ret = copy_from_user((void *)&query, (void *)arg, sizeof(query));
            if ( ret == IFX_SUCCESS ) {
                if ( query.module_id >= 0 && query.module_id < NUM_ENTITY(g_gpio_port_priv) ) {
                    query.pin_num = g_gpio_port_priv[query.module_id].pin_num;
                    for ( i = 0; i < query.pin_num && i < g_gpio_port_priv[query.module_id].pin_num; i++ )
                        query.pin[i] = g_gpio_port_priv[query.module_id].pin_status[i];
                    ret = copy_to_user((void *)arg, (void *)&query, sizeof(query));
                }
                else
                    ret = IFX_ERROR;
            }
        }
        break;
    case IFX_GPIO_IOC_PIN_QUERY:
        {
            struct ifx_gpio_ioctl_pin_query query = {0};
            int port, pin;

            ret = copy_from_user((void *)&query, (void *)arg, sizeof(query));
            if ( ret == IFX_SUCCESS ) {
                if ( query.pin >= 0 && query.pin <= g_max_pin_id ) {
                    port = IFX_GPIO_PIN_ID_TO_PORT(query.pin);
                    pin  = IFX_GPIO_PIN_ID_TO_PIN(query.pin);
                    query.module_id = g_gpio_port_priv[port].pin_status[pin];
                    ret = ifx_gpio_input_get(query.pin, query.module_id, &query.input);
                    if ( ret == IFX_SUCCESS )
                        ret = copy_to_user((void *)arg, (void *)&query, sizeof(query));
                }
                else
                    ret = IFX_ERROR;
            }
        }
        break;
    default:
        ret = -EOPNOTSUPP;
    }

    return ret;
}

static inline void proc_file_create(void)
{
    struct proc_dir_entry *res;

    g_gpio_dir = proc_mkdir("driver/ifx_gpio", NULL);

    create_proc_read_entry("version",
                            0,
                            g_gpio_dir,
                            proc_read_version,
                            NULL);

    res = create_proc_entry("dbg",
                            0,
                            g_gpio_dir);
    if ( res != NULL ) {
        res->read_proc  = proc_read_dbg;
        res->write_proc = proc_write_dbg;
    }

    create_proc_read_entry("setup",
                            0,
                            g_gpio_dir,
                            proc_read_setup,
                            NULL);

    res = create_proc_entry("module",
                            0,
                            g_gpio_dir);
    if ( res != NULL ) {
        res->read_proc  = proc_read_module;
        res->write_proc = proc_write_module;
    }

    create_proc_read_entry("board",
                            0,
                            g_gpio_dir,
                            proc_read_board,
                            NULL);
}

static inline void proc_file_delete(void)
{
    remove_proc_entry("board", g_gpio_dir);

    remove_proc_entry("module", g_gpio_dir);

    remove_proc_entry("setup", g_gpio_dir);

    remove_proc_entry("dbg", g_gpio_dir);

    remove_proc_entry("version", g_gpio_dir);

    remove_proc_entry("driver/ifx_gpio", NULL);
}

static int proc_read_version(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int len = 0;

    len += ifx_gpio_version(buf + len);

    if ( offset >= len ) {
        *start = buf;
        *eof = 1;
        return 0;
    }
    *start = buf + offset;
    if ( (len -= offset) > count )
        return count;
    *eof = 1;
    return len;
}

static int proc_read_dbg(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int len = 0;

    len += sprintf(buf + len, "error print      - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_ERR)              ? "enabled" : "disabled");
    len += sprintf(buf + len, "debug print      - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_DEBUG_PRINT)      ? "enabled" : "disabled");
    len += sprintf(buf + len, "assert           - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_ASSERT)           ? "enabled" : "disabled");

    if ( offset >= len ) {
        *start = buf;
        *eof = 1;
        return 0;
    }
    *start = buf + offset;
    if ( (len -= offset) > count )
        return count;
    *eof = 1;
    return len;
}

static int proc_write_dbg(struct file *file, const char *buf, unsigned long count, void *data)
{
    static const char *dbg_enable_mask_str[] = {
        " error print",
        " err",
        " debug print",
        " dbg",
        " assert",
        " assert",
        " all"
    };
    static const int dbg_enable_mask_str_len[] = {
        12, 4,
        12, 4,
        7,  7,
        4
    };
    u32 dbg_enable_mask[] = {
        DBG_ENABLE_MASK_ERR,
        DBG_ENABLE_MASK_DEBUG_PRINT,
        DBG_ENABLE_MASK_ASSERT,
        DBG_ENABLE_MASK_ALL
    };

    char str[2048];
    char *p;

    int len, rlen;

    int f_enable = 0;
    int i;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
        return 0;

    if ( strincmp(p, "enable", 6) == 0 ) {
        p += 6;
        f_enable = 1;
    }
    else if ( strincmp(p, "disable", 7) == 0 ) {
        p += 7;
        f_enable = -1;
    }
    else if ( strincmp(p, "help", 4) == 0 || *p == '?' )
        printk("echo <enable/disable> [err/dbg/assert/all] > /proc/driver/ifx_gpio/dbg\n");

    if ( f_enable ) {
        if ( *p == 0 ) {
            if ( f_enable > 0 )
                g_dbg_enable |= DBG_ENABLE_MASK_ALL;
            else
                g_dbg_enable &= ~DBG_ENABLE_MASK_ALL;
        }
        else {
            do {
                for ( i = 0; i < NUM_ENTITY(dbg_enable_mask_str); i++ )
                    if ( strincmp(p, dbg_enable_mask_str[i], dbg_enable_mask_str_len[i]) == 0 ) {
                        if ( f_enable > 0 )
                            g_dbg_enable |= dbg_enable_mask[i >> 1];
                        else
                            g_dbg_enable &= ~dbg_enable_mask[i >> 1];
                        p += dbg_enable_mask_str_len[i];
                        break;
                    }
            } while ( i < NUM_ENTITY(dbg_enable_mask_str) );
        }
    }

    return count;
}

/*
 *  proc_read_setup - Create proc file output
 *  @buf:      Buffer to write the string to
 *  @start:    not used (Linux internal)
 *  @offset:   not used (Linux internal)
 *  @count:    not used (Linux internal)
 *  @eof:      Set to 1 when all data is stored in buffer
 *  @data:     not used (Linux internal)
 *
 *  This function creates the output for the port proc file, by reading
 *  all appropriate registers and displaying the content as a table.
 *
 *      Return Value:
 *  @len = Lenght of data in buffer
 */
static int proc_read_setup(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    static char *gpio_suffix_str[] = {
        "OUT",
        "IN",
        "DIR",
        "ALTSEL0",
        "ALTSEL1",
        "OD",
        "STOFF",
        "PUDSEL",
        "PUDEN",
    };

    int len = 0;

    unsigned long reg_val;
    int i, j, k;

    len += sprintf(buf + len, "\nIFX GPIO Pin Settings\n");
    for ( i = 0; i < NUM_ENTITY(gpio_suffix_str); i++ ) {
        if ( i != 0 )
            len += sprintf(buf + len, "\n~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
        len += sprintf(buf + len, "%-7s: FEDCBA9876543210\n", gpio_suffix_str[i]);
        len += sprintf(buf + len, "-------------------------\n");
        for ( j = 0; j < NUM_ENTITY(g_gpio_port_priv); j++ ) {
            //printk("addr = %08x, %08x\n", (unsigned int)(&g_gpio_port_priv[j].reg.gpio_out + i), (unsigned int)*(&g_gpio_port_priv[j].reg.gpio_out + i));
            if ( *(&g_gpio_port_priv[j].reg.gpio_out + i) )
                reg_val = **(&g_gpio_port_priv[j].reg.gpio_out + i);
            else
                reg_val = 0;
            reg_val &= (1 << g_gpio_port_priv[j].pin_num) - 1;
            len += sprintf(buf + len, "     P%d: ", j);
            for ( k = 15; k >= 0; k-- )
                len += sprintf(buf + len, (reg_val & (1 << k)) ? "X" : " ");
            len += sprintf(buf + len, "\n");
        }
    }

    if ( offset >= len ) {
        *start = buf;
        *eof = 1;
        return 0;
    }
    *start = buf + offset;
    if ( (len -= offset) > count )
        return count;
    *eof = 1;
    return len;
}

/*
 *  proc_read_module - Create proc file output
 *  @buf:      Buffer to write the string to
 *  @start:    not used (Linux internal)
 *  @offset:   not used (Linux internal)
 *  @count:    not used (Linux internal)
 *  @eof:      Set to 1 when all data is stored in buffer
 *  @data:     not used (Linux internal)
 *
 *  This function creates the output for GPIO module usage.
 *
 *      Return Value:
 *  @len = Lenght of data in buffer
 */
static int proc_read_module(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    long len = 0;

    int module_id;
    int i, j;
    char *p;

    len += sprintf(buf + len,       "\nIFX GPIO Pin Usage\n");

    /* The first part */
    len += sprintf(buf + len,       "    0         1         2         3         4         5         6         7\n");
    len += sprintf(buf + len,       "-------------------------------------------------------------------------------\n");
    for ( i = 0; i < NUM_ENTITY(g_gpio_port_priv); i++ ) {
        len += sprintf(buf + len,   "P%d  ", i);
        for ( j = 0; j < 8; j++ ) {
            if ( j < g_gpio_port_priv[i].pin_num ) {
                module_id = g_gpio_port_priv[i].pin_status[j];
                p = g_gpio_module_name[module_id & GPIO_PIN_STATUS_MODULE_ID_MASK];
            }
            else
                p = "N/A";
            len += sprintf(buf + len, "%-10s", p);
        }
        len += sprintf(buf + len, "\n");
    }
    len += sprintf(buf + len,       "\n");
    len += sprintf(buf + len,       "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    len += sprintf(buf + len,       "\n");

    /* The second part */
    len += sprintf(buf + len,       "    8         9         10        11        12        13        14        15\n");
    len += sprintf(buf + len,       "-------------------------------------------------------------------------------\n");
    for ( i = 0; i < NUM_ENTITY(g_gpio_port_priv); i++ ) {
        len += sprintf(buf + len,   "P%d  ", i);
        for ( j = 8; j < 16; j++ ) {
            if ( j < g_gpio_port_priv[i].pin_num ) {
                module_id = g_gpio_port_priv[i].pin_status[j];
                p = g_gpio_module_name[module_id & GPIO_PIN_STATUS_MODULE_ID_MASK];
            }
            else
                p = "N/A";
            len += sprintf(buf + len, "%-10s", p);
        }
        len += sprintf(buf + len, "\n");
    }
    len += sprintf (buf + len, "\n");

    if ( offset >= len ) {
        *start = buf;
        *eof = 1;
        return 0;
    }
    *start = buf + offset;
    if ( (len -= offset) > count )
        return count;
    *eof = 1;
    return len;
}

static int proc_write_module(struct file *file, const char *buf, unsigned long count, void *data)
{
    char local_buf[2048];
    int len;
    char *p1, *p2;
    int colon = 1;

    unsigned int cmd = 0;
    char *str_cmd[] = {"register", "reg", "deregister", "dereg", "unreg"};
#define PROC_GPIO_REGISTER_MODULE       1
#define PROC_GPIO_DEREGISTER_MODULE     2
    unsigned int str_to_cmd_map[] = {PROC_GPIO_REGISTER_MODULE, PROC_GPIO_REGISTER_MODULE, PROC_GPIO_DEREGISTER_MODULE, PROC_GPIO_DEREGISTER_MODULE, PROC_GPIO_DEREGISTER_MODULE};
    int i;

    len = sizeof(local_buf) < count ? sizeof(local_buf) - 1 : count;
    len = len - copy_from_user(local_buf, buf, len);
    local_buf[len] = 0;

    p1 = local_buf;
    while ( get_token(&p1, &p2, &len, &colon) )
    {
        for ( i = 0; i < NUM_ENTITY(str_cmd); i++ )
            if ( stricmp(p1, str_cmd[i]) == 0 ) {
                cmd = str_to_cmd_map[i];
                goto PROC_WRITE_MODULE_CONTINUE_LOOP;
            }

        if ( cmd ) {
            for ( i = IFX_GPIO_MODULE_MIN + 1; i < NUM_ENTITY(g_gpio_module_name); i++ )
                if ( stricmp(p1, g_gpio_module_name[i]) == 0 ) {
                    if ( cmd == PROC_GPIO_REGISTER_MODULE ) {
                        if ( ifx_gpio_register(i | GPIO_PIN_STATUS_APP_MASK) != IFX_SUCCESS )
                            printk("failed in registering module \"%s\"\n", g_gpio_module_name[i]);
                    }
                    else
                        ifx_gpio_deregister(i | GPIO_PIN_STATUS_APP_MASK);
                    break;
                }
            if ( i == NUM_ENTITY(g_gpio_module_name) )
                printk("unkown module name, please read help with command \"echo help > /proc/driver/ifx_gpio/module\"\n");
        }

PROC_WRITE_MODULE_CONTINUE_LOOP:
        p1 = p2;
        colon = 1;
    }

    if ( !cmd ) {
        printk("Usage:\n");
        printk("  echo <register|deregister> <module name> > /proc/driver/ifx_gpio/module\n");
        printk("Module Name List:\n");
        for ( i = IFX_GPIO_MODULE_MIN + 1; i < NUM_ENTITY(g_gpio_module_name); i++ )
            printk("  %s\n", g_gpio_module_name[i]);
    }

    return count;
}

static int proc_read_board(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = offset + count;
    char *pstr;
    char str[1024];
    int llen;

    struct ifx_gpio_ioctl_pin_config *p;
    int item_no;
    int i;

    pstr = *start = buf;

    for ( i = IFX_GPIO_MODULE_MIN + 1; i < IFX_GPIO_MODULE_MAX; i++ ) {
        p = g_board_gpio_pin_map;
        item_no = 0;
        while ( p->module_id != IFX_GPIO_PIN_AVAILABLE ) {
            if ( (p->module_id & GPIO_PIN_STATUS_MODULE_ID_MASK) == i ) {
                llen = item_no++ == 0 ? sprintf(str, "%s\n", g_gpio_module_name[i]) : 0;
                llen += print_board(str + llen, p);
                if ( len <= offset && len + llen > offset ) {
                    memcpy(pstr, str + offset - len, len + llen - offset);
                    pstr += len + llen - offset;
                }
                else if ( len > offset ) {
                    memcpy(pstr, str, llen);
                    pstr += llen;
                }
                len += llen;
                if ( len >= len_max )
                    goto PROC_READ_BOARD_OVERRUN_END;
            }

            p++;
        }
    }

    *eof = 1;

    return len - offset;

PROC_READ_BOARD_OVERRUN_END:

    return len - llen - offset;
}

static inline int stricmp(const char *p1, const char *p2)
{
    int c1, c2;

    while ( *p1 && *p2 )
    {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) )
            return c1;
        p1++;
        p2++;
    }

    return *p1 - *p2;
}

static inline int strincmp(const char *p1, const char *p2, int n)
{
    int c1 = 0, c2;

    while ( n && *p1 && *p2 )
    {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) )
            return c1;
        p1++;
        p2++;
        n--;
    }

    return n ? *p1 - *p2 : c1;
}

static inline int get_token(char **p1, char **p2, int *len, int *colon)
{
    int tlen = 0;

    while ( *len && !((**p1 >= 'A' && **p1 <= 'Z') || (**p1 >= 'a' && **p1<= 'z') || (**p1 >= '0' && **p1<= '9')) )
    {
        (*p1)++;
        (*len)--;
    }
    if ( !*len )
        return 0;

    if ( *colon )
    {
        *colon = 0;
        *p2 = *p1;
        while ( *len && **p2 > ' ' && **p2 != ',' )
        {
            if ( **p2 == ':' )
            {
                *colon = 1;
                break;
            }
            (*p2)++;
            (*len)--;
            tlen++;
        }
        **p2 = 0;
    }
    else
    {
        *p2 = *p1;
        while ( *len && **p2 > ' ' && **p2 != ',' )
        {
            (*p2)++;
            (*len)--;
            tlen++;
        }
        **p2 = 0;
    }

    return tlen;
}

static inline int print_board(char *buf, struct ifx_gpio_ioctl_pin_config *p)
{
    char *str_flag[] = {
        "OD(1)",        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET        (1 << 0)
        "OD(0)",        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_CLEAR      (1 << 1)
        "PUDSEL(1)",    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET    (1 << 2)
        "PUDSEL(0)",    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_CLEAR  (1 << 3)
        "PUDEN(1)",     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET     (1 << 4)
        "PUDEN(0)",     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_CLEAR   (1 << 5)
        "STOFF(1)",     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_SET     (1 << 6)
        "STOFF(0)",     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_CLEAR   (1 << 7)
        "DIROUT",       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT       (1 << 8)
        "DIRIN",        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN        (1 << 9)
        "OUTPUT(1)",    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET    (1 << 10)
        "OUTPUT(0)",    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_CLEAR  (1 << 11)
        "ALTSEL0(1)",   //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   (1 << 12)
        "ALTSEL0(0)",   //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR (1 << 13)
        "ALTSEL1(1)",   //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET   (1 << 14)
        "ALTSEL1(0)",   //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR (1 << 15)
    };
    int len = 0;
    unsigned int config;
    int bit;

    len += sprintf(buf + len, "  %d.%-2d -", IFX_GPIO_PIN_ID_TO_PORT(p->pin), IFX_GPIO_PIN_ID_TO_PIN(p->pin));

    for ( bit = clz(config = p->config); bit >= 0; bit = clz(config ^= 1 << bit) )
        len += sprintf(buf + len, " %s", str_flag[bit]);
    len += sprintf(buf + len, "\n");

    return len;
}

static inline int ifx_gpio_version(char *buf)
{
    return ifx_drv_ver(buf, "GPIO", IFX_GPIO_VER_MAJOR, IFX_GPIO_VER_MID, IFX_GPIO_VER_MINOR);
}

static inline void ifx_gpio_early_register(void)
{
    int i;

    for ( i = IFX_GPIO_MODULE_MIN + 1; i < IFX_GPIO_MODULE_MAX; i++ )
        ifx_gpio_register(i | IFX_GPIO_MODULE_EARLY_REGISTER);
}



/*
 * ####################################
 *           Global Function
 * ####################################
 */

/*!
  \fn       int ifx_gpio_register(int module_id)
  \brief    Register GPIO pins for module ID.

            This function search GPIO configuration table defined in board dependant
            file. Each entry with given module ID will be reserved and configured.

  \param    module_id     ID of module to be registered.
  \return   IFX_SUCCESS   Register successfully.
  \return   IFX_ERROR     Register fail.
  \ingroup  IFX_GPIO_API
 */
int ifx_gpio_register(int module_id)
{
    int (*func[])(int, int) = {
        ifx_gpio_open_drain_set,    //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET        (1 << 0)
        ifx_gpio_open_drain_clear,  //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OD_CLEAR      (1 << 1)
        ifx_gpio_pudsel_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET    (1 << 2)
        ifx_gpio_pudsel_clear,      //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_CLEAR  (1 << 3)
        ifx_gpio_puden_set,         //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET     (1 << 4)
        ifx_gpio_puden_clear,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_CLEAR   (1 << 5)
        ifx_gpio_stoff_set,         //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_SET     (1 << 6)
        ifx_gpio_stoff_clear,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_STOFF_CLEAR   (1 << 7)
        ifx_gpio_dir_out_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT       (1 << 8)
        ifx_gpio_dir_in_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN        (1 << 9)
        ifx_gpio_output_set,        //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET    (1 << 10)
        ifx_gpio_output_clear,      //  #define IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_CLEAR  (1 << 11)
        ifx_gpio_altsel0_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   (1 << 12)
        ifx_gpio_altsel0_clear,     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR (1 << 13)
        ifx_gpio_altsel1_set,       //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET   (1 << 14)
        ifx_gpio_altsel1_clear,     //  #define IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR (1 << 15)
    };
    struct ifx_gpio_ioctl_pin_config *p = g_board_gpio_pin_map;
    int tmp_module_id = module_id & (GPIO_PIN_STATUS_MODULE_ID_MASK | IFX_GPIO_MODULE_EARLY_REGISTER);
    int early_register_mask = (module_id & IFX_GPIO_MODULE_EARLY_REGISTER) ? ~0 : ~IFX_GPIO_MODULE_EARLY_REGISTER;
    int p_module_id;

    int ret;
    unsigned int bit;
    int i;

    while ( p->module_id != IFX_GPIO_PIN_AVAILABLE ) {
        p_module_id = p->module_id & early_register_mask;
        if ( tmp_module_id == p_module_id ) {
			//printk("\n\n\n\nmodule is %d, pin is %d\n", p_module_id, p->pin);
            ret = ifx_gpio_pin_reserve(p->pin, module_id);
            if ( ret != IFX_SUCCESS )
                goto IFX_GPIO_REGISTER_FAIL;
            for ( bit = IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET, i = 0; bit <= IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR; bit <<= 1, i++ )
                if ( (p->config & bit) ) {
					//printk("func i is %d\n", i);
                    ret = func[i](p->pin, module_id);
                    if ( ret != IFX_SUCCESS )
                        goto IFX_GPIO_REGISTER_FAIL;
                }
				//printk("\n\n\n\n");
        }

        p++;
    }

    return IFX_SUCCESS;

IFX_GPIO_REGISTER_FAIL:
    ifx_gpio_deregister(module_id);
    return ret;
}
EXPORT_SYMBOL(ifx_gpio_register);

/*!
  \fn       int ifx_gpio_deregister(int module_id)
  \brief    Free GPIO pins for module ID.

            This function search GPIO configuration table defined in board dependant
            file. Each entry with given module ID will be freed.

  \param    module_id     ID of module to be freed.
  \return   IFX_SUCCESS   Register successfully.
  \return   IFX_ERROR     Register fail.
  \ingroup  IFX_GPIO_API
 */
int ifx_gpio_deregister(int module_id)
{
    int tmp_module_id = module_id & GPIO_PIN_STATUS_MODULE_ID_MASK;
    int i, j;

    for ( i = 0; i < NUM_ENTITY(g_gpio_port_priv); i++ ) {
        for ( j = 0; j < g_gpio_port_priv[i].pin_num; j++ ) {
            if ( tmp_module_id == (g_gpio_port_priv[i].pin_status[j] & GPIO_PIN_STATUS_MODULE_ID_MASK) )
                ifx_gpio_pin_free(IFX_GPIO_PIN_ID(i, j), module_id);
        }
    }

    return IFX_SUCCESS;
}
EXPORT_SYMBOL(ifx_gpio_deregister);

/*!
  \fn       int ifx_gpio_pin_reserve(int pin, int module_id)
  \brief    Reserve pin for usage.

            This function reserves a given pin for usage by the given module.

  \param    pin        Pin to be reserved.
  \param    module_id  Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS   Pin reserved successfully.
  \return   IFX_ERROR     Invalid pin ID or already used.
  \ingroup  IFX_GPIO_API
*/
int ifx_gpio_pin_reserve(int pin, int module_id)
{
    int port;
    unsigned long sys_flags;

    if ( g_max_pin_id >= 0 ) {
        GPIO_FUNC_PIN_ID_CHECK(pin);
        GPIO_FUNC_MODULE_ID_CHECK(module_id);
        GPIO_FUNC_PIN_ID_BREAKDOWN(pin, port, pin);

        spin_lock_irqsave(&g_gpio_lock, sys_flags);
        if ( g_gpio_port_priv[port].pin_status[pin] == IFX_GPIO_PIN_AVAILABLE ) {
            g_gpio_port_priv[port].pin_status[pin] = module_id;
            spin_unlock_irqrestore(&g_gpio_lock, sys_flags);
            return IFX_SUCCESS;
        }
        else
            module_id = g_gpio_port_priv[port].pin_status[pin];
        spin_unlock_irqrestore(&g_gpio_lock, sys_flags);
    }
    else {
        //GPIO_FUNC_PIN_ID_BREAKDOWN(pin, port, pin);
        //g_gpio_port_priv[port].pin_status[pin] = module_id;
        return IFX_SUCCESS;
    }

    err("Pin ID %d (port %d, pin %d) has been reserved by module %s from %s!",
        IFX_GPIO_PIN_ID(port, pin), port, pin,
        g_gpio_module_name[module_id & GPIO_PIN_STATUS_MODULE_ID_MASK],
        (module_id & GPIO_PIN_STATUS_APP_MASK) ? "user level application" : ((module_id & IFX_GPIO_MODULE_EARLY_REGISTER) ? "kernel init" : "kernel module"));

    return IFX_ERROR;
}
EXPORT_SYMBOL(ifx_gpio_pin_reserve);

/*!
  \fn       int ifx_gpio_pin_free(int pin, int module_id)
  \brief    Free pin.

            This function frees a given pin and clears the entry in the usage
            map.

  \param    pin         Pin to be freed.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS   Pin freed successfully.
  \return   IFX_ERROR     Invalid pin ID or not used.
  \ingroup  IFX_GPIO_API
 */
int ifx_gpio_pin_free(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        g_gpio_port_priv[port].pin_status[pin] = IFX_GPIO_PIN_AVAILABLE;
    );
}
EXPORT_SYMBOL(ifx_gpio_pin_free);

/*!
  \fn       int ifx_gpio_open_drain_set(int pin, int module_id)
  \brief    Enable Open Drain for given pin.

            This function enables Open Drain mode for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_open_drain_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_od )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_od);
    );
}
EXPORT_SYMBOL(ifx_gpio_open_drain_set);

/*!
  \fn       int ifx_gpio_open_drain_clear(int pin, int module_id)
  \brief    Disable Open Drain for given pin.

            This function disables Open Drain mode for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_open_drain_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_od )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_od);
    );
}
EXPORT_SYMBOL(ifx_gpio_open_drain_clear);

/*!
  \fn       int ifx_gpio_pudsel_set(int pin, int module_id)
  \brief    Enable Pull-Up resistor for given pin.

            This function enables Pull-Up resistor for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_pudsel_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_pudsel )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_pudsel);
    );
}
EXPORT_SYMBOL(ifx_gpio_pudsel_set);

/*!
  \fn       int ifx_gpio_pudsel_clear(int pin, int module_id)
  \brief    Enable Pull-Down resistor for given pin.

            This function enables Pull-Down resistor for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_pudsel_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_pudsel )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_pudsel);
    );
}
EXPORT_SYMBOL(ifx_gpio_pudsel_clear);

/*!
  \fn       int ifx_gpio_puden_set(int pin, int module_id)
  \brief    Enable Pull-Up/Down feature for given pin.

            This function enables Pull-Up/Down feature for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_puden_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_puden )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_puden);
    );
}
EXPORT_SYMBOL(ifx_gpio_puden_set);

/*!
  \fn       int ifx_gpio_puden_set(int pin, int module_id)
  \brief    Disable Pull-Up/Down feature for given pin.

            This function disables Pull-Up/Down feature for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_puden_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_puden )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_puden);
    );
}
EXPORT_SYMBOL(ifx_gpio_puden_clear);

/*!
  \fn       int ifx_gpio_stoff_set(int pin, int module_id)
  \brief    Enable Schmitt Trigger for given pin.

            This function enables Schmitt Trigger for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_stoff_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_stoff )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_stoff);
    );
}
EXPORT_SYMBOL(ifx_gpio_stoff_set);

/*!
  \fn       int ifx_gpio_stoff_clear(int pin, int module_id)
  \brief    Disable Schmitt Trigger for given pin.

            This function disables Schmitt Trigger for the given pin.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_stoff_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_stoff )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_stoff);
    );
}
EXPORT_SYMBOL(ifx_gpio_stoff_clear);

/*!
  \fn       int ifx_gpio_dir_out_set(int pin, int module_id)
  \brief    Configure given pin for output.

            This function configures the given pin for output.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_dir_out_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_dir )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_dir);
    );
}
EXPORT_SYMBOL(ifx_gpio_dir_out_set);

/*!
  \fn       int ifx_gpio_dir_in_set(int pin, int module_id)
  \brief    Configure given pin for input.

            This function configures the given pin for input.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_dir_in_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_dir )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_dir);
    );
}
EXPORT_SYMBOL(ifx_gpio_dir_in_set);

/*!
  \fn       int ifx_gpio_output_set(int pin, int module_id)
  \brief    Configure given pin to output 1 (high level).

            This function configures the given pin to output 1 (high level).

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_output_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_out )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_out);
    );
}
EXPORT_SYMBOL(ifx_gpio_output_set);

/*!
  \fn       int ifx_gpio_output_clear(int pin, int module_id)
  \brief    Configure given pin to output 0 (low level).

            This function configures the given pin to output 0 (low level).

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_output_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_out )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_out);
    );
}
EXPORT_SYMBOL(ifx_gpio_output_clear);

/*!
  \fn       int ifx_gpio_input_get(int pin, int module_id, int *input)
  \brief    Get input from given pin.

            This function gets input from the given pin.

  \param    pin         Pin to get input.
  \param    module_id   Module ID to identify the owner of a pin.
  \param    input       Pointer to variable to store the input value.

  \return   IFX_SUCCESS Input value got successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_input_get(int pin, int module_id, int *input)
{
    if ( (unsigned int)input < KSEG0 )
        return IFX_ERROR;

    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_in )
            *input = (IFX_REG_R32(g_gpio_port_priv[port].reg.gpio_in) >> pin) & 0x01;
    );
}
EXPORT_SYMBOL(ifx_gpio_input_get);

/*!
  \fn       int ifx_gpio_altsel0_set(int pin, int module_id)
  \brief    Set Alternative Select 0 with 1.

            This function sets Alternative Select 0 of the given pin with 1.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_altsel0_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_altsel0 )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_altsel0);
    );
}
EXPORT_SYMBOL(ifx_gpio_altsel0_set);

/*!
  \fn       int ifx_gpio_altsel0_clear(int pin, int module_id)
  \brief    Clear Alternative Select 0 with 0.

            This function clears Alternative Select 0 of the given pin with 0.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_altsel0_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_altsel0 )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_altsel0);
    );
}
EXPORT_SYMBOL(ifx_gpio_altsel0_clear);

/*!
  \fn       int ifx_gpio_altsel1_set(int pin, int module_id)
  \brief    Set Alternative Select 1 with 1.

            This function sets Alternative Select 0 of the given pin with 1.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_altsel1_set(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_altsel1 )
            IFX_REG_W32_MASK(0, 1 << pin, g_gpio_port_priv[port].reg.gpio_altsel1);
    );
}
EXPORT_SYMBOL (ifx_gpio_altsel1_set);

/*!
  \fn       int ifx_gpio_altsel1_clear(int pin, int module_id)
  \brief    Clear Alternative Select 1 with 0.

            This function clears Alternative Select 0 of the given pin with 0.

  \param    pin         Pin to be configured.
  \param    module_id   Module ID to identify the owner of a pin.

  \return   IFX_SUCCESS Pin configured successfully.
  \return   IFX_ERROR   Invalid pin ID or not used.
  \ingroup IFX_GPIO_API
 */
int ifx_gpio_altsel1_clear(int pin, int module_id)
{
    GPIO_FUNC_OPR(
        if ( g_gpio_port_priv[port].reg.gpio_altsel1 )
            IFX_REG_W32_MASK(1 << pin, 0, g_gpio_port_priv[port].reg.gpio_altsel1);
    );
}
EXPORT_SYMBOL(ifx_gpio_altsel1_clear);



/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

static int __init ifx_gpio_pre_init(void)
{
    int max_port;

    spin_lock_init(&g_gpio_lock);

    max_port = NUM_ENTITY(g_gpio_port_priv);
    g_max_pin_id = (max_port - 1) * IFX_GPIO_PIN_NUMBER_PER_PORT + g_gpio_port_priv[max_port - 1].pin_num - 1;

    return 0;
}

/*
 *  ifx_gpio_init - Initialize port structures
 *
 *  This function initializes the internal data structures of the driver
 *  and will create the proc file entry and device.
 *
 *      Return Value:
 *  @OK = OK
 */
static int __init ifx_gpio_init(void)
{
    int ret;
    char ver_str[128] = {0};
	
	/* added by yangxv for gpio, 2012.4 */
#ifdef TL_GPIO
	int irq_ret = 0;

	struct proc_dir_entry *p_wps_proc = NULL;
	struct proc_dir_entry *p_wlan_status_proc = NULL;
	struct proc_dir_entry *p_wps_status_proc = NULL;
	struct proc_dir_entry *p_internet_proc = NULL;
	struct proc_dir_entry *p_dsl_proc = NULL;
	struct proc_dir_entry *p_usb_power_proc = NULL;
#endif

    /* register port device */
    ret = register_chrdev(IFX_GPIO_MAJOR, "ifx_gpio", &g_gpio_fops);
    if ( ret != 0 ) {
        err("Can not register GPIO device - %d", ret);
        return ret;
    }

    proc_file_create();

    ifx_gpio_early_register();

    ifx_gpio_version(ver_str);
    printk(KERN_INFO "%s", ver_str);

	/* added by yangxv for gpio, 2012.4 */
#ifdef TL_GPIO

	l_gconfig[TL_LED_USB1].gpio = usb1_led;
	l_gconfig[TL_LED_USB2].gpio = usb2_led;
	l_gconfig[TL_LED_ADSL].gpio = adsl_led;
	l_gconfig[TL_LED_INTERNET].gpio = internet_led;
	l_gconfig[TL_LED_WPS].gpio = wps_led;


	/* Init reset button, use GPIO input */
	ifx_gpio_pin_reserve(reset_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_in_set(reset_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(reset_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(reset_gpio, IFX_GPIO_MODULE_TEST);

	/* Init wifi botton, use GPIO input */
	ifx_gpio_pin_reserve(wlan_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_in_set(wlan_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(wlan_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(wlan_gpio, IFX_GPIO_MODULE_TEST);

	/* Init wps button, use INT */
	ifx_gpio_pin_reserve(wps_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(wps_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_in_set(wps_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(wps_gpio, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_set(wps_gpio, IFX_GPIO_MODULE_TEST);

	/* Init LAN led */
	ifx_gpio_pin_reserve(lan3_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(lan3_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(lan3_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_set(lan3_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_set(lan3_led, IFX_GPIO_MODULE_TEST);

	ifx_gpio_pin_reserve(lan4_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(lan4_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(lan4_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(lan4_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_set(lan4_led, IFX_GPIO_MODULE_TEST);

	/* Init USB led */
	ifx_gpio_pin_reserve(usb1_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(usb1_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(usb1_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(usb1_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(usb1_led, IFX_GPIO_MODULE_TEST);	

	ifx_gpio_pin_reserve(usb2_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(usb2_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(usb2_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(usb2_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(usb2_led, IFX_GPIO_MODULE_TEST);

	/* init ADSL led */
	ifx_gpio_pin_reserve(adsl_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(adsl_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(adsl_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(adsl_led, IFX_GPIO_MODULE_TEST);

	/* init Internet led */
	ifx_gpio_pin_reserve(internet_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(internet_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(internet_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(internet_led, IFX_GPIO_MODULE_TEST);

	/* Init WPS led */
	ifx_gpio_pin_reserve(wps_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_open_drain_set(wps_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_dir_out_set(wps_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel0_clear(wps_led, IFX_GPIO_MODULE_TEST);
	ifx_gpio_altsel1_clear(wps_led, IFX_GPIO_MODULE_TEST);


	g_usbLedTbl[0].arg = TL_LED_USB1;
	g_usbLedTbl[0].fpLetOn = tl_gpio_turnon;
	g_usbLedTbl[0].fpLetOff = tl_gpio_turnoff;
	g_usbLedTbl[0].fpLetBlink = tl_gpio_blinking;

	g_usbLedTbl[1].arg = TL_LED_USB2;
	g_usbLedTbl[1].fpLetOn = tl_gpio_turnon;
	g_usbLedTbl[1].fpLetOff = tl_gpio_turnoff;
	g_usbLedTbl[1].fpLetBlink = tl_gpio_blinking;

	/* init led check timer for led blink */
	init_timer(&l_led_timer);
	l_led_timer.expires = jiffies + (HZ * 5);
	l_led_timer.function = led_check;
	l_led_timer.data = 0x0;
	add_timer(&l_led_timer);

	/* reset button workqueue */
	l_reset_button_workqueue = create_singlethread_workqueue("reset");

	if (NULL == l_reset_button_workqueue) {
		printk(KERN_ERR "%s couldn't create workqueue\n", "reset");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&l_reset_button_work, reset_button_func);

	queue_delayed_work(l_reset_button_workqueue, &l_reset_button_work, (HZ * 5));

	/* register WPS gpio INT 
	 * before set, IFX_ICU_EIU_EXIN_C & IFX_ICU_EIU_INEN both are 0
	 */
	(*IFX_ICU_EIU_EXIN_C) |= 0x00002000; //set EXIN3 to level triggered	
	(*IFX_ICU_EIU_INEN) |= 0x00000008; //enable EXIN3

	irq_ret = request_irq(WPS_IRQ_NUM, (void*)wps_handler, IRQF_DISABLED, "wps_gpio", NULL);
	if (irq_ret < 0) 
	{
		printk(KERN_WARNING "GPIO: unable to get irq %d\n", WPS_IRQ_NUM);
	}

	/* register WPS&Internet LED proc */
	g_tplink = proc_mkdir("tplink", NULL);
	
	if (g_tplink)
	{
		p_wps_proc = create_proc_entry("led_wps", 0, g_tplink);
		if (p_wps_proc)
		{
			p_wps_proc->read_proc  = led_wps_read_proc;
			p_wps_proc->write_proc = led_wps_write_proc;
		}

		p_internet_proc = create_proc_entry("led_internet", 0, g_tplink);
		if (p_internet_proc)
		{
			p_internet_proc->read_proc  = led_internet_read_proc;
			p_internet_proc->write_proc = led_internet_write_proc;
		}

		p_dsl_proc = create_proc_entry("led_dsl", 0, g_tplink);
		if (p_dsl_proc)
		{
			p_dsl_proc->read_proc  = led_dsl_read_proc;
			p_dsl_proc->write_proc = led_dsl_write_proc;
		}

		p_wlan_status_proc =create_proc_entry("wlan_status", 0, g_tplink);
		if (p_wlan_status_proc)
		{
			p_wlan_status_proc->read_proc  = wlan_status_read_proc;
			p_wlan_status_proc->write_proc = NULL;			
		}

		p_wps_status_proc =create_proc_entry("wps_status", 0, g_tplink);
		if (p_wps_status_proc)
		{
			p_wps_status_proc->read_proc  = wps_status_read_proc;
			p_wps_status_proc->write_proc = wps_status_write_proc;			
		}

		p_usb_power_proc = create_proc_entry("usb_power", 0, g_tplink);
		if (p_usb_power_proc)
		{
			p_usb_power_proc->read_proc  = usb_power_read_proc;
			p_usb_power_proc->write_proc = usb_power_write_proc;
		}
	}
	else
	{
		printk("Get proc directory error\n");
	}

#endif /* TL_GPIO */

    return IFX_SUCCESS;
}

static void __exit ifx_gpio_exit(void)
{
    proc_file_delete();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    if ( unregister_chrdev(IFX_GPIO_MAJOR, "ifx_gpio") ) {
        err("Can not unregister GPIO device (major %d)!", IFX_GPIO_MAJOR);
        return;
    }
#else
    unregister_chrdev(IFX_GPIO_MAJOR, "ifx_gpio");
#endif

		/* added by yangxv for gpio, 2012.4 */
#ifdef TL_GPIO
	flush_workqueue(l_reset_button_workqueue);
	destroy_workqueue(l_reset_button_workqueue);

	del_timer_sync(&l_led_timer);
#endif /* TL_GPIO */
}
/* To be called earlier than other drivers as well as kernel modules */
postcore_initcall(ifx_gpio_pre_init);
module_init(ifx_gpio_init);
module_exit(ifx_gpio_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Bing-Tao.Xu@infineon.com");
MODULE_SUPPORTED_DEVICE ("Infineon IFX CPE Reference Board");
MODULE_DESCRIPTION ("Infineon technologies GPIO device driver");

