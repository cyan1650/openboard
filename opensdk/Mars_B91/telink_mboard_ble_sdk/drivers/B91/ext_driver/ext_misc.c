/********************************************************************************************************
 * @file	ext_misc.c
 *
 * @brief	This is the header file for B91
 *
 * @author	BLE Group
 * @date	2020
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
#include "ext_misc.h"

/******************************* trng_start ******************************************************************/
void generateRandomNum(int len, unsigned char *data)
{
	int i;
	unsigned int randNums = 0;
    /* if len is odd */
	for (i=0; i<len; i++ ) {
		if( (i & 3) == 0 ){
			randNums = rand();
		}

		data[i] = randNums & 0xff;
		randNums >>=8;
	}
}
/******************************* trng_end ********************************************************************/



/******************************* flash_start *****************************************************************/
_attribute_ram_code_ unsigned int flash_get_jedec_id_ram(void)
{
#if SUPPORT_PFT_ARCH
	reg_irq_threshold = 1;
#else
	unsigned int r  = core_interrupt_disable();
#endif
	extern void flash_send_cmd(flash_command_e cmd);
	flash_send_cmd(FLASH_GET_JEDEC_ID);
	unsigned char manufacturer = mspi_read();
	unsigned char mem_type = mspi_read();
	unsigned char cap_id = mspi_read();
	mspi_high();
	CLOCK_DLY_5_CYC;
#if SUPPORT_PFT_ARCH
	reg_irq_threshold = 0;
#else
	core_restore_interrupt(r);
#endif
	return (unsigned int)((manufacturer << 24 | mem_type << 16 | cap_id));
}

_attribute_text_code_ unsigned int flash_get_jedec_id(void)
{
	__asm__("csrci 	mmisc_ctl,8");//disable BTB
	unsigned int id;
	id = flash_get_jedec_id_ram();
	__asm__("csrsi 	mmisc_ctl,8");//enable BTB

	return id;
}

_attribute_data_retention_   _attribute_aligned_(4)	flash_capacity_e	flash_capacity = FLASH_SIZE_1M;;

void flash_set_capacity(flash_capacity_e flash_cap)
{
	flash_capacity = flash_cap;
}

flash_capacity_e flash_get_capacity(void)
{
	return flash_capacity;
}
/******************************* flash_end *****************************************************************/



/******************************* uart_start *****************************************************************/
 _attribute_ram_code_ void uart_receive_dma_set(dma_chn_e chn, unsigned char * addr,unsigned int rev_size)
{
	 dma_chn_dis(chn);
	/*In order to be able to receive data of unknown length(A0 doesn't suppport),the DMA SIZE is set to the longest value 0xffffffff.After entering suspend and wake up, and then continue to receive, 
	DMA will no longer move data from uart fifo, because DMA thinks that the last transmission was not completed and must disable dma_chn first.modified by minghai,confirmed qiangkai 2020.11.26.*/
	dma_set_address(chn,reg_uart_data_buf_adr(UART0),(unsigned int)convert_ram_addr_cpu2bus(addr));
	if(0xff== g_chip_version)
	{
		dma_set_size(chn, rev_size, DMA_WORD_WIDTH);
	}
	else
	{
	    reg_dma_size(chn)=0xffffffff;
	}

	dma_chn_en(chn);
}
/******************************* uart_end *******************************************************************/




 /******************************* plic_start *****************************************************************/
 _attribute_ram_code_
unsigned int cpu_stall_WakeUp_By_RF_SystemTick(int WakeupSrc, unsigned short rf_mask, unsigned int tick)
{
    unsigned short rf_irq_mask_save =0;
    unsigned char systimer_irq_mask_save = 0;

    unsigned int r=core_interrupt_disable();

    rf_irq_mask_save = reg_rf_irq_mask;
    systimer_irq_mask_save = reg_system_irq_mask;

	if( WakeupSrc & FLD_IRQ_ZB_RT_EN )
	{
		reg_rf_irq_mask = rf_mask;
	}

	if( WakeupSrc & FLD_IRQ_SYSTEM_TIMER )
	{
		stimer_set_irq_capture(tick);
		stimer_set_irq_mask(FLD_SYSTEM_IRQ);
	}

	core_entry_wfi_mode();//WFI instruction enables the processor to enter the wait-for-interrupt (WFI) mode

	reg_rf_irq_status = FLD_RF_IRQ_ALL;
	stimer_clr_irq_status(FLD_SYSTEM_IRQ);

	reg_rf_irq_mask = rf_irq_mask_save;
	reg_system_irq_mask = systimer_irq_mask_save;

	core_restore_interrupt(r);

}

 /******************************* plic_end *******************************************************************/
