/********************************************************************************************************
 * @file	ll_adv.c
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




_attribute_ble_data_retention_	_attribute_aligned_(4)	u8	blc_adv_channel[3] = {37, 38, 39};
_attribute_ble_data_retention_	_attribute_aligned_(4)	u32	blt_advExpectTime;
_attribute_ble_data_retention_	_attribute_aligned_(4)	u8 	advData_backup[8];
_attribute_ble_data_retention_	_attribute_aligned_(4)	u32 blc_rcvd_connReq_tick;

_attribute_ble_data_retention_	_attribute_aligned_(4)	ll_adv2conn_callback_t		ll_adv2conn_cb = NULL;
_attribute_ble_data_retention_	_attribute_aligned_(4)	ll_module_adv_callback_t	ll_module_adv_cb = NULL;
_attribute_ble_data_retention_	_attribute_aligned_(4)	ll_module_adv_callback_t	ll_module_advSlave_cb = NULL;

_attribute_ble_data_retention_	_attribute_aligned_(4)	st_ll_adv_t  blta;

#if (MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_ble_data_retention_  _attribute_aligned_(4)	u8 adv_rx_buff[64];
#endif

extern blt_event_callback_t		blt_p_event_callback ;
extern _attribute_aligned_(4) st_ll_conn_slave_t		bltc;
extern _attribute_aligned_(4) st_ll_pm_t  bltPm;
extern ll_adv2conn_callback_t ll_adv2conn_cb;






_attribute_ble_data_retention_
rf_packet_adv_t	pkt_adv = {
		8,										// dma_len

		LL_TYPE_ADV_IND,						// type
		0,										// RFU
		0,										// ChSel: only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr
		0,										// rxAddr

		6,										// rf_len
		{0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5},	// advA
												// data
};


_attribute_ble_data_retention_
rf_packet_scan_rsp_t	pkt_scan_rsp = {
		8,										// dma_len

		LL_TYPE_SCAN_RSP,						// type
		0,										// RFU
		0,										// ChSel: only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr
		0,										// rxAddr

		6,										// rf_len
		{0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5},	// advA
												// data
};

_attribute_data_retention_	u8 adv_decrease_time_opt = 0;
void bls_adv_decrease_time_optimize ()
{
	adv_decrease_time_opt = 1;
}
void bls_adv_decrease_time_optimize_clear ()
{
	adv_decrease_time_opt = 0;
}


/**
 * @brief      for user to initialize legacy advertising module
 * 			   notice that only one module can be selected between legacy advertising module and extended advertising module
 * @param	   none
 * @return     none
 */
void blc_ll_initLegacyAdvertising_module(void)
{
	ll_module_adv_cb = blt_send_adv;
	pFunc_ll_SetAdv_Enable = bls_ll_setAdvEnable;

	smemcpy(pkt_adv.advA, bltMac.macAddress_public, BLE_ADDR_LEN);
	smemcpy(pkt_scan_rsp.advA, bltMac.macAddress_public, BLE_ADDR_LEN);

	blta.adv_interval = 200000 * SYSTEM_TIMER_TICK_1US;
	bltParam.adv_interval_check_enable = 1;

	bltParam.adv_version = ADV_LEGACY_MASK;   //core_4.2 Legacy ADV
}







#if (LL_FEATURE_ENABLE_LL_PRIVACY)

void blt_ll_advRpaUpdate(void)
{																		 //advertister's address
	if(blc_ll_resolvGetRpaByAddr(blta.advPeerAddr, blta.advPeerAddrType, pkt_adv.advA, 1)){
		pkt_adv.txAddr = BLE_ADDR_RANDOM;

		/*
		 * The adva in this data packet will be the same as the adva being advertised, and
		 * will be based on the peer identity address in the set advertising parameters.
		 */
		pkt_scan_rsp.txAddr = BLE_ADDR_RANDOM;
	}
	else{
		if(blta.own_addr_type & 1){ //3 & 1 = 1: random
			pkt_adv.txAddr = BLE_ADDR_RANDOM;
			memcpy(pkt_adv.advA, bltMac.macAddress_random, BLE_ADDR_LEN);

			pkt_scan_rsp.txAddr = BLE_ADDR_RANDOM;
		}
		else{ //2 & 1 = 0: public
			pkt_adv.txAddr = BLE_ADDR_PUBLIC;
			memcpy(pkt_adv.advA, bltMac.macAddress_public, BLE_ADDR_LEN);

			pkt_scan_rsp.txAddr = BLE_ADDR_PUBLIC;
		}
	}

	memcpy(pkt_scan_rsp.advA, pkt_adv.advA, BLE_ADDR_LEN);

	if((blta.adv_type == ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY) || \
	   (blta.adv_type == ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY )){
																			//initiator's address
        if(blc_ll_resolvGetRpaByAddr(blta.advPeerAddr, blta.advPeerAddrType, pkt_adv.data, 0)){
            pkt_adv.rxAddr = BLE_ADDR_RANDOM;
        }
        else{
        	/*
        	 * If an IRK is not available in the Link Layer Resolving List or the IRK is set to zero for the
        	 * peer device, then the target's device address (TargetA field) shall use the Identity Address
        	 * when entering the Advertising State and using connectable directed events.
        	 */
            pkt_adv.rxAddr = (blta.advPeerAddrType & 1) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
            memcpy(pkt_adv.data, blta.advPeerAddr, BLE_ADDR_LEN);
        }
    }
}


void blt_ll_advSetRpaTmoFlg(void)
{
	if(bltParam.adv_en && blta.own_addr_type > OWN_ADDRESS_RANDOM){
		blta.advRpaTmoFlg = 1;
	}
}


void blt_ll_advChkRpaTmo(void)
{
	if(blta.own_addr_type <= OWN_ADDRESS_RANDOM){
		return;
	}

	if(blta.advRpaTmoFlg){
		blta.advRpaTmoFlg = 0;

		blt_ll_advRpaUpdate();

		//TODO: ext adv direct pkt
	}
}

#endif



void bls_ll_continue_adv_after_scan_req(u8 enable)
{
	bltParam.blc_continue_adv_en = enable;
}


void blc_ll_setAdvCustomedChannel (u8 chn0, u8 chn1, u8 chn2)
{
	blc_adv_channel[0] = chn0;
	blc_adv_channel[1] = chn1;
	blc_adv_channel[2] = chn2;
}





ble_sts_t	bls_ll_setAdvDuration (u32 duration_us, u8 duration_en)
{
	if(blta.adv_type == ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY){
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}
	else{
		blta.adv_duration_us = duration_us;
		blta.adv_duration_en = duration_en;
	}

	return BLE_SUCCESS;
}



