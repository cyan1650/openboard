/********************************************************************************************************
 * @file	ll_ext_adv.c
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


#if (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING)

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	#define	ADV_DURATION_STALL_EN		0//todo
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	#define	ADV_DURATION_STALL_EN		1
#endif
                                     //random 0~31 (32~36 not used)  core_740<2:0> is always 0
#define BLT_GENERATE_AUX_CHN		(bltExtA.custom_aux_chn ?  bltExtA.custom_aux_chn : ((clock_time()>>3) & 0x1F ) )



extern u32 		blt_advExpectTime;
extern	u32 	blc_rcvd_connReq_tick;

extern	blt_event_callback_t		blt_p_event_callback ;
extern	_attribute_aligned_(4)	st_ll_conn_slave_t		bltc;

extern 	u8		blc_adv_channel[];


extern	ll_adv2conn_callback_t ll_adv2conn_cb;



_attribute_data_retention_	 _attribute_aligned_(4)
rf_pkt_ext_adv_t	pkt_auxChainInd_aucScanRsp = {
		0,										// dma_len			set when sending packet

		LL_TYPE_AUX_CHAIN_IND,					// type				no need change, LL_TYPE_AUX_CHAIN_IND/LL_TYPE_AUX_SCAN_RSP are same 0x07
		0,										// RFU
		0,										// "ChSel" only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr	  		set when sending packet
		0,										// rxAddr     		no need change, LL_TYPE_AUX_CHAIN_IND/LL_TYPE_AUX_SCAN no "TargetA"

		0,										// rf_len			set when sending packet

		0,										// ext_hdr_len		set when sending packet
		LL_EXTADV_MODE_NON_CONN_NON_SCAN,		// adv_mode			no need change,	LL_TYPE_AUX_CHAIN_IND/LL_TYPE_AUX_SCAN_RSP are same 0x00

		0,										// ext_hdr_flg 		set when sending packet

};



// pkt_pri_scanrsp is only for leagcy ADV
// pkt_pri_scanrsp is shared by all adv_sets to save SRAM,
//		so header stored by ll_ext_adv_t.,
// 		public address stored by bltMac.macAddress_public,  random address stored by ll_ext_adv_t.rand_adr
//		scanRsp data stored by (*pScanRspData) from API blc_ll_initExtScanRspDataBuffer
_attribute_data_retention_
rf_pkt_pri_scanrsp_t	pkt_pri_scanrsp = {
		sizeof (rf_pkt_pri_scanrsp_t) - 4,		// dma_len
		LL_TYPE_SCAN_RSP,						// "type": 0x04
		0,										// RFU:0
		0,										// "ChSel" only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr
		0,							          	// rxAddr: scanRsp has no peer device address, so "rxAddr" invalid, always be 0'b.
		sizeof (rf_pkt_pri_scanrsp_t) - 6,		// rf_len
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// advA
};


_attribute_data_retention_
rf_pkt_aux_conn_rsp_t	pkt_aux_conn_rsp = {
		sizeof (rf_pkt_aux_conn_rsp_t) - 4,		// dma_len

		LL_TYPE_AUX_CONNNECT_RSP,				// type
		0,										// RFU
		0,										// "ChSel" only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
		0,										// txAddr	  		may change
		0,										// rxAddr     		may change

		sizeof (rf_pkt_aux_conn_rsp_t) - 6,		// rf_len

		13,										// ext_hdr_len: Extended Header Flags(1) + AdvA(6) + TargetA(6)
		LL_EXTADV_MODE_NON_CONN_NON_SCAN,		// adv_mode

		EXTHD_BIT_ADVA | EXTHD_BIT_TARGETA,  	// ext_hdr_flg : AdvA | TargetA

		{0,0,0,0,0,0},							// advA				need change
		{0,0,0,0,0,0},							// targetA			need change
};







_attribute_data_retention_	_attribute_aligned_(4)	ll_adv_mng_t		bltExtA;



_attribute_data_retention_	_attribute_aligned_(4)	ll_ext_adv_t		*blt_pextadv = NULL;  //global adv_set data pointer
_attribute_data_retention_	_attribute_aligned_(4)	ll_ext_adv_t		*cur_pextadv = NULL;  //latest adv_set pointer, for ADV setting
_attribute_data_retention_	_attribute_aligned_(4)	ll_ext_adv_t		*p_ext_adv;           //for immediate use



_attribute_data_retention_	_attribute_aligned_(4)	u8	*pExtAdvData				= NULL;
_attribute_data_retention_	_attribute_aligned_(4)	u8	*pExtScanRspData			= NULL;


void blc_ll_setAuxAdvChnIdxByCustomers(u8 aux_chn)
{
	bltExtA.custom_aux_chn = aux_chn;
}

ble_sts_t ll_setExtAdv_Enable(int adv_enable){
	blc_ll_setExtAdvEnable_1(cur_pextadv->extAdv_en | BLC_FLAG_STK_ADV, 1, cur_pextadv->adv_handle, 	 0, 	  0);
    return 0;
}

extern ll_module_adv_callback_t	   ll_module_adv_cb;

// if pSecAdv can set to NULL, means using legacy ADV
void blc_ll_initExtendedAdvertising_module(	u8 *pAdvCtrl, u8 *pPriAdv,int num_sets)
{
	LL_FEATURE_MASK_0 |= (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING			<<12);

	ll_module_adv_cb = blt_ext_adv_proc;
	pFunc_ll_SetAdv_Enable= ll_setExtAdv_Enable;


	blt_pextadv = (ll_ext_adv_t *)pAdvCtrl;

	bltExtA.maxNum_advSets = num_sets;

	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear ADV handle

		(blt_pextadv + i)->primary_adv = (rf_pkt_pri_adv_t *)(pPriAdv + i * MAX_LENGTH_PRIMARY_ADV_PKT);

		//set some default value
		(blt_pextadv + i)->adv_handle = INVALID_ADVHD_FLAG;
		(blt_pextadv + i)->pri_phy = BLE_PHY_1M;
		(blt_pextadv + i)->sec_phy = BLE_PHY_1M;
		(blt_pextadv + i)->coding_ind = LE_CODED_S2;

	}
	bltExtA.last_advHand = INVALID_ADVHD_FLAG;




#if (MCU_CORE_TYPE == MCU_CORE_9518)
	if(clock_get_system_clk() == SYSCLK_16M){  //16M
		bltExtA.T_AUX_RSP_INTVL  = SYSTEM_TIMER_TICK_1US/2;
	}
	else if(clock_get_system_clk() == SYSCLK_24M){  //24M
		bltExtA.T_AUX_RSP_INTVL  = SYSTEM_TIMER_TICK_1US*5/2;
	}
	else if(clock_get_system_clk() == SYSCLK_32M){  //32M
		bltExtA.T_AUX_RSP_INTVL  = SYSTEM_TIMER_TICK_1US*3;
	}
	else if(clock_get_system_clk() == SYSCLK_48M){  //48M
		bltExtA.T_AUX_RSP_INTVL  = SYSTEM_TIMER_TICK_1US*4;
	}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)

	if(clock_get_system_clk() == SYS_CLK_16M_Crystal){  //16M
		bltExtA.T_SCAN_RSP_INTVL = 0;
		bltExtA.T_AUX_RSP_INTVL  = 8;
	}
	else if(clock_get_system_clk() == SYS_CLK_24M_Crystal){  //24M
		bltExtA.T_SCAN_RSP_INTVL = 32;
		bltExtA.T_AUX_RSP_INTVL  = 32;
	}
	else if(clock_get_system_clk() == SYS_CLK_32M_Crystal){  //32M
		bltExtA.T_SCAN_RSP_INTVL = 40;
		bltExtA.T_AUX_RSP_INTVL  = 32;
	}
	else if(clock_get_system_clk() == SYS_CLK_48M_Crystal){  //48M
		bltExtA.T_SCAN_RSP_INTVL = 48;
		bltExtA.T_AUX_RSP_INTVL  = 40;
	}
#endif

	//ADV random delay: 10ms most in BLE_spec
	// 8ms: (8<<14)-1 : 2^17 -1 = 0x1FFFF,  clock_time() & 0x1FFFF, max 0x1FFFF, 131072/16000 = 8.192 mS ~= 8mS
	bltExtA.rand_delay = 0x1FFFF;


#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
	//TODO:  if aux_connect is used, csa2 relative Sram code should be valid
#endif




	bltParam.adv_version = ADV_EXTENDED_MASK;   //core_5.0 Extended ADV

}



//set extended secondary ADV packet for all ADV sets
void blc_ll_initExtSecondaryAdvPacketBuffer(u8 *pSecAdv, int sec_adv_buf_len)
{

	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear ADV handle
		(blt_pextadv + i)->secondary_adv = (rf_pkt_ext_adv_t *)(pSecAdv + i * sec_adv_buf_len);
	}
}


//set extended ADV data buffer for all ADV sets
void blc_ll_initExtAdvDataBuffer(u8 *pExtAdvData, int max_len_advData)
{

	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear ADV handle
		(blt_pextadv+i)->maxLen_advData = max_len_advData;
		(blt_pextadv+i)->dat_extAdv = (u8*)(pExtAdvData + max_len_advData*i);
	}
}

//set extended scan response data buffer for all ADV sets
void blc_ll_initExtScanRspDataBuffer(u8 *pScanRspData, int max_len_scanRspData)
{
	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear ADV handle
		(blt_pextadv+i)->maxLen_scanRsp = max_len_scanRspData;
		(blt_pextadv+i)->dat_scanRsp = (u8*)(pScanRspData + max_len_scanRspData*i);
	}
}


//set extended ADV data information for set index(0,1,2...)
void blc_ll_initExtAdvDataBuffer_by_set(int set_index, u8 *pExtAdvData, int max_len_advData)
{

}


//set extended scan response data information for set index(0,1,2...)
void blc_ll_initExtScanRspDataBuffer_by_set(int set_index, u8 *pScanRspData, int max_len_scanRspData)
{

}





u16 		blc_ll_readMaxAdvDataLength(void)
{

	return 	blt_pextadv->maxLen_advData;   //give the first adv_set data to host(if HCI project, all adv_set use same maxLength,
										  //  if other project, we can provide different max Length for different adv_set to save SRAM)
}

u8		 	blc_ll_readNumberOfSupportedAdvSets(void)
{
	return		bltExtA.maxNum_advSets;
}








ble_sts_t  blc_ll_setAdvRandomAddr(u8 advHandle, u8* rand_addr)
{

	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchAvailableAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_MEM_CAP_EXCEEDED;
		}
	}


	smemcpy(cur_pextadv->rand_adr, rand_addr, 6);
	cur_pextadv->rand_adr_flg = 1;


	return BLE_SUCCESS;
}




ble_sts_t 	blc_ll_setExtAdvParam(  adv_handle_t advHandle, 		advEvtProp_type_t adv_evt_prop, u32 pri_advIntervalMin, 		u32 pri_advIntervalMax,
									u8 pri_advChnMap,	 			own_addr_type_t ownAddrType, 	u8 peerAddrType, 			u8  *peerAddr,
									adv_fp_type_t advFilterPolicy,  tx_power_t adv_tx_pow,			le_phy_type_t pri_adv_phy, 	u8 sec_adv_max_skip,
									le_phy_type_t sec_adv_phy, 	 	u8 adv_sid, 					u8 scan_req_noti_en)
{


	// last_advHand stores latest advHandle processed, corresponding to the latest adv_set used, this can save some time.
	//if advHandle a new one, need find a new adv_set for it, and last_advHand & blt_pextadv both updated.
	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchAvailableAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_MEM_CAP_EXCEEDED;
		}
	}

	if (pri_adv_phy != BLE_PHY_1M && pri_adv_phy != BLE_PHY_CODED ) {  //primary ADV can only use 1M_PHY or Coded_PHY
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}


	adv_evt_prop &= 0x1F;  //remove ADVEVT_PROP_MASK_ANON_ADV & ADVEVT_PROP_MASK_INC_TX_PWR

	if(adv_evt_prop & ADVEVT_PROP_MASK_LEGACY){   //Legacy ADV

		if(adv_evt_prop == ADV_EVT_PROP_LEGACY_NON_CONNECTABLE_NON_SCANNABLE_UNDIRECTED){
			#if BQB_5P0_TEST_ENABLE	  //do not know the reason, to pass HCI/DDI/BI-01-C
				if(pri_advIntervalMin < 0x20 || pri_advIntervalMax < 0x20){
					return HCI_ERR_INVALID_HCI_CMD_PARAMS;
				}
				else if(pri_advIntervalMin >= 0x20 && pri_advIntervalMax >= 0x20){
					return HCI_ERR_UNSUPPORTED_FEATURE_PARAM_VALUE;
				}
			#endif
		}

		if(pri_adv_phy != BLE_PHY_1M){   //legacy ADV can only use 1M PHY
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}

	}
	else{

		if(pri_adv_phy == BLE_PHY_2M){   //ADV_EXT_IND can only use 1M PHY & Coded PHY
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}

	}


	//extended ADV can not be 1. direct high duty
	//						  2. connectable and scannable both
	if( ((adv_evt_prop & ADVEVT_PROP_MASK_LEGACY_HD_DIRECTED) == ADVEVT_PROP_MASK_HD_DIRECTED)  || \
		((adv_evt_prop & ADVEVT_PROP_MASK_LEGACY_CONNECTABLE_SCANNABLE)== ADVEVT_PROP_MASK_CONNECTABLE_SCANNABLE))
	{
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}





	cur_pextadv->evt_props = (u16)adv_evt_prop;

	cur_pextadv->adv_chn_mask = pri_advChnMap;
	cur_pextadv->adv_chn_num = 0;
	for (int i=0; i<3; i++){    //calculate how many channel used
		if (pri_advChnMap & BIT(i)){
			cur_pextadv->adv_chn_num ++;
		}
	}

	cur_pextadv->own_addr_type = ownAddrType;
	cur_pextadv->peer_addr_type = peerAddrType;


	cur_pextadv->adv_filterPolicy = (u8)advFilterPolicy;

	cur_pextadv->pri_phy = pri_adv_phy;

	cur_pextadv->sec_phy = sec_adv_phy;
	cur_pextadv->adv_sid = adv_sid;
	cur_pextadv->scan_req_noti_en = scan_req_noti_en;

	smemcpy((char * )cur_pextadv->peer_addr, (char * )peerAddr, BLE_ADDR_LEN);

	//advInterval is 3 bytes(24 bit), max value is 0xFFFFFF(equal to 10485 S),
	//we only support 18bit(max 163.84S), due to system tick is 268S a circle
	//BLE4.2 16bit (40.96S)
	u32 adv_intervalMin = pri_advIntervalMin & 0x3FFFF;
	u32 adv_intervalMax = pri_advIntervalMax & 0x3FFFF;



	if(adv_evt_prop == ADV_EVT_PROP_LEGACY_CONNECTABLE_DIRECTED_HIGH_DUTY){
		adv_intervalMin = adv_intervalMax = ADV_INTERVAL_3_75MS;
	}


	#if 1
		cur_pextadv->advInt_use = (adv_intervalMin + adv_intervalMax)>>1;
	#else //multiple adv_sets

	#endif



	cur_pextadv->param_update_flag = 1;   //will update RF packet before sending_adv


	return BLE_SUCCESS;
}



ble_sts_t 	blc_hci_le_setExtAdvParam( hci_le_setExtAdvParam_cmdParam_t *para, u8 *pTxPower)
{

	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}

	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;


	//*pTxPower =   //TODO
	return blc_ll_setExtAdvParam( para->adv_handle,  	 para->advEvt_props, para->pri_advIntMin[0] | para->pri_advIntMin[1]<<8 , para->pri_advIntMax[0] | para->pri_advIntMax[1]<<8,
								  para->pri_advChnMap,	 para->ownAddrType,   para->peerAddrType, 								   para->peerAddr,
								  para->advFilterPolicy, para->adv_tx_pow,	  para->pri_adv_phy, 								   para->sec_adv_max_skip,
								  para->sec_adv_phy, 	 para->adv_sid, 	  para->scan_req_noti_en);

}


u8 	advData_backup[8];


ble_sts_t  blc_ll_setExtAdvData(u8 advHandle, data_oper_t operation, data_fragm_t fragment_prefer, u8 adv_dataLen, u8 *advdata)
{

	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;



	/*If the advertising set corresponding to the Advertising_Handle parameter does
	not exist, then the Controller shall return the error code Unknown Advertising
	Identifier (0x42). */
	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchAvailableAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
		}
	}



	if(operation == DATA_OPER_UNCHANGEED){ //Unchanged data, just update the ADV_DID(not change any AdvData)
		cur_pextadv->adv_did = ((clock_time()>>4) & 0xFFF)  | 0x001;
		return BLE_SUCCESS;  //not do anything else  (by sihui)
	}



	/* If the advertising set uses legacy advertising PDUs that support advertising data and either Operation is not 0x03 or the
	Advertising_Data_Length parameter exceeds 31 octets, the Controller shall return the error code Invalid HCI Command Parameters (0x12).*/
	if(cur_pextadv->evt_props & ADVEVT_PROP_MASK_LEGACY){
		if(adv_dataLen > 31 || operation != DATA_OPER_COMPLETE){
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}
	}

	/*If the advertising set specifies a type that does not support advertising data, the
	Controller shall return the error code Invalid HCI Parameters (0x12). */
	if( (cur_pextadv->evt_props & ADVEVT_PROP_MASK_CONNECTABLE_SCANNABLE) == ADVEVT_PROP_MASK_SCANNABLE){  //scannable event: no adv_data
		cur_pextadv->curLen_advData = 0; //clear, in case that other event change to Extended Scannable event
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}




	/*If advertising is currently enabled for the specified advertising set and
	Operation does not have the value 0x03 or 0x04, the Controller shall return the
	error code Command Disallowed (0x0C).*/
	/*If Operation is not 0x03 or 0x04 and Advertising_Data_Length is zero, the
	Controller shall return the error code Invalid HCI Command Parameters (0x12). */
	if((u8)operation < DATA_OPER_COMPLETE){

		if(cur_pextadv->extAdv_en){
			return HCI_ERR_CMD_DISALLOWED;
		}

		if(adv_dataLen == 0){
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}
	}


	/*If Operation indicates the start of new data (values 0x01 or 0x03), then any
	existing partial or complete scan response data shall be discarded. If the
	Scan_Response_Data_Length parameter is zero, then Operation shall be
	0x03; this indicates that any existing partial or complete data shall be deleted
	and no new data provided.*/
	int newLen_adv;
	if(adv_dataLen==0 && operation==DATA_OPER_COMPLETE){  //delete existing data
		cur_pextadv->curLen_advData = 0;
		newLen_adv = 0;
	}
	else if(operation==DATA_OPER_FIRST || operation==DATA_OPER_COMPLETE){
		cur_pextadv->curLen_advData = 0;
		newLen_adv = adv_dataLen;
	}
	else{
		newLen_adv = cur_pextadv->curLen_advData + adv_dataLen;
	}

	/*If the combined length of the data
	exceeds the capacity of the advertising set identified by the
	Advertising_Handle parameter (see Section 7.8.57 LE Read Maximum
	Advertising Data Length Command) or the amount of memory currently
	available, all the data shall be discarded and the Controller shall return the
	error code Memory Capacity Exceeded (0x07).*/
	if( newLen_adv > cur_pextadv->maxLen_advData){
		return HCI_ERR_MEM_CAP_EXCEEDED;
	}



	switch(operation){

		case DATA_OPER_INTER:
		{
			cur_pextadv->unfinish_advData = 1;
		}
		break;


		case DATA_OPER_FIRST:
		{
			cur_pextadv->unfinish_advData = 1;
		}
		break;


		case DATA_OPER_LAST:
		{
			cur_pextadv->unfinish_advData = 0;
		}
		break;


		case DATA_OPER_COMPLETE:
		{
			cur_pextadv->unfinish_advData = 0;
		}
		break;


		case DATA_OPER_UNCHANGEED:
		{

		}
		break;

		default:
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}


