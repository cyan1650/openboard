/********************************************************************************************************
 * @file	phy_test.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	06,2020
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


#define			PHY_CMD_SETUP							0
#define			PHY_CMD_RX								1
#define			PHY_CMD_TX								2
#define			PHY_CMD_END								3


#define 		PKT_TYPE_PRBS9 							0
#define 		PKT_TYPE_0X0F 							1
#define 		PKT_TYPE_0X55 							2
#define 		PKT_TYPE_0XFF 							3

#define			PKT_TYPE_HCI_PRBS9						0
#define			PKT_TYPE_HCI_0X0F						1
#define			PKT_TYPE_HCI_0X55 						2
#define			PKT_TYPE_HCI_PRBS15						3
#define			PKT_TYPE_HCI_0XFF 						4
#define			PKT_TYPE_HCI_0X00						5
#define			PKT_TYPE_HCI_0XF0						6
#define			PKT_TYPE_HCI_0XAA						7


#define TX_PKT_SHARE_SAVE_RAM		1


enum{
	PHY_EVENT_STATUS	 = 0,
	PHY_EVENT_PKT_REPORT = 0x8000,
};

enum{
	PHY_STATUS_SUCCESS 	 = 0,
	PHY_STATUS_FAIL 	 = 0x0001,
};


typedef struct {
	u8 cmd;
	u8 tx_start;
	u16 pkts;

	u32 tick_tx;
	u32 tick_tx2;
}phy_data_t;


static union pkt_length_u
{
	u8 len;
	struct len_t
	{
		u8 low:6;
		u8 upper:2;

	}l;
}pkt_length;

typedef struct{
    unsigned int len;        // data max 252
    unsigned char data[254];
}uart_phy_t;


uart_phy_t uart_txdata_buf;

_attribute_data_retention_	phy_data_t  blt_phyTest;

static rf_mode_e phy_test_mode = RF_MODE_BLE_1M;


#if TX_PKT_SHARE_SAVE_RAM //save ramcode
	_attribute_data_retention_	u8 *pkt_phytest;
#else
	u8		pkt_phytest [64] = {
			39, 0, 0, 0,
			0, 37,
			0, 1, 2, 3, 4, 5, 6, 7
	};
#endif


extern hci_fifo_t				bltHci_rxfifo;
extern hci_fifo_t			    bltHci_txfifo;
extern hci_fifo_t				bltHci_rxAclfifo; /* H2C */
extern blc_main_loop_phyTest_callback_t	blc_main_loop_phyTest_cb;

u8	phytest_rx_fifo[288];
u8	phytest_tx_fifo[288];

void blc_phy_initPhyTest_module(void)
{
	blc_main_loop_phyTest_cb = blt_phyTest_main_loop;
}


u8	phyTest_Channel (u8 chn)
{
	if (chn == 0)
	{
		return 37;
	}
	else if (chn < 12)
	{
		return chn - 1;
	}
	else if (chn == 12)
	{
		return 38;
	}
	else if (chn < 39)
	{
		return chn - 2;
	}
	else
	{
		return 39;
	}
}

void phyTest_PRBS9 (u8 *p, int n)
{
	//PRBS9: (x >> 1) | (((x<<4) ^ (x<<8)) & 0x100)
	u16 x = 0x1ff;
	for (int i=0; i<n; i++)
	{
		u8 d = 0;
		for (int j=0; j<8; j++)
		{
			if (x & 1)
			{
				d |= BIT(j);
			}
			x = (x >> 1) | (((x<<4) ^ (x<<8)) & 0x100);
		}
		*p++ = d;
	}
}

void phyTest_PRBS15 (u8 *p, int n)
{
	u16 x = 0x7fff;
	for (int i=0; i<n; i++)
	{
		u8 d = 0;
		for (int j=0; j<8; j++)
		{
			if (x & 1)
			{
				d |= BIT(j);
			}
			x = (x >> 1) | (((x<<13) ^ (x<<14)) & 0x4000);
		}
		*p++ = d;
	}
}