/*	This code in RF irq and system irq put in RAM by force
 * Because of the flash resource contention problem, when the
 * flash access is interrupted by a higher priority interrupt,
 * the interrupt processing function cannot operate the flash
*/
#if (STACK_IRQ_CODE_IN_SRAM_DUE_TO_FLASH_OPERATION)
_attribute_ram_code_
#endif
ble_sts_t    bls_ll_setAdvEnable(int adv_enable)
{

	if(bltParam.adv_hci_cmd & ADV_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}



	u8 en = adv_enable & 0xff;


	if( (adv_enable & BLC_FLAG_STK_ADV) ||  bltParam.blt_state == BLS_LINK_STATE_IDLE || bltParam.blt_state == BLS_LINK_STATE_ADV )
	{
		reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;			//reset rptr = wptr


		reg_rf_irq_mask = 0;
		CLEAR_ALL_RFIRQ_STATUS;

//		blt_advExpectTime = clock_time () + blta.adv_interval;
		blt_advExpectTime = clock_time() + 2*SYSTEM_TIMER_TICK_1MS;


		if(en)  //enable
		{
			if(bltParam.blt_state != BLS_LINK_STATE_ADV)   // idle/conn_slave -> adv
			{
				bltParam.blt_state = BLS_LINK_STATE_ADV;
				blta.adv_begin_tick = clock_time();  //update
			}
		}
		else  //disable
		{
			if(bltParam.blt_state != BLS_LINK_STATE_IDLE)  //adv/conn_slave -> idle
			{
				bltParam.blt_state = BLS_LINK_STATE_IDLE;
			}
		}
	}

	bltParam.adv_hci_cmd |= ADV_LEGACY_MASK;

	bltParam.adv_en = en;

	return BLE_SUCCESS;
}




