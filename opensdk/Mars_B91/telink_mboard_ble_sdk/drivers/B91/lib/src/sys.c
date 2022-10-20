/********************************************************************************************************
 * @file	sys.c
 *
 * @brief	This is the source file for B91
 *
 * @author	Driver Group
 * @date	2019
 *
 * @par		Copyright (c) 2019, Telink Semiconductor (Shanghai) Co., Ltd.
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
#include "lib/include/pm.h"
#include "lib/include/sys.h"
#include "core.h"
#include "compiler.h"
#include "analog.h"
#include "gpio.h"
#include "mspi.h"
#include "stimer.h"
#include "dma.h"


unsigned int g_chip_version=0;

extern void pm_update_status_info(void);

#if 0
/**
 * @brief   	This function serves to initialize system.
 * @param[in]	power_mode	- power mode(LDO/DCDC/LDO_DCDC)
 * @param[in]	vbat_v		- vbat voltage type: 0 vbat may be greater than 3.6V,  1 vbat must be below 3.6V.
 * @return  	none
 */
void sys_init(power_mode_e power_mode, vbat_type_e vbat_v)
{
	/**
	 * reset function will be cleared by set "1",which is different from the previous configuration.
	 * This setting turns off the TRNG and NPE modules in order to test power consumption.The current
	 * decrease about 3mA when those two modules be turn off.changed by zhiwei,confirmed by kaixin.20200828.
	 */
	reg_rst 	= 0xffbbffff;
	reg_clk_en 	= 0xffbbffff;

	analog_write_reg8(0x8c,0x02);		//<1>:reg_xo_en_clk_ana_ana=1

	//when VBAT power supply > 4.1V and LDO switch to DCDC,DCDC_1V8 voltage will ascend to the supply power in a period time,
	//cause the program can not run. Need to trim down dcdc_flash_out before switch power mode.
	//confirmed by haitao,modify by yi.bao(20210119)
	if(DCDC_1P4_DCDC_1P8 == power_mode)
	{
		analog_write_reg8(0x0c, 0x40);  //poweron_dft: 0x44 --> 0x40.
										//<2:0> dcdc_trim_flash_out,flash/codec 1.8V/2.8V trim down 0.2V in DCDC mode.
	}
	analog_write_reg8(0x0a, power_mode);//poweron_dft:	0x90.
										//<0-1>:pd_dcdc_ldo_sw,	default:00, dcdc & bypass ldo status bits.
										//		dcdc_1p4	dcdc_1p8	ldo_1p4		ldo_1p8
										//00:		N			N			Y			Y
										//01:		Y			N			N			Y
										//10:		Y			N			N			N
										//11:		Y			Y			N			N
    analog_write_reg8(0x0b, 0x3b);		//poweron_dft:	0x7b -> 0x3b.
										//<6>:mscn_pullup_res_enb,	default:1,->0 enable 1M pullup resistor for mscn PAD.
	analog_write_reg8(0x05,analog_read_reg8(0x05) & (~BIT(3)));//poweron_dft:	0x02 -> 0x02.
										//<3>:24M_xtl_pd,		default:0,->0 Power up 24MHz XTL oscillator.
	analog_write_reg8(0x06,analog_read_reg8(0x06) & ~(BIT(0) | vbat_v | BIT(6) | BIT(7)));//poweron_dft:	0xff -> 0x36 or 0x3e.
										//<0>:pd_bbpll_ldo,		default:1,->0 Power on ana LDO.
										//<3>:pd_vbus_sw,		default:1,->0 Power up of bypass switch.
										//<6>:spd_ldo_pd,		default:1,->0 Power up spd ldo.
										//<7>:dig_ret_pd,		default:1,->0 Power up retention  ldo.
	analog_write_reg8(0x01, 0x45);		//poweron_dft:	0x44 -> 0x45.
										//<0-2>:bbpll_ldo_trim,			default:100,->101 measured 1.186V.The default value is sometimes crashes.
										//<4-6>:ana_ldo_trim,1.0-1.4V	default:100,->100 analog LDO output voltage trim: 1.2V
	//When using the default value, during the USB charging process, the audio output will hear a sizzling electric current.
	//This problem can be solved by increasing the OCP current limit value to avoid unnecessary shutdown.
	//confirmed by ya.yang, modify by weihua.zhang(20210805)
	analog_write_reg8(0x1c, 0x4c);		//poweron_dft:	0x40 -> 0x4c.
										//<2-3>:ocp_i_cross_trim,	default:00,->11 the current limit value of OCP is configured to the maximum value.

	//in B91,the dma_mask is turned on by default and cleared uniformly during initialization.
	for(unsigned char dma_chn =0;dma_chn<= 7;dma_chn++)
	{
		dma_clr_irq_mask(dma_chn,TC_MASK|ERR_MASK|ABT_MASK);
	}
	pm_update_status_info();
	g_pm_vbat_v = vbat_v>>3;

	//xo_ready check should be done after Xtal manual on_off, we put it here to save code running time, code running time between
	//Xtal manual on_off and xo_ready check can be used as Xtal be stable timimg.
	while( BIT(7) != (analog_read_reg8(0x88) & (BIT(7))));	//<7>: xo_ready_ana, R, aura xtl ready signal.

	//When bbpll_ldo_trim is set to the default voltage value, when doing high and low temperature stability tests,it is found that
	//there is a crash.The current workaround is to set other voltage values to see if it is stable.If it fails,repeat the setting
	//up to three times.The bbpll ldo trim must wait until 24M is stable.(add by weihua.zhang, confirmed by yi.bao and wenfeng 20200924)
	pm_wait_bbpll_done();

	if(g_pm_status_info.mcu_status == MCU_STATUS_DEEPRET_BACK)
	{
		pm_stimer_recover();
	}else{
#if SYS_TIMER_AUTO_MODE
	reg_system_ctrl |=(FLD_SYSTEM_TIMER_AUTO|FLD_SYSTEM_32K_TRACK_EN);	//enable 32k track and stimer auto.
	reg_system_tick = 0x01;	//initial next tick is 1,kick system timer
#else
	reg_system_ctrl	|= FLD_SYSTEM_32K_TRACK_EN | FLD_SYSTEM_TIMER_EN;	//enable 32k track and stimer. Wait for pll to stabilize before using stimer.
#endif
	}

	g_chip_version = read_reg8(0x1401fd);

	//if clock src is PAD or PLL, and hclk = 1/2cclk, use reboot may cause problem, need deep to resolve(add by yi.bao, confirm by guangjun 20201016)
	if(g_pm_status_info.mcu_status == MCU_STATUS_REBOOT_BACK)
	{
		//Use PM_ANA_REG_POWER_ON_CLR_BUF0 BIT(1) to represent the reboot+deep process, which is related to the function pm_update_status_info.
		analog_write_reg8(PM_ANA_REG_POWER_ON_CLR_BUF0, analog_read_reg8(PM_ANA_REG_POWER_ON_CLR_BUF0) | BIT(1));	//(add by weihua.zhang, confirmed by yi.bao 20201222)
		pm_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_TIMER, PM_TICK_STIMER_16M, (stimer_get_tick() + 100*SYSTEM_TIMER_TICK_1MS));
	}
	//**When testing AES_demo, it was found that the timing of baseband was wrong when it was powered on, which caused some of
	//the registers of ceva to go wrong, which caused the program to run abnormally.(add by weihua.zhang, confirmed by junwen 20200819)
	else if(0xff == g_chip_version)	//A0
	{
		if(g_pm_status_info.mcu_status == MCU_STATUS_POWER_ON)	//power on
		{
			analog_write_reg8(0x7d, 0x80);	//power on baseband
			pm_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_TIMER, PM_TICK_STIMER_16M, (stimer_get_tick() + 100*SYSTEM_TIMER_TICK_1MS));
		}
	}
	analog_write_reg8(0x7d, 0x80);		//poweron_dft:	0x03 -> 0x80.
										//<0>:pg_zb_en,		default:1,->0 power on baseband.
										//<1>:pg_usb_en,	default:1,->0 power on usb.
										//<2>:pg_npe_en,	default:1,->0 power on npe.
										//<7>:pg_clk_en,	default:0,->1 enable change power sequence clk.

	//when VBAT power supply > 4.1V and LDO switch to DCDC,DCDC_1V8 voltage will ascend to the supply power in a period time,
	//cause the program can not run. Need to trim down dcdc_flash_out before switch power mode,refer to the configuration above [analog_write_reg8(0x0c, 0x40)],
	/*Then restore the default value[analog_write_reg8(0x0c, 0x44)].There is a process of switching from LDO to DCDC, which needs to wait for a period of time, so it is restored here,
	confirmed by haitao,modify by minghai.duan(20211018)*/
	if(DCDC_1P4_DCDC_1P8 == power_mode)
	{
		analog_write_reg8(0x0c, 0x44);  //poweron_dft: 0x40 --> 0x44.
										//<2:0> dcdc_trim_flash_out,flash/codec 1.8V/2.8V in DCDC mode.
	}
}
#endif
/**
 * @brief      This function performs a series of operations of writing digital or analog registers
 *             according to a command table
 * @param[in]  pt - pointer to a command table containing several writing commands
 * @param[in]  size  - number of commands in the table
 * @return     number of commands are carried out
 */

