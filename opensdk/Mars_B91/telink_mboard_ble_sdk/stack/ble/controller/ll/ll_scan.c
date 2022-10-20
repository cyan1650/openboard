/********************************************************************************************************
 * @file	ll_scan.c
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




extern u8		blc_adv_channel[];

u32		blts_scan_interval;


_attribute_data_retention_ _attribute_aligned_(4) st_ll_scan_t  blts;




rf_packet_scan_req_t	pkt_scan_req = {
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		0x800004,								// dma_len, rf_tx_packet_dma_len(pkt_scan_req.rf_len + 2);//4 bytes align
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		sizeof (rf_packet_scan_req_t) - 4,		// dma_len
	#endif

		LL_TYPE_SCAN_REQ,						// type
		0,
		0,
		0,
		0,

		0x0C,									// rf_len, sizeof (rf_packet_scan_req_t) - 6
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,},	// scanA
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,},   //advA
};





	///// scanning module ////
int  blc_ll_procScanPkt(u8 *raw_pkt, u8 *new_pkt, u32 tick_now);
int  blc_ll_procScanData(u8 *raw_pkt);

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	void blc_ll_initScanning_module(void)
	{
		blc_ll_procScanPktCb = blc_ll_procScanPkt;
		blc_ll_procScanDatCb = blc_ll_procScanData;


		smemcpy(pkt_scan_req.scanA, bltMac.macAddress_public, BLE_ADDR_LEN);
		blts_scan_interval = 60000 * SYSTEM_TIMER_TICK_1US;  //60ms
	}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	void blc_ll_initScanning_module(u8 *public_adr)
	{
		blc_ll_procScanPktCb = blc_ll_procScanPkt;
		blc_ll_procScanDatCb = blc_ll_procScanData;


		memcpy(pkt_scan_req.scanA, public_adr, BLE_ADDR_LEN);
		blts_scan_interval = 60000 * SYSTEM_TIMER_TICK_1US;  //60ms
	}
#endif










#define			NUM_OF_SCAN_DEVICE				16
#define			NUM_OF_SCAN_RSP_DEVICE			8

u8		blc_scan_device[NUM_OF_SCAN_DEVICE][8];
u8		blc_scanRsp_device[NUM_OF_SCAN_RSP_DEVICE][8];


/*	This code in RF irq and system irq put in RAM by force
 * Because of the flash resource contention problem, when the
 * flash access is interrupted by a higher priority interrupt,
 * the interrupt processing function cannot operate the flash
*/
#if (STACK_IRQ_CODE_IN_SRAM_DUE_TO_FLASH_OPERATION)
_attribute_ram_code_
#endif
void blc_ll_switchScanChannel (int scan_mode, int set_chn)
{

#if (LL_FEATURE_ENABLE_LE_2M_PHY || LL_FEATURE_ENABLE_LE_CODED_PHY)
	if(ll_phy_switch_cb){
		if(BLE_PHY_1M != bltPHYs.cur_llPhy){   //switch back to 1M PHY
			ll_phy_switch_cb(BLE_PHY_1M, LE_CODED_S2);
		}
	}
#endif

	static u32	bls_scan_num = 0;
	static u32	tick_scan = 0;

	int scan_window_hit = (u32)(clock_time() - tick_scan) > (blts_scan_interval - 500);
	if (scan_window_hit)
	{
		tick_scan = clock_time ();
	}


	if (scan_window_hit || set_chn)
	{
		++bls_scan_num;
	}




	u8 chn = blc_adv_channel[bls_scan_num%3];
	rf_set_tx_rx_off ();
	rf_set_ble_channel (chn);
	blt_ll_set_ble_access_code_adv ();
	rf_set_ble_crc_adv ();
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
	rf_set_rx_maxlen(37);
	#endif
	if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_RX_ON);  }

	CLEAR_ALL_RFIRQ_STATUS;
	rf_set_rxmode ();

	if(scan_mode){

		systimer_set_irq_capture( clock_time() + blts_scan_interval);
	}
}