ble_sts_t  bls_ll_setAdvData(u8 *data, u8 len)
{
	if(bltParam.adv_hci_cmd & ADV_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_LEGACY_MASK;



	if(len > ADV_MAX_DATA_LEN ) {
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}


	if(len > 0){
		if( blta.adv_type == ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY || \
			blta.adv_type == ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY) {
			//data backup
			smemcpy(advData_backup, data, 6);
			if(len > 6){
				smemcpy(pkt_adv.data + 6, data + 6, len - 6);
			}
			advData_backup[6] = len + BLE_ADDR_LEN;
		}
		else{

			smemcpy(pkt_adv.data, data, len);
			pkt_adv.rf_len = len + BLE_ADDR_LEN;
		}
	}
	else{
		pkt_adv.rf_len = BLE_ADDR_LEN;
	}



	return BLE_SUCCESS;
}

ble_sts_t bls_ll_setScanRspData(u8 *data, u8 len)
{
	if(bltParam.adv_hci_cmd & ADV_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_LEGACY_MASK;


	if(len > ADV_MAX_DATA_LEN) {
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}

	smemcpy(pkt_scan_rsp.data, data, len);
	pkt_scan_rsp.rf_len = len + 6;

	pkt_scan_rsp.dma_len = rf_tx_packet_dma_len(pkt_scan_rsp.rf_len + 2);

	return BLE_SUCCESS;
}


ble_sts_t bls_ll_setAdvInterval(u16 intervalMin, u16 intervalMax)
{
	blta.adv_interval = intervalMin * SYSTEM_TIMER_TICK_625US;
	blta.advInt_min = intervalMin;
	blta.advInt_rand = intervalMax - intervalMin  + 1;

	return BLE_SUCCESS;
}

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
ble_sts_t  bls_ll_setAdvType(u8 advType)
{
	blta.adv_type = advType;
	if(advType == ADV_TYPE_CONNECTABLE_UNDIRECTED)
	{   		  // ADV_IND
		pkt_adv.type = LL_TYPE_ADV_IND;
	}
	else if(advType == ADV_TYPE_SCANNABLE_UNDIRECTED)
	{		  // ADV_SCAN_IND
		pkt_adv.type = LL_TYPE_ADV_SCAN_IND;
	}
	else if(advType == ADV_TYPE_NONCONNECTABLE_UNDIRECTED)
	{   // ADV_NONCONN_IND
		pkt_adv.type = LL_TYPE_ADV_NONCONN_IND;
	}
	else
	{    //ADV_INDIRECT_IND (high duty cycle) / ADV_INDIRECT_IND (low duty cycle)
		pkt_adv.type = LL_TYPE_ADV_DIRECT_IND;
	}
	return BLE_SUCCESS;
}

ble_sts_t blt_set_adv_addrtype(u8* cmdPara)
{
	if(cmdPara[0] == OWN_ADDRESS_PUBLIC)
	{
		pkt_adv.txAddr = BLE_ADDR_PUBLIC;
		memcpy(bltMac.macAddress_public,cmdPara+1,6);
		memcpy(pkt_adv.advA, bltMac.macAddress_public, BLE_ADDR_LEN);
	}
	else if(cmdPara[0] == OWN_ADDRESS_RANDOM)
	{
		pkt_adv.txAddr = BLE_ADDR_RANDOM;
		memcpy(bltMac.macAddress_random,cmdPara+1,6);
		memcpy(pkt_adv.advA, bltMac.macAddress_random, BLE_ADDR_LEN);
	}
	return BLE_SUCCESS;
}

//byte[0]: RxAdd = 0: public , RxAdd = 1: random
//byte[1]~byte[6]: Initiator Address
u8 blt_set_adv_direct_init_addrtype(u8* cmdPara)
{
	pkt_adv.type = LL_TYPE_ADV_DIRECT_IND;

	if(cmdPara[0] == OWN_ADDRESS_PUBLIC)
	{
		pkt_adv.rxAddr = BLE_ADDR_PUBLIC;
	}
	else if(cmdPara[0] == OWN_ADDRESS_RANDOM)
	{
		pkt_adv.rxAddr = BLE_ADDR_RANDOM;
	}

	memcpy(pkt_adv.data, cmdPara+1, BLE_ADDR_LEN);
	pkt_adv.rf_len = 12;

	return BLE_SUCCESS;
}

#endif ///ending of "#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)"

ble_sts_t bls_ll_setAdvChannelMap(adv_chn_map_t adv_channelMap)
{
	blta.adv_chn_mask = (u8)adv_channelMap;
	return BLE_SUCCESS;
}




ble_sts_t bls_ll_setAdvFilterPolicy(adv_fp_type_t advFilterPolicy)
{
	blta.adv_filterPolicy = (u8)advFilterPolicy;
	return BLE_SUCCESS;
}


void blc_ll_setAdvIntervalCheckEnable(u8 enable)
{
	if(enable)
		bltParam.adv_interval_check_enable = 1;
	else
		bltParam.adv_interval_check_enable = 0;
}


ble_sts_t   bls_ll_setAdvParam( u16 intervalMin,  u16 intervalMax,  adv_type_t advType,  		 	  own_addr_type_t ownAddrType,  \
							     u8 peerAddrType, u8  *peerAddr,    adv_chn_map_t 	adv_channelMap,   adv_fp_type_t   advFilterPolicy)
{



#if (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING)


	if(advType == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){
		#if BQB_5P0_TEST_ENABLE  //do not know the reason, to pass HCI/DDI/BI-02-C
			if(intervalMin < 0x20 || intervalMax < 0x20){
				return HCI_ERR_INVALID_HCI_CMD_PARAMS;
			}
			else if(intervalMin >= 0x20 && intervalMax >= 0x20){
				return HCI_ERR_UNSUPPORTED_FEATURE_PARAM_VALUE;
			}
		#endif
	}

	if(bltParam.adv_version == ADV_EXTENDED_MASK){
		return BLE_SUCCESS;  //when Extended ADV is using, this old method not allowed for users
	}
#endif








#if 0  //more check condition
	if(intervalMin > intervalMax) {
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}
	if(!(adv_channelMap & BLT_ENABLE_ADV_ALL)) {
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}
#endif

	//check adv interval for adv type
	u16 adv_intervalMin = intervalMin;
	u16 adv_intervalMax = intervalMax;
if (bltParam.adv_interval_check_enable){
	if(advType == ADV_TYPE_CONNECTABLE_UNDIRECTED || advType == ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY||advType == ADV_TYPE_SCANNABLE_UNDIRECTED || advType == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){
		if(intervalMin < ADV_INTERVAL_20MS || intervalMax > ADV_INTERVAL_10_24S) {
			return  HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}
	}
	else if(advType == ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY){
		adv_intervalMin = adv_intervalMax = ADV_INTERVAL_3_75MS;
		blta.adv_duration_us = 1280000;
		blta.adv_duration_en = 1;
	}
	else{
		return  HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}
}
else if(advType == ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY){
		adv_intervalMin = adv_intervalMax = ADV_INTERVAL_3_75MS;
		blta.adv_duration_us = 1280000;
		blta.adv_duration_en = 1;
	}



	blta.adv_type = (u8)advType;
	if(advType == ADV_TYPE_CONNECTABLE_UNDIRECTED){   		  // ADV_IND
		pkt_adv.type = LL_TYPE_ADV_IND;
	}
	else if(advType == ADV_TYPE_SCANNABLE_UNDIRECTED){		  // ADV_SCAN_IND
		pkt_adv.type = LL_TYPE_ADV_SCAN_IND;
	}
	else if(advType == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){   // ADV_NONCONN_IND
		pkt_adv.type = LL_TYPE_ADV_NONCONN_IND;
	}
	else{    //ADV_INDIRECT_IND (high duty cycle) / ADV_INDIRECT_IND (low duty cycle)
		pkt_adv.type = LL_TYPE_ADV_DIRECT_IND;
		pkt_adv.rxAddr = peerAddrType;

		advData_backup[7] = 1;
		advData_backup[6] = pkt_adv.rf_len;
		smemcpy(advData_backup, pkt_adv.data, 6);
		pkt_adv.rf_len = 12;
		//Notice: if the own_addr_type' value is 2 or 3, initA field in adv pkt maybe instead by peer rap addr.
		smemcpy(pkt_adv.data, peerAddr, BLE_ADDR_LEN);

#if (LL_FEATURE_ENABLE_LL_PRIVACY)
		memcpy(blta.advPeerAddr, peerAddr, BLE_ADDR_LEN); //idAddr
		blta.advPeerAddrType = peerAddrType; //idAddrType
#endif
	}


	//if direct adv -> other type adv,  need restore some param in pkt_adv
	if(pkt_adv.type != LL_TYPE_ADV_DIRECT_IND && advData_backup[7]){
		advData_backup[7] = 0;
		pkt_adv.rxAddr = BLE_ADDR_PUBLIC;
		pkt_adv.rf_len = advData_backup[6];
		smemcpy(pkt_adv.data, advData_backup, 6);
	}


	bltParam.advSetParam = 1; //mark

	//record ADR type
	blta.own_addr_type = ownAddrType; //1,2,3,4

	//Own address and address type process
	if(ownAddrType == OWN_ADDRESS_PUBLIC){
		pkt_adv.txAddr = BLE_ADDR_PUBLIC;
		smemcpy(pkt_adv.advA, bltMac.macAddress_public, BLE_ADDR_LEN);

		pkt_scan_rsp.txAddr = BLE_ADDR_PUBLIC;
		smemcpy(pkt_scan_rsp.advA, bltMac.macAddress_public, BLE_ADDR_LEN);
	}
	else if(ownAddrType == OWN_ADDRESS_RANDOM){
		pkt_adv.txAddr = BLE_ADDR_RANDOM;
		smemcpy(pkt_adv.advA, bltMac.macAddress_random, BLE_ADDR_LEN);


		pkt_scan_rsp.txAddr = BLE_ADDR_RANDOM;
		smemcpy(pkt_scan_rsp.advA, bltMac.macAddress_random, BLE_ADDR_LEN);

	}
	else{ // OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC or OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM
#if (LL_FEATURE_ENABLE_LL_PRIVACY)
		/*
		 * If Own_Address_Type equals 0x02 or 0x03, the Peer_Address parameter contains the peer's Identity
		 * Address and the Peer_Address_Type parameter contains the Peer's Identity Type (i.e. 0x00 or 0x01).
		 */
		memcpy(blta.advPeerAddr, peerAddr, BLE_ADDR_LEN); //idAddr
		blta.advPeerAddrType = peerAddrType; //idAddrType

		blt_ll_advRpaUpdate(); //This will generate an RPA for both initiator addr and adva
#else
		//If we dont support privacy some address types wont work
		return  HCI_ERR_UNSUPPORTED_FEATURE_PARAM_VALUE;
#endif
	}

	//param setting
	blta.adv_chn_mask = (u8)adv_channelMap;
	blta.adv_filterPolicy = (u8)advFilterPolicy;

	blta.adv_interval = adv_intervalMin * SYSTEM_TIMER_TICK_625US;
	blta.advInt_min = adv_intervalMin;
	blta.advInt_rand = adv_intervalMax - adv_intervalMin  + 1;



	return BLE_SUCCESS;
}


ble_sts_t bls_hci_le_setAdvParam(adv_para_t *para)
{

	if(bltParam.adv_hci_cmd & ADV_EXTENDED_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_LEGACY_MASK;


	ble_sts_t status;
	status = bls_ll_setAdvParam(para->intervalMin, para->intervalMax, para->advType, para->ownAddrType, \
								 para->peerAddrType, para->peerAddr, para->advChannelMap, para->advFilterPolicy );

	return status;
}





void blc_ll_procConnectReq(u8 * prx, u8 txAddr)
{

	//add the dispatch when the connect packets is meaning less
	//interval: 7.5ms - 4s -> 6 - 3200
	// 0<= latency <= (Timeout/interval) - 1, timeout*10/(interval*1.25)= timeout*8/inetrval
	// 0 <= winOffset <= interval
	// 1.25 <= winSize <= min(10ms, interval - 1.25)    10ms/1.25ms = 8
	//Timeout: 100ms-32s   -> 10- 3200     timeout >= (latency+1)*interval*2
	//hop :bit<0-5> ,range is 5 -16;
	rf_packet_connect_t *connReq_ptr = (rf_packet_connect_t *)(prx + 0);
	if(    connReq_ptr->interval < 6 || connReq_ptr->interval > 3200 	\
		|| connReq_ptr->winSize < 1  || connReq_ptr->winSize > 8 || connReq_ptr->winSize>=connReq_ptr->interval	\
		|| connReq_ptr->timeout < 10 || connReq_ptr->timeout > 3200  	\
	/*	|| connReq_ptr->winOffset > connReq_ptr->interval			 	\ */
		|| (connReq_ptr->hop & 0x1f) < 5 || (connReq_ptr->hop & 0x1f) >16 ) {

		//SAVE_ERRCONN_DATA(prx + 4);  //debug
		return ;
	}
	if( !connReq_ptr->chm[0]){
		if( !connReq_ptr->chm[1] && !connReq_ptr->chm[2] &&  \
			!connReq_ptr->chm[3] && !connReq_ptr->chm[4]){

			//SAVE_ERRCONN_DATA(prx + 4);  //debug
			return;
		}
	}
	if(connReq_ptr->latency){
		if(connReq_ptr->latency  >  ((connReq_ptr->timeout<<3)/connReq_ptr->interval)){

			//SAVE_ERRCONN_DATA(prx + 4);  //debug
			return;
		}
	}

#if (LL_FEATURE_ENABLE_LL_PRIVACY)

	if(ll_adv2conn_cb){
		ll_adv2conn_cb(prx, FALSE);  //blt_connect
	}

	bltParam.adv_scanReq_connReq = 2;

#else
	if(blta.adv_type != ADV_TYPE_SCANNABLE_UNDIRECTED && \
	  (pkt_adv.type == LL_TYPE_ADV_DIRECT_IND  || \
	   !(blta.adv_filterPolicy & ALLOW_CONN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList(txAddr, prx+6)) ){

		if(ll_adv2conn_cb){
			ll_adv2conn_cb(prx, FALSE);  //blt_connect
		}

		bltParam.adv_scanReq_connReq = 2;
	}
#endif
}


_attribute_ram_code_ void bls_ll_procRxPacket(u8 *prx, u16 * pmac)
{

	u32 rx_begin_tick = clock_time ();
	rf_packet_scan_req_t * pReq = (rf_packet_scan_req_t *) (prx + 0);
	u16 *advA16 = (u16 *)pkt_adv.advA;

#if (LL_FEATURE_ENABLE_LL_PRIVACY)
	u8* peerAddr = pReq->scanA;
	u8  peerAddrType = pReq->txAddr;
#endif

	rf_set_dma_tx_addr((unsigned int)(&pkt_scan_rsp));//Todo:need check by sunwei
	// (rf_len + 10)*8:  10 = preamble 1B + access_code 4B + header 2B + CRC 3B
	// header 2B + PDU "rf_len" B + CRC 3B  is dma data, at least 4B is arrived when  "*ph" not 0
	//  so rest longest data is: rf_len + 2 + 3 - 4 = rf_len + 1
	// only 2 kind of request:  scan_req rf_len = 12, conn_req rf_len = 34
	// RX packet reset timing is (rf_len+1)*8 = (34+1)*8 = 280
	// add 20 uS for RX status delay, 200+20 = 300uS
	while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 300 * SYSTEM_TIMER_TICK_1US);

	if(prx[5] == 12) //scan_req
	{
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		rf_tx_settle_adjust(LL_SCANRSP_TX_SETTLE);  //LL_INIT_TX_SETTLE + 15,  84 - 78 = 6
		//(rf_len+5)*8,  5: header 2 + CRC 3
		//(rf_len+5)*8 + 150 - (TX settle+preamble 5B) - (timeStamp capture delay)
		u32 t = reg_rf_timestamp + ((12*8 + 52) *SYSTEM_TIMER_TICK_1US);  //68-15=53
		#if 0//will cost some time
		if( tick1_exceed_tick2(clock_time(), t) ){
			t = clock_time();
		}
		#endif
		reg_rf_ll_cmd_schedule = t;

		//reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;	// Enable cmd_schedule mode
		reg_rf_ll_cmd = FLD_RF_R_CMD_TRIG | FLD_RF_R_STX; // single TX
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		reg_rf_ll_cmd_schedule = clock_time() + blta.T_SCAN_RSP_INTVL;
		reg_rf_ll_cmd = FLD_RF_R_CMD_TRIG | FLD_RF_R_STX; // 0x85--single TX

	#endif

		if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_TX_ON);  }
	}


	reg_rf_irq_status = FLD_RF_IRQ_RX;


	if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(prx) && RF_BLE_RF_PACKET_CRC_OK(prx) )
	{
		//See if AdvA in the request (scan or connect) matches what we sent
		if (MAC_MATCH16(pmac, advA16))
		{
#if (LL_FEATURE_ENABLE_LL_PRIVACY)
			u8 rpaPassed = 1; //dft passed
			u8 pduType = pReq->type;
			bool advDirected = pkt_adv.type == LL_TYPE_ADV_DIRECT_IND;

			//The Adv filter_Policy parameter shall be ignored when directed adv is used.
			u8 advFilterPolicy = advDirected ? 0 : blta.adv_filterPolicy;
			u8 needChkWl = advFilterPolicy & ((pduType == LL_TYPE_SCAN_REQ) ? ALLOW_SCAN_WL : ALLOW_CONN_WL);

			if(blc_ll_resolvIsAddrRlEnabled()){ //same as:  ll_resolvingList_tbl.addrRlEn
				ll_resolv_list_t* rl = NULL;
				if(IS_RESOLVABLE_PRIVATE_ADDR(peerAddrType, peerAddr)){
					blta.advRpaRlIdx = blc_ll_resolvPeerRpaResolvedAny(peerAddr);
					if(blta.advRpaRlIdx != -1){ //rpa resolved successfully
						blta.advRpaResoved = 1;
						if(needChkWl){
							rl = &ll_resolvingList_tbl.rlList[blta.advRpaRlIdx];
							peerAddr = rl->rlIdAddr;
							peerAddrType = rl->rlIdAddrType;
						}
					}
					else if(needChkWl){ //if rpa failed, check if need check WL.
						rpaPassed = 0;//fail
					}
				}
				else{//Check privacy mode, not dependent on whitelist
					rl = blt_ll_resolvFindRlEntry(peerAddr, peerAddrType);
					if(rl && (rl->rlPrivMode == PRIVACY_NETWORK_MODE) && rl->rlHasPeerRpa){
						rpaPassed = 0;//fail
					}
				}
			}

			/*
			 * If the advertiser receives a SCAN_REQ PDU that contains its device address from a scanner allowed by the
			 * advertising filter policy it shall reply with a SCAN_RSP PDU on the same advertising channel index.
			 */
			if(rpaPassed && ((!needChkWl) || ll_searchAddrInWhiteListTbl(peerAddrType, peerAddr))){

				u8 addrValid = 1; //dft: pass

				if(pduType == LL_TYPE_SCAN_REQ ){
					//Setup to transmit the scan response if appropriate
					if(!advDirected){
						//sleep_us(500);  //when scanRsp data is not 31 bytes, 500us is wasteful
						sleep_us(pkt_scan_rsp.rf_len*8 + 204 );   //max rf_len = 37 bytes (PDU 31 bytes), 500us = rf_len*8 + 204
						blt_p_event_callback (BLT_EV_FLAG_SCAN_RSP, NULL, 0);

						bltParam.adv_scanReq_connReq = 2;
					}
				}
				else if(pduType == LL_TYPE_CONNNECT_REQ){  // && (pReq->rf_len&0x3f) == 34)

					STOP_RF_STATE_MACHINE; //Stop Scan_rsp transition immediately

					if(addrValid){
						if(pduType != LL_TYPE_ADV_SCAN_IND){ // && != LL_TYPE_ADV_NONCONN_IND
							blc_rcvd_connReq_tick = clock_time();

							if(blta.advRpaResoved){ //the address need to be updated for host interaction
								blc_ll_resolvSetPeerRpaByIdx(blta.advRpaRlIdx, peerAddr); //Update resolving list with current peer RPA

								//keep received InitA with identity address
								smemcpy((char*)bltc.conn_peer_addr, (char*)ll_resolvingList_tbl.rlList[blta.advRpaRlIdx].rlIdAddr, BLE_ADDR_LEN);

								/*
								 * Core spec5.2 page2498: peer_addr_type:2 Public Identity Address, 3: Random(Static)Identity Address
								 * Mark peer addr type(7.8.12 LE Create Connection command: own_adder_type: 0x00, 0x01, 0x02, 0x03).
								 */
								bltc.conn_peer_addr_type = ll_resolvingList_tbl.rlList[blta.advRpaRlIdx].rlIdAddrType + 2; //Peer address type is an identity address
							}
							else{
								bltc.conn_peer_addr_type = pReq->txAddr;
								smemcpy((char*)bltc.conn_peer_addr, (char*)pReq->scanA, BLE_ADDR_LEN);
							}
							blc_ll_procConnectReq(prx,  pReq->txAddr);
						}
					}
				}
			}
#else
			if (pReq->type == LL_TYPE_SCAN_REQ )
			{

				if(  pkt_adv.type != LL_TYPE_ADV_DIRECT_IND &&  \
					 (!(blta.adv_filterPolicy & ALLOW_SCAN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6) )){

					//sleep_us(500);  //when scanRsp data is not 31 bytes, 500us is wasteful
					sleep_us(pkt_scan_rsp.rf_len*8 + 214 );   //max rf_len = 37 bytes (PDU 31 bytes), 500us = rf_len*8 + 204
					blt_p_event_callback (BLT_EV_FLAG_SCAN_RSP, NULL, 0);

					if(!bltParam.blc_continue_adv_en){
						bltParam.adv_scanReq_connReq = 2;
					}
				}

			}
			else if (pReq->type == LL_TYPE_CONNNECT_REQ)  // && (pReq->rf_len&0x3f) == 34)
			{
				/*When receiving the connect request, if the current adv type is a direct adv, you need to determine whether
				  the Initial addr in the connect request is consistent with the target address in the direct adv.*/
				#if	LL_MASTER_PRA_FIX_EN
				//not consider ll_list more than 1 yet.need to check every list.
					if( ((pkt_adv.type == LL_TYPE_ADV_DIRECT_IND) &&
						((smemcmp(pReq->scanA,pkt_adv.data,BLE_ADDR_LEN) == 0) || \
						 (IS_RESOLVABLE_PRIVATE_ADDR(pReq->txAddr,pReq->scanA) && (blt_ll_searchAddr_in_ResolvingList(pReq->txAddr, pReq->scanA))))) || \
						 (pkt_adv.type == LL_TYPE_ADV_IND)){
						STOP_RF_STATE_MACHINE;
						blc_rcvd_connReq_tick = clock_time();
						blc_ll_procConnectReq(prx,  pReq->txAddr);
					}

				#else
					/* Note: Private addresses are not considered here */
					if(((pkt_adv.type == LL_TYPE_ADV_DIRECT_IND) && (memcmp(pReq->scanA,pkt_adv.data,BLE_ADDR_LEN) == 0)) || (pkt_adv.type == LL_TYPE_ADV_IND)){
						STOP_RF_STATE_MACHINE;
						blc_rcvd_connReq_tick = clock_time();
						blc_ll_procConnectReq(prx,  pReq->txAddr);
					}
				#endif

			}
#endif
		}
	}

	STOP_RF_STATE_MACHINE;
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	prx[0] = 1;
#endif
}