ble_sts_t blt_phyTest_setReceiverTest_V1 (u8 rx_chn)
{

	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}

	blt_phyTest.pkts = 0;
	rf_set_tx_rx_off_auto_mode();
	rf_set_ble_chn ( phyTest_Channel(rx_chn) );
	rf_start_fsm(FSM_SRX, NULL,reg_system_tick);
	rf_set_rx_settle_time(85);
	rf_set_rx_maxlen(0xff);//max length is 255
	blt_phyTest.cmd = PHY_CMD_RX;
	return BLE_SUCCESS;
}


ble_sts_t blt_phyTest_2wireUart_setTransmitterTest_V1(u8 tx_chn, u8 length, u8 pkt_type,rf_mode_e mod)
{
	unsigned int transLen;
	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}


	if (pkt_type == PKT_TYPE_PRBS9)
	{
		pkt_phytest[4] = 0;
		phyTest_PRBS9 (pkt_phytest + 6, length);
	}
	else if (pkt_type == PKT_TYPE_0X0F)
	{
		pkt_phytest[4] = 1;
		memset (pkt_phytest + 6, 0x0f, length);
	}
	else if(pkt_type == PKT_TYPE_0X55)
	{
		pkt_phytest[4] = 2;
		memset (pkt_phytest + 6, 0x55, length);
	}
	else if(pkt_type == PKT_TYPE_0XFF)
	{
		if(mod == RF_MODE_LR_S2_500K || mod == RF_MODE_LR_S8_125K)
		{
			pkt_phytest[4] = 4;
			memset (pkt_phytest + 6, 0xff, length);
		}
	}
	transLen = length + 2;
	transLen = rf_tx_packet_dma_len(transLen);
	pkt_phytest[3] = (transLen >> 24)&0xff;
	pkt_phytest[2] = (transLen >> 16)&0xff;
	pkt_phytest[1] = (transLen >> 8)&0xff;
	pkt_phytest[0] = transLen & 0xff;
	pkt_phytest[5] = length;
	rf_set_ble_chn ( phyTest_Channel(tx_chn) );
	rf_set_tx_rx_off_auto_mode();
	reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
	rf_set_tx_dma(0,288);

	blt_phyTest.pkts = 0;
	blt_phyTest.tx_start = 1;
	blt_phyTest.cmd = PHY_CMD_TX;

	return BLE_SUCCESS;
}