int write_reg_table(const tbl_cmd_set_t * pt, int size)
{
	int l=0;

	while (l<size) {
		unsigned int  cadr = ((unsigned int)0x80000000) | pt[l].adr;
		unsigned char cdat = pt[l].dat;
		unsigned char ccmd = pt[l].cmd;
		unsigned char cvld =(ccmd & TCMD_UNDER_WR);
		ccmd &= TCMD_MASK;
		if (cvld) {
			if (ccmd == TCMD_WRITE) {
				write_reg8 (cadr, cdat);
			}
			else if (ccmd == TCMD_WAREG) {
				analog_write_reg8 (cadr, cdat);
			}
			else if (ccmd == TCMD_WAIT) {
				delay_us(pt[l].adr*256 + cdat);
			}
		}
		l++;
	}
	return size;

}

/**
 * @brief     this function servers to get data(BIT0~BIT31) from EFUSE.efuse default value is 0.
 * @param[in] none
 * @return    data(BIT0~BIT31)
 */
unsigned int  efuse_get_low_word(void)
{
	unsigned int efuse_info;
	write_reg8(0x1401f4, 0x65);        //efuse data out enable
	efuse_info= read_reg32(0x1401c8);  //read the low word data
	write_reg8(0x1401f4, 0x00);        //efuse data out disable
	return efuse_info ;
}

