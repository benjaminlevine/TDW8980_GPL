/*****************************************************************************
 **   FILE NAME       : ifxusb_cif.h
 **   PROJECT         : IFX USB sub-system V3
 **   MODULES         : IFX USB sub-system Host and Device driver
 **   SRC VERSION     : 3.2
 **   DATE            : 1/Jan/2011
 **   AUTHOR          : Chen, Howard
 **   DESCRIPTION     : The Core Interface provides basic services for accessing and
 **                     managing the IFX USB hardware. These services are used by both the
 **                     Host Controller Driver and the Peripheral Controller Driver.
 **   FUNCTIONS       :
 **   COMPILER        : gcc
 **   REFERENCE       : Synopsys DWC-OTG Driver 2.7
 **   COPYRIGHT       :  Copyright (c) 2010
 **                      LANTIQ DEUTSCHLAND GMBH,
 **                      Am Campeon 3, 85579 Neubiberg, Germany
 **
 **    This program is free software; you can redistribute it and/or modify
 **    it under the terms of the GNU General Public License as published by
 **    the Free Software Foundation; either version 2 of the License, or
 **    (at your option) any later version.
 **
 **  Version Control Section  **
 **   $Author$
 **   $Date$
 **   $Revisions$
 **   $Log$       Revision history
 *****************************************************************************/

/*
 * This file contains code fragments from Synopsys HS OTG Linux Software Driver.
 * For this code the following notice is applicable:
 *
 * ==========================================================================
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */

/*!
 \defgroup IFXUSB_DRIVER_V3 IFX USB SS Project
 \brief IFX USB subsystem V3.x
 */

/*!
 \defgroup IFXUSB_CIF Core Interface APIs
 \ingroup IFXUSB_DRIVER_V3
 \brief The Core Interface provides basic services for accessing and
        managing the IFXUSB hardware. These services are used by both the
        Host Controller Driver and the Peripheral Controller Driver.
 */


/*!
 \file ifxusb_cif.h
 \ingroup IFXUSB_DRIVER_V3
 \brief This file contains the interface to the IFX USB Core.
 */

#if !defined(__IFXUSB_CIF_H__)
#define __IFXUSB_CIF_H__

#include <linux/workqueue.h>

#include <linux/version.h>
#include <asm/param.h>

#include "ifxusb_plat.h"
#include "ifxusb_regs.h"

#ifdef __DEBUG__
	#include "linux/timer.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define IFXUSB_PARAM_SPEED_HIGH 0 /*!< Build stage parameter: High Speed */
#define IFXUSB_PARAM_SPEED_FULL 1 /*!< Build stage parameter: Full Speed */

#define IFXUSB_EP_SPEED_LOW     0 /*!< Run-Time Status: High Speed */
#define IFXUSB_EP_SPEED_FULL    1 /*!< Run-Time Status: Full Speed */
#define IFXUSB_EP_SPEED_HIGH    2 /*!< Run-Time Status: Low Speed */

#define IFXUSB_EP_TYPE_CTRL     0 /*!< Run-Time Status: CTRL */
#define IFXUSB_EP_TYPE_ISOC     1 /*!< Run-Time Status: ISOC */
#define IFXUSB_EP_TYPE_BULK     2 /*!< Run-Time Status: BULK */
#define IFXUSB_EP_TYPE_INTR     3 /*!< Run-Time Status: INTR */

#define IFXUSB_HC_PID_DATA0     0     /*!< Run-Time Data Toggle: Data 0 */
#define IFXUSB_HC_PID_DATA2     1     /*!< Run-Time Data Toggle: Data 2 */
#define IFXUSB_HC_PID_DATA1     2     /*!< Run-Time Data Toggle: Data 1 */
#define IFXUSB_HC_PID_MDATA     3     /*!< Run-Time Data Toggle: MData */
#define IFXUSB_HC_PID_SETUP     3     /*!< Run-Time Data Toggle: Setup */


/*!
 \addtogroup IFXUSB_CIF
 */
/*@{*/