ble_sts_t blt_phyTest_hci_setTransmitterTest_V1(u8 tx_chn, u8 length, u8 pkt_type)
{
	unsigned int transLen;
	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}
	if (pkt_type == PKT_TYPE_HCI_PRBS9)
	{
		pkt_phytest[4] = 0;
		phyTest_PRBS9 (pkt_phytest + 6, length);
	}
	else if (pkt_type == PKT_TYPE_HCI_0X0F)
	{
		pkt_phytest[4] = 1;
		memset (pkt_phytest + 6, 0x0f, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0X55)
	{
		pkt_phytest[4] = 2;
		memset (pkt_phytest + 6, 0x55, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_PRBS15)
	{
		pkt_phytest[4] = 3;
		phyTest_PRBS15 (pkt_phytest + 6, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XFF)
	{
		pkt_phytest[4] = 4;
		memset (pkt_phytest + 6, 0xff, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0X00)
	{
		pkt_phytest[4] = 5;
		memset (pkt_phytest + 6, 0x00, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XF0)
	{
		pkt_phytest[4] = 6;
		memset (pkt_phytest + 6, 0xf0, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XAA)
	{
		pkt_phytest[4] = 7;
		memset (pkt_phytest + 6, 0xaa, length);
	}
	transLen = length + 2;
	transLen = rf_tx_packet_dma_len(transLen);
	pkt_phytest[3] = (transLen >> 24)&0xff;
	pkt_phytest[2] = (transLen >> 16)&0xff;
	pkt_phytest[1] = (transLen >> 8)&0xff;
	pkt_phytest[0] = transLen & 0xff;
	pkt_phytest[5] = length;
	rf_set_ble_chn ( phyTest_Channel(tx_chn) );
	reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
	rf_set_tx_dma(0,288);
	blt_phyTest.pkts = 0;
	blt_phyTest.tx_start = 1;
	blt_phyTest.cmd = PHY_CMD_TX;
	return BLE_SUCCESS;
}

ble_sts_t blt_phyTest_hci_setReceiverTest_V2 (u8 rx_chn,u8 phy_mode,u8 modulation_index)
{
	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}
	blt_phyTest.pkts = 0;
	rf_set_ble_chn ( phyTest_Channel(rx_chn) );
	if(phy_mode == 0x01)
	{
		blt_InitPhyTestDriver(RF_MODE_BLE_1M);
	}
	else if(phy_mode == 0x02)
	{
		blt_InitPhyTestDriver(RF_MODE_BLE_2M);
	}
	else if(phy_mode == 0x03)
	{
		blt_InitPhyTestDriver(RF_MODE_LR_S2_500K);
	}
	if(modulation_index == 0x00)
	{}
	else if(modulation_index == 0x01)
	{}

	rf_set_tx_rx_off_auto_mode();
	rf_set_ble_chn ( phyTest_Channel(rx_chn) );
	rf_start_fsm(FSM_SRX, NULL,reg_system_tick);
	rf_set_rx_settle_time(85);
	rf_set_rx_maxlen(0xff);//max length is 255
	blt_phyTest.cmd = PHY_CMD_RX;
	return BLE_SUCCESS;
}



ble_sts_t blt_phyTest_hci_setTransmitterTest_V2 (u8 tx_chn, u8 length, u8 pkt_type,u8 phy_mode)
{
	unsigned int transLen;
	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}
	if(phy_mode == 0x01)
	{
		blt_InitPhyTestDriver(RF_MODE_BLE_1M);
	}
	else if(phy_mode == 0x02)
	{
		blt_InitPhyTestDriver(RF_MODE_BLE_2M);
	}
	else if(phy_mode == 0x03)
	{
		blt_InitPhyTestDriver(RF_MODE_LR_S8_125K);
	}
	else if(phy_mode == 0x04)
	{
		blt_InitPhyTestDriver(RF_MODE_LR_S2_500K);
	}
	if (pkt_type == PKT_TYPE_HCI_PRBS9)
	{
		pkt_phytest[4] = 0;
		phyTest_PRBS9 (pkt_phytest + 6, length);
	}
	else if (pkt_type == PKT_TYPE_HCI_0X0F)
	{
		pkt_phytest[4] = 1;
		memset (pkt_phytest + 6, 0x0f, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0X55)
	{
		pkt_phytest[4] = 2;
		memset (pkt_phytest + 6, 0x55, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_PRBS15)
	{
		pkt_phytest[4] = 3;
		phyTest_PRBS15 (pkt_phytest + 6, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XFF)
	{
		pkt_phytest[4] = 4;
		memset (pkt_phytest + 6, 0xff, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0X00)
	{
		pkt_phytest[4] = 5;
		memset (pkt_phytest + 6, 0x00, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XF0)
	{
		pkt_phytest[4] = 6;
		memset (pkt_phytest + 6, 0xf0, length);
	}
	else if(pkt_type == PKT_TYPE_HCI_0XAA)
	{
		pkt_phytest[4] = 7;
		memset (pkt_phytest + 6, 0xaa, length);
	}
	transLen = length + 2;
	transLen = rf_tx_packet_dma_len(transLen);
	pkt_phytest[3] = (transLen >> 24)&0xff;
	pkt_phytest[2] = (transLen >> 16)&0xff;
	pkt_phytest[1] = (transLen >> 8)&0xff;
	pkt_phytest[0] = transLen & 0xff;
	pkt_phytest[5] = length;
	rf_set_ble_chn ( phyTest_Channel(tx_chn) );
	reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
	rf_set_tx_dma(0,288);
	blt_phyTest.pkts = 0;
	blt_phyTest.tx_start = 1;
	blt_phyTest.cmd = PHY_CMD_TX;
	return BLE_SUCCESS;
}

ble_sts_t blt_phyTest_setTestEnd (u8 *pkt_num)
{
	if(!bltParam.phy_en){  //must set phy mode
		blc_phy_setPhyTestEnable(BLC_PHYTEST_ENABLE);
	}


	pkt_num[0] = U16_LO(blt_phyTest.pkts);
	pkt_num[1] = U16_HI(blt_phyTest.pkts);

	rf_set_tx_rx_off ();

	blt_phyTest.pkts = 0;  //clear


	blt_phyTest.cmd = PHY_CMD_END;
	return BLE_SUCCESS;
}

ble_sts_t blt_phyTest_2wire_setReset(void)
{

	STOP_RF_STATE_MACHINE;
	rf_set_tx_rx_off ();
	reg_rf_irq_status = 0xffff;

	blt_phyTest.pkts = 0;

	return BLE_SUCCESS;
}


ble_sts_t blc_phy_setPhyTestEnable (u8 en)
{
	u32 r = irq_disable();


	if(en && !bltParam.phy_en)
	{
		systimer_irq_disable();
		systimer_clr_irq_status();
		reg_rf_irq_mask = 0;
		CLEAR_ALL_RFIRQ_STATUS;

		bltParam.blt_state = BLS_LINK_STATE_IDLE;

		rf_access_code_comm(0x29417671);
		rf_set_rx_dma(phytest_rx_fifo, 1, 288);

		rf_mode_init();
		rf_set_ble_1M_NO_PN_mode();
		rf_pn_disable();
		write_reg8(0x140802,0x4b);
		#if TX_PKT_SHARE_SAVE_RAM
				//todo why not use blt_txfifo
				pkt_phytest = phytest_tx_fifo;
				*(u32 *)pkt_phytest = 39;
				pkt_phytest[4] = 0;
				pkt_phytest[5] = 37;
		#endif
	}

	else if(!en && bltParam.phy_en)
	{
		start_reboot();  //clear all status
	}



	bltParam.phy_en = en;


	irq_restore(r);

	return BLE_SUCCESS;
}

bool 	  blc_phy_isPhyTestEnable(void)
{
	return bltParam.phy_en;
}


int blt_phytest_cmd_handler (u8 *p, int n)
{
	//Commands and Events are sent most significant byte (MSB) first, followed
	//by the least significant byte (LSB).
	blt_phyTest.cmd = p[0] >> 6;
	u16 phy_event = 0;


	if (blt_phyTest.cmd == PHY_CMD_SETUP)		//reset
	{
		u8 ctrl = p[0]&0x3f;
		u8 para = (p[1] >> 2)&0x3f;

		if(ctrl==0)
		{
			if(para==0)
			{
				pkt_length.l.upper =0;
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_FAIL;
			}

			blt_InitPhyTestDriver(RF_MODE_BLE_1M);
		}
		else if(ctrl== 1)
		{
			if((para>=0) && (para<=3))
			{
				pkt_length.l.upper = para &0x03;
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_FAIL;
			}
		}
		else if(ctrl==2)
		{
			if(para==1)//BLE 1M
			{
				blt_InitPhyTestDriver(RF_MODE_BLE_1M);
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else if(para==2)//BLE 2M
			{
				blt_InitPhyTestDriver(RF_MODE_BLE_2M);
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else if(para==3)//s=8
			{
				blt_InitPhyTestDriver(RF_MODE_LR_S8_125K);
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else if(para==4)//s=2
			{
				blt_InitPhyTestDriver(RF_MODE_LR_S2_500K);
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
			}
			else
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_FAIL;
			}
		}
		else if(ctrl==3)
		{
			//TODO standard modulation / stable modulation
			phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
		}
		else if(ctrl==4)
		{
			phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS | BIT(1) | BIT(2);
		}
		else if(ctrl==5)
		{
			if(para==0)
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS | (251<<1);
			}
			else if(para==1)
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS | (17040 << 1);
			}
			else if(para==2)
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS | (251<<1);
			}
			else if(para==3)
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS | (17040 << 1);
			}
			else
			{
				phy_event = PHY_EVENT_STATUS | PHY_STATUS_FAIL;
			}
		}
		blt_phyTest_2wire_setReset();
	}
	else if (blt_phyTest.cmd == PHY_CMD_RX)	//rx
	{
		u8 chn =  p[0] & 0x3f;
		pkt_length.l.low  = (p[1] >> 2) & 0x3f;

		blt_phyTest_setReceiverTest_V1(chn);
		phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
	}
	else if (blt_phyTest.cmd == PHY_CMD_TX)	//tx
	{
		u8 chn =  p[0] & 0x3f;
		u8 pkt_type = p[1] & 0x03;
		pkt_length.l.low =  (p[1] >> 2) & 0x3f;

		blt_phyTest_2wireUart_setTransmitterTest_V1(chn, pkt_length.len, pkt_type, phy_test_mode);
		phy_event = PHY_EVENT_STATUS | PHY_STATUS_SUCCESS;
	}
	else  if(blt_phyTest.cmd == PHY_CMD_END)				//end
	{
		u16 pkt_num;
		phy_event = PHY_EVENT_PKT_REPORT | blt_phyTest.pkts;
		blt_phyTest_setTestEnd((u8 *)&pkt_num);
	}

	u8 returnPara[2] = {phy_event>>8, phy_event};
	blc_hci_send_data(HCI_FLAG_EVENT_PHYTEST_2_WIRE_UART, returnPara, 2);


	return 0;
}

unsigned int blt_getPktInterval(unsigned char payload_len, rf_mode_e mode)
{
	unsigned int total_len,byte_time=8;
	unsigned char preamble_len;
	unsigned int total_time;

	if(mode==RF_MODE_BLE_1M)//1m
	{
		preamble_len = read_reg8(0x140802) & 0x1f ;
		total_len = preamble_len + 4 + 2 + payload_len +3; // preamble + access_code + header + payload + crc
		byte_time = 8;
		return (((byte_time * total_len + 249  + 624)/625)*625);
	}
	else if(mode==RF_MODE_BLE_2M)//2m
	{
		preamble_len = read_reg8(0x140802) & 0x1f ;
		total_len = preamble_len + 4 + 2 + payload_len +3; // preamble + access_code + header + payload + crc
		byte_time = 4;
		return (((byte_time * total_len + 249  + 624)/625)*625);
	}
	else if(mode==RF_MODE_LR_S2_500K) // s=2
	{
		byte_time = 2;	//2us/bit
		total_time = (80 + 256 + 16 + 24) + (16 + payload_len*8 + 24 +3)*byte_time; // preamble + access_code + coding indicator + TERM1 + header + payload + crc + TERM2
		return (((total_time + 249  + 624)/625)*625);
	}
	else if(mode==RF_MODE_LR_S8_125K)//s=8
	{
		byte_time = 8;	//8us/bit
		total_time = (80 + 256 + 16 + 24) + (16 + payload_len*8 + 24 +3)*byte_time; // preamble + access_code + coding indicator + TERM1 + header + payload + crc + TERM2
		return (((total_time + 249  + 624)/625)*625);
	}
	return 0;
}

_attribute_ram_code_ int blt_phyTest_main_loop(void)
{
	//phytest depend on blc_hci_rx_handler/blc_hci_tx_handler, so it must before phytest
	//------------------   HCI -------------------------------
	extern blc_hci_rx_handler_t		blc_hci_rx_handler;
	extern blc_hci_tx_handler_t		blc_hci_tx_handler;
	///////// RX //////////////
	if (blc_hci_rx_handler)
	{
		blc_hci_rx_handler ();
	}
	///////// TX //////////////
	if (blc_hci_tx_handler)
	{
		blc_hci_tx_handler ();
	}


	if (blt_phyTest.cmd == PHY_CMD_TX)
	{
		if (reg_rf_irq_status & FLD_RF_IRQ_TX || blt_phyTest.tx_start)
		{
			if(blt_phyTest.tx_start)
			{
				blt_phyTest.tick_tx = clock_time();
				blt_phyTest.tx_start = 0;
			}
			else
			{
				blt_phyTest.pkts ++;
			}

			rf_set_tx_rx_off_auto_mode();
			rf_start_fsm(FSM_STX,pkt_phytest, blt_phyTest.tick_tx);
			blt_phyTest.tick_tx += (blt_getPktInterval(pkt_length.len, phy_test_mode) * SYSTEM_TIMER_TICK_1US ) ;//625*SYSTEM_TIMER_TICK_1US;//
			reg_rf_irq_status = FLD_RF_IRQ_TX;
		}
	}


	else if (blt_phyTest.cmd == PHY_CMD_RX)
	{
		if (reg_rf_irq_status & FLD_RF_IRQ_RX)
		{
			reg_rf_irq_status = FLD_RF_IRQ_RX;
			u8 * raw_pkt = (u8 *)phytest_rx_fifo;
//			if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )
			if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) && (REG_ADDR8(0x140840)&0xf0)==0)
			{
				blt_phyTest.pkts++;
			}
			rf_set_tx_rx_off_auto_mode();
			rf_start_fsm(FSM_SRX, NULL,reg_system_tick);
			rf_set_rx_settle_time(85);
		}
	}



	return 0;
}



_attribute_ram_code_ int blc_phyTest_hci_rxUartCb (void)
{
	if(hci_fifo_get(&bltHci_rxfifo) == 0)
	{
		return 0;
	}

	if(rf_receiving_flag() != 0)
	{
		return 0;
	}

	u8* p = hci_fifo_get(&bltHci_rxfifo);
	u8  rx_len = p[0]; //usually <= 255 so 1 byte should be sufficient

	if (rx_len)
	{
		blc_hci_handler(&p[4], rx_len);

		hci_fifo_pop(&bltHci_rxfifo);
	}

	return 0;
}

_attribute_ram_code_ int blc_phyTest_2wire_rxUartCb (void)
{
	if(hci_fifo_get(&bltHci_rxfifo) == 0)
	{
		return 0;
	}

	if(rf_receiving_flag() != 0)
	{
		return 0;
	}

	u8* p = hci_fifo_get(&bltHci_rxfifo);
	u8 rx_len = p[0];

	if (rx_len)
	{
		blt_phytest_cmd_handler(&p[4], rx_len);
		hci_fifo_pop(&bltHci_rxfifo);
	}
	return 0;
}


_attribute_ram_code_ int blc_phyTest_2wire_txUartCb(void)
{
	if(hci_fifo_get(&bltHci_txfifo) == 0)
	{
		return 0;
	}
//
	u8 *p = hci_fifo_get (&bltHci_txfifo);

	if (p)
	{
		memcpy(&uart_txdata_buf.data, p + 2, p[0]+p[1]*256);
		uart_txdata_buf.len = p[0]+p[1]*256 ;
		uart_send_dma(UART0,uart_txdata_buf.data,uart_txdata_buf.len);
		hci_fifo_pop (&bltHci_txfifo);
	}
	return 0;
}


void blt_InitPhyTestDriver(rf_mode_e rf_mode)
{

	if(rf_mode == RF_MODE_BLE_1M)
	{
		if(phy_test_mode != RF_MODE_BLE_1M)
		{
			rf_ble_switch_phy(BLE_PHY_1M,0);
			phy_test_mode = RF_MODE_BLE_1M;
		}
	}
	else if(rf_mode == RF_MODE_BLE_2M)
	{
		if(phy_test_mode != RF_MODE_BLE_2M)
		{
			rf_ble_switch_phy(BLE_PHY_2M,0);
			phy_test_mode = RF_MODE_BLE_2M;
		}
	}
	else if(rf_mode == RF_MODE_LR_S2_500K)
	{
		if(phy_test_mode != RF_MODE_LR_S2_500K)
		{
			rf_ble_switch_phy(BLE_PHY_CODED,LE_CODED_S2);
			phy_test_mode = RF_MODE_LR_S2_500K;
		}
	}
	else if(rf_mode == RF_MODE_LR_S8_125K)
	{
		if(phy_test_mode != RF_MODE_LR_S8_125K)
		{
			rf_ble_switch_phy(BLE_PHY_CODED,LE_CODED_S8);
			phy_test_mode = RF_MODE_LR_S8_125K;
		}
	}
	rf_pn_disable();
}



