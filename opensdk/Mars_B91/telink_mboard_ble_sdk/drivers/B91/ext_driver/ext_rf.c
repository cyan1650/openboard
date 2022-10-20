/********************************************************************************************************
 * @file	ext_rf.c
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

#include "../lib/include/rf.h"
#include "../dma.h"

#include "ext_rf.h"

volatile unsigned int TXADDR = 0xc0013000;

#define   BLE_TXDMA_DATA        (0x140800 + 0x84)      //0x140884
#define   BLE_RXDMA_DATA        (0x140800 + 0x80)      //0x140880

extern dma_config_t rf_tx_dma_config;
extern dma_config_t rf_rx_dma_config;
extern rf_mode_e   g_rfmode;

_attribute_ram_code_
void ble_rf_set_tx_dma(unsigned char fifo_dep, unsigned char fifo_byte_size)
{
	/*
	 * FLD_RF_TX_MULTI_EN must enable
	 * 1. FLD_RF_TX_MULTI_EN = 0, whatever reg_rf_bb_tx_chn_dep, can not send OK
	 * 2. FLD_RF_TX_MULTI_EN = 1, reg_rf_bb_tx_chn_dep = 0, 2, 3, both OK
	 */
	reg_rf_bb_auto_ctrl |= (FLD_RF_TX_MULTI_EN | FLD_RF_CH_0_RNUM_EN_BK);//u_pd_mcu.u_dmac.atcdmac100_ahbslv.tx_multi_en,rx_multi_en,ch_0_rnum_en_bk
	reg_rf_bb_tx_chn_dep = fifo_dep;//tx_chn_dep = 2^2 =4 (have 4 fifo)
	reg_rf_bb_tx_size	= fifo_byte_size;//tx_idx_addr = {tx_chn_adr*bb_tx_size,4'b0}// in this setting the max data in one dma buffer is 0x20<<4.And the The product of fifo_dep and bytesize cannot exceed 0xfff

	dma_set_address(DMA0, TXADDR, BLE_TXDMA_DATA);   // TXADDR=0xc0013000;
}

void ble_tx_dma_config(void){
	dma_config(DMA0,&rf_tx_dma_config);
}

_attribute_ram_code_
void ble_rf_set_rx_dma(unsigned char *buff, unsigned char fifo_byte_size)
{

	buff +=4;

	reg_rf_bb_auto_ctrl |= (FLD_RF_RX_MULTI_EN | FLD_RF_CH_0_RNUM_EN_BK);//ch0_rnum_en_bk,tx_multi_en,rx_multi_en

	//TODO: check with Qiangkai
	reg_rf_rx_wptr_mask = 0; //rx_wptr_real=rx_wptr & mask:After receiving 4 packets,the address returns to original address.mask value must in (0x01,0x03,0x07,0x0f)
	reg_rf_bb_rx_size = fifo_byte_size;//rx_idx_addr = {rx_wptr*bb_rx_size,4'h0}// in this setting the max data in one dma buffer is 0x20<<4.

//	dma_config(DMA1,&rf_rx_dma_config);   // reg_dma_ctrl(DMA1) = 0xc0aa1200;

	dma_set_address(DMA1, BLE_RXDMA_DATA, (u32)(buff));
	reg_dma_size(DMA1)=0xffffffff;

}

void ble_rx_dma_config(void){
	dma_config(DMA1,&rf_rx_dma_config);
}