#if 0
	//note that: Extended Scannable Undirected and Directed event can not set advData, so it can not set ADV_DID
	//but it need ADV_DID(ADI filed valid for EXT_ADV_IND in BLE Spec), we need set ADV_DID in API "blc_ll_setExtScanRspData"
	if(operation != DATA_OPER_UNCHANGEED){ // DATA_OPER_UNCHANGEED need update ADV_DID
		cur_pextadv->adv_did = ((clock_time()>>4) & 0xFFF)  | 0x001;   //quick random value for adv_did
	}
#endif




	//copy data
	if(newLen_adv && adv_dataLen){
		smemcpy((char *)(cur_pextadv->dat_extAdv + cur_pextadv->curLen_advData), (char *)advdata, adv_dataLen);
	}
	cur_pextadv->curLen_advData = newLen_adv;





	/*The LE_Set_Extended_Advertising_Data command is used to set the data
	used in advertising PDUs that have a data field. This command may be issued
	at any time after an advertising set identified by the Advertising_Handle
	parameter has been created using the LE Set Extended Advertising
	Parameters Command (see Section 7.8.53), regardless of whether advertising
	in that set is enabled or disabled.
	If advertising is currently enabled for the specified advertising set, the
	Controller shall use the new data in subsequent extended advertising events
	for this advertising set. If an extended advertising event is in progress when
	this command is issued, the Controller may use the old or new data for that
	event.
	If advertising is currently disabled for the specified advertising set, the data
	shall be kept by the Controller and used once advertising is enabled for that
	set */
	if(!cur_pextadv->unfinish_advData){   //data completed


		//last data, need generate 1 new ADV_DID now(even if new advData are all same as previous, so ADV_DID will change every time when application layer call API "blc_ll_setExtAdvData",)
		//note that: Extended Scannable Undirected and Directed event can not set advData, so it can not set ADV_DID
		//but it need ADV_DID information(ADI filed valid for EXT_ADV_IND in BLE Spec), we need set ADV_DID in API "blc_ll_setExtScanRspData"
		cur_pextadv->adv_did = ((clock_time()>>4) & 0xFFF)  | 0x001;   //quick random value for adv_did


		cur_pextadv->param_update_flag = 1;  //will update RF packet before sending_adv

	}

	return BLE_SUCCESS;
}



volatile u8 dbg_scan_rsp;
ble_sts_t blc_ll_setExtScanRspData(u8 advHandle, data_oper_t operation, data_fragm_t fragment_prefer, u8 scanRsp_dataLen, u8 *scanRspData)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;


	/*If the advertising set corresponding to the Advertising_Handle parameter does
	not exist, then the Controller shall return the error code Unknown Advertising
	Identifier (0x42). */
	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchAvailableAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
		}
	}


	/* If the advertising set uses scannable legacy advertising PDUs and either
	Operation is not 0x03 or the Scan_Response_Data_Length parameter
	exceeds 31 octets, the Controller shall return the error code Invalid HCI
	Command Parameters (0x12).*/
	if( (cur_pextadv->evt_props & ADVEVT_PROP_MASK_LEGACY_SCANNABLE) == ADVEVT_PROP_MASK_LEGACY_SCANNABLE){
		if(scanRsp_dataLen > 31 || operation!=DATA_OPER_COMPLETE){
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}
	}


	/*If advertising is currently enabled for the specified advertising set and
	Operation does not have the value 0x03, the Controller shall return the error
	code Command Disallowed (0x0C). */
	if(cur_pextadv->extAdv_en && (u8)operation != DATA_OPER_COMPLETE){
		return HCI_ERR_CMD_DISALLOWED;
	}



	/*If the Advertising_Data_Length parameter is zero and Operation is 0x03, any
	existing partial or complete data shall be deleted (with no new data provided). */
	int newLen_scanRsp;
	if(scanRsp_dataLen==0 && operation==DATA_OPER_COMPLETE){
		//    Delete data  //
		cur_pextadv->curLen_scanRsp = 0;
		newLen_scanRsp = 0;
		cur_pextadv->unfinish_scanRsp = 0;
		return BLE_SUCCESS;
	}
	/*If the advertising set is non-scannable and the Host uses this command other
	than to delete existing data, the Controller shall return the error code Invalid
	HCI Parameters (0x12).*/
	else if( !(cur_pextadv->evt_props & ADVEVT_PROP_MASK_SCANNABLE) ){
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}
	/*If Operation indicates the start of new data (values 0x01 or 0x03), then any
	existing partial or complete advertising data shall be discarded.*/
	else if(operation==DATA_OPER_FIRST || operation==DATA_OPER_COMPLETE){
		cur_pextadv->curLen_scanRsp = 0;
		newLen_scanRsp = scanRsp_dataLen;  // discarded any existing partial or complete advertising data
	}
	else{
		newLen_scanRsp = cur_pextadv->curLen_scanRsp + scanRsp_dataLen;
	}

	/*If the combined length of the data
	exceeds the capacity of the advertising set identified by the
	Advertising_Handle parameter (see Section 7.8.57 LE Read Maximum
	Advertising Data Length Command) or the amount of memory currently
	available, all the data shall be discarded and the Controller shall return the
	error code Memory Capacity Exceeded (0x07).*/
	if( newLen_scanRsp > cur_pextadv->maxLen_advData){
		return HCI_ERR_MEM_CAP_EXCEEDED;
	}




	switch(operation){

		case DATA_OPER_INTER:
		{
			cur_pextadv->unfinish_scanRsp = 1;
		}
		break;


		case DATA_OPER_FIRST:
		{
			cur_pextadv->unfinish_scanRsp = 1;
		}
		break;


		case DATA_OPER_LAST:
		{
			cur_pextadv->unfinish_scanRsp = 0;
		}
		break;


		case DATA_OPER_COMPLETE:
		{
			cur_pextadv->unfinish_scanRsp = 0;
		}
		break;


		case DATA_OPER_UNCHANGEED:
		{

		}
		break;

		default:
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}


	//copy data
	if(newLen_scanRsp && scanRsp_dataLen){
		smemcpy( (char*)(cur_pextadv->dat_scanRsp + cur_pextadv->curLen_scanRsp), (char*)scanRspData, scanRsp_dataLen);
	}

	cur_pextadv->curLen_scanRsp = newLen_scanRsp;




	//note that: Extended Scannable Undirected and Directed event can not set advData, so it can not set ADV_DID
	//but it need ADV_DID(ADI filed valid for EXT_ADV_IND in BLE Spec), so we need ADV DID here
	if(!cur_pextadv->unfinish_scanRsp && (cur_pextadv->evt_props & ADVEVT_PROP_MASK_LEGACY_CONNECTABLE_SCANNABLE) == ADVEVT_PROP_MASK_SCANNABLE){
		cur_pextadv->adv_did = ((clock_time()>>4) & 0xFFF)  | 0x001;   //quick random value for adv_did
	}



	return BLE_SUCCESS;
}