/*! typedef ifxusb_params_t
 \brief IFXUSB Parameters structure.
       This structure is used for both importing from insmod stage and run-time storage.
       These parameters define how the IFXUSB controller should be configured.
 */
typedef struct ifxusb_params
{
	int32_t dma_burst_size;  /*!< The DMA Burst size (applicable only for Internal DMA
	                              Mode). 0(for single), 1(incr), 4(incr4), 8(incr8) 16(incr16)
	                          */
	                         /* Translate this to GAHBCFG values */
	int32_t speed;           /*!< Specifies the maximum speed of operation in host and device mode.
	                              The actual speed depends on the speed of the attached device and
	                              the value of phy_type. The actual speed depends on the speed of the
	                              attached device.
	                              0 - High Speed (default)
	                              1 - Full Speed
                              */

	int32_t data_fifo_size;   /*!< Total number of dwords in the data FIFO memory. This
	                               memory includes the Rx FIFO, non-periodic Tx FIFO, and periodic
	                               Tx FIFOs.
	                               32 to 32768
	                           */
	#ifdef __IS_DEVICE__
		int32_t rx_fifo_size; /*!< Number of dwords in the Rx FIFO in device mode.
		                           16 to 32768
		                       */


		int32_t tx_fifo_size[MAX_EPS_CHANNELS]; /*!< Number of dwords in each of the Tx FIFOs in device mode.
		                                             4 to 768
		                                         */
		#ifdef __DED_FIFO__
			int32_t thr_ctl;        /*!< Threshold control on/off */
			int32_t tx_thr_length;  /*!< Threshold length for Tx */
			int32_t rx_thr_length;  /*!< Threshold length for Rx*/
		#endif
	#else //__IS_HOST__
		int32_t host_channels;      /*!< The number of host channel registers to use.
		                                 1 to 16
		                             */

		int32_t rx_fifo_size;       /*!< Number of dwords in the Rx FIFO in host mode.
		                                16 to 32768
		                             */

		int32_t nperio_tx_fifo_size;/*!< Number of dwords in the non-periodic Tx FIFO in host mode.
		                                 16 to 32768
		                             */

		int32_t perio_tx_fifo_size; /*!< Number of dwords in the host periodic Tx FIFO.
		                                 16 to 32768
		                             */
	#endif //__IS_HOST__

	int32_t max_transfer_size;      /*!< The maximum transfer size supported in bytes.
	                                     2047 to 65,535
	                                 */

	int32_t max_packet_count;       /*!< The maximum number of packets in a transfer.
	                                     15 to 511  (default 511)
	                                 */
	int32_t phy_utmi_width;         /*!< Specifies the UTMI+ Data Width.
	                                     8 or 16 bits (default 16)
	                                 */

	#ifdef __IS_DEVICE__
	int32_t turn_around_time_hs;    /*!< Specifies the Turn-Around time at HS*/
	int32_t turn_around_time_fs;    /*!< Specifies the Turn-Around time at FS*/
	#endif

	int32_t timeout_cal;            /*!< Specifies the Timeout_Calibration*/

	#if defined(__WITH_OC_HY__)
		int32_t oc_hy;
	#endif
} ifxusb_params_t;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*! typedef ifxusb_core_if_t
 \brief The ifx_core_if structure contains information needed to manage
       the IFX USB controller acting in either host or device mode. It
       represents the programming view of the controller as a whole.
 */