_attribute_ram_code_
void rf_drv_ble_init(void){

	//To modify DCOC parameters
	write_reg16(0x140ed2,0x199b);		//default:0x15bb;DCOC_SFIIP DCOC_SFQQP ,DCOC_SFQQ

	//Setting for blanking
#if RF_RX_SHORT_MODE_EN
	write_reg8(0x140c7b,0x0e);			//default :0xf6;BLANK_WINDOW
#else
	write_reg8(0x140c7b,0xfe);
#endif

	//To set AGC thresholds
	write_reg16(0x140e4a,0x090e);		//default:0x0689;POW_000_001,POW_001_010_H
//	write_reg16(0x140e4e,0x0f09);		//default:0x0f09;POW_100_101 ,POW_101_100_L,POW_101_100_H;
	write_reg32(0x140e54,0x080c090e);	//default:0x078c0689,POW_001_010_L,POW_001_010_H,POW_011_100_L,POW_011_100_H
//	write_reg16(0x140e58,0x0f09);		//default: 0x0f09;POW_101_100_L,POW_101_100_H
	//For optimum preamble detection
	write_reg16(0x140c76,0x7350);		//default:0x7357;FREQ_CORR_CFG2_0,FREQ_CORR_CFG2_1
#if RF_RX_SHORT_MODE_EN
	write_reg16(0x14083a,0x6586);		//default:0x2d4e;rx_ant_offset  rx_dly(0x140c7b,0x140c79,0x14083a,0x14083b),samp_offset
#endif
	analog_write_reg8(0x8b,0x04);		//default:0x06;FREQ_CORR_CFG2_1

	write_reg8(0x140e3d,0x61);			//default:0x61;ble:bw_code
	write_reg32(0x140e20,0x23200a16);	//default:0x23200a16;sc_code,if_freq,IF = 1Mhz,BW = 1Mhz,HPMC_EXP_DIFF_COUNT_L,HPMC_EXP_DIFF_COUNT_H
//	write_reg8(0x140e3f,0x00);			//default:0x00;250k modulation index:telink add rx for 250k/500k
//	write_reg8(0x140c3f,0x00);			//default:0x00;LOW_RATE_EN bit<1>:1 enable bit<2>:0 250k
	write_reg8(0x140c20,0x84);			//default:0xc4;script cc
	write_reg8(0x140c22,0x00);			//default:0x00;modem:BLE_MODE_TX,2MBPS
	write_reg8(0x140c4d,0x01);			//default:0x01;r_rxchn_en_i:To modem
	write_reg8(0x140c4e,0x1e);			//default:0x1e;ble sync thre:To modem
//	write_reg8(0x140c21,0x00);			//default:0x00;modem:ZIGBEE_MODE:01
//	write_reg8(0x140c23,0x00);			//default:0x00;modem:ZIGBEE_MODE_TX
//	write_reg8(0x140c2a,0x10);			//default:0x10;modem:disable MSK
//	write_reg8(0x140c3d,0x00);			//default:0x00;modem:zb_sfd_frm_ll
//	write_reg8(0x140c2c,0x38);			//default:0x38;modem:zb_dis_rst_pdet_isfd
	write_reg16(0x140c36,0x0eb7);		//default:0x0eb7;LR_NUM_GEAR_L,LR_NUM_GEAR_H,
	write_reg16(0x140c38,0x71c4);		//default:0x71c4;LR_TIM_EDGE_DEV,LR_TIM_REC_CFG_1
	write_reg8(0x140c73,0x01);			//default:0x01;TOT_DEV_RST

#if RF_RX_SHORT_MODE_EN
	write_reg8(0x140c79,0x38);			//default:0x00;RX_DIS_PDET_BLANK
#else
	write_reg8(0x140c79,0x08);
#endif

	write_reg8(0x140c9a,0x00);			//default:0x08;tx_tp_align
	write_reg16(0x140cc2,0x4b39);		//default:0x4836;grx_0,grx_1
	write_reg32(0x140cc4,0x796e6256);	//default:0x796e6254;grx_2,grx_3,grx_4,grx_5

	write_reg32(0x140800,0x4446081f);	//default:0x4442001f;tx_mode,zb_pn_en,preamble set 6 for BLE,bit<0:1>private mode control. bit<2:3> tx mode

	write_reg16(0x140804,0x04f5);		//default:0x04f5; bit<4>mode:1->1m;bit<0:3>:ble head.ble whiting;lr mode bit<4:5> 0:off,3:125k,2:500k

	//access code, can save
	write_reg32(0x140808,0x00000000);	//default:0xf8118ac9;

//	write_reg8(0x140821,0xa1);			//default:0xa1;rx packet len 0 enable
//	write_reg8(0x140822,0x00);			//default:0x00;rxchn_man_en
//	write_reg8(0x140c4c,0x4c);			//default:0x4c;RX:acc_len modem

	write_reg8(0x140c4e, 31); 			//default:0x1e;access code bit number match threshold
	write_reg8(0x140830, 0x36);			//default:0x3c;disable tx timestamp en, add by LiBiao

//	//copy from QingHua's code
//	write_reg32(0x80140860, 0x5f4f4434);  //grx_3~0
//	write_reg16(0x80140864, 0x766b);      //grx_5~4

	write_reg8(0x80140a06, 0x00); 		//LL_RXWAIT, default 0x0009
	write_reg8(0x80140a0c, 0x50); 		//LL_RXSTL   default 0x0095
	write_reg8(0x80140a0e, 0x00); 		//LL_TXWAIT, default 0x0009
	write_reg8(0x80140a10, 0x00); 		//LL_ARD,    default 0x0063


	reg_rf_modem_mode_cfg_rx1_0 &= ~FLD_RF_LR_TRIG_MODE;		//coded phy accesscode triggle mode: manual mode

}