ble_sts_t   blt_ll_enableExtAdv(int adv_enable)  //only for Stack, app can not use
{

	//NOTE: code below only correct for single adv_set, need improve for multiple adv_sets later

	u8 en = adv_enable & 0xff;
	bltParam.adv_en = en;

	if( (adv_enable & BLC_FLAG_STK_ADV) ||  bltParam.blt_state == BLS_LINK_STATE_IDLE || bltParam.blt_state == BLS_LINK_STATE_ADV )
	{
		reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;			//reset rptr = wptr

		reg_rf_irq_mask = 0;
		CLEAR_ALL_RFIRQ_STATUS;

		if(en)  //enable
		{
			if(bltParam.blt_state != BLS_LINK_STATE_ADV)   // idle/conn_slave -> adv_state
			{
				bltParam.blt_state = BLS_LINK_STATE_ADV;
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
	else{
		return HCI_ERR_CMD_DISALLOWED;
	}


	return BLE_SUCCESS;
}




ble_sts_t blc_ll_setExtAdvEnable_1(u32 extAdv_en, u8 sets_num, u8 advHandle, 	 u16 duration, 	  u8 max_extAdvEvt)
{
	/*If the advertising set corresponding to the Advertising_Handle[i] parameter
	does not exist, then the Controller shall return the error code Unknown
	Advertising Identifier (0x42).*/
	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchExistingAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
		}
	}

	/*If the advertising data or scan response data in the advertising set is not
	complete, the Controller shall return the error code Command Disallowed
	(0x0C).*/
	if(cur_pextadv->unfinish_advData || cur_pextadv->unfinish_scanRsp){
		return HCI_ERR_CMD_DISALLOWED;
	}


	/*If the advertising set's Own_Address_Type parameter is set to 0x01 and the
	random address for the advertising set has not been initialized, the Controller
	shall return the error code Invalid HCI Command Parameters (0x12).*/
	if( cur_pextadv->own_addr_type &&  !cur_pextadv->rand_adr_flg){
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}



	int adv_state_change = 0;
	/*If the LE_Set_Extended_Advertising_Enable command is sent again for an
	advertising set while that set is enabled, the timer used for the duration and the
	number of events counter are reset and any change to the random address
	shall take effect.*/
	if(extAdv_en){   //Duration[i] and Max_Extended_Advertising_Events[i] are ignored when Enable is set to 0x00.

		if(cur_pextadv->evt_props == ADV_EVT_PROP_LEGACY_CONNECTABLE_DIRECTED_HIGH_DUTY){
			cur_pextadv->adv_duration_tick = 1280*SYSTEM_TIMER_TICK_1MS; //high duty, 1.28 S
		}
		else{
			cur_pextadv->adv_duration_tick = duration*10*SYSTEM_TIMER_TICK_1MS;
		}


		cur_pextadv->max_ext_adv_evt = max_extAdvEvt;
		cur_pextadv->run_ext_adv_evt = 0;


		// disable -> enable
		if(!cur_pextadv->extAdv_en){
			adv_state_change = 1;
			cur_pextadv->adv_begin_tick = clock_time() + 2*SYSTEM_TIMER_TICK_1MS;  //ADV begin immediately
		}


	}
	else{
		cur_pextadv->max_ext_adv_evt = 0;
		cur_pextadv->adv_duration_tick = 0;
		cur_pextadv->run_ext_adv_evt = 0;

		// enable -> disable
		if(cur_pextadv->extAdv_en){
			adv_state_change = 1;
		}
	}
	cur_pextadv->extAdv_en = extAdv_en;


	if(adv_state_change){


		#if 0  // multiple adv_sets
			//TODO: find the nearest adv_set's adv_event_tick, set to blt_advExpectTime
		#else  // single adv_set
				blt_advExpectTime = cur_pextadv->adv_event_tick;
		#endif
	}

	return blt_ll_enableExtAdv(extAdv_en);
}


u8 dbg_adv_en[8];
ble_sts_t blc_ll_setExtAdvEnable_n(u32 extAdv_en, u8 sets_num, u8 *pData)
{

	u8 ret_status = BLE_SUCCESS;

	if(sets_num){

		for(int i=0;i<sets_num;i++){
			// pData:  handle[0.1..n-1] +  duration[0.1..n-1] + max_event[0.1..n-1]
			ret_status = blc_ll_setExtAdvEnable_1(extAdv_en, 1, pData[i], pData[sets_num+i*2] | pData[sets_num+i*2+1]<<8,  pData[sets_num*3+i]);

			if( ret_status != BLE_SUCCESS){
				return ret_status;
			}
		}
	}
	else{  // sets_num is 0
		/*If the Enable parameter is set to 0x01 (Advertising is enabled) and
		Number_of_Sets is set to 0x00, the Controller shall return the error code
		Invalid HCI Command Parameters (0x12).*/
		if(extAdv_en){
			return HCI_ERR_INVALID_HCI_CMD_PARAMS;
		}
		/*If Enable and Number_of_Sets are both set to 0x00, then all advertising sets
		are disabled.*/
		else{  //extAdv_en is 0
			//TODO
		}
	}



	return BLE_SUCCESS;
}


ble_sts_t blc_hci_le_setExtAdvEnable(u8 extAdv_en, u8 sets_num, u8 *pData)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;



	/*If the Enable parameter is set to 0x01 (Advertising is enabled) and
	Number_of_Sets is set to 0x00, the Controller shall return the error code
	Invalid HCI Command Parameters (0x12).*/
	if(extAdv_en && !sets_num){
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}



#if 1
	if(sets_num == 1)  //simple 1 set ADV
	{
		return blc_ll_setExtAdvEnable_1(extAdv_en, 1, pData[0],  pData[1] | pData[2]<<8 ,pData[3]);
	}
	else
#endif
	{
		return blc_ll_setExtAdvEnable_n(extAdv_en, sets_num, pData);
	}
}





void blt_clearAdvSetsParam(ll_ext_adv_t		*pEadv)
{
#if 0  //TODO: process later
	pEadv->adv_handle = INVALID_ADVHD_FLAG;  //clear
	pEadv->extAdv_en = 0;  //TODO


	pEadv->pri_phy = pEadv->sec_phy = BLE_PHY_1M;


	*(u32 *)(&pEadv->max_ext_adv_evt) = 0;


	*(u32 *)(&pEadv->adv_filterPolicy) = 0;

	*(u32 *)(&pEadv->with_aux_adv_ind) = 0;

	*(u32 *)(&pEadv->adv_did) = 0;

	*(u32 *)(&pEadv->advInt_use) = 0;


	pEadv->curLen_advData = 0; //The data shall be discarded when the advertising set is removed

	pEadv->curLen_scanRsp = 0;

	pEadv->send_dataLenBackup = 0;



	smemset( (char * )pEadv->rand_adr, 0, 6);
	smemset( (char * )pEadv->peer_addr, 0, 6);
#endif
}




ble_sts_t	blc_ll_removeAdvSet(u8 advHandle)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;



	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear using ADV handle

		p_ext_adv = blt_pextadv+i;

		if( p_ext_adv->adv_handle == advHandle){  //match

			if(p_ext_adv->extAdv_en){   // || (blt_pextadv+i)->periodicAdv_en){
				return HCI_ERR_CMD_DISALLOWED;
			}
			else{
				blt_clearAdvSetsParam(p_ext_adv);
				bltExtA.useNum_advSets --;

				if(bltExtA.last_advHand == advHandle){
					bltExtA.last_advHand = INVALID_ADVHD_FLAG;
				}


				return BLE_SUCCESS;
			}

		}

	}


	return HCI_ERR_UNKNOWN_ADV_IDENTIFIER;  //no adv_handle match
}



ble_sts_t	blt_ll_clearAdvSets(void)
{

	for(int i=0;i<bltExtA.maxNum_advSets; i++){  //clear using ADV handle

		p_ext_adv = blt_pextadv+i;

		if(p_ext_adv->extAdv_en){   // || (blt_pextadv+i)->periodicAdv_en){
			return HCI_ERR_CMD_DISALLOWED;
		}

		blt_clearAdvSetsParam(p_ext_adv);;
	}

	bltExtA.last_advHand = INVALID_ADVHD_FLAG;
	bltExtA.useNum_advSets = 0;


	return BLE_SUCCESS;
}


ble_sts_t	blc_ll_clearAdvSets(void)
{

	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;

	return blt_ll_clearAdvSets();
}




ble_sts_t		blc_ll_setDefaultExtAdvCodingIndication(u8 advHandle, le_ci_prefer_t prefer_CI)
{
	if(advHandle != bltExtA.last_advHand){
		if( blt_ll_searchAvailableAdvSet(advHandle) == INVALID_ADVHD_FLAG){
			return HCI_ERR_MEM_CAP_EXCEEDED;
		}
	}


	if(prefer_CI == CODED_PHY_PREFER_S2){
		cur_pextadv->coding_ind = LE_CODED_S2;
	}
	else if(prefer_CI == CODED_PHY_PREFER_S8){
		cur_pextadv->coding_ind = LE_CODED_S8;
	}


	return BLE_SUCCESS;
}










#if (LL_FEATURE_ENABLE_LE_PERIODIC_ADVERTISING)
ble_sts_t	blc_ll_setPeriodicAdvParam(u8 advHandle, u16 periodic_advIntervalMin, u16 periodic_advIntervalMax, u16 periodic_advProp)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;


	//TODO: add code implementation
	return BLE_SUCCESS;
}


ble_sts_t  blc_ll_setPeriodicAdvData(u8 advHandle, data_oper_t operation, u8 adv_dataLen, u8 *advdata)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;


	//TODO: add code implementation
	return BLE_SUCCESS;
}

ble_sts_t 	blc_ll_setPeriodicAdvEnable(u8 periodicAdv_en, u8 advHandle)
{
	//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
	if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
		return HCI_ERR_CMD_DISALLOWED;
	}
	bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;


	//TODO: add code implementation
	return BLE_SUCCESS;
}

//API
ble_sts_t	blc_ll_setPeriodicAdvParam(u8 advHandle, u16 periodic_advIntervalMin, u16 periodic_advIntervalMax, u16 periodic_advProp);
ble_sts_t  	blc_ll_setPeriodicAdvData(u8 advHandle, data_oper_t operation, u8 adv_dataLen, u8 *advdata);
ble_sts_t 	blc_ll_setPeriodicAdvEnable(u8 periodicAdv_en, u8 advHandle);

#endif





int  		blt_ext_adv_proc(void)
{

	for(int i=0; i<bltExtA.maxNum_advSets; i++)
	{
		p_ext_adv = blt_pextadv + i;
		if( p_ext_adv->extAdv_en &&  (u32)(clock_time() - p_ext_adv->adv_event_tick) < BIT(31)    ){ //timer trigger


			u32 adv_send_tick = clock_time();

			blt_send_adv2();


			//NOTE: code below only correct for single adv_set, need improve for multiple adv_sets later
			if (bltParam.blt_state == BLS_LINK_STATE_ADV)
			{

				u32 adv_tick_inc = p_ext_adv->advInt_use * SYSTEM_TIMER_TICK_625US + (adv_send_tick & bltExtA.rand_delay);
				u32 delta_tick = clock_time() - adv_send_tick;

				if( adv_tick_inc < (delta_tick + 1000*SYSTEM_TIMER_TICK_1US) ){  //in case that ADV timing too long, exceed adv_interval
					adv_tick_inc = delta_tick + 5000*SYSTEM_TIMER_TICK_1US;
				}


				if(abs( (int)(p_ext_adv->adv_event_tick - adv_send_tick) ) < 5000 * SYSTEM_TIMER_TICK_1US){
					p_ext_adv->adv_event_tick += adv_tick_inc;
				}
				else{
					p_ext_adv->adv_event_tick  = adv_send_tick + adv_tick_inc;
				}


				#if 0  // multiple adv_sets
					//TODO:  ADV timing management, blt_advExpectTime is used as the earliest ADV time for different adv_sets.
				#else  // single adv_set
					blt_advExpectTime = p_ext_adv->adv_event_tick;
				#endif

				//reg_system_tick_irq = adv_send_tick + BIT(31);  //adv_state, system tick IRQ will not happen
			}
			else if(bltParam.blt_state == BLS_LINK_STATE_CONN) //enter conn_state
			{

				systimer_clr_irq_status();
				systimer_irq_enable();
				systimer_set_irq_capture(bltc.connExpectTime);//need check

			#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
				u8 * prx = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.wptr & (blt_rxfifo.num-1)));
			#else
				u8 * prx = (u8 *) adv_rx_buff;
			#endif
			
				blt_p_event_callback (BLT_EV_FLAG_CONNECT, prx+DMA_RFRX_OFFSET_DATA, 34); ///140us later than before using 16M system clock.


				break;
			}


		}


	}

	return 0;
}