typedef struct ifxusb_core_if
{
	ifxusb_params_t      params;  /*!< Run-time Parameters */

	uint8_t  core_no;             /*!< core number (used as id when multi-core case */
	char    *core_name;           /*!< core name used for registration and informative purpose*/
	int      irq;                 /*!< irq number this core is hooked */

	/*****************************************************************
	 * Structures and pointers to physical register interface.
	 *****************************************************************/
	/** Core Global registers starting at offset 000h. */
	ifxusb_core_global_regs_t *core_global_regs;  /*!< pointer to Core Global Registers, offset at 000h */

	/** Host-specific registers */
	#ifdef __IS_HOST__
		/** Host Global Registers starting at offset 400h.*/
		ifxusb_host_global_regs_t *host_global_regs; /*!< pointer to Host Global Registers, offset at 400h */
			#define IFXUSB_HOST_GLOBAL_REG_OFFSET 0x400
		/** Host Port 0 Control and Status Register */
		volatile uint32_t *hprt0;                    /*!< pointer to HPRT0 Registers, offset at 440h */
			#define IFXUSB_HOST_PORT_REGS_OFFSET 0x440
		/** Host Channel Specific Registers at offsets 500h-5FCh. */
		ifxusb_hc_regs_t *hc_regs[MAX_EPS_CHANNELS]; /*!< pointer to Host-Channel n Registers, offset at 500h */
			#define IFXUSB_HOST_CHAN_REGS_OFFSET 0x500
			#define IFXUSB_CHAN_REGS_OFFSET 0x20
	#endif

	/** Device-specific registers */
	#ifdef __IS_DEVICE__
		/** Device Global Registers starting at offset 800h */
		ifxusb_device_global_regs_t *dev_global_regs; /*!< pointer to Device Global Registers, offset at 800h */
			#define IFXUSB_DEV_GLOBAL_REG_OFFSET 0x800

		/** Device Logical IN Endpoint-Specific Registers 900h-AFCh */
		ifxusb_dev_in_ep_regs_t     *in_ep_regs[MAX_EPS_CHANNELS]; /*!< pointer to Device IN-EP Registers, offset at 900h */
			#define IFXUSB_DEV_IN_EP_REG_OFFSET 0x900
			#define IFXUSB_EP_REG_OFFSET 0x20
		/** Device Logical OUT Endpoint-Specific Registers B00h-CFCh */
		ifxusb_dev_out_ep_regs_t    *out_ep_regs[MAX_EPS_CHANNELS];/*!< pointer to Device OUT-EP Registers, offset at 900h */
			#define IFXUSB_DEV_OUT_EP_REG_OFFSET 0xB00
	#endif

	/** Power and Clock Gating Control Register */
	volatile uint32_t *pcgcctl;                                    /*!< pointer to Power and Clock Gating Control Registers, offset at E00h */
		#define IFXUSB_PCGCCTL_OFFSET 0xE00

	/** Push/pop addresses for endpoints or host channels.*/
	uint32_t *data_fifo[MAX_EPS_CHANNELS];    /*!< pointer to FIFO access windows, offset at 1000h */
		#define IFXUSB_DATA_FIFO_OFFSET 0x1000
		#define IFXUSB_DATA_FIFO_SIZE   0x1000

	uint32_t *data_fifo_dbg;                 /*!< pointer to FIFO debug windows, offset at 1000h */

	/** Hardware Configuration -- stored here for convenience.*/
	hwcfg1_data_t hwcfg1;  /*!< preserved Hardware Configuration 1 */
	hwcfg2_data_t hwcfg2;  /*!< preserved Hardware Configuration 2 */
	hwcfg3_data_t hwcfg3;  /*!< preserved Hardware Configuration 3 */
	hwcfg4_data_t hwcfg4;  /*!< preserved Hardware Configuration 3 */
	uint32_t      snpsid;  /*!< preserved SNPSID */

	/*****************************************************************
	 * Run-time informations.
	 *****************************************************************/
	/* Set to 1 if the core PHY interface bits in USBCFG have been  initialized. */
	unsigned phy_init_done     : 1 ;/*!< indicated PHY is initialized. */
	unsigned issuspended       : 1 ;

	#ifdef __IS_HOST__
		uint8_t queuing_high_bandwidth; /*!< Host mode, Queueing High Bandwidth. */
	#endif

	#if defined(__UNALIGNED_BUF_ADJ__) || defined(__UNALIGNED_BUF_CHK__)
		uint32_t unaligned_mask;
	#endif
} ifxusb_core_if_t;

/*@}*//*IFXUSB_CIF*/


