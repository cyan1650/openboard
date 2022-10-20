/********************************************************************************************************
 * @file	main.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
 *
 * @par     Copyright (c) 2020, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Redistribution and use in source and binary forms, with or without
 *          modification, are permitted provided that the following conditions are met:
 *
 *              1. Redistributions of source code must retain the above copyright
 *              notice, this list of conditions and the following disclaimer.
 *
 *              2. Unless for usage inside a TELINK integrated circuit, redistributions
 *              in binary form must reproduce the above copyright notice, this list of
 *              conditions and the following disclaimer in the documentation and/or other
 *              materials provided with the distribution.
 *
 *              3. Neither the name of TELINK, nor the names of its contributors may be
 *              used to endorse or promote products derived from this software without
 *              specific prior written permission.
 *
 *              4. This software, with or without modification, must only be used with a
 *              TELINK integrated circuit. All other usages are subject to written permission
 *              from TELINK and different commercial license may apply.
 *
 *              5. Licensee shall be solely responsible for any claim to the extent arising out of or
 *              relating to such deletion(s), modification(s) or alteration(s).
 *
 *          THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *          ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *          WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *          DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
 *          DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *          (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *          LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *          ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *          (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *          SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************************************/
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "app.h"

#if(FREERTOS_ENABLE)
#include <FreeRTOS.h>
#include <task.h>
#endif
/**
 * @brief		BLE SDK RF interrupt handler.
 * @param[in]	none
 * @return      none
 */
_attribute_ram_code_
void rf_irq_handler(void)
{
	DBG_CHN14_HIGH;

	irq_blt_sdk_handler ();

	DBG_CHN14_LOW;
}



/**
 * @brief		BLE SDK System timer interrupt handler.
 * @param[in]	none
 * @return      none
 */
_attribute_ram_code_
void stimer_irq_handler(void)
{
	DBG_CHN15_HIGH;

	irq_blt_sdk_handler ();

	DBG_CHN15_LOW;
}




#if (FREERTOS_ENABLE)
static void led_task(void *pvParameters){
	reg_gpio_pb_oen &= ~ GPIO_PB7;
	while(1){
		reg_gpio_pb_out |= GPIO_PB7;
		printf("LED ON;\r\n");
		vTaskDelay(1000);
		printf("LED OFF;\r\n");
		reg_gpio_pb_out &= ~GPIO_PB7;
		vTaskDelay(1000);
	}
}
void proto_task( void *pvParameters );
#endif

/**
 * @brief		This is main function
 * @param[in]	none
 * @return      none
 */
_attribute_ram_code_ int main (void)   //must on ramcode
{
	DBG_CHN0_LOW;
	blc_pm_select_internal_32k_crystal();

	sys_init(DCDC_1P4_DCDC_1P8,VBAT_MAX_VALUE_GREATER_THAN_3V6);

	/* detect if MCU is wake_up from deep retention mode */
	int deepRetWakeUp = pm_is_MCU_deepRetentionWakeup();  //MCU deep retention wakeUp

	CCLK_16M_HCLK_16M_PCLK_16M;

	rf_drv_ble_init();

	gpio_init(!deepRetWakeUp);



	if(!deepRetWakeUp){//read flash size
		#if (BATT_CHECK_ENABLE)
			user_battery_power_check();
		#endif

		blc_readFlashSize_autoConfigCustomFlashSector();

		#if (FLASH_FIRMWARE_CHECK_ENABLE)
			blt_firmware_completeness_check();
		#endif

		#if FIRMWARES_SIGNATURE_ENABLE
			blt_firmware_signature_check();
		#endif
	}

	/* load customized freq_offset cap value. */
	blc_app_loadCustomizedParameters();

	if( deepRetWakeUp ){ //MCU wake_up from deepSleep retention mode
		user_init_deepRetn ();
	}
	else{ //MCU power_on or wake_up from deepSleep mode
		user_init_normal();
#if (FREERTOS_ENABLE)
		extern void blc_ll_set_freertos_en(u8 en);
		blc_ll_set_freertos_en(1);
#endif
	}

#if (FREERTOS_ENABLE)

	extern void vPortRestoreTask();
	if( deepRetWakeUp ){
		printf("enter restor work.\r\n");
		vPortRestoreTask();

	}else{
		xTaskCreate( led_task, "tLed", configMINIMAL_STACK_SIZE, (void*)0, (tskIDLE_PRIORITY+1), 0 );
		xTaskCreate( proto_task, "tProto", 2*configMINIMAL_STACK_SIZE, (void*)0, (tskIDLE_PRIORITY+1), 0 );
	//	xTaskCreate( ui_task, "tUI", configMINIMAL_STACK_SIZE, (void*)0, tskIDLE_PRIORITY + 1, 0 );
		vTaskStartScheduler();
	}
#else
	irq_enable();

	while (1) {
		main_loop ();
	}
	return 0;
#endif
}

#if (FREERTOS_ENABLE)
//  !!! should notify those tasks that ulTaskNotifyTake long time and should be wakeup every time PM wakeup
void vPortWakeupNotify(){
}
#endif