void blc_ll_procLegacyConnectReq(u8 * prx, u8 txAddr)
{

	//add the dispatch when the connect packets is meaning less
	//interval: 7.5ms - 4s -> 6 - 3200
	// 0<= latency <= (Timeout/interval) - 1, timeout*10/(interval*1.25)= timeout*8/inetrval
	// 0 <= winOffset <= interval
	// 1.25 <= winSize <= min(10ms, interval - 1.25)    10ms/1.25ms = 8
	//Timeout: 100ms-32s   -> 10- 3200     timeout >= (latency+1)*interval*2
	rf_packet_connect_t *connReq_ptr = (rf_packet_connect_t *)(prx + 0);
	if(    connReq_ptr->interval < 6 || connReq_ptr->interval > 3200 	\
		|| connReq_ptr->winSize < 1  || connReq_ptr->winSize > 8 || connReq_ptr->winSize>=connReq_ptr->interval	\
		|| connReq_ptr->timeout < 10 || connReq_ptr->timeout > 3200  	\
	/*	|| connReq_ptr->winOffset > connReq_ptr->interval			 	\ */
		|| connReq_ptr->hop == 0){

		return ;
	}
	if( !connReq_ptr->chm[0]){
		if( !connReq_ptr->chm[1] && !connReq_ptr->chm[2] &&  \
			!connReq_ptr->chm[3] && !connReq_ptr->chm[4]){

			return;
		}
	}
	if(connReq_ptr->latency){
		if(connReq_ptr->latency  >  ((connReq_ptr->timeout<<3)/connReq_ptr->interval)){

			return;
		}
	}



	if(    p_ext_adv->evt_props != ADV_EVT_PROP_LEGACY_SCANNABLE_UNDIRECTED && \
		( (p_ext_adv->evt_props & ADVEVT_PROP_MASK_DIRECTED)   || \
		 !(p_ext_adv->adv_filterPolicy & ALLOW_CONN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList(txAddr, prx+6)) ){

		if(ll_adv2conn_cb){
			ll_adv2conn_cb(prx, FALSE);  //blt_connect
		}

		bltParam.adv_scanReq_connReq = 2;
	}
}



_attribute_ram_code_ void blt_ll_procLegacyRxPacket(u8 *prx, u16 *pmac)
{
	rf_pkt_pri_adv_t* pAdv = p_ext_adv->primary_adv;

	u32 rx_begin_tick = clock_time ();
	rf_packet_scan_req_t * pReq = (rf_packet_scan_req_t *) (prx + 0);
	u16 *advA16 = (u16 *)pAdv->advA;

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	pkt_pri_scanrsp.dma_len = rf_tx_packet_dma_len(pkt_pri_scanrsp.rf_len + 2);//4 bytes align
	reg_dma_src_addr(DMA0)=convert_ram_addr_cpu2bus(&pkt_pri_scanrsp);

//	REG_ADDR16(0x800c0c) = (u16)((u32)&pkt_pri_scanrsp);  //get ready scan_rsp as early as possible

	while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 400 * SYSTEM_TIMER_TICK_1US);


	if(prx[5] == 12) //scan_req
	{
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
	}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	reg_dma3_addr = (u16)((u32)&pkt_pri_scanrsp);  //get ready scan_rsp as early as possible   REG_ADDR16(0x800c0c)

	while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (clock_time() - rx_begin_tick) < 400 * SYSTEM_TIMER_TICK_1US);

	reg_rf_ll_cmd_schedule = clock_time() + bltExtA.T_SCAN_RSP_INTVL; //  REG_ADDR32(0xf18)
	reg_rf_ll_cmd = FLD_RF_R_CMD_TRIG | FLD_RF_R_STX; // 0x85--single TX    REG_ADDR8 (0xf00)
#endif



	reg_rf_irq_status = FLD_RF_IRQ_RX;



	if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(prx) && RF_BLE_RF_PACKET_CRC_OK(prx) )
	{

		if (MAC_MATCH16(pmac, advA16))
		{
			if (pReq->type == LL_TYPE_SCAN_REQ )
			{

				if(  pAdv->type != LL_TYPE_ADV_DIRECT_IND &&  \
					 (!(p_ext_adv->adv_filterPolicy & ALLOW_SCAN_WL) || ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6) )){

					sleep_us(500);

					blt_p_event_callback (BLT_EV_FLAG_SCAN_RSP, NULL, 0);

					if(!bltParam.blc_continue_adv_en){
						bltParam.adv_scanReq_connReq = 2;
					}
				}

			}
			else if (pReq->type == LL_TYPE_CONNNECT_REQ)  // && (pReq->rf_len&0x3f) == 34)
			{
				STOP_RF_STATE_MACHINE;

				blc_rcvd_connReq_tick = clock_time();
				blc_ll_procLegacyConnectReq(prx,  pReq->txAddr);
			}
		}
	}

	STOP_RF_STATE_MACHINE;
	
	#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		prx[0] = 1;
	#endif
}


//rf_pkt_pri_adv_t* pCurAdv;
int  blt_send_legacy_adv(void)
{


//	pCurAdv = p_ext_adv->primary_adv;  //use "pCurAdv" to optimize code
	p_ext_adv->primary_adv->dma_len = rf_tx_packet_dma_len(p_ext_adv->primary_adv->rf_len + 2);//4 bytes align

	u32 t_us = (p_ext_adv->primary_adv->rf_len + 10) * 8 + 400;  //timing come form old SDK

	for (int i=0; i<3; i++)
	{
		if (p_ext_adv->adv_chn_mask & BIT(i))
		{

			STOP_RF_STATE_MACHINE;						// stop SM
			rf_set_ble_channel (blc_adv_channel[i]);


			reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;
			////////////// start TX //////////////////////////////////
//			if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_TX_ON);  }

			u32 tx_begin_tick;
			if(p_ext_adv->evt_props == ADV_EVT_PROP_LEGACY_NON_CONNECTABLE_NON_SCANNABLE_UNDIRECTED){ //ADV_NONCONN_IND
				rf_start_fsm(FSM_STX,(void *)p_ext_adv->primary_adv, clock_time() + 100);
				tx_begin_tick = clock_time ();
				while (!(reg_rf_irq_status & FLD_RF_IRQ_TX) && (clock_time() - tx_begin_tick) < (t_us - 200)*SYSTEM_TIMER_TICK_1US);
			}
			else{
				rf_start_fsm(FSM_TX2RX,(void *)p_ext_adv->primary_adv, clock_time() + 100);
				tx_begin_tick = clock_time ();

			#if (MCU_CORE_TYPE == MCU_CORE_9518)
				u8 * prx = (u8 *) (adv_rx_buff);
			#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
				u8 * prx = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.wptr & (blt_rxfifo.num-1)));
			#endif
				u16 *pmac = (u16*)(prx + 12);

				volatile u32 *ph  = (u32 *) (prx + 4);
				ph[0] = 0;

		/////////////////////////////////////////////////////////////////////////////////////////
		//here waiting for TX done,
				// ADV data 0  bytes, rf_len = 6+ 0 = 6,  TX time = (10+6) *8 = 128 uS
				// ADV data 31 bytes, rf_len = 6+31 = 37, TX time = (10+37)*8 = 376 uS
				// 128 ~ 376 uS can be used

				//set scanRsp packet here to save some time(if set after scanRsp is trigger, cost some time, DMA data may ERROR)

				pkt_pri_scanrsp.txAddr = p_ext_adv->primary_adv->txAddr;  //scanRsp's RxAdd is same as advPkt's RxAdd

				if(pkt_pri_scanrsp.txAddr == BLE_ADDR_RANDOM){
					smemcpy( (char *)pkt_pri_scanrsp.advA, (char *)p_ext_adv->rand_adr, BLE_ADDR_LEN );
				}
				else{
					smemcpy( (char *)pkt_pri_scanrsp.advA, (char *)bltMac.macAddress_public, BLE_ADDR_LEN );
				}
				smemcpy( (char *)pkt_pri_scanrsp.data, (char *)p_ext_adv->dat_scanRsp,  p_ext_adv->curLen_scanRsp );
				pkt_pri_scanrsp.rf_len = p_ext_adv->curLen_scanRsp + 6;
				pkt_pri_scanrsp.dma_len =rf_tx_packet_dma_len(pkt_pri_scanrsp.rf_len + 2);



				while(!(reg_rf_irq_status & FLD_RF_IRQ_TX) );
		/////////////////////////////////////////////////////////////////////////////////////////
				reg_rf_irq_status = FLD_RF_IRQ_TX;
				//if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_RX_ON);  }

				while (!(*ph) && (u32)(clock_time() - tx_begin_tick) < t_us * SYSTEM_TIMER_TICK_1US);	//wait packet from master


				if (*ph){
					blt_ll_procLegacyRxPacket(prx, pmac);
				}


				if(bltParam.adv_scanReq_connReq == 2){
					i = 4;  //break;
				}
			}


//			if(blc_rf_pa_cb){	blc_rf_pa_cb(PA_TYPE_OFF);  }


		}

	}

	return 0;
}