/**************************************************************************
 	 adv type		    pkt_adv.type    		SCAN_REQ	CONNNECT_REQ

	ADV_IND				0 : LL_TYPE_ADV_IND				yes			yes
	ADV_DIRECT_IND		1 : LL_TYPE_ADV_DIRECT_IND		no			yes(*)    	no need check whitelist
	ADV_NONCONN_IND		2 : LL_TYPE_ADV_NONCONN_IND		no			no
	ADV_SCAN_IND		6 : LL_TYPE_ADV_SCAN_IND	    yes			no
 *************************************************************************/
_attribute_data_retention_	advertise_prepare_handler_t advertise_prepare_handler = 0;
void bls_set_advertise_prepare (void *p)
{
	advertise_prepare_handler = p;
}

int  blt_send_adv(void)
{

	if(blta.adv_duration_en && clock_time_exceed(blta.adv_begin_tick, blta.adv_duration_us) ){
		blta.adv_duration_en = 0;
		bls_ll_setAdvEnable(BLC_ADV_DISABLE);  //adv disable

		blt_p_event_callback (BLT_EV_FLAG_ADV_DURATION_TIMEOUT, NULL, 0);
	}


	int ready = 1;
	if (advertise_prepare_handler)
	{
		ready = advertise_prepare_handler (&pkt_adv);
	}


	u8 * prx = NULL;

	if(bltParam.adv_en && ready)
	{

		#if (LL_FEATURE_ENABLE_LE_2M_PHY || LL_FEATURE_ENABLE_LE_CODED_PHY)
			if(ll_phy_switch_cb){
				if(BLE_PHY_1M != bltPHYs.cur_llPhy){   //switch back to 1M PHY
					ll_phy_switch_cb(BLE_PHY_1M, LE_CODED_S2);
				}
			}
		#endif

		#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
			pkt_adv.chan_sel = bltParam.local_chSel;
		#endif


		#if (LL_FEATURE_ENABLE_LL_PRIVACY)
			blta.advRpaRlIdx = -1;  //init an invalid value
			blta.advRpaResoved = 0; //dft rpa unresolved
		#endif
		
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			ble_rf_set_tx_dma(0, 3);  //48/16=3
			reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;

			ble_rf_set_rx_dma((u8*)adv_rx_buff, 4);
		#endif
		
		rf_set_tx_rx_off();//must add
		blt_ll_set_ble_access_code_adv ();
		rf_set_ble_crc_adv ();
		rf_tx_settle_adjust(LL_ADV_TX_SETTLE);   //TODO: check with QiangKai & QingHua
		reg_rf_irq_mask = 0;
		
		pkt_adv.dma_len = rf_tx_packet_dma_len(pkt_adv.rf_len + 2);//4 bytes align

		u32  t_us = (pkt_adv.rf_len + 10) * 8 + 400;

		u32	 fst_check = t_us - 70;

		bltParam.adv_scanReq_connReq = 1;  //mark adv sending


		for (int i=0; i<3; i++)
		{
			if (blta.adv_chn_mask & BIT(i))
			{

				STOP_RF_STATE_MACHINE;						// stop SM
				rf_set_ble_channel (blc_adv_channel[i]);


				reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;
				////////////// start TX //////////////////////////////////

				if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_TX_ON);  }


				u32 tx_begin_tick;
				if(blta.adv_type == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){ //ADV_NONCONN_IND

					rf_start_fsm(FSM_STX,(void *)&pkt_adv, clock_time() + 100);
					tx_begin_tick = clock_time ();
					while (!(reg_rf_irq_status & FLD_RF_IRQ_TX) && (clock_time() - tx_begin_tick) < (t_us - 200)*SYSTEM_TIMER_TICK_1US);
				}
				else{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					prx = (u8 *) (adv_rx_buff);
					//TODO: must change for DLE
					rf_set_rx_maxlen(34); //conn_Req rf_len rf_set_rx_maxlen
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					prx = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.wptr & (blt_rxfifo.num-1)));
				#endif
					#if(FREERTOS_ENABLE)
					extern u8 x_freertos_on;
					u32 r = 0;
					if( x_freertos_on ){
						r = irq_disable();
					}
					#endif
					
					rf_start_fsm(FSM_TX2RX,(void *)&pkt_adv, clock_time() + 100);
					tx_begin_tick = clock_time ();

					u16 *pmac = (u16*)(prx + 12);

					volatile u32 *ph  = (u32 *) (prx + 4);
					ph[0] = 0;

					while(!(reg_rf_irq_status & FLD_RF_IRQ_TX) );
					reg_rf_irq_status = FLD_RF_IRQ_TX;
					if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_RX_ON);  }