int	blc_ll_filterAdvDevice (u8 type, u8 * mac)
{
	static u8 duplicate_filter = 0;
	if (!mac)
	{
		duplicate_filter = type;
		blts.scanDevice_num = 0;
		return 0;
	}
	if (!duplicate_filter)
	{
		return 0;
	}
	for (int i=0; i<blts.scanDevice_num; i++)
	{
		if (blc_scan_device[i][0] && type == blc_scan_device[i][1] && memcmp (mac, blc_scan_device[i] + 2, 6) == 0)
		{
			return 1;
		}
	}
	if (blts.scanDevice_num >= NUM_OF_SCAN_DEVICE)
	{
		blts.scanDevice_num = NUM_OF_SCAN_DEVICE - 1;
		smemcpy (blc_scan_device[0], blc_scan_device[1], 8 * (NUM_OF_SCAN_DEVICE - 1));
	}
	blc_scan_device[blts.scanDevice_num][0] = 1;
	blc_scan_device[blts.scanDevice_num][1] = type;
	smemcpy (blc_scan_device[blts.scanDevice_num] + 2, mac, 6);
	blts.scanDevice_num++;

	return 0;
}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_ram_code_
#endif
int blc_ll_addScanRspDevice(u8 type, u8 *mac)
{
	if (blts.scanRspDevice_num >= NUM_OF_SCAN_RSP_DEVICE)
	{
		blts.scanRspDevice_num = NUM_OF_SCAN_RSP_DEVICE - 1;
		smemcpy (blc_scanRsp_device[0], blc_scanRsp_device[1], 8 * (NUM_OF_SCAN_RSP_DEVICE - 1));
	}
	blc_scanRsp_device[blts.scanRspDevice_num][1] = type;
	smemcpy (blc_scanRsp_device[blts.scanRspDevice_num] + 2, mac, 6);
	blts.scanRspDevice_num++;

	return 1;
}

void blc_ll_clearScanRspDevice(void)
{
	blts.scanRspDevice_num = 0;
}


_attribute_ram_code_ bool blc_ll_isScanRspReceived(u8 type, u8 *mac)
{
	u16 *mac16= (u16 *)mac;

	for (int i=0; i<blts.scanRspDevice_num; i++)
	{
		u16 *dev16 = (u16*)(blc_scanRsp_device[i] + 2);
		if (type == blc_scanRsp_device[i][1] && MAC_MATCH16(mac16, dev16))
		{
			return 1;
		}
	}


	return 0;
}



ll_procScanPkt_callback_t  blc_ll_procScanPktCb = NULL;
ll_procScanDat_callback_t  blc_ll_procScanDatCb = NULL;

ble_sts_t   bls_ll_setScanReqAllEnable(u8 scan_req_all_enable)
{
	blts.scan_req_all_en=scan_req_all_enable;
	return BLE_SUCCESS;
}