int  blt_ll_updateAdvPacket(void)
{

	int directed_adv =  p_ext_adv->evt_props & ADVEVT_PROP_MASK_DIRECTED;

	if(p_ext_adv->evt_props & ADVEVT_PROP_MASK_LEGACY)  //legacy ADV PDU
	{

			rf_pkt_pri_adv_t* pLgAdv = p_ext_adv->primary_adv;   //p_legacy_adv


			//CONNECTABLE_DIRECTED_LOW_DUTY or CONNECTABLE_DIRECTED_HIGH_DUTY
			if(directed_adv){    // /ADV_INDIRECT_IND (high duty cycle) or ADV_INDIRECT_IND (low duty cycle)
				pLgAdv->rxAddr = cur_pextadv->peer_addr_type;
				pLgAdv->type = LL_TYPE_ADV_DIRECT_IND;

				// fill in ADV data
				smemcpy(pLgAdv->data, cur_pextadv->peer_addr, BLE_ADDR_LEN);
				pLgAdv->rf_len = 12;
				pLgAdv->dma_len = 14;

				#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
					pLgAdv->chan_sel = bltParam.local_chSel;
				#endif
			}
			else{
				pLgAdv->rxAddr = BLE_ADDR_PUBLIC;
				if(p_ext_adv->evt_props == ADV_EVT_PROP_LEGACY_CONNECTABLE_SCANNABLE_UNDIRECTED){   		  // ADV_IND
					pLgAdv->type = LL_TYPE_ADV_IND;

					#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
						pLgAdv->chan_sel = bltParam.local_chSel;
					#endif
				}
				else if(p_ext_adv->evt_props == ADV_EVT_PROP_LEGACY_SCANNABLE_UNDIRECTED){		  // ADV_SCAN_IND
					pLgAdv->type = LL_TYPE_ADV_SCAN_IND;
				}
				else if(p_ext_adv->evt_props == ADV_EVT_PROP_LEGACY_NON_CONNECTABLE_NON_SCANNABLE_UNDIRECTED){   // ADV_NONCONN_IND
					pLgAdv->type = LL_TYPE_ADV_NONCONN_IND;
				}



				// fill in ADV data
				if(p_ext_adv->curLen_advData){  //ADV data is ready
					smemcpy(pLgAdv->data, p_ext_adv->dat_extAdv, p_ext_adv->curLen_advData);
				}
				pLgAdv->rf_len = p_ext_adv->curLen_advData + BLE_ADDR_LEN;
				pLgAdv->dma_len = pLgAdv->rf_len + 2;

			}

			//Own address and address type process
			if(cur_pextadv->own_addr_type == OWN_ADDRESS_PUBLIC){
				pLgAdv->txAddr = BLE_ADDR_PUBLIC;
				smemcpy(pLgAdv->advA, bltMac.macAddress_public, BLE_ADDR_LEN);

				//p_ext_adv->hd_scanrsp.txAddr = BLE_ADDR_PUBLIC;  //no need now, scanRsp's RxAdd is same as advPkt's RxAdd
			}
			else if(cur_pextadv->own_addr_type == OWN_ADDRESS_RANDOM){
				pLgAdv->txAddr = BLE_ADDR_RANDOM;
				smemcpy(pLgAdv->advA, p_ext_adv->rand_adr, BLE_ADDR_LEN);

				//p_ext_adv->hd_scanrsp.txAddr = BLE_ADDR_RANDOM;  //no need now, scanRsp's RxAdd is same as advPkt's RxAdd
			}
			else{   //OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC / OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM
				// TODO
			}




	}
	else  //Extended ADV
	{

			int cur_adv_mode = (p_ext_adv->evt_props & ADVEVT_PROP_MASK_CONNECTABLE_SCANNABLE);

			/***********************  ADV_EXT_IND prepare  **********************************************************/
			rf_pkt_adv_ext_ind_1* p_adv_ext_ind_1 = (rf_pkt_adv_ext_ind_1* )p_ext_adv->primary_adv;
			rf_pkt_adv_ext_ind_2* p_adv_ext_ind_2 = (rf_pkt_adv_ext_ind_2* )p_ext_adv->primary_adv;

			p_adv_ext_ind_1->type = LL_TYPE_ADV_EXT_IND;
			p_adv_ext_ind_1->chan_sel = 0;  //"ChSel" only valid in ADV_IND/ADV_DIRECT_IND/CONNECT_IND, other packet set 0'b
			p_adv_ext_ind_1->adv_mode = cur_adv_mode;


			if(!cur_adv_mode && !p_ext_adv->curLen_advData){  //Non_Connectable Non_Scannable, no ADV data
				p_ext_adv->with_aux_adv_ind = 0;
			}
			else{
				p_ext_adv->with_aux_adv_ind = 1;
			}

			p_adv_ext_ind_1->txAddr = 0;  //clear
			p_adv_ext_ind_1->rxAddr = 0;  //clear
			p_adv_ext_ind_2->ext_hdr_len = 6;  //Extended Header Flags(1) + ADI(2) + AuxPtr(3)
			p_adv_ext_ind_2->ext_hdr_flg = EXTHD_BIT_ADI | EXTHD_BIT_AUX_PTR;
			if(p_ext_adv->with_aux_adv_ind)
			{   //add ADI & AuxPtr

				//ADI information
				p_adv_ext_ind_2->sid = p_ext_adv->adv_sid;
				p_adv_ext_ind_2->did = p_ext_adv->adv_did;

				//part of AuxPtr
				p_adv_ext_ind_2->ca = EXT_ADV_PDU_AUXPTR_CA_0_50_PPM;  		   			// 0~50 ppm
				p_adv_ext_ind_2->offset_unit = EXT_ADV_PDU_AUXPTR_OFFSET_UNITS_30_US;   //30us unit
				p_adv_ext_ind_2->aux_phy = p_ext_adv->sec_phy - 1; // le_phy_type_t 1/2/3 corresponding 0/1/2 in packet
			}
			else{  // without auxiliary packet
				//both have "AdvA"
				if(cur_pextadv->own_addr_type == OWN_ADDRESS_PUBLIC){
					p_adv_ext_ind_1->txAddr = BLE_ADDR_PUBLIC;
					smemcpy( (char * )p_adv_ext_ind_1->advA, (char * )bltMac.macAddress_public, BLE_ADDR_LEN);
				}
				else if(cur_pextadv->own_addr_type == OWN_ADDRESS_RANDOM){
					p_adv_ext_ind_1->txAddr = BLE_ADDR_RANDOM;
					smemcpy( (char * )p_adv_ext_ind_1->advA, (char * )p_ext_adv->rand_adr, BLE_ADDR_LEN);
				}
				else{   //OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC / OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM
					// TODO
				}

				// "ADV_EVT_PROP_EXTENDED_NON_CONNECTABLE_NON_SCANNABLE_DIRECTED" has TargetA
				if(directed_adv){  //directed, with "TargetA"
					p_adv_ext_ind_1->rxAddr = cur_pextadv->peer_addr_type;
					smemcpy( (char * )p_adv_ext_ind_1->targetA, (char * )cur_pextadv->peer_addr, BLE_ADDR_LEN);

					p_adv_ext_ind_1->ext_hdr_len = 13;  //Extended Header Flags(1) + AdvA(6) + TargetA(6)
					p_adv_ext_ind_1->ext_hdr_flg = EXTHD_BIT_ADVA | EXTHD_BIT_TARGETA;

				}
				else{ ////undirected, no "TargetA"
					p_adv_ext_ind_1->ext_hdr_len = 7;  //Extended Header Flags(1) + AdvA(6)
					p_adv_ext_ind_1->ext_hdr_flg = EXTHD_BIT_ADVA;
				}
			}

			p_adv_ext_ind_1->rf_len  = p_adv_ext_ind_1->ext_hdr_len + 1;
			p_adv_ext_ind_1->dma_len = p_adv_ext_ind_1->rf_len + 2;
			/**********************************************************************************************************/




			/***********************  AUX_ADV_IND prepare  **************************************************************/
			if(	p_ext_adv->with_aux_adv_ind)
			{

				rf_pkt_aux_adv_ind_1* p_aux_adv_ind_1 = (rf_pkt_aux_adv_ind_1* )p_ext_adv->secondary_adv;
				rf_pkt_aux_adv_ind_2* p_aux_adv_ind_2 = (rf_pkt_aux_adv_ind_2* )p_ext_adv->secondary_adv;

				// type & txAddr &  rxAddr
				p_aux_adv_ind_1->type = LL_TYPE_AUX_ADV_IND;   //can do it when initialization


				//Own address and address type process
				if(p_ext_adv->own_addr_type == OWN_ADDRESS_PUBLIC){
					p_aux_adv_ind_1->txAddr = BLE_ADDR_PUBLIC;
					smemcpy( (char * )p_aux_adv_ind_1->advA, (char * )bltMac.macAddress_public, BLE_ADDR_LEN);
				}
				else if(p_ext_adv->own_addr_type == OWN_ADDRESS_RANDOM){
					p_aux_adv_ind_1->txAddr = BLE_ADDR_RANDOM;
					smemcpy( (char * )p_aux_adv_ind_1->advA, (char * )p_ext_adv->rand_adr, BLE_ADDR_LEN);
				}
				else{
					//TODO:
				}



				p_aux_adv_ind_1->adv_mode = cur_adv_mode;

				if(directed_adv){  //directed, with "TargetA"
					p_aux_adv_ind_2->ext_hdr_len = 15;  // Extended Header Flags(1) + AdvA(6) + TargetA(6) + ADI(2)
					p_aux_adv_ind_2->ext_hdr_flg = EXTHD_BIT_ADVA | EXTHD_BIT_TARGETA | EXTHD_BIT_ADI;

					p_aux_adv_ind_2->rxAddr = p_ext_adv->peer_addr_type;
					smemcpy( (char * )p_aux_adv_ind_2->targetA, (char * )p_ext_adv->peer_addr, BLE_ADDR_LEN);

					p_aux_adv_ind_2->did = p_ext_adv->adv_did;
					p_aux_adv_ind_2->sid = p_ext_adv->adv_sid;
				}
				else{ // undirected, no "TargetA"
					p_aux_adv_ind_1->rxAddr = 0;

					p_aux_adv_ind_1->ext_hdr_len = 9;   // Extended Header Flags(1) + AdvA(6) + ADI(2)
					p_aux_adv_ind_1->ext_hdr_flg = EXTHD_BIT_ADVA | EXTHD_BIT_ADI;

					p_aux_adv_ind_1->did = p_ext_adv->adv_did;
					p_aux_adv_ind_1->sid = p_ext_adv->adv_sid;
				}



				if(cur_adv_mode) //Connectable only or Scannable only
				{

					//According to BLE Spec curLen_advData should not very big, 1 packet is enough(add some check when set AdvData)
					if(p_ext_adv->curLen_advData){  //ADV data valid, only "Connectable only" event can have AdvData
						if( directed_adv ){ //Connectable Directed
							smemcpy( (char * )p_aux_adv_ind_2->dat, (char * )p_ext_adv->dat_extAdv, p_ext_adv->curLen_advData);
						}
						else{  //Connectable Undirected
							smemcpy( (char * )p_aux_adv_ind_1->dat, (char * )p_ext_adv->dat_extAdv, p_ext_adv->curLen_advData);
						}
					}

					p_aux_adv_ind_1->rf_len  = p_aux_adv_ind_1->ext_hdr_len + 1 + p_ext_adv->curLen_advData;  // ext_hdr_len + 1 + curLen_advData
					p_aux_adv_ind_1->dma_len = p_aux_adv_ind_1->rf_len + 2;
				}
				else     // Non_Connectable Non_Scannable
				{
					//AUX_ADV_IND		Undirected : rf_len = 10,  Extended_Header_Length(1) + Extended_Header_Flags(1) + AdvA(6) + ADI(2)
					//			   			max rest data 255-10=245,    if add AuxPtr, max 242
					//					Directed :   rf_len = 16,  Extended_Header_Length(1) + Extended Header Flags(1) + AdvA(6) + TargetA(6) + ADI(2)
					//			   			max rest data 255-16=239,   if add AuxPtr, max 236
					int max_advData = directed_adv ? 239 : 245;

					if(p_ext_adv->curLen_advData > max_advData){  //with AUX_CHAIN_IND
						p_ext_adv->with_aux_chain_ind = 1;
						p_ext_adv->send_dataLen = max_advData - 3;

						///// add AuxPtr //////////
						p_aux_adv_ind_1->ext_hdr_flg |= EXTHD_BIT_AUX_PTR; // + AuxPtr(3 byte)
						p_aux_adv_ind_1->ext_hdr_len += 3;


						//AuxPtr: channel index(6) + CA(1) + OffsetUnits(1) + AuxOffset(13) + AuxPHY(3)
						if(directed_adv){
							rf_pkt_aux_adv_ind_4* p_aux_adv_ind_4 = (rf_pkt_aux_adv_ind_4* )p_ext_adv->secondary_adv;

							p_aux_adv_ind_4->ca = EXT_ADV_PDU_AUXPTR_CA_0_50_PPM;
							p_aux_adv_ind_4->offset_unit = EXT_ADV_PDU_AUXPTR_OFFSET_UNITS_30_US;
							p_aux_adv_ind_4->aux_phy = (p_ext_adv->sec_phy - 1);

							smemcpy( (char * )(p_aux_adv_ind_4->dat), (char * )(p_ext_adv->dat_extAdv + 0), p_ext_adv->send_dataLen);
						}
						else{
							rf_pkt_aux_adv_ind_3* p_aux_adv_ind_3 = (rf_pkt_aux_adv_ind_3* )p_ext_adv->secondary_adv;

							p_aux_adv_ind_3->ca = EXT_ADV_PDU_AUXPTR_CA_0_50_PPM;
							p_aux_adv_ind_3->offset_unit = EXT_ADV_PDU_AUXPTR_OFFSET_UNITS_30_US;
							p_aux_adv_ind_3->aux_phy = (p_ext_adv->sec_phy - 1);

							smemcpy( (char * )(p_aux_adv_ind_3->dat), (char * )(p_ext_adv->dat_extAdv + 0), p_ext_adv->send_dataLen);
						}


						p_aux_adv_ind_1->rf_len = 255;
					}
					else{  //no AUX_CHAIN_IND
						p_ext_adv->send_dataLen = p_ext_adv->curLen_advData;
						p_aux_adv_ind_1->rf_len = (directed_adv ? 16 : 10) + p_ext_adv->curLen_advData;


						if(directed_adv){
							smemcpy( (char * )(p_aux_adv_ind_2->dat), (char * )(p_ext_adv->dat_extAdv + 0), p_ext_adv->send_dataLen);
						}
						else{
							smemcpy( (char * )(p_aux_adv_ind_1->dat), (char * )(p_ext_adv->dat_extAdv + 0), p_ext_adv->send_dataLen);
						}

					}
					p_ext_adv->send_dataLenBackup = p_ext_adv->send_dataLen;
					p_aux_adv_ind_1->dma_len = p_aux_adv_ind_1->rf_len + 2;
				}

			}
			/**********************************************************************************************************************/

	}  //end of Extended ADV


	return 0;
}

#if(ADV_DURATION_STALL_EN)
_attribute_ram_code_
unsigned int cpu_stall_WakeUp_By_RF_SystemTick(int WakeupSrc, unsigned short rf_mask, unsigned int tick)
{
    unsigned int wakeup_src = 0;
    unsigned short rf_irq_mask_save =0;
    unsigned int irq_mask_save =0;
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)//todo eagle close; kite/vulture open
    rf_irq_mask_save = reg_rf_irq_mask;
    irq_mask_save = reg_irq_mask;

	#if (MCU_CORE_TYPE == MCU_CORE_9518)
    	plic_interrupt_disable
		if (WakeupSrc & STIMER_IRQ_MASK)
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		if (WakeupSrc & FLD_IRQ_SYSTEM_TIMER)
	#endif
	{

		systimer_set_irq_capture(tick);//need check
		systimer_clr_irq_status();


		systimer_irq_disable();

    }

    if (WakeupSrc & FLD_IRQ_ZB_RT_EN)
    {
    	reg_rf_irq_mask = rf_mask;
    	reg_irq_mask &= (~FLD_IRQ_ZB_RT_EN);
    }

    REG_ADDR32(0x78) |= WakeupSrc;

    write_reg8(0x6f, 0x80); //stall mcu
    asm("tnop");
    asm("tnop");

    //store the wakeup source
    wakeup_src = REG_ADDR32(0x40);

    //clear the source
    systimer_clr_irq_status(); // clear timer1 irq source

    CLEAR_ALL_RFIRQ_STATUS;//clear rf irq status
    //restore register
    reg_rf_irq_mask  = rf_irq_mask_save;
    reg_irq_mask = irq_mask_save;
#endif
    return wakeup_src;
}
#endif


/**********************************************************************************************************************

			37				   38               39
       _____________	 _____________    _____________
      |             |	|             |	 |             |
      | ADV_EXT_IND |   | ADV_EXT_IND |  | ADV_EXT_IND |
 	  |_____________|	|_____________|  |_____________|
														   	   _____________		 _____________		 _____________
															  |             |		|             |		|             |
															  | AUX_ADV_IND |		|AUX_CHAIN_IND|		|AUX_CHAIN_IND|
															  |_____________|		|_____________|		|_____________|


*********************************************************************************************************************/
#define	FSM_TRIGGER_EARLY_WAIT_TICK			(10*SYSTEM_TIMER_TICK_1US)