/**
 * @brief     this function servers to get data(BIT32~BIT63) from EFUSE.efuse default value is 0.
 * @param[in] none
 * @return    data(BIT32~BIT63)
 */
unsigned int  efuse_get_high_word(void)
{
	unsigned int efuse_info;
	write_reg8(0x1401f4, 0x65);         //efuse data out enable
	efuse_info = read_reg32(0x1401cc);  //read the high word(BIT<63:32>) data
	write_reg8(0x1401f4, 0x00);         //efuse data out disable
	return efuse_info ;
}

/**
 * @brief     this function servers to get calibration value from EFUSE.
 * 			  Only the two-point calibration gain and offset of GPIO sampling are saved in the efuse of B91.
 * @param[out]gain - gpio_calib_value.
 * @param[out]offset - gpio_calib_value_offset.
 * @return    1 means there is a calibration value in efuse, and 0 means there is no calibration value in efuse.
 */
unsigned char efuse_get_adc_calib_value(unsigned short* gain, signed char* offset)
{
	unsigned short  efuse_4to18bit_info = (efuse_get_low_word() >> 4) & 0x7fff;
	if(0 != efuse_4to18bit_info)
	{
		//Before the gain is stored in efuse, in order to reduce the number of bits occupied, 1000 is subtracted.
		//gpio_calib_value:bit[12:4]+1000
		*gain = (efuse_4to18bit_info & 0x1ff) + 1000;//unit: mv
		//gpio_calib_value_offset:bit[18:13]-20
		*offset = ((efuse_4to18bit_info >> 9) & 0x3f) - 20;//unit: mv
		return 1;
	}
	else
		//If efuse_4to18bit_info is 0, there is no calibration value in efuse.
		return 0;
}

/**
 * @brief     this function servers to set data(BIT0~BIT63) to EFUSE.VDD25FE(pin 15) is need to give 2.5V power when write.
 *            if you write the data into efuse,you need to power off-power on to make it take effect.
 * @param[in] data - the data need to be write(2 word)
 * @return    none
 */
void efuse_set_data(unsigned int* data)
{
	write_reg8(0x1401f4, 0x65);
	write_reg8(0x1401c7, 0x01);		//write_enable
	write_reg8(0x1401c7, 0x05);     //write_enable & clock_enable
	write_reg32(0x1401c8, data[0]); //low word
	write_reg32(0x1401cc, data[1]); //high word
	write_reg8(0x1401c7, 0x03);     //trig write
	while(BIT(1) == (read_reg8(0x1401c7) & BIT(1)));  //wait write done
	write_reg8(0x1401c7, 0x00);     //write_disable
	write_reg8(0x1401f4, 0x00);
}

/**********************************************************************************************************************
 *                    						local function implementation                                             *
 *********************************************************************************************************************/
