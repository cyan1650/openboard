/********************************************************************************************************
 * @file	common_dbg.c
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
#include "common_dbg.h"


#if (DUMP_STR_EN || APP_DUMP_EN)

MYFIFO_INIT_IRAM(print_fifo, 288, 32);



void app_dmup_debug_init(void)
{
	my_usb_init(0x120, &print_fifo);
	usb_set_pin_en ();
}

#endif





#if (EXCEPT_HANDLE_DEBUG_ENABLE)

volatile u32 dbg_except_handler = 0;

volatile u32 g_mcause;
volatile u32 g_mepc;
volatile u32 g_mtval;
volatile u32 g_mdcause;
volatile u32 g_mscratch;
/**
 * @brief		BLE SDK System except handler.
 * @param[in]	none
 * @return      none
 */
_attribute_ram_code_
void except_handler()
{
	gpio_write(GPIO_LED_WHITE, LED_ON_LEVAL);

	g_mcause = read_csr(NDS_MCAUSE);
	g_mepc = read_csr(NDS_MEPC);
	g_mtval = read_csr(NDS_MTVAL);
	g_mdcause = read_csr(NDS_MDCAUSE);
	g_mscratch = read_csr(NDS_MSCRATCH);
	#if 0		//Enable this if you need to read values of the exception by BDT.
		while(1){
			printf("enter except_handler.\r\n");

			for(volatile unsigned int i = 0; i < 0xffff; i++)
			{
				asm("nop");
				dbg_except_handler++;
			}
		}
	#else		//If the exception may occur and the system need to be rebooted , enable this.
		start_reboot(); //reboot the MCU
	#endif
}
#endif
