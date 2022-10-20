/********************************************************************************************************
 * @file	ll_misc.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
 *
 * @par		Copyright (c) 2020, Telink Semiconductor (Shanghai) Co., Ltd.
 *			All rights reserved.
 *
 *          The information contained herein is confidential property of Telink
 *          Semiconductor (Shanghai) Co., Ltd. and is available under the terms
 *          of Commercial License Agreement between Telink Semiconductor (Shanghai)
 *          Co., Ltd. and the licensee or the terms described here-in. This heading
 *          MUST NOT be removed from this file.
 *
 *          Licensee shall not delete, modify or alter (or permit any third party to delete, modify, or
 *          alter) any information contained herein in whole or in part except as expressly authorized
 *          by Telink semiconductor (shanghai) Co., Ltd. Otherwise, licensee shall be solely responsible
 *          for any claim to the extent arising out of or relating to such deletion(s), modification(s)
 *          or alteration(s).
 *
 *          Licensees are granted free, non-transferable use of the information in this
 *          file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided.
 *
 *******************************************************************************************************/
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/controller/ble_controller.h"











#if (BLMS_DEBUG_EN)
_attribute_ram_code_
int blt_ll_error_debug(u32 x)
{
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		irq_disable();
		write_dbg32(DBG_SRAM_ADDR, x);
		gpio_write(GPIO_PB7, 1);  // GPIO_LED_RED
		while(1){
			#if (APP_DUMP_EN)
				myudb_usb_handle_irq();
			#endif
		}
	#else
		irq_disable();
		write_reg32(0x40000, x);
		gpio_write(GPIO_PD2, 1);  // GPIO_LED_RED
		while(1);
	#endif
}
#endif