/*!
 \fn    void *ifxusb_alloc_buf(size_t size, int clear)
 \brief This function is called to allocate buffer of specified size.
        The allocated buffer is mapped into DMA accessable address.
 \param    size Size in BYTE to be allocated
 \param    clear 0: don't do clear after buffer allocated, other: do clear to zero
 \return   0/NULL: Fail; uncached pointer of allocated buffer
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void *ifxusb_alloc_buf_h(size_t size, int clear);
#else
extern void *ifxusb_alloc_buf_d(size_t size, int clear);
#endif


/*!
 \fn    void ifxusb_free_buf(void *vaddr)
 \brief This function is called to free allocated buffer.
 \param vaddr the uncached pointer of the buffer
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_free_buf_h(void *vaddr);
#else
extern void ifxusb_free_buf_d(void *vaddr);
#endif

/*!
 \fn    int ifxusb_core_if_init(ifxusb_core_if_t *_core_if,
                        int               _irq,
                        uint32_t          _reg_base_addr,
                        uint32_t          _fifo_base_addr,
                        uint32_t          _fifo_dbg_addr)
 \brief This function is called to initialize the IFXUSB CSR data
        structures.  The register addresses in the device and host
        structures are initialized from the base address supplied by the
        caller.  The calling function must make the OS calls to get the
        base address of the IFXUSB controller registers.
 \param _core_if        Pointer of core_if structure
 \param _irq            irq number
 \param _reg_base_addr  Base address of IFXUSB core registers
 \param _fifo_base_addr Fifo base address
 \param _fifo_dbg_addr  Fifo debug address
 \return 0: success;
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern int ifxusb_core_if_init_h(ifxusb_core_if_t *_core_if,
#else
extern int ifxusb_core_if_init_d(ifxusb_core_if_t *_core_if,
#endif
                        int               _irq,
                        uint32_t          _reg_base_addr,
                        uint32_t          _fifo_base_addr,
                        uint32_t          _fifo_dbg_addr);


/*!
 \fn    void ifxusb_core_if_remove(ifxusb_core_if_t *_core_if)
 \brief This function free the mapped address in the IFXUSB CSR data structures.
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_core_if_remove_h(ifxusb_core_if_t *_core_if);
#else
extern void ifxusb_core_if_remove_d(ifxusb_core_if_t *_core_if);
#endif

/*!
 \fn    void ifxusb_enable_global_interrupts( ifxusb_core_if_t *_core_if )
 \brief This function enbles the controller's Global Interrupt in the AHB Config register.
 \param _core_if Pointer of core_if structure
 */
#ifdef __IS_HOST__
extern void ifxusb_enable_global_interrupts_h( ifxusb_core_if_t *_core_if );
#else
extern void ifxusb_enable_global_interrupts_d( ifxusb_core_if_t *_core_if );
#endif