_attribute_ram_code_
int  blt_send_extend_adv(void)
{

	// ADV_EXT_IND, no "TxPower", max length: ext_hdr_len 13 byte, rf_len 14 byte, dam_len 16 byte
	/*
	 * 												AdvA  TargetA  ADI	  Aux	Sync	Tx   ACAD	AdvData			Structure
																		  Ptr  	Info   Power
	Non-Connectable
	Non-Scannable   Undirected with    AUX			 X 	 	 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2

	Non-Connectable
	Non-Scannable	Directed   with	   AUX			 X	 	 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2


	Connectable 	Undirected					 	 X		 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2

	Connectable 	Directed					 	 X		 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2

	Scannable 		Undirected				     	 X		 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2

	Scannable 		Directed					 	 X		 X		M	   M	 X		X	   X	 X			rf_pkt_adv_ext_ind_2

	**/
	int directed_adv =  p_ext_adv->evt_props & ADVEVT_PROP_MASK_DIRECTED;
	int cur_dataLen = 0;
	int offset = 0;
	int aux_chn_backup;
	int aux_chn_index = BLT_GENERATE_AUX_CHN;

	if(p_ext_adv->with_aux_adv_ind){  //with auxiliary packet
		rf_pkt_adv_ext_ind_2* p_adv_ext_ind_2 = (rf_pkt_adv_ext_ind_2* )p_ext_adv->primary_adv;

		p_adv_ext_ind_2->chn_index = aux_chn_index;


		//TODO: update for different PHYs

		/**************************************************************************************************************************
		 * 	   PHYs			  timing(uS)
		 *   1M PHY   :    (rf_len + 10) * 8,      // 10 = 1(BLE preamble) + 9(accesscode 4 + crc 3 + header 2)
		 *   2M PHY   :	   (rf_len + 11) * 4	   // 11 = 2(BLE preamble) + 9(accesscode 4 + crc 3 + header 2)
		 *  Coded PHY :    376 + (rf_len*8+43)*S  		// 376uS = 80uS(preamble) + 256uS(Access Code) + 16uS(CI) + 24uS(TERM1)
		 *************************************************************************************************************************/
		#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
				int packet_us;
				if(p_ext_adv->pri_phy == BLE_PHY_1M){
					 packet_us = (p_ext_adv->primary_adv->rf_len + 10) * 8    + (TX_TX_DELAY_US + 20 + 29); //20: software loop running time margin,  29: packet_us/30, carry 1
				}
				else{  //Coded PHY
					 packet_us = (p_ext_adv->primary_adv->rf_len * 8 + 43) * p_ext_adv->coding_ind + 376    + (TX_TX_DELAY_US + 30 + 29);  //30: software loop running time margin(Coded PHY take more timing in TX done)
				}
		#else
				int packet_us = (p_ext_adv->primary_adv->rf_len + 10) * 8    + (TX_TX_DELAY_US + 20 + 29);
		#endif
		int packet_30us_unit = packet_us/30;
		packet_us = packet_30us_unit*30;   //to be 30uS*N
		p_ext_adv->primary_adv->dma_len = rf_tx_packet_dma_len(p_ext_adv->primary_adv->rf_len + 2);//4 bytes align

		// leave "TLK_T_MAFS" uS between AUX_ADV_IND and last ADV_EXT_IND

		int chn_num_index = 0;

		// offset: packet_30us_unit*N + TLK_T_MAFS = packet_30us_unit*(p_ext_adv->adv_chn_num - chn_num_index) + TLK_T_MAFS
		u32 tick_wait;
		u32 tx_begin_tick = clock_time() + 30*SYSTEM_TIMER_TICK_1US; //30 uS later send first packet
		for (int i=0; i<3; i++)
		{
			if (p_ext_adv->adv_chn_mask & BIT(i))
			{

				STOP_RF_STATE_MACHINE;						// stop SM
				rf_set_ble_channel (blc_adv_channel[i]);
				reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

				tick_wait = tx_begin_tick - FSM_TRIGGER_EARLY_WAIT_TICK;

				while((u32)(clock_time() - tick_wait) > BIT(30));
				rf_start_fsm(FSM_STX,(void *)p_ext_adv->primary_adv, tx_begin_tick);


				p_adv_ext_ind_2->aux_offset = packet_30us_unit*(p_ext_adv->adv_chn_num - chn_num_index) + (TLK_T_MAFS_30US_NUM - TX_TX_DELAY_US/30);

			#if(ADV_DURATION_STALL_EN)
				cpu_stall_WakeUp_By_RF_SystemTick(FLD_IRQ_ZB_RT_EN, FLD_RF_IRQ_TX, 0);
			#else
				while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish
			#endif

				tx_begin_tick += packet_us*SYSTEM_TIMER_TICK_1US;  //next STX tick


				chn_num_index ++;
			}
		}



		tx_begin_tick += (TLK_T_MAFS - TX_TX_DELAY_US )*SYSTEM_TIMER_TICK_1US;



		//switch to secondary ADV PHY
		#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
			if(p_ext_adv->sec_phy != bltPHYs.cur_llPhy || (p_ext_adv->coding_ind != bltPHYs.cur_CI && bltPHYs.cur_llPhy == BLE_PHY_CODED)){

				if(ll_phy_switch_cb){
					ll_phy_switch_cb(p_ext_adv->sec_phy, p_ext_adv->coding_ind);
				}

				if(p_ext_adv->sec_phy == BLE_PHY_CODED){
					rf_trigle_codedPhy_accesscode();
				}
			}


			if(bltPHYs.cur_llPhy == BLE_PHY_CODED){
				rf_tx_settle_adjust(LL_ADV_TX_STL_CODED);
			}
			else if(bltPHYs.cur_llPhy == BLE_PHY_2M){
				rf_tx_settle_adjust(LL_ADV_TX_STL_2M);
			}
			else{
				rf_tx_settle_adjust(LL_ADV_TX_SETTLE);
			}
		#endif



		// "TLK_T_MAFS" uS for AUX_ADV_IND packet combination
		rf_pkt_aux_adv_ind_1* p_aux_adv_ind_1 = (rf_pkt_aux_adv_ind_1* )p_ext_adv->secondary_adv;
		rf_pkt_aux_adv_ind_2* p_aux_adv_ind_2 = (rf_pkt_aux_adv_ind_2* )p_ext_adv->secondary_adv;

		int cur_adv_mode = (p_ext_adv->evt_props & ADVEVT_PROP_MASK_CONNECTABLE_SCANNABLE);



		if(cur_adv_mode)  //Connectable only or Scannable only
		{
			p_ext_adv->secondary_adv->dma_len = rf_tx_packet_dma_len(p_ext_adv->secondary_adv->rf_len + 2);//4 bytes align

			STOP_RF_STATE_MACHINE;
			rf_set_ble_channel (aux_chn_index);
			reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

			//send only 1 AUX_ADV_IND, no AuxPtr, then listen to AUX_SCAN_REQ or AUX_CONN_REQ
			tick_wait = tx_begin_tick - FSM_TRIGGER_EARLY_WAIT_TICK;
			while((u32)(clock_time() - tick_wait) > BIT(30));

			#if(LL_FEATURE_ENABLE_LE_2M_PHY || LL_FEATURE_ENABLE_LE_CODED_PHY)
				reg_rf_rx_timeout = 0xffff;
			#else
				//reg_rf_rx_timeout = 250; //dft
			#endif

			#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x) //eagle--0; kite/vulutre--1  //save SRAM for other project which not use Extend_ADV (rf_start_stx2rx no need in ramCode)
				write_reg32(reg_rf_ll_cmd_schedule, tx_begin_tick);
				//write_reg8(reg_rf_ll_ctrl_3, read_reg8(reg_rf_ll_ctrl_3) | 0x04);	// core_f16<2>  already set in previous "rf_start_stx"
				write_reg8  (reg_rf_ll_cmd, 0x87);	// single tx2rx
				write_reg16 (reg_dma3_addr, (unsigned short)((unsigned int)p_ext_adv->secondary_adv));
			#else
				rf_start_fsm(FSM_TX2RX,(void *)p_ext_adv->secondary_adv, tx_begin_tick);
			#endif
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			u8 * prx = (u8 *) (adv_rx_buff);
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			u8 * prx = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.wptr & (blt_rxfifo.num-1)));
		#endif
			volatile u32 *ph  = (u32 *) (prx + 4);
			ph[0] = 0;


			while(!(reg_rf_irq_status & FLD_RF_IRQ_TX) );  	//wait for TX finish
			reg_rf_irq_status = FLD_RF_IRQ_TX;
			u32 tx_end_tick = clock_time();


			/////////////////////////////////////////////////////////////////////////////////////////
			//here waiting for RX packet, at least 150uS available: 150uS + 48uS(6 byte preamble) + 32uS(4 byte AccessCode) + 32uS(DMA 4 byte)
			//can prepare AUX_SCAN_RSP(first packet) for "Scannable only" event, cause ScanRsp packet may very long,
			//														and 150uS is too quick when AUX_SCAN_REQ is coming
			rf_pkt_aux_scan_rsp_t* p_aux_scan_rsp = (rf_pkt_aux_scan_rsp_t* )(&pkt_auxChainInd_aucScanRsp);
			if(cur_adv_mode == LL_EXTADV_MODE_SCAN){

				if(cur_pextadv->own_addr_type == OWN_ADDRESS_PUBLIC){
					p_aux_scan_rsp->txAddr = BLE_ADDR_PUBLIC;
					smemcpy( (char * )p_aux_scan_rsp->advA, (char * )bltMac.macAddress_public, BLE_ADDR_LEN);
				}
				else if(cur_pextadv->own_addr_type == OWN_ADDRESS_RANDOM){
					p_aux_scan_rsp->txAddr = BLE_ADDR_RANDOM;
					smemcpy( (char * )p_aux_scan_rsp->advA, (char * )p_ext_adv->rand_adr, BLE_ADDR_LEN);
				}


				//max advData length: 255 - 8(Extended Header Length + Extended Header Flag + AdvA_6) = 247
				if(p_ext_adv->curLen_scanRsp > MAX_ADVDATA_NUM_AUX_SCANRSP){  //with AUX_CHAIN_IND, AuxPtr valid
					p_aux_scan_rsp->ext_hdr_len = 10; //Extended Header Flags(1) + AdvA(6) + TargetA(3)
					p_aux_scan_rsp->ext_hdr_flg = EXTHD_BIT_ADVA | EXTHD_BIT_AUX_PTR;


					cur_dataLen = MAX_ADVDATA_NUM_AUX_SCANRSP - 3;  //AuxPtr will use 3 bytes

					//AuxPtr calculate after receive AUX_SCAN_REQ
					#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
						if(p_ext_adv->sec_phy == BLE_PHY_1M){
							packet_us = (255 + 10) * 8    + TLK_T_MAFS + 29;
						}
						else if(p_ext_adv->sec_phy == BLE_PHY_2M){
							packet_us = (255 + 11) * 4    + TLK_T_MAFS + 29;
						}
						else{  //Coded PHY
							packet_us = (255*8 + 43)*LE_CODED_S8 + 376    + TLK_T_MAFS + 29;//when p_ext_adv->coding_ind set LE_CODED_S2, master may send s8 PDU AUX_SCAN_REQ
						}
					#else
						packet_us = 265 * 8 + TLK_T_MAFS + 29;   //TODO: update for different PHYs,   29: /30, carry 1
					#endif
					packet_30us_unit = packet_us/30;
					packet_us = packet_30us_unit*30;  //to be 30uS * N


					p_aux_scan_rsp->dat[0] = aux_chn_index | EXT_ADV_PDU_AUXPTR_CA_0_50_PPM<<6;  //CA: 1, 0~50 ppm; OffsetUnits: 1, 30 uS
					p_aux_scan_rsp->dat[1] = packet_30us_unit & 0xff;   // aux_offset:13 bit
					p_aux_scan_rsp->dat[2] = (packet_30us_unit >> 8) &0x1F;
					p_aux_scan_rsp->dat[2] |= (p_ext_adv->sec_phy - 1)<<5;   //TODO: process PHYs

					p_aux_scan_rsp->rf_len = 255;
					smemcpy( (char * )(p_aux_scan_rsp->dat + 3), (char * )p_ext_adv->dat_scanRsp, MAX_ADVDATA_NUM_AUX_SCANRSP - 3);
				}
				else{  //no AUX_CHAIN_IND, AuxPtr invalid
					p_aux_scan_rsp->ext_hdr_len = 7;  //Extended Header Flags(1) + AdvA(6)
					p_aux_scan_rsp->ext_hdr_flg = EXTHD_BIT_ADVA;

					cur_dataLen = p_ext_adv->curLen_scanRsp;

					p_aux_scan_rsp->rf_len = 8 + cur_dataLen;  //8 : Extended Header Length(1) + Extended Header Flag(1) + AdvA(6)

					smemcpy( (char * )(p_aux_scan_rsp->dat + 0), (char * )p_ext_adv->dat_scanRsp, cur_dataLen);
				}

				p_ext_adv->send_dataLen = cur_dataLen;   //record sending data length for first packet
				p_aux_scan_rsp->dma_len = rf_tx_packet_dma_len(p_aux_scan_rsp->rf_len + 2);

			}
#if 1		/////////////////////////////////////////////////////////////////////////////////////////
			u32  t_us = 0;//wait packet from master RX timeout margin tick.
			#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
				if(p_ext_adv->sec_phy == BLE_PHY_1M){
					t_us = (10+5) * 8 + 150;
				}
				else if(p_ext_adv->sec_phy == BLE_PHY_2M){
					t_us = (10+6) * 4 + 150;
				}
				else if (p_ext_adv->sec_phy  == BLE_PHY_CODED){
					t_us = 376 + 10 * 8 * LE_CODED_S8 + 150;//
//					t_us = 376 + 10 * 8 * p_ext_adv->coding_ind + 150;
				}
			#else
				t_us = (10+5) * 8 + 150;
			#endif

			while (!(*ph) && (u32)(clock_time() - tx_end_tick) < t_us * SYSTEM_TIMER_TICK_1US);	//wait packet from master

#else
			u32  t_us;
			if(bltPHYs.cur_llPhy == BLE_PHY_CODED)
			{
				t_us = 5000; //todo calculate the time of access code
			}
			else
			{
				t_us = (p_aux_adv_ind_1->rf_len + 10) * 8 + 400; //TODO: process different PHYs
			}
			while (!(*ph) && (u32)(clock_time() - tx_begin_tick) < t_us * SYSTEM_TIMER_TICK_1US);	//wait packet from master
#endif



			if (*ph){

				rf_pkt_ext_scan_req_t * pReq = (rf_pkt_ext_scan_req_t *) (prx + 0);
				u16 *advA16 = (u16 *)p_aux_adv_ind_1->advA;	    	//local device's Address
				u16 *peerSearchA16 = (u16 *)pReq->advA;  			//advA in "AUX_SCAN_REQ" and "AUX_CONNECT_REQ"

				u16 *targetA16 = (u16 *)p_aux_adv_ind_2->targetA;
				u16 *peerA16 = (u16 *)pReq->scanA;					//scanA in "AUX_SCAN_REQ" or initA in "AUX_CONNECT_REQ"
#if 1
				u32 t_rx_timeout_us = 0;
				#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
					if(p_ext_adv->sec_phy == BLE_PHY_1M){
						t_rx_timeout_us = ((u32)pReq->rf_len + 10) * 8 + 150+30; //30: margin
					}
					else if(p_ext_adv->sec_phy == BLE_PHY_2M){
						t_rx_timeout_us = ((u32)pReq->rf_len + 11) * 4 + 150+30; //30: margin
					}
					else if (p_ext_adv->sec_phy  == BLE_PHY_CODED){
						t_rx_timeout_us = 376 + ((u32)pReq->rf_len * 8 + 43) * LE_CODED_S8 + 150+30; //30: margin
//						t_rx_timeout_us = 376 + ((u32)pReq->rf_len * 8 + 43) * p_ext_adv->coding_ind + 150+30; //30: margin
					}
				#else
					t_rx_timeout_us = (pReq->rf_len + 10) * 8 + 150+30; //30: margin
				#endif
				while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) && (u32)(clock_time() - tx_end_tick) < t_rx_timeout_us * SYSTEM_TIMER_TICK_1US);