void rf_switchPhyTestMode(rf_mode_e mode)
{
	if(mode == RF_MODE_BLE_1M)
	{
		write_reg8(0x401, 0x00);	//PN disable
		write_reg8(0x402, 0x46);
		write_reg8(0x404, 0xd5);	//PN disable
	}
	else if(mode == RF_MODE_BLE_2M)
	{
		write_reg8(0x401, 0x00);	//PN disable
		write_reg8(0x404, 0xc5);	//PN disable
	}
	else if(mode == RF_MODE_LR_S2_500K)
	{
		write_reg8(0x401, 0x00);
	}
	else if(mode == RF_MODE_LR_S8_125K)
	{
		write_reg8(0x401, 0x00);
	}

	write_reg32 (0x800408, 0x29417671);	//accesscode: 1001-0100 1000-0010 0110-1110 1000-1110   29 41 76 71
	write_reg8 (0x800405, read_reg8(0x405)|0x80); //todo register have changed

}

_attribute_ram_code_
void rf_start_fsm (fsm_mode_e mode, void* tx_addr, unsigned int tick)
{
	if(mode==FSM_BRX){
		write_reg32 (0x80140a28, 0x0fffffff);
	}
	reg_rf_ll_cmd_schedule = tick;
	reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;	// Enable cmd_schedule mode.
	reg_rf_ll_cmd = mode;

	if(tx_addr){
		dma_set_src_address(DMA0,(unsigned int)tx_addr);
	}
}




static inline void ble_rf_set_chn(signed char chn)
{
	unsigned int freq_low;
	unsigned int freq_high;
	unsigned int chnl_freq;
	unsigned char ctrim;
	unsigned int freq;

	freq = 2400+chn;
	if(freq >= 2550){
		ctrim = 0;
	}
	else if(freq >= 2520){
		ctrim = 1;
	}
	else if(freq >= 2495){
		ctrim = 2;
	}
	else if(freq >= 2465){
		ctrim = 3;
	}
	else if(freq >= 2435){
		ctrim = 4;
	}
	else if(freq >= 2405){
		ctrim = 5;
	}
	else if(freq >= 2380){
		ctrim = 6;
	}
	else{
		ctrim = 7;
	}

	extern unsigned char  g_single_tong_freqoffset;
	chnl_freq = freq*2 + g_single_tong_freqoffset;
	freq_low  = (chnl_freq & 0x7f);
	freq_high = ((chnl_freq>>7)&0x3f);

	write_reg8(0x140e44,  (read_reg8(0x140e44) | 0x01 ));
	write_reg8(0x140e44,  (read_reg8(0x140e44) & 0x01) | freq_low << 1);
	write_reg8(0x140e45,  (read_reg8(0x140e45) & 0xc0) | freq_high);
	write_reg8(0x140e29,  (read_reg8(0x140e29) & 0x3f) | (ctrim<<5) );  //FE_CTRIM
}


_attribute_ram_code_ //must be RamCode
void rf_set_ble_channel (signed char chn_num)
{
    write_reg8 (0x14080d, chn_num);
	if (chn_num < 11)
		chn_num += 2;
	else if (chn_num < 37)
		chn_num += 3;
	else if (chn_num == 37)
		chn_num = 1;
	else if (chn_num == 38)
		chn_num = 13;
	else if	(chn_num == 39)
		chn_num = 40;
	else if	(chn_num < 51)
		chn_num = chn_num;
	else if(chn_num <= 61)
		chn_num = -61 + chn_num;

	chn_num = chn_num << 1;
	ble_rf_set_chn(chn_num);

}