_attribute_ram_code_ int  blc_ll_procScanPkt(u8 *raw_pkt, u8 *new_pkt, u32 tick_now)
{

	rf_packet_adv_t * pAdv = (rf_packet_adv_t *) (raw_pkt + DMA_RFRX_LEN_HW_INFO);
	u8 txAddrType = pAdv->txAddr;
	u8 rxAddrType = pAdv->rxAddr;
	u8 adv_type = pAdv->type;

	int activeScan_pkt = blts.scan_type == SCAN_TYPE_ACTIVE && (adv_type==LL_TYPE_ADV_IND || adv_type ==LL_TYPE_ADV_SCAN_IND);

	if(activeScan_pkt){
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			ble_rf_set_tx_dma(0, 3);  //48/16=3

			/* make sure tx_wptr = tx_rptr, can solve one problem: prevent TX packet come from Hardware TX FIFO
			 * Kite/826x can use one register to disable HW TX FIFO, but Eagle is difficult to control(confirmed by QiangKai)
			 * TX_wptr & TX_rptr should recover in next connection state*/
			reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
		#endif
		rf_tx_settle_adjust(LL_SCAN_TX_SETTLE);
		rf_ble_tx_on ();
		u32 t = reg_rf_timestamp + ((((raw_pkt[DMA_RFRX_OFFSET_RFLEN]+5)<<3) + 28) << 4);
		u32 diff = t - clock_time();
		if( diff > BIT(10) ){  //64us *16
			t = clock_time();
		}

		rf_start_fsm(FSM_STX,(void *)&pkt_scan_req, t);
	}



	int advPkt_valid = 0;
	if(adv_type < 3 || adv_type==LL_TYPE_ADV_SCAN_IND)
	{
		if(!blts.scan_filterPolicy || ll_searchAddr_in_WhiteList_and_ResolvingList(txAddrType, pAdv->advA) )
		{
			u16 *initA16 = (u16*)pAdv->data;
			u16 *scanA16 = (u16*)pkt_scan_req.scanA;
			if( adv_type != LL_TYPE_ADV_DIRECT_IND || ( rxAddrType==pkt_scan_req.txAddr && MAC_MATCH16(initA16 , scanA16)) )
			{
				advPkt_valid = 1;
			}
		}
	}


	if(advPkt_valid && activeScan_pkt && (blts.scan_req_all_en||!blc_ll_isScanRspReceived(txAddrType, pAdv->advA)) )
	{
#if 0
		u32 t = reg_rf_timestamp + ((((raw_pkt[DMA_RFRX_OFFSET_RFLEN]+5)<<3) + 28) << 4);
		u32 diff = t - clock_time();
		if( diff > BIT(10) ){  //64us *16
			t = clock_time();
		}

		rf_start_fsm(FSM_STX,(void *)&pkt_scan_req, t);
#endif

		reg_rf_irq_status = FLD_RF_IRQ_TX;
		pkt_scan_req.rxAddr = txAddrType;

#if (MCU_CORE_TYPE == MCU_CORE_9518) ///not use smemcpy is to remove warning ???Confusing, to check.
		*(u16 *)(pkt_scan_req.advA + 0) = *(u16 *)(pAdv->advA + 0);
		*(u16 *)(pkt_scan_req.advA + 2) = *(u16 *)(pAdv->advA + 2);
		*(u16 *)(pkt_scan_req.advA + 4) = *(u16 *)(pAdv->advA + 4);
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		smemcpy(pkt_scan_req.advA, pAdv->advA, 6);
#endif

		volatile u32 *ph  = (u32 *) (new_pkt + DMA_RFRX_OFFSET_HEADER);
		ph[0] = 0;  //clear mark

		activeScan_pkt = 2;
		while ( !(reg_rf_irq_status & FLD_RF_IRQ_TX) && (u32)(clock_time() - tick_now) < 356 * SYSTEM_TIMER_TICK_1US);  //176 + 150 + 30


		rf_ble_tx_done ();
		rf_set_rxmode ();

		u32 rx_begin_tick = clock_time();

		reg_rf_irq_status = FLD_RF_IRQ_TX;  //clear

		while (!(*ph) && (u32)(clock_time() - rx_begin_tick) < 300 * SYSTEM_TIMER_TICK_1US); //150 + pkt(22*8) + 150 + margin(50)

		if (*ph)
		{
			u32 rx_begin_tick = clock_time ();
																// (31+10)*8 + margin(50)
			while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 378 * SYSTEM_TIMER_TICK_1US);
			STOP_RF_STATE_MACHINE;
			reg_rf_irq_status = FLD_RF_IRQ_RX;

			if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(new_pkt) && RF_BLE_RF_PACKET_CRC_OK(new_pkt) )

			{
				rf_packet_scan_rsp_t * pRsp = (rf_packet_scan_rsp_t *) (new_pkt + DMA_RFRX_LEN_HW_INFO);
				u16 *rspAdv16 = (u16*)pRsp->advA;
				u16 *reqAdv16 = (u16*)pkt_scan_req.advA;

				if(MAC_MATCH16(rspAdv16, reqAdv16)){
					blt_rxfifo.wptr ++;
			#if (MCU_CORE_TYPE == MCU_CORE_9518)
				#if 0//TODO: Sihui, u16 is error
					reg_dma_rf_rx_addr = (u16)(u32) (blt_rxfifo.p_base + (blt_rxfifo.wptr & blt_rxfifo.mask) * blt_rxfifo.size); //set next buffer
				#endif
			#elif(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
				reg_dma_rf_rx_addr = (u16)(u32) (blt_rxfifo_b + (blt_rxfifo.wptr & (blt_rxfifo.num-1)) * blt_rxfifo.size); //set next buffer
			#endif
					blc_ll_addScanRspDevice(txAddrType, pAdv->advA);
				}
			}
		}

	}
	else{
		STOP_RF_STATE_MACHINE;
	}


	if(activeScan_pkt == 1)
	{
		rf_ble_tx_done ();
		rf_set_rxmode ();
	}

	return advPkt_valid;
}

u8				blm_scan_type;