/*!
 \fn    void ifxusb_disable_global_interrupts( ifxusb_core_if_t *_core_if )
 \brief This function disables the controller's Global Interrupt in the AHB Config register.
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_disable_global_interrupts_h( ifxusb_core_if_t *_core_if );
#else
extern void ifxusb_disable_global_interrupts_d( ifxusb_core_if_t *_core_if );
#endif

/*!
 \fn    void ifxusb_flush_tx_fifo( ifxusb_core_if_t *_core_if, const int _num )
 \brief Flush a Tx FIFO.
 \param _core_if Pointer of core_if structure
 \param _num Tx FIFO to flush. ( 0x10 for ALL TX FIFO )
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_flush_tx_fifo_h( ifxusb_core_if_t *_core_if, const int _num );
#else
extern void ifxusb_flush_tx_fifo_d( ifxusb_core_if_t *_core_if, const int _num );
#endif

/*!
 \fn    void ifxusb_flush_rx_fifo( ifxusb_core_if_t *_core_if )
 \brief Flush Rx FIFO.
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_flush_rx_fifo_h( ifxusb_core_if_t *_core_if );
#else
extern void ifxusb_flush_rx_fifo_d( ifxusb_core_if_t *_core_if );
#endif

/*!
 \fn    void ifxusb_flush_both_fifo( ifxusb_core_if_t *_core_if )
 \brief Flush ALL Rx and Tx FIFO.
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern void ifxusb_flush_both_fifo_h( ifxusb_core_if_t *_core_if );
#else
extern void ifxusb_flush_both_fifo_d( ifxusb_core_if_t *_core_if );
#endif


/*!
 \fn    int ifxusb_core_soft_reset(ifxusb_core_if_t *_core_if)
 \brief Do core a soft reset of the core.  Be careful with this because it
        resets all the internal state machines of the core.
 \param    _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
extern int ifxusb_core_soft_reset_h(ifxusb_core_if_t *_core_if);
#else
extern int ifxusb_core_soft_reset_d(ifxusb_core_if_t *_core_if);
#endif


/*!
 \brief Turn on the USB Core Power
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
*/
#ifdef __IS_HOST__
	extern void ifxusb_power_on_h (ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_power_on_d (ifxusb_core_if_t *_core_if);
#endif

/*!
 \fn    void ifxusb_power_off (ifxusb_core_if_t *_core_if)
 \brief Turn off the USB Core Power
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
*/
#ifdef __IS_HOST__
	extern void ifxusb_power_off_h (ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_power_off_d (ifxusb_core_if_t *_core_if);
#endif

/*!
 \fn    void ifxusb_phy_power_on (ifxusb_core_if_t *_core_if)
 \brief Turn on the USB PHY Power
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
*/
#ifdef __IS_HOST__
	extern void ifxusb_phy_power_on_h (ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_phy_power_on_d (ifxusb_core_if_t *_core_if);
#endif


/*!
 \fn    void ifxusb_phy_power_off (ifxusb_core_if_t *_core_if)
 \brief Turn off the USB PHY Power
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
*/
#ifdef __IS_HOST__
	extern void ifxusb_phy_power_off_h (ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_phy_power_off_d (ifxusb_core_if_t *_core_if);
#endif

/*!
 \fn    void ifxusb_hard_reset(ifxusb_core_if_t *_core_if)
 \brief Reset on the USB Core RCU
 \param _core_if Pointer of core_if structure
 \ingroup  IFXUSB_CIF
 */
#ifdef __IS_HOST__
	extern void ifxusb_hard_reset_h(ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_hard_reset_d(ifxusb_core_if_t *_core_if);
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef __IS_HOST__
	/*!
	 \fn    void ifxusb_host_core_init(ifxusb_core_if_t *_core_if, ifxusb_params_t  *_params)
	 \brief This function initializes the IFXUSB controller registers for  Host mode.
	        This function flushes the Tx and Rx FIFOs and it flushes any entries in the
	        request queues.
	 \param _core_if        Pointer of core_if structure
	 \param _params         parameters to be set
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_host_core_init(ifxusb_core_if_t *_core_if, ifxusb_params_t  *_params);

	/*!
	 \fn    void ifxusb_host_enable_interrupts(ifxusb_core_if_t *_core_if)
	 \brief This function enables the Host mode interrupts.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_host_enable_interrupts(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_host_disable_interrupts(ifxusb_core_if_t *_core_if)
	 \brief This function disables the Host mode interrupts.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_host_disable_interrupts(ifxusb_core_if_t *_core_if);

	#if defined(__IS_TWINPASS__)
		extern void ifxusb_enable_afe_oc(void);
	#endif

	/*!
	 \fn    void ifxusb_vbus_init(ifxusb_core_if_t *_core_if)
	 \brief This function init the VBUS control.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_vbus_init(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_vbus_free(ifxusb_core_if_t *_core_if)
	 \brief This function free the VBUS control.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_vbus_free(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_vbus_on(ifxusb_core_if_t *_core_if)
	 \brief Turn on the USB 5V VBus Power
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_vbus_on(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_vbus_off(ifxusb_core_if_t *_core_if)
	 \brief Turn off the USB 5V VBus Power
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_vbus_off(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    int ifxusb_vbus(ifxusb_core_if_t *_core_if)
	 \brief Read Current VBus status
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern int ifxusb_vbus(ifxusb_core_if_t *_core_if);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef __IS_DEVICE__
	/*!
	 \fn    void ifxusb_dev_enable_interrupts(ifxusb_core_if_t *_core_if)
	 \brief This function enables the Device mode interrupts.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_dev_enable_interrupts(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    uint32_t ifxusb_dev_get_frame_number(ifxusb_core_if_t *_core_if)
	 \brief Gets the current USB frame number. This is the frame number from the last SOF packet.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern uint32_t ifxusb_dev_get_frame_number(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_dev_ep_set_stall(ifxusb_core_if_t *_core_if, uint8_t _epno, uint8_t _is_in)
	 \brief Set the EP STALL.
	 \param _core_if        Pointer of core_if structure
	 \param _epno           EP number
	 \param _is_in          1: is IN transfer
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_dev_ep_set_stall(ifxusb_core_if_t *_core_if, uint8_t _epno, uint8_t _is_in);

	/*!
	 \fn    void ifxusb_dev_ep_clear_stall(ifxusb_core_if_t *_core_if, uint8_t _epno, uint8_t _ep_type, uint8_t _is_in)
	 \brief Set the EP STALL.
	 \param _core_if        Pointer of core_if structure
	 \param _epno           EP number
	 \param _ep_type        EP Type
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_dev_ep_clear_stall(ifxusb_core_if_t *_core_if, uint8_t _epno, uint8_t _ep_type, uint8_t _is_in);

	/*!
	 \fn    void ifxusb_dev_core_init(ifxusb_core_if_t *_core_if, ifxusb_params_t  *_params)
	 \brief  This function initializes the IFXUSB controller registers for Device mode.
	         This function flushes the Tx and Rx FIFOs and it flushes any entries in the
	         request queues.
	         This function validate the imported parameters and store the result in the CIF structure.
	             After
	 \param _core_if  Pointer of core_if structure
	 \param _params   structure of inported parameters
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_dev_core_init(ifxusb_core_if_t *_core_if, ifxusb_params_t  *_params);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__GADGET_LED__) || defined(__HOST_LED__)
	/*!
	 \fn    void ifxusb_led_init(ifxusb_core_if_t *_core_if)
	 \brief This function init the LED control.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_led_init(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_led_free(ifxusb_core_if_t *_core_if)
	 \brief This function free the LED control.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_led_free(ifxusb_core_if_t *_core_if);

	/*!
	 \fn    void ifxusb_led(ifxusb_core_if_t *_core_if)
	 \brief This function trigger the LED access.
	 \param _core_if        Pointer of core_if structure
	 \ingroup  IFXUSB_CIF
	 */
	extern void ifxusb_led(ifxusb_core_if_t *_core_if);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* internal routines for debugging */
#ifdef __IS_HOST__
	extern void ifxusb_dump_msg_h(const u8 *buf, unsigned int length);
	extern void ifxusb_dump_spram_h(ifxusb_core_if_t *_core_if);
	extern void ifxusb_dump_registers_h(ifxusb_core_if_t *_core_if);
#else
	extern void ifxusb_dump_msg_d(const u8 *buf, unsigned int length);
	extern void ifxusb_dump_spram_d(ifxusb_core_if_t *_core_if);
	extern void ifxusb_dump_registers_d(ifxusb_core_if_t *_core_if);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline uint32_t ifxusb_read_core_intr(ifxusb_core_if_t *_core_if)
{
	return (ifxusb_rreg(&_core_if->core_global_regs->gintsts) &
	        ifxusb_rreg(&_core_if->core_global_regs->gintmsk));
}

static inline uint32_t ifxusb_read_otg_intr (ifxusb_core_if_t *_core_if)
{
	return (ifxusb_rreg (&_core_if->core_global_regs->gotgint));
}

static inline uint32_t ifxusb_mode(ifxusb_core_if_t *_core_if)
{
	return (ifxusb_rreg( &_core_if->core_global_regs->gintsts ) & 0x1);
}
static inline uint8_t ifxusb_is_device_mode(ifxusb_core_if_t *_core_if)
{
	return (ifxusb_mode(_core_if) != 1);
}
static inline uint8_t ifxusb_is_host_mode(ifxusb_core_if_t *_core_if)
{
	return (ifxusb_mode(_core_if) == 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __IS_HOST__
	static inline uint32_t ifxusb_read_hprt0(ifxusb_core_if_t *_core_if)
	{
		hprt0_data_t hprt0;
		hprt0.d32 = ifxusb_rreg(_core_if->hprt0);
		hprt0.b.prtena = 0;
		hprt0.b.prtconndet = 0;
		hprt0.b.prtenchng = 0;
		hprt0.b.prtovrcurrchng = 0;
		return hprt0.d32;
	}

	static inline uint32_t ifxusb_read_host_all_channels_intr (ifxusb_core_if_t *_core_if)
	{
		return (ifxusb_rreg (&_core_if->host_global_regs->haint));
	}

	static inline uint32_t ifxusb_read_host_channel_intr (ifxusb_core_if_t *_core_if, int hc_num)
	{
		return (ifxusb_rreg (&_core_if->hc_regs[hc_num]->hcint));
	}
#endif

#ifdef __IS_DEVICE__
	static inline uint32_t ifxusb_read_dev_all_in_ep_intr(ifxusb_core_if_t *_core_if)
	{
		uint32_t v;
		v = ifxusb_rreg(&_core_if->dev_global_regs->daint) &
		    ifxusb_rreg(&_core_if->dev_global_regs->daintmsk);
		return (v & 0xffff);
	}

	static inline uint32_t ifxusb_read_dev_all_out_ep_intr(ifxusb_core_if_t *_core_if)
	{
		uint32_t v;
		v = ifxusb_rreg(&_core_if->dev_global_regs->daint) &
		    ifxusb_rreg(&_core_if->dev_global_regs->daintmsk);
		return ((v & 0xffff0000) >> 16);
	}

	static inline uint32_t ifxusb_read_dev_in_ep_intr(ifxusb_core_if_t *_core_if, int _ep_num)
	{
		uint32_t v;
		v = ifxusb_rreg(&_core_if->in_ep_regs[_ep_num]->diepint) &
		    ifxusb_rreg(&_core_if->dev_global_regs->diepmsk);
		return v;
	}

	static inline uint32_t ifxusb_read_dev_out_ep_intr(ifxusb_core_if_t *_core_if, int _ep_num)
	{
		uint32_t v;
		v = ifxusb_rreg(&_core_if->out_ep_regs[_ep_num]->doepint) &
		    ifxusb_rreg(&_core_if->dev_global_regs->doepmsk);
		return v;
	}

#endif

#ifdef __IS_HOST__
extern void ifxusb_attr_create_h (void *_dev);
extern void ifxusb_attr_remove_h (void *_dev);
#else
extern void ifxusb_attr_create_d (void *_dev);
extern void ifxusb_attr_remove_d (void *_dev);
#endif

#ifdef __IS_HOST__
extern void do_suspend_h(ifxusb_core_if_t *core_if);
extern void do_resume_h(ifxusb_core_if_t *_core_if);
#else
extern void do_suspend_d(ifxusb_core_if_t *core_if);
extern void do_resume_d(ifxusb_core_if_t *_core_if);
#endif

#if defined(__WITH_OC_HY__) && defined(__IS_HOST__)
	#if defined(__IS_DUAL__)
		extern uint32_t ifxusb_oc_get_hy(int port);
		extern void ifxusb_oc_set_hy(int port,uint32_t setting);
	#else
		extern uint32_t ifxusb_oc_get_hy(void);
		extern void ifxusb_oc_set_hy(uint32_t setting);
	#endif
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(__IFXUSB_CIF_H__)