#if(ADV_CURRENT_OPT)
					if(adv_decrease_time_opt)
					{
						while (!(*ph) && (u32)(clock_time() - tx_begin_tick) < t_us* SYSTEM_TIMER_TICK_1US)	//wait packet from master
						{
						#if (MCU_CORE_TYPE == MCU_CORE_9518)
							if( ((u32)(clock_time() - tx_begin_tick) > fst_check * SYSTEM_TIMER_TICK_1US) && ((read_reg8(0x140840)&0x07)<2))
							{
								break;
							}
						#endif
						}
					}
					else
#endif
					{
						while (!(*ph) && (u32)(clock_time() - tx_begin_tick) < t_us* SYSTEM_TIMER_TICK_1US);	//wait packet from master
					}

					if (*ph){
						bls_ll_procRxPacket(prx, pmac);
					}
					#if(FREERTOS_ENABLE)
					if( x_freertos_on ){
						irq_restore(r);
					}
					#endif


					if(bltParam.adv_scanReq_connReq == 2){
						i = 4;  //break;
					}
				}

				if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_OFF);  }

			}

		}

		STOP_RF_STATE_MACHINE;
		CLEAR_ALL_RFIRQ_STATUS;

		bltParam.adv_scanReq_connReq = 0;  //mclear adv sending


		if(blta.advInt_rand > 1){
			u16 randV = rand() % blta.advInt_rand;
			blta.adv_interval = (blta.advInt_min + randV )* SYSTEM_TIMER_TICK_625US;
		}
	}


	if (bltParam.blt_state == BLS_LINK_STATE_ADV)
	{
		u32 cur_tick = clock_time();
		if(abs( (int)(blt_advExpectTime - cur_tick) ) < 5000 * SYSTEM_TIMER_TICK_1US){
			blt_advExpectTime += blta.adv_interval;
		}
		else{
			blt_advExpectTime = cur_tick + blta.adv_interval;
		}
		// 8278 Need ensure reg system tick irq BIT<0:2> is 0;
		systimer_set_irq_capture( cur_tick + BIT(31));  //adv state, system tick irq will not happen


	#if(LL_FEATURE_ENABLE_LL_PRIVACY)
		 blt_ll_advChkRpaTmo(); //Regenerate the RPA's if RPA timer has passed timeout
	#endif

	}
	else if(bltParam.blt_state == BLS_LINK_STATE_CONN) //enter conn state
	{

		systimer_clr_irq_status();
		systimer_irq_enable();

		#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
			/* attention: must set these two value before "systimer_set_irq_capture", cause maybe connExpectTime is
			 *            a passed history value, once set, IRQ will come */
			bltParam.btxbrx_status = BTXBRX_NEARBY;
			bltParam.acl_conn_start_tick = bltc.connExpectTime;
			bltParam.conn_role = LL_ROLE_SLAVE;
		#endif

		systimer_set_irq_capture(bltc.connExpectTime);//need check
		//scanA advA accesscode crcint winsize  winOffset interval latency  timeout  chn[5]  hop
		blt_p_event_callback (BLT_EV_FLAG_CONNECT, prx+DMA_RFRX_OFFSET_DATA, 34); ///140us later than before using 16M system clock.
	}

	return 0;
}







