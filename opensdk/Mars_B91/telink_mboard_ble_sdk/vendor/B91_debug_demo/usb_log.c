/********************************************************************************************************
 * @file	usb_log.c
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

#include "app_config.h"
#include "nds_intrinsic.h"
#include "common/usb_dbg/myudb.h"


#if (TEST_MODE == USB_LOG_TEST)



/**************************notices******************************************
 * ********timer clock use APB clock ******************/
#define TIMER_MODE_GPIO_TRIGGER_TICK    0xf


#define	UART_TX_FIFO_SIZE			272
#define	UART_TX_FIFO_NUM			32

u8 		 	uart_txfifo_b[UART_TX_FIFO_SIZE * UART_TX_FIFO_NUM];
my_fifo_t	uart_txfifo = {
													UART_TX_FIFO_SIZE,
													UART_TX_FIFO_NUM,
													0,
													0,
													uart_txfifo_b,};



unsigned long long rdmcycle(void)
{

		do {
				unsigned long hi = read_csr(NDS_MCYCLEH);
				unsigned long lo = read_csr(NDS_MCYCLE);

				if (hi == read_csr(NDS_MCYCLEH))
					return ((unsigned long long)hi << 32) | lo;
		} while(1);
}


void user_init(void)
{
	//sys_clock_init(SYS_CLK_48M);
	//sys_clock_init(SYS_CLK_48M);
	myudb_usb_init (0x120,&uart_txfifo);		//0x120: usb sub-id


	usb_dp_pullup_en (1);
}

int g_cnt=0;
unsigned long t0,t1;
unsigned long long  cycle0,cycle1;

volatile float g_f = 3.56;
int  g_int = 2;
void main_loop(void)
{

	//g_f = 1.47 + 2.3;

	myudb_usb_handle_irq ();
	//delay_ms(200);
	//gpio_toggle(LED1);
	//log_uart(g_cnt++);

	    static u32 tick_t1 = 0;
	    static u32 tick_t2 = 0;
		int i;
		u32 len;
		if (clock_time_exceed (tick_t1, 100000))
		{

			tick_t1 = clock_time ();

			cycle1 = rdmcycle();

	        cycle1 = cycle1 - cycle0;


	        log_b16(1, SL16_CLKN, cycle1);

	        log_task(1,SL01_task_test,1);
			my_dump_str_data (1, "systick:", &t1, 4);
			my_dump_str_data (1, "cycle:", &cycle1, 8);
			log_task(1,SL01_task_test,0);

			log_tick(1,SLEV_rx_lmp);



		}

		if (clock_time_exceed (tick_t2, 220))
		{

			tick_t2 = clock_time ();
			log_task(1,SL01_detach_timeout,1);
			log_b16(1, SL16_FCNT, g_cnt);
			log_task(1,SL01_detach_timeout,0);
			g_cnt++;
			//log_event(1,SLEV_reserved);

		}



}

#endif //end of  (TEST_MODE == USB_LOG_TEST)