#else
				while (!(reg_rf_irq_status & FLD_RF_IRQ_RX) );  //TODO: 400 update for different PHYs  && (clock_time() - rx_begin_tick) < 400 * SYSTEM_TIMER_TICK_1US
#endif

				//trigger STX as quick as we can, to ensure 150 uS timing
				//there are some time to prepare data:  TX settle time(74uS) + 5 byte preamble time(40us for 1M PHY)
				reg_rf_ll_cmd_schedule = tx_begin_tick = clock_time() + bltExtA.T_AUX_RSP_INTVL;
				reg_rf_ll_cmd = FLD_RF_R_CMD_TRIG | FLD_RF_R_STX; // 0x85--single TX


				reg_rf_irq_status = FLD_RF_IRQ_RX;  //clear RX status


				if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(prx) && RF_BLE_RF_PACKET_CRC_OK(prx) )
				{

					if (MAC_MATCH16(peerSearchA16, advA16))
					{

						/*****************************************************************************************
															   AUX_SCAN_REQ	 AUX_CONNNECT_REQ

						Connectable	Undirected	AUX_ADV_IND	: 		NO				YES
						Connectable	  Directed	AUX_ADV_IND	: 		NO			    YES_2
						Scannable 	Undirected	AUX_ADV_IND	: 		YES				NO
						Scannable 	  Directed	AUX_ADV_IND : 	    YES_3			NO

						YES_2 :		Initiators other than the correctly addressed initiator shall not respond.
						YES_3 :		Scanners other than the correctly addressed scanner shall not respond.
						 ****************************************************************************************/
						if (pReq->type == LL_TYPE_AUX_SCAN_REQ && cur_adv_mode == LL_EXTADV_MODE_SCAN)
						{
						#if (MCU_CORE_TYPE == MCU_CORE_9518)
							pkt_auxChainInd_aucScanRsp.dma_len = rf_tx_packet_dma_len(pkt_auxChainInd_aucScanRsp.rf_len + 2);//4 bytes align
						#endif
							rf_set_dma_tx_addr((unsigned int)(&pkt_auxChainInd_aucScanRsp));//Todo:need check by sunwei


							if( ( directed_adv && MAC_MATCH16(targetA16, peerA16))  || \
								(!directed_adv && (!(p_ext_adv->adv_filterPolicy & ALLOW_SCAN_WL) ||  ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6))))
							{

//								blt_p_event_callback (BLT_EV_FLAG_SCAN_RSP, NULL, 0);

								while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish
								reg_rf_irq_status = FLD_RF_IRQ_TX;


								//sending all scan_rsp data by AUX_CHAIN_IND
								aux_chn_backup = aux_chn_index;
								while(p_ext_adv->send_dataLen < p_ext_adv->curLen_scanRsp)
								{
									aux_chn_index = BLT_GENERATE_AUX_CHN;

									if(p_ext_adv->curLen_scanRsp - p_ext_adv->send_dataLen > MAX_ADVDATA_NUM_AUX_SCANRSP){
										cur_dataLen = MAX_ADVDATA_NUM_AUX_SCANRSP - 3;
										offset = 3;

										//AuxPtr: channel index(6) + CA(1) + OffsetUnits(1) + AuxOffset(13) + AuxPHY(3)
										p_aux_scan_rsp->dat[0] = aux_chn_index | BIT(6);  //CA: 1, 0~50 ppm; OffsetUnits: 1, 30 uS
										p_aux_scan_rsp->dat[1] = packet_30us_unit & 0xff;   // aux_offset:13 bit
										p_aux_scan_rsp->dat[2] = (packet_30us_unit >> 8) &0x1F;
										p_aux_scan_rsp->dat[2] |= (p_ext_adv->sec_phy - 1)<<5;   //TODO: process PHYs

										p_aux_scan_rsp->rf_len = 255;
									}
									else{
										//no AUX_CHAIN_IND, AuxPtr invalid
										p_aux_scan_rsp->ext_hdr_len = 7;  //Extended Header Flags(1) + AdvA(6)
										p_aux_scan_rsp->ext_hdr_flg = EXTHD_BIT_ADVA;

										cur_dataLen = p_ext_adv->curLen_scanRsp - p_ext_adv->send_dataLen;
										offset = 0;

										p_aux_scan_rsp->rf_len = 8 + cur_dataLen;
									}
									p_aux_scan_rsp->dma_len = rf_tx_packet_dma_len(p_aux_scan_rsp->rf_len + 2);

									smemcpy( (char * )(p_aux_scan_rsp->dat + offset), (char * )(p_ext_adv->dat_scanRsp + p_ext_adv->send_dataLen) , cur_dataLen);

									p_ext_adv->send_dataLen += cur_dataLen;  //update

									tx_begin_tick += packet_us*SYSTEM_TIMER_TICK_1US; //calculate TX tick
									pkt_auxChainInd_aucScanRsp.dma_len = rf_tx_packet_dma_len(pkt_auxChainInd_aucScanRsp.rf_len + 2);//4 bytes align
								
									STOP_RF_STATE_MACHINE;
									rf_set_ble_channel (aux_chn_backup);
									aux_chn_backup = aux_chn_index;
									reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

									tick_wait = tx_begin_tick - FSM_TRIGGER_EARLY_WAIT_TICK;

									while((u32)(clock_time() - tick_wait) > BIT(30));

									rf_start_fsm(FSM_STX,(void *)(&pkt_auxChainInd_aucScanRsp), tx_begin_tick);

									while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish
									reg_rf_irq_status = FLD_RF_IRQ_TX;
								}
								p_ext_adv->send_dataLen = 0; //clear

							}

						}
						else if (pReq->type == LL_TYPE_AUX_CONNNECT_REQ && cur_adv_mode == LL_EXTADV_MODE_CONN)
						{
							blc_rcvd_connReq_tick = clock_time();
						
							pkt_aux_conn_rsp.dma_len = rf_tx_packet_dma_len(pkt_aux_conn_rsp.rf_len + 2);//4 bytes align
							//get ready TX packet data
							rf_set_dma_tx_addr((unsigned int)(&pkt_aux_conn_rsp));//Todo:need check by sunwei

							pkt_aux_conn_rsp.txAddr = p_aux_adv_ind_1->txAddr;   //txAddr same ad AUX_ADV_IND
							pkt_aux_conn_rsp.rxAddr = pReq->txAddr;				 //rxAddr copy from txAddr in "AUX_CONNECT_REQ"
							smemcpy( (char * )pkt_aux_conn_rsp.advA, 	(char * )p_aux_adv_ind_1->advA, BLE_ADDR_LEN);
							smemcpy( (char * )pkt_aux_conn_rsp.targetA, (char * )pReq->scanA, BLE_ADDR_LEN);


							// Timing executing codes below must ensure that AUX_CONNECT_RSP sending successfully
							//1M PHY no problem, but when Coded PHY, we should consider the timg,  TODO
							if( ( directed_adv && MAC_MATCH16(targetA16, peerA16))  || \
								(!directed_adv && (!(p_ext_adv->adv_filterPolicy & ALLOW_CONN_WL) ||  ll_searchAddr_in_WhiteList_and_ResolvingList( pReq->txAddr, prx+6))))
							{
								blt_ll_procAuxConnectReq(prx);

							}
							while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish

							STOP_RF_STATE_MACHINE;
						}
					}
				}

				STOP_RF_STATE_MACHINE;  //ensure that FSM stopped

			}


		}
		else     // Non_Connectable Non_Scannable
		{
			p_ext_adv->secondary_adv->dma_len = rf_tx_packet_dma_len(p_ext_adv->secondary_adv->rf_len+2);
			//AUX_ADV_IND		Undirected : rf_len = 10,  Extended_Header_Length(1) + Extended_Header_Flags(1) + AdvA(6) + ADI(2)
			//			   			max rest data 255-10=245,    if add AuxPtr, max 242
			//					Directed :   rf_len = 16,  Extended_Header_Length(1) + Extended Header Flags(1) + AdvA(6) + TargetA(6) + ADI(2)
			//			   			max rest data 255-16=239,   if add AuxPtr, max 236
			STOP_RF_STATE_MACHINE;
			rf_set_ble_channel (aux_chn_index);
			reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

			tick_wait = tx_begin_tick - FSM_TRIGGER_EARLY_WAIT_TICK;
			while((u32)(clock_time() - tick_wait) > BIT(30));

			rf_start_fsm(FSM_STX,(void *)p_ext_adv->secondary_adv, tx_begin_tick);


			///////////// Wait for TX done, combine  AUX_CHAIN_IND  here //////////////////////////////
			if(p_ext_adv->with_aux_chain_ind){
				//NOTE: this value can calculate previously, define in code
				#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
					if(p_ext_adv->sec_phy == BLE_PHY_1M){
						packet_us = (255 + 10) * 8    + TLK_T_MAFS + 29;
					}
					else if(p_ext_adv->sec_phy == BLE_PHY_2M){
						packet_us = (255 + 11) * 4    + TLK_T_MAFS + 29;
					}
					else{  //Coded PHY
						packet_us = (255*8 + 43)*p_ext_adv->coding_ind + 376    + TLK_T_MAFS + 29;
					}
				#else
					packet_us = 265 * 8 + TLK_T_MAFS + 29;   //TODO: update for different PHYs,   29: /30, carry 1
				#endif
				packet_30us_unit = packet_us/30;
				packet_us = packet_30us_unit*30;  //to be 30uS * N

				aux_chn_index =  BLT_GENERATE_AUX_CHN;

				//AuxPtr: channel index(6) + CA(1) + OffsetUnits(1) + AuxOffset(13) + AuxPHY(3)
				if(directed_adv){
					rf_pkt_aux_adv_ind_4* p_aux_adv_ind_4 = (rf_pkt_aux_adv_ind_4* )p_ext_adv->secondary_adv;
					p_aux_adv_ind_4->chn_index = aux_chn_index;
					p_aux_adv_ind_4->aux_offset = packet_30us_unit;
				}
				else{
					rf_pkt_aux_adv_ind_3* p_aux_adv_ind_3 = (rf_pkt_aux_adv_ind_3* )p_ext_adv->secondary_adv;
					p_aux_adv_ind_3->chn_index = aux_chn_index;
					p_aux_adv_ind_3->aux_offset = packet_30us_unit;
				}
			}

		#if(ADV_DURATION_STALL_EN)
			cpu_stall_WakeUp_By_RF_SystemTick(FLD_IRQ_ZB_RT_EN, FLD_RF_IRQ_TX, 0);
		#else
			///////////////////////////////////////////////////////////////////////////////////////////
			while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish
		#endif



			///AUX_CHAIN_IND				 rf_len = 4,  Extended_Header_Length(1) + Extended_Header_Flags(1) +  ADI(2)
			//						max rest data 255-4=251,    if add AuxPtr, max 248(MAX_ADVDATA_NUM_AUX_CHAIN_IND_1)
			aux_chn_backup = aux_chn_index;
			if(p_ext_adv->with_aux_chain_ind){

				tx_begin_tick += packet_us*SYSTEM_TIMER_TICK_1US;


				rf_pkt_aux_chain_ind_1* p_aux_chain_ind_1 = (rf_pkt_aux_chain_ind_1* )(&pkt_auxChainInd_aucScanRsp);

				p_aux_chain_ind_1->txAddr = 0;
				p_aux_chain_ind_1->sid = p_ext_adv->adv_sid; //ADI information
				p_aux_chain_ind_1->did = p_ext_adv->adv_did;

				while(p_ext_adv->send_dataLen  < p_ext_adv->curLen_advData)
				{

					aux_chn_index =  BLT_GENERATE_AUX_CHN;

					if(p_ext_adv->curLen_advData - p_ext_adv->send_dataLen > MAX_ADVDATA_NUM_AUX_CHAIN_IND_1){
						cur_dataLen = MAX_ADVDATA_NUM_AUX_CHAIN_IND_1 - 3;

						// with AuxPtr
						p_aux_chain_ind_1->ext_hdr_len = 6;  //Extended Header Flags(1) + ADI(2) + AuxPtr(3)
						p_aux_chain_ind_1->ext_hdr_flg = EXTHD_BIT_ADI | EXTHD_BIT_AUX_PTR;

						//AuxPtr: channel index(6) + CA(1) + OffsetUnits(1) + AuxOffset(13) + AuxPHY(3)
						p_aux_chain_ind_1->dat[0] = aux_chn_index | EXT_ADV_PDU_AUXPTR_CA_0_50_PPM<<6;  //CA: 1, 0~50 ppm; OffsetUnits: 1, 30 uS
						p_aux_chain_ind_1->dat[1] = packet_30us_unit & 0xff;   // aux_offset:13 bit
						p_aux_chain_ind_1->dat[2] = (packet_30us_unit >> 8) &0x1F;
						p_aux_chain_ind_1->dat[2] |= (p_ext_adv->sec_phy - 1)<<5;   //TODO: process PHYs

						p_aux_chain_ind_1->rf_len = 255;
						smemcpy( (char * )(p_aux_chain_ind_1->dat + 3), (char * )(p_ext_adv->dat_extAdv + p_ext_adv->send_dataLen), MAX_ADVDATA_NUM_AUX_CHAIN_IND_1 - 3);
					}
					else{
						cur_dataLen = p_ext_adv->curLen_advData - p_ext_adv->send_dataLen;

						// no AuxPtr
						p_aux_chain_ind_1->ext_hdr_len = 3;  //Extended Header Flags(1) + ADI(2)
						p_aux_chain_ind_1->ext_hdr_flg = EXTHD_BIT_ADI;

						p_aux_chain_ind_1->rf_len = 4 + cur_dataLen;
						smemcpy( (char * )(p_aux_chain_ind_1->dat + 0), (char * )(p_ext_adv->dat_extAdv + p_ext_adv->send_dataLen), cur_dataLen);
					}
					p_ext_adv->send_dataLen += cur_dataLen;

					p_aux_chain_ind_1->dma_len = p_aux_chain_ind_1->rf_len + 2;
					pkt_auxChainInd_aucScanRsp.dma_len = rf_tx_packet_dma_len(pkt_auxChainInd_aucScanRsp.rf_len + 2);//4 bytes align
					
					STOP_RF_STATE_MACHINE;
					rf_set_ble_channel (aux_chn_backup);
					aux_chn_backup = aux_chn_index;
					reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

					tick_wait = tx_begin_tick - FSM_TRIGGER_EARLY_WAIT_TICK;


					while((u32)(clock_time() - tick_wait) > BIT(30));


					rf_start_fsm(FSM_STX,(void *)(&pkt_auxChainInd_aucScanRsp), tx_begin_tick);
			#if(ADV_DURATION_STALL_EN)
				cpu_stall_WakeUp_By_RF_SystemTick(FLD_IRQ_ZB_RT_EN, FLD_RF_IRQ_TX, 0);
			#else

					while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));  //wait for TX finish
			#endif


					tx_begin_tick += packet_us*SYSTEM_TIMER_TICK_1US;

				}

				p_ext_adv->send_dataLen = p_ext_adv->send_dataLenBackup; // restore
			}
		}
	}
	else{  //no auxiliary packet: Non_Connectable Non_Scannable undirected/directed
		blt_send_extend_no_aux_adv();  //no timing requirement, so no need in Ram_Code
	}


	return 0;
}