int  blc_ll_procScanData(u8 *raw_pkt)
{
	if(hci_le_eventMask & HCI_LE_EVT_MASK_ADVERTISING_REPORT)
	{
		if(blts.scan_en || blts.scan_extension_mask)
		{
			u8 type = raw_pkt[DMA_RFRX_OFFSET_HEADER] & 15;
			// adv: 6 AdvData0--31  31+6 = 37
			if ( (type < 3 || type == LL_TYPE_SCAN_RSP || type == LL_TYPE_ADV_SCAN_IND) && ((raw_pkt[DMA_RFRX_OFFSET_RFLEN]<=37) && (raw_pkt[DMA_RFRX_OFFSET_RFLEN]>=6)) )
			{
				//rf_len <= 37 is OK, if bigger than 37, temp_buff may be Error
				//rf_len >= 6 is OK, if lesser than 6, below execute pa->len = len - 6, and memcpy can cause a stack overflow
#if 1
				u8 temp_buff[48];  //14 + 31
				event_adv_report_t * pa = (event_adv_report_t *) temp_buff;

				u8 len = raw_pkt[DMA_RFRX_OFFSET_RFLEN];

				//note that: direct_adv should not report data[0..5](initA)  (BQB result)
				if(type == LL_TYPE_ADV_DIRECT_IND){
					if(len < 12){// Adv_direct_IND include AdvA and TargetA, so the len must larger than 12
						return false;
					}
					len = len - 6;
				}

				pa->subcode = HCI_SUB_EVT_LE_ADVERTISING_REPORT;
				pa->nreport = 1;


				//note that: adv report event type value different from adv type
				if(type == LL_TYPE_ADV_NONCONN_IND){
					pa->event_type = ADV_REPORT_EVENT_TYPE_NONCONN_IND;
				}
				else if(type == LL_TYPE_ADV_SCAN_IND){
					pa->event_type = ADV_REPORT_EVENT_TYPE_SCAN_IND;
				}
				else{
					pa->event_type = type;
				}

				pa->adr_type = raw_pkt[DMA_RFRX_OFFSET_HEADER] & BIT(6) ? 1 : 0;
				smemcpy (pa->mac, raw_pkt + DMA_RFRX_OFFSET_HEADER + 2, 6);

				if (type == LL_TYPE_SCAN_RSP){

				}
				else if(blc_ll_filterAdvDevice (pa->adr_type, pa->mac)){
					return 0;
				}

				pa->len = len - 6;

				smemcpy(pa->data, raw_pkt + DMA_RFRX_OFFSET_HEADER + 8, pa->len);

				pa->data[pa->len] = raw_pkt[DMA_RFRX_OFFSET_RSSI(raw_pkt)] - 110;

#else
				event_adv_report_t * pa = (event_adv_report_t *) (raw_pkt + DMA_RFRX_OFFSET_HEADER - 3);

				u8 len = raw_pkt[DMA_RFRX_OFFSET_RFLEN];

				//note that: direct_adv should not report data[0..5](initA)  (BQB result)
				if(type == LL_TYPE_ADV_DIRECT_IND){
					len = raw_pkt[DMA_RFRX_OFFSET_RFLEN] - 6;
				}


				pa->subcode = HCI_SUB_EVT_LE_ADVERTISING_REPORT;
				pa->nreport = 1;


				//note that: adv report event type value different from adv type
				if(type == LL_TYPE_ADV_NONCONN_IND){
					pa->event_type = ADV_REPORT_EVENT_TYPE_NONCONN_IND;
				}
				else if(type == LL_TYPE_ADV_SCAN_IND){
					pa->event_type = ADV_REPORT_EVENT_TYPE_SCAN_IND;
				}
				else{
					pa->event_type = type;
				}

				pa->adr_type = raw_pkt[DMA_RFRX_OFFSET_HEADER] & BIT(6) ? 1 : 0;
				smemcpy (pa->mac, raw_pkt+DMA_RFRX_OFFSET_DATA, 6);

				if (type == LL_TYPE_SCAN_RSP){

				}
				else if(blc_ll_filterAdvDevice (pa->adr_type, pa->mac)){
					return 0;
				}

				pa->len = len - 6;
				pa->data[pa->len] = raw_pkt[DMA_RFRX_OFFSET_RSSI(raw_pkt)] - 110;

#endif

				//blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, (u8 *)pa, len + 6);
				blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, (u8 *)pa, len + (blm_scan_type & 0x80 ? 8 : 6));

			}
		}
	}

	return 1;
}