#if (BLT_ADV_IN_CONN_SLAVE_EN)


typedef struct{
	u8 	adv_type;
	u8 	chn_mask;
	u8 	filterPolicy;

}adv_slave_t;

adv_slave_t	 advS;

rf_packet_adv_t	pktAdv_slave = {
		sizeof (rf_packet_adv_t) - 4,		// dma_len

		LL_TYPE_ADV_IND,						// type
		0,										// RFU
		0,										// ChSel: only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr
		0,										// rxAddr

		sizeof (rf_packet_adv_t) - 6,		// rf_len
		{0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5},	// advA
		// data
};


rf_packet_scan_rsp_t	pktScanRsp_slave = {
		sizeof (rf_packet_scan_rsp_t) - 4,		// dma_len

		LL_TYPE_SCAN_RSP,						// type
		0,										// RFU
		0,										// ChSel: only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr
		0,										// rxAddr

		sizeof (rf_packet_scan_rsp_t) - 6,		// rf_len
		{0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5},	// advA
};





/**
 * @brief      This function is used to set the advertising parameters in connect slave role.
 * @param[in]  *adv_data -  advertising data buffer
 * @param[in]  advData_len - The number of significant octets in the Advertising_Data.
 * @param[in]  *scanRsp_data -  Scan_Response_Data buffer
 * @param[in]  scanRspData_len - The number of significant octets in the Scan_Response_Data.
 * @param[in]  advType - Advertising_Type
 * @param[in]  ownAddrType - Own_Address_Type
 * @param[in]  adv_channelMap - Advertising_Channel_Map
 * @param[in]  advFilterPolicy - Advertising_Filter_Policy
 * @return      Status - 0x00: BLE success; 0x01-0xFF: fail
 */