#if 0   // just for debug: test 1M/2M/Coded PHY timing
		// Extended, None_Connectable_None_Scannable undirected, without auxiliary packet
_attribute_ram_code_ void blt_send_extend_no_aux_adv(void)
{

//	rf_ble_switch_phy(BLE_PHY_CODED, LE_CODED_S8);  //debug
//	rf_trigle_codedPhy_accesscode();

	rf_set_ble_channel (37);

	for (int i=0; i<3; i++)
	{
		STOP_RF_STATE_MACHINE;						// stop SM

		reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

		rf_start_fsm(FSM_STX,(void *)p_ext_adv->primary_adv, clock_time());
		while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));
	}
}

#else
void blt_send_extend_no_aux_adv(void)
{
	p_ext_adv->primary_adv->dma_len = rf_tx_packet_dma_len(p_ext_adv->primary_adv->rf_len + 2);//4 bytes align

	for (int i=0; i<3; i++)
	{
		if (p_ext_adv->adv_chn_mask & BIT(i))
		{
			STOP_RF_STATE_MACHINE;						// stop SM
			rf_set_ble_channel (blc_adv_channel[i]);

			reg_rf_irq_status = FLD_RF_IRQ_TX | FLD_RF_IRQ_RX;

			//u32 t_us = (p_ext_adv->primary_adv->rf_len + 10) * 8 + 400;  //timing come form old SDK, need change for different PHY
			//u32 tx_begin_tick;

			rf_start_fsm(FSM_STX,(void *)p_ext_adv->primary_adv, clock_time() + 100);
			//tx_begin_tick = clock_time ();
			//while (!(reg_rf_irq_status & FLD_RF_IRQ_TX) && (clock_time() - tx_begin_tick) < (t_us - 200)*SYSTEM_TIMER_TICK_1US);
		#if(ADV_DURATION_STALL_EN)
				cpu_stall_WakeUp_By_RF_SystemTick(FLD_IRQ_ZB_RT_EN, FLD_RF_IRQ_TX, 0);
		#else
			while (!(reg_rf_irq_status & FLD_RF_IRQ_TX));
		#endif
		}
	}
}
#endif



void blt_ll_procAuxConnectReq(u8 * prx)
{

	//add the dispatch when the connect packets is meaning less
	//interval: 7.5ms - 4s -> 6 - 3200
	// 0<= latency <= (Timeout/interval) - 1, timeout*10/(interval*1.25)= timeout*8/inetrval
	// 0 <= winOffset <= interval
	// 1.25 <= winSize <= min(10ms, interval - 1.25)    10ms/1.25ms = 8
	//Timeout: 100ms-32s   -> 10- 3200     timeout >= (latency+1)*interval*2
	rf_packet_connect_t *connReq_ptr = (rf_packet_connect_t *)(prx + 0);
	if(    connReq_ptr->interval < 6 || connReq_ptr->interval > 3200 	\
		|| connReq_ptr->winSize < 1  || connReq_ptr->winSize > 8 || connReq_ptr->winSize>=connReq_ptr->interval	\
		|| connReq_ptr->timeout < 10 || connReq_ptr->timeout > 3200  	\
	/*	|| connReq_ptr->winOffset > connReq_ptr->interval			 	\ */
		|| connReq_ptr->hop == 0){

		return ;
	}
	if( !connReq_ptr->chm[0]){
		if( !connReq_ptr->chm[1] && !connReq_ptr->chm[2] &&  \
			!connReq_ptr->chm[3] && !connReq_ptr->chm[4]){

			return;
		}
	}
	if(connReq_ptr->latency){
		if(connReq_ptr->latency  >  ((connReq_ptr->timeout<<3)/connReq_ptr->interval)){

			return;
		}
	}




	if(ll_adv2conn_cb){
		ll_adv2conn_cb(prx, TRUE);  //blt_connect
	}
}



int  blt_send_adv2(void)
{


	int advSet_terminate_status = BLE_SUCCESS;

#if 1
	//ADV duration elapse
	if(p_ext_adv->adv_duration_tick && ((unsigned int)(clock_time() - p_ext_adv->adv_begin_tick) > p_ext_adv->adv_duration_tick) ){
		advSet_terminate_status = HCI_ERR_ADVERTISING_TIMEOUT;
		p_ext_adv->adv_duration_tick = 0;
	}
#endif



	if(!advSet_terminate_status)  //p_ext_adv->extAdv_en
	{
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		ble_rf_set_tx_dma(0, 17);  //48/16=3
		reg_dma_tx_rptr = FLD_DMA_RPTR_CLR;
		//Switch dma rx buffer to ADV's dma rx buffer
		ble_rf_set_rx_dma((u8*)adv_rx_buff, 4);//change
		rf_set_rx_maxlen(34);
	#endif
		rf_set_tx_rx_off();//must add
		rf_set_ble_crc_adv ();
		blt_ll_set_ble_access_code_adv ();
		reg_rf_irq_mask = 0;		  //very important: suspend ana_34<0> will reset  core_f1c(by SIHUI)

		#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
			if(p_ext_adv->pri_phy != bltPHYs.cur_llPhy || (p_ext_adv->coding_ind != bltPHYs.cur_CI && bltPHYs.cur_llPhy == BLE_PHY_CODED)){
				if(ll_phy_switch_cb){
					ll_phy_switch_cb(p_ext_adv->pri_phy, p_ext_adv->coding_ind);
				}
			}

			if(bltPHYs.cur_llPhy == BLE_PHY_CODED){
				rf_trigle_codedPhy_accesscode();
				rf_tx_settle_adjust(LL_ADV_TX_STL_CODED);
			}
			else{
				rf_tx_settle_adjust(LL_ADV_TX_SETTLE);
			}
		#else
			rf_tx_settle_adjust(LL_ADV_TX_SETTLE);
		#endif





		if(p_ext_adv->param_update_flag){
			blt_ll_updateAdvPacket();
			p_ext_adv->param_update_flag = 0;
		}


		bltParam.adv_scanReq_connReq = 1;  //mark adv sending


		if(p_ext_adv->evt_props & ADVEVT_PROP_MASK_LEGACY){
			blt_send_legacy_adv();
		}
		else{
			blt_send_extend_adv();
		}


		STOP_RF_STATE_MACHINE;
		CLEAR_ALL_RFIRQ_STATUS;
		bltParam.adv_scanReq_connReq = 0;  //clear ADV sending
	}





	//Max_Extended_Advertising_Events reached
	if(p_ext_adv->max_ext_adv_evt && p_ext_adv->run_ext_adv_evt>=p_ext_adv->max_ext_adv_evt)
	{
		//if connection created, and event number reached at the same time, connection created takes higher priority
		if(bltParam.blt_state != BLS_LINK_STATE_CONN){
			advSet_terminate_status = HCI_ERR_LIMIT_REACHED;
		}
		p_ext_adv->max_ext_adv_evt = 0;
	}
	else
	{
		p_ext_adv->run_ext_adv_evt++;
	}



#if 1  // ADV set terminated event
	if(advSet_terminate_status || bltParam.blt_state == BLS_LINK_STATE_CONN){

		if(advSet_terminate_status){  //not connection created
			blt_ll_enableExtAdv(0);
		}

		if(hci_le_eventMask & HCI_LE_EVT_MASK_EXTENDED_ADVERTISING_SET_TERMINATED){

			u8 result[8];  //6 byte is enough
			hci_le_advSetTerminatedEvt_t *pEvt = (hci_le_advSetTerminatedEvt_t *)result;

			pEvt->subEventCode = HCI_SUB_EVT_LE_ADVERTISING_SET_TERMINATED;
			pEvt->advHandle = p_ext_adv->adv_handle;
			if((p_ext_adv->adv_handle!=0) && (p_ext_adv->adv_handle <= 0xef)){  //connection created
				pEvt->status = BLE_SUCCESS;
				pEvt->connHandle = BLS_CONN_HANDLE;  //The connection Handle is only valid when ADV ends because a connection was created
				pEvt->num_compExtAdvEvt = p_ext_adv->run_ext_adv_evt;;
			}
			else{
				pEvt->status = advSet_terminate_status;
				pEvt->connHandle = 0xFFFF; //invalid
				pEvt->num_compExtAdvEvt = p_ext_adv->run_ext_adv_evt;
			}

			blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 6);
		}

	}
#endif





	return 0;
}










u8 blt_ll_searchExistingAdvSet(u8 advHandle)
{

	for(int i=0;i<bltExtA.maxNum_advSets; i++)  //find existing ADV SET
	{

		if((blt_pextadv+i)->adv_handle == advHandle){  //existing ADV set match
			//update
			bltExtA.last_advHand = advHandle;

			cur_pextadv = blt_pextadv + i;  //update cur_pextadv, point to the new adv_set

			return i;
		}
	}

	return INVALID_ADVHD_FLAG;  //no ADV set available
}



u8 blt_ll_searchAvailableAdvSet(u8 advHandle)
{

	u8 index_advSets = blt_ll_searchExistingAdvSet(advHandle);
	if(index_advSets != INVALID_ADVHD_FLAG){
		return index_advSets;
	}


	for(int i=0;i<bltExtA.maxNum_advSets; i++)  //find available ADV SET
	{

		if((blt_pextadv+i)->adv_handle == INVALID_ADVHD_FLAG){  //if INVALID_ADVHD_FLAG, this ADV set can be allocated to new advHandle
			bltExtA.last_advHand = advHandle;
			bltExtA.useNum_advSets++;

			cur_pextadv = blt_pextadv + i;    //update cur_pextadv, point to the new adv_set
			cur_pextadv->adv_handle = advHandle;

			return i;
		}
	}

	return INVALID_ADVHD_FLAG;  //no ADV set available
}







//ADV random delay: 10ms most in BLE_spec
// 8ms: (8<<14)-1 : 2^17 -1 = 0x1FFFF,  clock_time() & 0x1FFFF, max 0x1FFFF, 131072/16000 = 8.192 mS ~= 8mS
// 4ms: (4<<14)-1 : 2^16 -1 = 0x0FFFF,  clock_time() & 0x0FFFF, max 0x0FFFF, 4 mS
// 2ms: (2<<14)-1 : 2^15 -1 = 0x07FFF,  clock_time() & 0x07FFF, max 0x07FFF, 2 mS
// 1ms: (1<<14)-1 : 2^14 -1 = 0x03FFF,  clock_time() & 0x03FFF, max 0x03FFF, 1 mS
// 0: (clock_time() & 0 ), no delay
void		blc_ll_setMaxAdvDelay_for_AdvEvent(u8 max_delay_ms)    //unit: mS, only 8/4/2/1/0  available
{
	if(max_delay_ms){
		bltExtA.rand_delay = (max_delay_ms<<14) - 1;
	}
	else{
		bltExtA.rand_delay = 0;
	}
}


void		blt_ll_reset_ext_adv(void)
{
	blt_ll_clearAdvSets();   //can not use blc_ll_clearAdvSets, cause "adv_hci_cmd" will be set
}


#endif