ble_sts_t blc_ll_setScanParameter (scan_type_t scan_type, u16 scan_interval, u16 scan_window, own_addr_type_t  ownAddrType, scan_fp_type_t scanFilter_policy)
{
	if(bltParam.scan_hci_cmd & SCAN_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.scan_hci_cmd |= SCAN_LEGACY_MASK;


	blm_scan_type = (u8)scan_type;


	blts.scan_type = (u8)scan_type;		//0 for passive scan; 1 for active scan
	blts_scan_interval = scan_interval * SYSTEM_TIMER_TICK_625US;
	blts.scan_filterPolicy = (u8)scanFilter_policy;
	//window

	//adr_type
	if(ownAddrType == OWN_ADDRESS_PUBLIC){
		pkt_scan_req.txAddr = BLE_ADDR_PUBLIC;
		smemcpy(pkt_scan_req.scanA, bltMac.macAddress_public, BLE_ADDR_LEN);
	}
	else if(ownAddrType == OWN_ADDRESS_RANDOM){
		pkt_scan_req.txAddr = BLE_ADDR_RANDOM;
		smemcpy(pkt_scan_req.scanA, bltMac.macAddress_random, BLE_ADDR_LEN);
	}
	else{   //OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC / OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM
       //......
	}


	return BLE_SUCCESS;
}




ble_sts_t blc_ll_setScanEnable (scan_en_t scan_enable, dupFilter_en_t filter_duplicate)
{
	if(bltParam.scan_hci_cmd & SCAN_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.scan_hci_cmd |= SCAN_LEGACY_MASK;


	if(bltParam.blt_state & (BLS_LINK_STATE_ADV | BLS_LINK_STATE_CONN) ){
		return 	LL_ERR_CURRENT_STATE_NOT_SUPPORTED_THIS_CMD;
	}


	blts.scan_en = (u8)scan_enable;
	blts.filter_dup = (u8)filter_duplicate;


	blc_ll_filterAdvDevice (blts.filter_dup, 0);
	blc_ll_clearScanRspDevice();

	if(scan_enable){  //enable
		if(bltParam.blt_state != BLS_LINK_STATE_SCAN){  //idle -> scan

			//feedback from customer: they need scan state working ASAP after scan enabled
			//here 1000uS margin is to prevent some boundary error(for example, some IRQ triggers and cost a lot of time)
			systimer_set_irq_capture( clock_time () + 1000*SYSTEM_TIMER_TICK_1US);

			rf_tx_settle_adjust(LL_SCAN_TX_SETTLE);

			systimer_clr_irq_status();
			systimer_irq_enable();

			CLEAR_ALL_RFIRQ_STATUS;
			reg_rf_irq_mask = FLD_RF_IRQ_RX;
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			rf_set_rx_maxlen(37);
		#endif
			bltParam.blt_state = BLS_LINK_STATE_SCAN;
		}
	}
	else{  //disable
		if(bltParam.blt_state == BLS_LINK_STATE_SCAN){  //scan -> idle
			bltParam.blt_state = BLS_LINK_STATE_IDLE;
			systimer_irq_disable();


			reg_rf_irq_mask = 0;
			CLEAR_ALL_RFIRQ_STATUS;
			if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_OFF);  }
		}
	}

	//blta.adv_en = 0;   //@@@@@@@@@@@@@@@@@@@@

	return BLE_SUCCESS;
}




ble_sts_t    blc_ll_addScanningInAdvState(void)
{
	blts.scan_extension_mask |= BLS_FLAG_SCAN_IN_ADV_MODE;

	return BLE_SUCCESS;
}

ble_sts_t    blc_ll_removeScanningFromAdvState(void)
{
	blts.scan_extension_mask &= ~BLS_FLAG_SCAN_IN_ADV_MODE;

	return BLE_SUCCESS;
}



ble_sts_t    blc_ll_addScanningInConnSlaveRole(void)
{
	blts.scan_extension_mask |= BLS_FLAG_SCAN_IN_SLAVE_MODE;

	return BLE_SUCCESS;
}

ble_sts_t    blc_ll_removeScanningFromConnSLaveRole(void)
{
	blts.scan_extension_mask &= ~BLS_FLAG_SCAN_IN_SLAVE_MODE;

	return BLE_SUCCESS;
}