ble_sts_t 	blc_ll_setAdvParamInConnSlaveRole( u8 		  *adv_data,  u8              advData_len, u8 *scanRsp_data,  			 u8 scanRspData_len,
											   adv_type_t  advType,   own_addr_type_t ownAddrType, adv_chn_map_t adv_channelMap, adv_fp_type_t advFilterPolicy)
{
	if(advData_len > 0){
		smemcpy(pktAdv_slave.data, adv_data, advData_len);
		pktAdv_slave.rf_len = advData_len + BLE_ADDR_LEN;
//		pktAdv_slave.dma_len = pktAdv_slave.rf_len + 2;
		pktAdv_slave.dma_len = rf_tx_packet_dma_len(pktAdv_slave.rf_len + 2);
	}

	if(scanRspData_len > 0){
		smemcpy(pktScanRsp_slave.data, scanRsp_data, scanRspData_len);
		pktScanRsp_slave.rf_len = scanRspData_len + 6;
//		pktScanRsp_slave.dma_len = pktScanRsp_slave.rf_len + 2;
		pktScanRsp_slave.dma_len = rf_tx_packet_dma_len(pktScanRsp_slave.rf_len + 2);
	}




	advS.adv_type = advType;
	if(advType == ADV_TYPE_CONNECTABLE_UNDIRECTED){   		  // ADV_IND
		pktAdv_slave.type = LL_TYPE_ADV_IND;
	}
	else if(advType == ADV_TYPE_SCANNABLE_UNDIRECTED){		  // ADV_SCAN_IND
		pktAdv_slave.type = LL_TYPE_ADV_SCAN_IND;
	}
	else if(advType == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){   // ADV_NONCONN_IND
		pktAdv_slave.type = LL_TYPE_ADV_NONCONN_IND;
	}
	else{

	}



	//Own address and address type process
	if(ownAddrType == OWN_ADDRESS_PUBLIC){
		pktAdv_slave.txAddr = BLE_ADDR_PUBLIC;
		smemcpy(pktAdv_slave.advA, bltMac.macAddress_public, BLE_ADDR_LEN);

		pktScanRsp_slave.txAddr = BLE_ADDR_PUBLIC;
		smemcpy(pktScanRsp_slave.advA, bltMac.macAddress_public, BLE_ADDR_LEN);
	}
	else if(ownAddrType == OWN_ADDRESS_RANDOM){
		pktAdv_slave.txAddr = BLE_ADDR_RANDOM;
		smemcpy(pktAdv_slave.advA, bltMac.macAddress_random, BLE_ADDR_LEN);

		pktScanRsp_slave.txAddr = BLE_ADDR_RANDOM;
		smemcpy(pktScanRsp_slave.advA, bltMac.macAddress_random, BLE_ADDR_LEN);
	}
	else{

	}


	advS.chn_mask = (u8)adv_channelMap;
	advS.filterPolicy = (u8)advFilterPolicy;

	return BLE_SUCCESS;

}


/*	This code in RF irq and system irq put in RAM by force
 * Because of the flash resource contention problem, when the
 * flash access is interrupted by a higher priority interrupt,
 * the interrupt processing function cannot operate the flash
*/
#if (STACK_IRQ_CODE_IN_SRAM_DUE_TO_FLASH_OPERATION)
_attribute_ram_code_
#endif
int  blc_ll_sendAdvInSlaveRole(void)
{

	if(!bltParam.adv_en){
		return 0;
	}


#if (MCU_CORE_TYPE == MCU_CORE_9518)
	ble_rf_set_tx_dma(0, 3);  //48/16=3

	/* make sure tx_wptr = tx_rptr, can solve one problem: prevent TX packet come from Hardware TX FIFO
	 * Kite/826x can use one register to disable HW TX FIFO, but Eagle is difficult to control(confirmed by QiangKai)
	 * TX_wptr & TX_rptr should recover in next connection state*/
	reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;

	rf_set_tx_rx_off();
	blt_ll_set_ble_access_code_adv ();
	rf_set_ble_crc_adv ();
	rf_tx_settle_adjust(LL_ADV_TX_SETTLE);			//adjust TX settle time [adv]
	reg_rf_irq_mask = 0;

	pktAdv_slave.dma_len = rf_tx_packet_dma_len(pktAdv_slave.rf_len + 2);//4 bytes alignCLR;
	ble_rf_set_rx_dma((u8*)adv_rx_buff, 4);

	u32  t_us = (pktAdv_slave.rf_len + 10) * 8 + 400;
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	CLEAR_ALL_RFIRQ_STATUS;

	//reg_rf_irq_mask = 0;
	tx_settle_adjust(LL_ADV_TX_SETTLE);			//adjust TX settle time [adv]

	blt_ll_set_ble_access_code_adv ();
	rf_set_ble_crc_adv ();

	pktAdv_slave.dma_len = rf_tx_packet_dma_len(pktAdv_slave.rf_len + 2);//4 bytes alignCLR;
	u32  t_us = (pktAdv_slave.rf_len + 10) * 8 + 370;

	reg_dma_rf_tx_mode = 0x00; //DMA: ble fifo mode disable.
#endif

	bltParam.adv_scanReq_connReq = 1;  //mark adv sending
	for (int i=0; i<3; i++)
	{
		if (advS.chn_mask & BIT(i)) {
			STOP_RF_STATE_MACHINE;						// stop SM
			rf_set_ble_channel (blc_adv_channel[i]);   //phy poweron in this func


			reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;
			////////////// start TX //////////////////////////////////

			if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_TX_ON);  }

			u32 tx_begin_tick;
			if(advS.adv_type == ADV_TYPE_NONCONNECTABLE_UNDIRECTED){ //ADV_NONCONN_IND
				rf_start_fsm(FSM_STX,(void *)&pktAdv_slave, clock_time() + 100);
				tx_begin_tick = clock_time ();
				while (!(reg_rf_irq_status & FLD_RF_IRQ_TX) && (clock_time() - tx_begin_tick) < (t_us - 200)*SYSTEM_TIMER_TICK_1US);
			}
			else{
				
				rf_start_fsm(FSM_TX2RX,(void *)&pktAdv_slave, clock_time() + 100);
				tx_begin_tick = clock_time ();
				
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					u8 * prx = (u8 *) (adv_rx_buff);
					rf_set_rx_maxlen(34);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					u8 * prx = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.wptr & (blt_rxfifo.num-1)));
				#endif
				
				u16 *pmac = (u16*)(prx + 12);

				volatile u32 *ph  = (u32 *) (prx + 4);
				ph[0] = 0;
				
			#if (MCU_CORE_TYPE == MCU_CORE_9518)
				while(!(reg_rf_irq_status & FLD_RF_IRQ_TX) );
				reg_rf_irq_status = FLD_RF_IRQ_TX;

				if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_RX_ON);  }
			#endif

				while (!(*ph) && (u32)(clock_time() - tx_begin_tick) < t_us * SYSTEM_TIMER_TICK_1US);	//wait packet from master

				if (*ph){
					u32 rx_begin_tick = clock_time ();
					
					#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
						if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_RX_ON);  }
					#endif

					rf_packet_scan_req_t * pReq = (rf_packet_scan_req_t *) (prx + 0);
					u16 *advA16 = (u16 *)pktAdv_slave.advA;
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					reg_dma_src_addr(DMA0)=((unsigned int)((unsigned char*)(((u32)&pktScanRsp_slave)))-0x80000 + 0xc0200000);
					reg_rf_ll_ctrl3 |= FLD_RF_R_CMD_SCHDULE_EN;	// Enable cmd_schedule mode

					while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 400 * SYSTEM_TIMER_TICK_1US);

					reg_rf_ll_cmd_schedule = clock_time() + blta.T_SCAN_RSP_INTVL;
					reg_rf_ll_cmd = 0x85;	// single TX

					reg_rf_irq_status = FLD_RF_IRQ_RX;
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 400 * SYSTEM_TIMER_TICK_1US);
				#endif


					if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(prx) && RF_BLE_RF_PACKET_CRC_OK(prx) )
					{
						if (pReq->type == LL_TYPE_SCAN_REQ && MAC_MATCH16(pmac, advA16))
						{
						#if (MCU_CORE_TYPE == MCU_CORE_9518)
							if( !(advS.filterPolicy & ALLOW_SCAN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6) ){

								sleep_us(pktScanRsp_slave.rf_len*8 + 204 );   //max rf_len = 37 bytes (PDU 31 bytes), 500us = rf_len*8 + 204
								if(!bltParam.blc_continue_adv_en){
									bltParam.adv_scanReq_connReq = 2;
								}
							}
						#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
							STOP_RF_STATE_MACHINE;

							reg_rf_ll_cmd_schedule = clock_time();
							reg_rf_ll_cmd = FLD_RF_R_CMD_TRIG | FLD_RF_R_STX; // 0x85--single TX
							reg_dma3_addr = (u16)((u32)&pktScanRsp_slave); //REG_ADDR16(0x800c0c)

							if( !(advS.filterPolicy & ALLOW_SCAN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6) ){

								sleep_us(500);
								if(!bltParam.blc_continue_adv_en){
									bltParam.adv_scanReq_connReq = 2;
								}
							}
							//STOP_RF_STATE_MACHINE;
						#endif
						}
					}

					STOP_RF_STATE_MACHINE;
					#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
						prx[0] = 1;
					#endif
				}

				if(bltParam.adv_scanReq_connReq == 2){
					i=4;  //break
				}
			}


			if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_OFF);  }
		}
	}
	bltParam.adv_scanReq_connReq = 0; //clear adv sending
	
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	reg_dma_rf_tx_mode = 0x80; //DMA : ble fifo mode enable.
#endif
	//clear stx2rx stateMachine status
	STOP_RF_STATE_MACHINE;

	CLEAR_ALL_RFIRQ_STATUS;

	return 0;
}





/**
 * @brief      This function is used to add advertise state in connect state of slave role.
 * @param[in]  none.
 * @return     Status - 0x00: BLE success; 0x01-0xFF: fail
 */
ble_sts_t    blc_ll_addAdvertisingInConnSlaveRole(void)
{
	bltParam.adv_extension_mask |= BLS_FLAG_ADV_IN_SLAVE_MODE;
	ll_module_advSlave_cb = blc_ll_sendAdvInSlaveRole;


	return BLE_SUCCESS;
}



/**
 * @brief      This function is used to remove advertisement state in connect state of slave role.
 * @param[in]  none.
 * @return      Status - 0x00: BLE success; 0x01-0xFF: fail
 */
ble_sts_t    blc_ll_removeAdvertisingFromConnSLaveRole(void)
{
	bltParam.adv_extension_mask &= ~BLS_FLAG_ADV_IN_SLAVE_MODE;
	ll_module_advSlave_cb = NULL;

	return BLE_SUCCESS;
}


#endif




