/********************************************************************************************************
 * @file	ll.c
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


_attribute_data_retention_	 u32 LL_FEATURE_MASK_0 = LL_FEATURE_MASK_BASE0;
_attribute_data_retention_	 u32 LL_FEATURE_MASK_1 = LL_FEATURE_MASK_BASE1;

_attribute_data_retention_   ll_mac_t  bltMac;
_attribute_data_retention_   _attribute_aligned_(4) volatile st_ll_para_t  bltParam;
_attribute_data_retention_	 _attribute_aligned_(4) ll_data_extension_t  bltData;
_attribute_data_retention_   _attribute_aligned_(4) bb_sts_t				blt_bb;

_attribute_data_retention_ ll_SetExtAdv_Enable_callback_t    pFunc_ll_SetAdv_Enable;

_attribute_data_retention_	u32		blc_tlkEvent_pending = 0;

_attribute_data_retention_	u8 		blc_tlkEvent_data[16];  //save 24 byte


#if (MCU_CORE_TYPE == MCU_CORE_9518)  //Kite/Vulture not use
	_attribute_data_retention_ ll_fifo_t			blt_txfifo;
	_attribute_data_retention_ ll_fifo_t			blt_rxfifo;
#endif

#if (LL_FEATURE_ENABLE_LL_PRIVACY)
_attribute_data_retention_	ll_rpa_tmo_mainloop_callback_t		ll_rpa_tmo_main_loop_cb = NULL;
#endif




_attribute_data_retention_	l2cap_handler_t 				 	blc_l2cap_handler = NULL;

_attribute_data_retention_  ll_host_mainloop_callback_t    		ll_host_main_loop_cb = NULL;

_attribute_data_retention_	ll_enc_done_callback_t				ll_encryption_done_cb = NULL;

_attribute_data_retention_	ll_enc_pause_callback_t				ll_encryption_pause_cb = NULL;

_attribute_data_retention_	ll_irq_tx_callback_t				ll_irq_tx_cb = NULL;

_attribute_data_retention_	ll_irq_rx_data_callback_t			ll_irq_rx_data_cb = NULL;

ll_irq_rx_post_callback_t			ll_irq_rx_post_cb = NULL;  //slave not use, so no need retention

_attribute_data_retention_	ll_irq_systemTick_conn_callback_t	ll_irq_systemTick_conn_cb = NULL;

_attribute_data_retention_	blc_main_loop_data_callback_t		blc_main_loop_data_cb = NULL;
_attribute_data_retention_	blc_main_loop_post_callback_t		blc_main_loop_post_cb = NULL;

_attribute_data_retention_	blc_main_loop_phyTest_callback_t	blc_main_loop_phyTest_cb = NULL;


ll_module_init_callback_t	   ll_module_init_cb;


extern ll_module_adv_callback_t	   ll_module_adv_cb;
extern ll_module_adv_callback_t	   ll_module_advSlave_cb;

extern   ble_crypt_para_t 	blc_cyrpt_para;

extern 	_attribute_aligned_(4) st_ll_pm_t  bltPm;

_attribute_data_retention_
blt_event_callback_t		blt_event_func[BLT_EV_MAX_NUM] = {0};



_attribute_data_retention_	u32 lmp_tick=0;
_attribute_data_retention_	u32 lmp_timeout =0;

#if( FREERTOS_ENABLE )
_attribute_data_retention_	u8 	x_freertos_on = 0;
#endif

u8 const cmd_length_array[26] = {12, 8, 2, 23, 13,  1,  1, 2, 9, 9, \
								 1,  1, 6,  2,  9, 24, 24, 3, 1, 1,
								 9,  9, 3,  3,  5,  3,  };


#if (LL_FEATURE_ENABLE_LL_PRIVACY)
void blc_ll_registerRpaTmoMainloopCallback (ll_rpa_tmo_mainloop_callback_t cb)
{
	ll_rpa_tmo_main_loop_cb = cb;
}
#endif


void blc_ll_registerHostMainloopCallback (ll_host_mainloop_callback_t cb)
{
	ll_host_main_loop_cb = cb;
}




void blc_ll_registerConnectionEncryptionDoneCallback(ll_enc_done_callback_t  cb)
{
	ll_encryption_done_cb = cb;
}

#if(LL_PAUSE_ENC_FIX_EN)
void blc_ll_registerConnectionEncryptionPauseCallback(ll_enc_pause_callback_t  cb)
{
	ll_encryption_pause_cb = cb;
}
#endif

_attribute_data_retention_	ll_conn_complete_handler_t		ll_connComplete_handler = NULL;
_attribute_data_retention_	ll_conn_terminate_handler_t		ll_connTerminate_handler = NULL;
void blc_ll_registerConnectionCompleteHandler(ll_conn_complete_handler_t  handler)
{
	ll_connComplete_handler = handler;
}

void blc_ll_registerConnectionTerminateHandler(ll_conn_terminate_handler_t  handler)
{
	ll_connTerminate_handler = handler;
}



bool blc_ll_isControllerEventPending(void)
{
	return blc_tlkEvent_pending;
}


_attribute_data_retention_	u32		hci_tlk_module_eventMask = 0;  //default 32 event all enable
ble_sts_t 	bls_hci_mod_setEventMask_cmd(u32 evtMask)
{
	hci_tlk_module_eventMask = evtMask;
	return BLE_SUCCESS;
}

_attribute_ram_code_ void blt_event_callback_func (u8 e, u8 *p, int n)
{

	if(hci_tlk_module_eventMask){   //for module
		if(hci_tlk_module_eventMask & BIT(e)){
			blc_hci_send_event (HCI_FLAG_EVENT_TLK_MODULE | e, p,n);//blc_hci_event_handler
		}
	}
	else{
		if (blt_event_func[e]){
			blt_event_func[e](e, p, n);
		}
	}
}

_attribute_data_retention_ blt_event_callback_t		blt_p_event_callback;


void bls_app_registerEventCallback (u8 e, blt_event_callback_t p)
{
	blt_event_func[e] = p;
}





ble_sts_t  blc_ll_readBDAddr(u8 *addr)
{
	smemcpy(addr, bltMac.macAddress_public, BLE_ADDR_LEN);
	return BLE_SUCCESS;
}

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)

bool 	blc_ll_isValidPublicAddr(const u8* addr)
{
    for(int i = 0; i < BLE_ADDR_LEN; i++){//Can not be all zeros
        if(addr[i]){
            return 1;
        }
    }

    return 0;
}


bool 	blc_ll_isValidRandomAddr(const u8* addr)
{
    /* Make sure all bits are neither one nor zero */
    u16 sum = 0;
    for(int i = 0; i < (BLE_ADDR_LEN -1); i++){
        sum += addr[i];
    }
    sum += addr[5] & 0x3f;

    if((sum == 0) || (sum == ((5*0xFF) + 0x3F))){
        return 0;
    }

    //Get the upper two bits of the address
    u8 rc = 1;
    u8 addr_type = addr[5] & 0xc0;

    if(addr_type == 0x40){ //Resolvable addr type
        sum = addr[3] + addr[4] + (addr[5] & 0x3F);
        if ((sum == 0) || (sum == (0xFF * 2 + 0x3F))) {
            rc = 0;
        }
    }
    else if (addr_type == 0){ //non-resolvable addr type
        // Can not be equal to the public addr
        if (!memcmp(bltMac.macAddress_public, addr, BLE_ADDR_LEN)) {
            rc = 0;
        }
    }
    else if (addr_type == 0xc0){ //Static random address
		//Check nothing
    }
    else { //Invalid adder type
        rc = 0;
    }

    return rc;
}



bool 	blc_ll_isValidOwnAddrByAddrType(u8 ownAddrType, const u8* randomAddr)
{
    int rc;

    switch (ownAddrType){
		case OWN_ADDRESS_PUBLIC:
	#if (LL_FEATURE_ENABLE_LL_PRIVACY)
		case OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC:
	#endif
			rc = blc_ll_isValidPublicAddr(bltMac.macAddress_public);
			break;

		case OWN_ADDRESS_RANDOM:
	#if (LL_FEATURE_ENABLE_LL_PRIVACY)
		case OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM:
	#endif
			rc = blc_ll_isValidRandomAddr(randomAddr);
			break;

		default:
			rc = 0;
			break;
    }

    return rc;
}
#endif  ///#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)

#if (MCU_CORE_TYPE == MCU_CORE_9518)
ble_sts_t blc_ll_setRandomAddr(u8 *randomAddr)
{
	if(bltParam.blt_state == BLS_LINK_STATE_INIT)
	{
		/* TP/CON/INI/BV-01-C Test that the IUT responds with Command Disallowed to an LE Set
		Random Address command when initiating*/
		return HCI_ERR_CMD_DISALLOWED;
	}
	smemcpy(bltMac.macAddress_random, randomAddr, BLE_ADDR_LEN);
	return BLE_SUCCESS;
}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
ble_sts_t blc_ll_setRandomAddr(u8 *randomAddr)
{
	u8 advExtEn = 0;
	#if (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING)
		extern ll_ext_adv_t* cur_pextadv;
		advExtEn = cur_pextadv->extAdv_en;
	#endif

	/*
	 * If the Host issues this command when any of advertising (created using legacy advertising commands), scanning,
	 * or initiating are enabled, the Controller shall return the error code Command Disallowed (0x0C).
	 */
	if(((bltParam.adv_en && !advExtEn) || blts.scan_en || blm_create_connection)){
		return HCI_ERR_CMD_DISALLOWED;
	}

	if(!blc_ll_isValidRandomAddr(randomAddr)){
		return HCI_ERR_INVALID_HCI_CMD_PARAMS;
	}

	memcpy(bltMac.macAddress_random, randomAddr, BLE_ADDR_LEN);

	return BLE_SUCCESS;
}
#endif

u8 	blc_ll_getLatestAvgRSSI(void)
{
	return 	bltParam.ll_recentAvgRSSI;
}









ble_sts_t 	blc_hci_le_readBufferSize_cmd(u8 *pData)
{

#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)

	pData[0] = U16_LO(bltData.supportedMaxTxOctets);
	pData[1] = U16_HI(bltData.supportedMaxTxOctets);

#else
	pData[0] = U16_LO(HCI_MAX_ACL_DATA_LEN);
	pData[1] = U16_HI(HCI_MAX_ACL_DATA_LEN);
#endif

	pData[2] = 4;


	return BLE_SUCCESS;
}




ble_sts_t blc_hci_le_getLocalSupportedFeatures(u8 *features) {

	features[0] = LL_FEATURE_BYTE_0;
	features[1] = LL_FEATURE_BYTE_1;
	features[2] = LL_FEATURE_BYTE_2;
	features[3] = LL_FEATURE_BYTE_3;
	features[4] = LL_FEATURE_BYTE_4;
	features[5] = LL_FEATURE_BYTE_5;
	features[6] = LL_FEATURE_BYTE_6;
	features[7] = LL_FEATURE_BYTE_7;

	return BLE_SUCCESS;
}


ble_sts_t 	blc_hci_writeAuthenticatedPayloadTimeout_cmd(u16 connHandle, u16 to_n_10ms)
{
#if (LE_AUTHENTICATED_PAYLOAD_TIMEOUT_SUPPORT_EN)
	bltParam.to_us_LE_Authenticated_Payload = to_n_10ms * 10000;
#endif
	return BLE_SUCCESS;
}



/*
 * @brief  API to Get p-256 Public Key.
 *
 * */
ble_sts_t blc_ll_getP256publicKeyStart (void)
{
	return BLE_SUCCESS;
}

/*
 * @brief  API to Generate DHKey.
 *
 * */
ble_sts_t blc_ll_generateDHkey (u8* remote_public_key)
{
	return BLE_SUCCESS;
}



/**********************************************************************
 *
 * 	Core4.2  Data Length Extension
 *
 *********************************************************************/




#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
/**
 * @brief      this function is used to exchange data length
 * @param[in]  opcode -  LL_LENGTH_RSP/LL_LENGTH_REQ
 * 			   maxTxOct - Maximum TX packet length
 * @return     status, 0x00:  succeed
 * 					   other: failed
 */
void 	blc_ll_initDataLengthExtension (void)
{
	//TX: dataLen + 10(dmaLen(4) + llid +  rflen  + mic(4))
	//RX: 24 + dataLen
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	bltData.connInitialMaxTxOctets =   MAX_OCTETS_DATA_LEN_27;
	bltData.connMaxTxOctets = 			MAX_OCTETS_DATA_LEN_27;
	bltData.connEffectiveMaxTxOctets = MAX_OCTETS_DATA_LEN_27;

	bltData.connEffectiveMaxRxOctets = MAX_OCTETS_DATA_LEN_27;
	bltData.connRxDiff100 = 0;
	bltData.connTxDiff100 = 0;

	if(blt_txfifo.size <= 40){
		bltData.supportedMaxTxOctets =  MAX_OCTETS_DATA_LEN_27;
	}
	else{
		bltData.supportedMaxTxOctets = blt_txfifo.size - 12;  //actual 10(dmaLen:4  header:2  mic:4)

		if(bltData.supportedMaxTxOctets > MAX_OCTETS_DATA_LEN_EXTENSION){
			bltData.supportedMaxTxOctets = MAX_OCTETS_DATA_LEN_EXTENSION;
		}
	}

	if(blt_rxfifo.size <= 64){
		bltData.supportedMaxRxOctets =  bltData.connMaxRxOctets = MAX_OCTETS_DATA_LEN_27;
	}
	else{
		bltData.supportedMaxRxOctets =  bltData.connMaxRxOctets = blt_rxfifo.size - 24;

		if(bltData.supportedMaxRxOctets > MAX_OCTETS_DATA_LEN_EXTENSION){
			bltData.supportedMaxRxOctets = bltData.connMaxRxOctets = MAX_OCTETS_DATA_LEN_EXTENSION;
		}
	}
#endif
}

u16  blc_ll_setInitTxDataLength (u16 maxTxOct)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	bltData.connInitialMaxTxOctets = min(maxTxOct, bltData.supportedMaxTxOctets);
	bltData.connMaxTxOctets = bltData.connInitialMaxTxOctets;

	if(bltData.connInitialMaxTxOctets > MAX_OCTETS_DATA_LEN_27){
		bltData.connMaxTxRxOctets_req = DATA_LENGTH_REQ_PENDING;
	}

	return bltData.connInitialMaxTxOctets;
#endif
}

//used for test mode
void   blc_hci_telink_setIRXTxDataLength (u8 req_en, u16 maxRxOct, u16 maxTxOct)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	bltData.connMaxRxOctets = maxRxOct;
	bltData.supportedMaxRxOctets = maxRxOct;
	bltData.connMaxTxOctets = 	maxTxOct;
	bltData.supportedMaxTxOctets = maxTxOct;

	if(req_en){
		blt_ll_exchangeDataLength(LL_LENGTH_REQ, maxTxOct);
	}

#endif
}

#endif ///ending of "#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)"



ble_sts_t blt_ll_exchangeDataLength (u8 opcode, u16 maxTxOct)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	bltData.connMaxTxOctets = min(maxTxOct, bltData.supportedMaxTxOctets);

	u8	dat[12] = {0x03, 0x09, opcode};
	dat[3] = bltData.connMaxRxOctets;		//maxRxOctets
	dat[4] = 0;
	u16 t = LL_PACKET_OCTET_TIME (bltData.connMaxRxOctets);
	dat[5] = t;								//maxRxTime
	dat[6] = t >> 8;

	dat[7] = bltData.connMaxTxOctets;		//maxTxOctets
	dat[8] = 0;
	t = LL_PACKET_OCTET_TIME (bltData.connMaxTxOctets);
	dat[9] = t;								//maxTxTime
	dat[10] = t >> 8;

	if(ll_push_tx_fifo_handler (BLS_CONN_HANDLE | HANDLE_STK_FLAG, (u8 *)dat)){  // BLS_CONN_HANDLE here for master is OK
		bltData.connMaxTxRxOctets_req = DATA_LENGTH_REQ_DONE;
	}
	else{
		bltData.connMaxTxRxOctets_req = DATA_LENGTH_REQ_PENDING;
	}
#endif
	return BLE_SUCCESS;
}

ble_sts_t blc_hci_setTxDataLength (u16 connHandle, u16 tx, u16 txtime)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	if(bltData.connMaxTxRxOctets_req == DATA_LENGTH_REQ_DONE || tx == MAX_OCTETS_DATA_LEN_27){

	}
	else{
		blt_ll_exchangeDataLength(LL_LENGTH_REQ, tx);
	}
#endif
	return BLE_SUCCESS;
}


ble_sts_t 	blc_hci_readSuggestedDefaultTxDataLength (u8 *tx, u8 *txtime)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	*tx++ = U16_LO(bltData.connInitialMaxTxOctets);
	*tx = 0;  //max 251*tx

	u16 time = LL_PACKET_OCTET_TIME(bltData.connInitialMaxTxOctets);
	*txtime++ = U16_LO(time);
	*txtime = U16_HI(time);

#endif
	return BLE_SUCCESS;

}

ble_sts_t 	blc_hci_writeSuggestedDefaultTxDataLength (u16 tx, u16 txtime)
{
#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	bltData.connInitialMaxTxOctets = min(tx, bltData.supportedMaxTxOctets);

	if(bltData.connInitialMaxTxOctets > MAX_OCTETS_DATA_LEN_27){
		bltData.connMaxTxRxOctets_req = DATA_LENGTH_REQ_PENDING;
	}
#endif
	return BLE_SUCCESS;
}


/*
 * @brief API to encrypt plaintextData to encrypteTextData
 *
 * */
int blc_ll_encrypted_data(u8*key, u8*plaintextData, u8* encrypteTextData)
{
	/* Core 5.2 Spec | Vol 4, Part E page 1886, 5.2 Section
	 * Unless noted otherwise, all parameter values are sent and received in little-endian
	 * format (i.e. for multi-octet parameters the rightmost (Least Significant Octet) is
	 * transmitted first). */

	/* Sample dara refer to Core 5.2 Spec | Vol 6, Part C page 3078
	 HCI_LE_Encrypt (length 0x20) C command
		Pars (LSO to MSO) bf 01 fb 9d 4e f3 bc 36 d8 74 f5 39 41 38 68 4c 13 02 f1 e0 df
		ce bd ac 79 68 57 46 35 24 13 02
		Key (16-octet value MSO to LSO): 0x4C68384139F574D836BCF34E9DFB01BF
		Plaintext_Data (16-octet value MSO to LSO): 0x0213243546576879acbdcedfe0f10213
		HCI_Command_Complete (length 0x14) C event
		Pars (LSO to MSO) 02 17 20 00 66 c6 c2 27 8e 3b 8e 05 3e 7e a3 26 52 1b ad 99
		Num_HCI_Command_Packets: 0x02
		Command_Opcode (2-octet value MSO to LSO): 0x2017
		Status: 0x00
		Encrypted_Data (16-octet value MSO to LSO): 0x99ad1b5226a37e3e058e3b8e27c2c666
	 */

	aes_ll_encryption(key, plaintextData, encrypteTextData);

	return BLE_SUCCESS;
}


/*
 * @brief 	API to request the Controller to generate 8 bytes of random data to be sent to the Host.
 *
 * */
int blc_ll_getRandomNumber (u8* randomNumber)
{
	// len = 8
	generateRandomNum(8, randomNumber);

#if(LL_TEST_SEC_SCAN_BV01C)
	//0x70, 0xfb, 0x59
	randomNumber[0] = 0x70;
	randomNumber[1] = 0xfb;
	randomNumber[2] = 0x59;
#else
	randomNumber[2] = (randomNumber[2]&0x3f) | 0x40;
#endif

	return BLE_SUCCESS;
}


void blc_ll_initStandby_module (u8 *public_adr)
{
	blt_p_event_callback = blt_event_callback_func;

	smemcpy(bltMac.macAddress_public, public_adr, BLE_ADDR_LEN);

	#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		blc_ll_initDataLengthExtension(); //TODO: SiHui
	#endif



	#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
		if(flash_type & FLASH_SONOS_ARCH){
			flash_change_rw_func(flash_read_data, blt_ll_write_sonos_flash);
		}
	#endif


	bltParam.conn_role = LL_ROLE_NONE;
}








/**************************************************
 * @param: num.Set RX more data supported max nums, avoid rx fifo overflow
 * @Notice:HW supported max more data numbers is 7.
 */
void blc_ll_init_max_md_nums(u8 num)
{
	bltParam.md_max_nums = num;
}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
#define	RXDMA_DATA_ADDR			(0x140800 + 0x80)	//0x140880
#define	RF_RX_DECODING_PKT		((read_reg8(0x140840)>>1) & 3)
#endif

_attribute_ram_code_ void irq_blc_ll_rx(void)
{

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	//if RF_MANUAL_RX is decoding the packet, do not switch the Rx_DMA address
	if(RF_RX_DECODING_PKT){
		if(bltParam.blc_state == BLE_STATE_SCAN || bltParam.blc_state == BLE_STATE_INIT || bltParam.blc_state == BLE_STATE_SCAN_IN_SLAVE || bltParam.blc_state == BLE_STATE_SCAN_IN_ADV){
			reg_rf_irq_status = FLD_RF_IRQ_RX;
			return;
		}
	}
#endif


#if (MCU_CORE_TYPE == MCU_CORE_9518)
	u8 * raw_pkt = (u8 *) (blt_rxfifo.p_base + (blt_rxfifo.wptr++ & blt_rxfifo.mask) * blt_rxfifo.size);
	u8 * new_pkt = (blt_rxfifo.p_base + (blt_rxfifo.wptr & blt_rxfifo.mask) * blt_rxfifo.size);

	//TODO: rf_rx_dma_config cannot be switched while RF is in RX
	bltParam.acl_rx_dma_buff = (u32)new_pkt; //Update the next acl dma rx buffer
	ble_rf_set_rx_dma((u8*)bltParam.acl_rx_dma_buff, bltParam.acl_rx_dma_size);
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	u8 * raw_pkt = (u8 *) (blt_rxfifo_b + (blt_rxfifo.wptr++ & (blt_rxfifo.num-1)) * blt_rxfifo.size);
	u8 * new_pkt = (blt_rxfifo_b + (blt_rxfifo.wptr & (blt_rxfifo.num-1)) * blt_rxfifo.size);
	reg_dma_rf_rx_addr = (u16)(u32)new_pkt;
#endif


	reg_rf_irq_status = FLD_RF_IRQ_RX;


	u32 tick_now = clock_time();
	u8	next_buffer = 0;
	raw_pkt[2] = 0;


	if(bltParam.ble_state == BLE_STATE_BRX_S){
		//update duration, extend RX window when received a packet in case of more data, regardless of CRC correct or wrong
	    //if RX window too big, use FSM timeout to make sure that it can not exceed next BRX point

		int time_add_us = 0;
		#if (LL_FEATURE_ENABLE_LE_CODED_PHY)
			if(blt_conn_phy.conn_cur_phy == BLE_PHY_CODED){  //timing for Coded PHY S8, do not care about S2
				//only support rf_len max to 27 bytes(DLE not support)
				//150 + 2448(TX, rf_len 27 max) + 150 +  2448(RX, rf_len 27 max) =  5196, add some margin -> 5300 uS
				time_add_us = 3200;   //5300 - 2100 = 3200
			}
			// 150uS + TX_pkt_time(100byte: 880us) + 150uS + RX_pkt_time(100byte:880us) = 2060uS, add some margin, 2100uS
			else if(bltData.connTxDiff100 && reg_dma_tx_wptr != reg_dma_tx_rptr) //data packet is in TX FIFO, maybe long packet will send
		#else
			if(bltData.connTxDiff100 && reg_dma_tx_wptr != reg_dma_tx_rptr)     //data packet is in TX FIFO, maybe long packet will send
		#endif
			{
				time_add_us = bltData.connTxDiff100*8;
			}

		//todo coded PHY
		// 8278 Need ensure reg system tick irq BIT<0:2> is 0;
		systimer_set_irq_capture( tick_now + (2100 + time_add_us)*SYSTEM_TIMER_TICK_1US);

	#if	(BQB_5P0_TEST_ENABLE)
		extern st_ll_conn_slave_t		bltc;
		if(bltc.conn_establish_pending_flag)
			bltc.conn_establish_pending_flag = 0;
	#endif

	}


	if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )
	{
		DBG_CHN3_TOGGLE;

		log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_rxCrc);

		if(bltParam.blt_state == BLS_LINK_STATE_INIT)
		{
			if(ll_module_init_cb && 1)  // init state, can not calculate CRC, cost too much time leading to connReq timing ERR
			{
				ll_module_init_cb(raw_pkt, tick_now);  //blc_ll_procInitPkt
			}
		}
		#if BLT_SCAN_EN
			else if (bltParam.blt_state & (BLS_LINK_STATE_ADV | BLS_LINK_STATE_SCAN))
			{
				if(1) //scan state, can not calculate CRC, cost too much time leading to Master's ScanReq/ConnReq timing ERR
				{
					if(blc_ll_procScanPktCb)
					{
						next_buffer = blc_ll_procScanPktCb(raw_pkt, new_pkt, tick_now);  // blc_ll_procScanPkt
					}
				}
			}
		#endif
		//------------------- BRX or BTX --------------------------------------
		else if (bltParam.blt_state == BLS_LINK_STATE_CONN)
		{

			if( (bltParam.ble_state == BLE_STATE_BRX_S || bltParam.ble_state == BLE_STATE_BTX_S) && ll_irq_rx_data_cb){

				//if rx md bigger then or usr rx fifo is not enough, We can assume that the CRC24 of the packet is incorrect.
				if( (bltParam.md_max_nums && blttcon.conn_rx_num >= bltParam.md_max_nums) || ((u8)(blt_rxfifo.wptr - blt_rxfifo.rptr)&63) >= blt_rxfifo.num){
					bltParam.drop_rx_data = 1;
					blc_tlkEvent_pending |= EVENT_MASK_RX_DATA_ABANDOM;
				}

				//TODO:if RF_len > 147byte, software crc24 check time will bigger then (150+80)us, stop RF FSM will not effect ACK!!!
				#if (FIX_HW_CRC24_EN) //sys clk: 16M CRC24 software check time -> T(us) = 0.885*len + 34; sys clk: 32M -> T(us) = 0.44*len + 17
					if(raw_pkt[DMA_RFRX_OFFSET_RFLEN] > 140){
						// 16M or 24M sys clk: crc24 check len > 140(margin), skip crc24 software check
					}
					else if(!bltParam.drop_rx_data){
						u16  crc24_payload_off = DMA_RFRX_OFFSET_CRC24(raw_pkt); //notice: crc24_payload_off maye bigger then 255!!
						extern u32 blt_packet_crc24_opt();//remove warning
						u32  crc24_check_val = blt_packet_crc24_opt(raw_pkt+DMA_RFRX_OFFSET_HEADER, raw_pkt[DMA_RFRX_OFFSET_RFLEN]+2, revert_conn_crc, Crc24Lookup);
						bltParam.drop_rx_data = !CRC_MATCH8(((u8*)&crc24_check_val),(raw_pkt+crc24_payload_off));  //CRC ERR -> drop rx data
					}
				#endif


				next_buffer = ll_irq_rx_data_cb(raw_pkt, tick_now); //irq_blc_slave_rx_data or irq_blc_master_rx_data
			}
			else if(blc_ll_procScanPktCb && bltParam.ble_state == BLE_STATE_BRX_E){
				next_buffer = blc_ll_procScanPktCb(raw_pkt, new_pkt, tick_now);  // blc_ll_procScanPkt
			}
		}

		bltParam.drop_rx_data = 0;
		
		blc_ll_recordRSSI(raw_pkt[DMA_RFRX_OFFSET_RSSI(raw_pkt)]);
	}



	blttcon.conn_rx_num ++;


	if(ll_irq_rx_post_cb){
		ll_irq_rx_post_cb();	//irq_blc_master_rx_post
	}


	if (!next_buffer)			//reuse buffer
	{
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			blt_rxfifo.wptr--;
			bltParam.acl_rx_dma_buff = (u32)raw_pkt; //Reuse the last dma rx buffer
			ble_rf_set_rx_dma((u8*)bltParam.acl_rx_dma_buff, bltParam.acl_rx_dma_size);
		#else
			blt_rxfifo.wptr--;
			reg_dma_rf_rx_addr = (u16)(u32)raw_pkt;
		#endif
	}



	

	#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		raw_pkt[0] = 1;
	#endif
}



_attribute_ram_code_ void irq_ll_system_timer(void)
{
#if BLT_SCAN_EN
	if(bltParam.blt_state & (BLS_LINK_STATE_SCAN | BLS_LINK_STATE_INIT) )
	{
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		if(bltParam.blt_state & BLS_LINK_STATE_SCAN){
			bltParam.blc_state = BLE_STATE_SCAN;
		}
		else{
			bltParam.blc_state = BLE_STATE_INIT;
		}
	#endif
		blc_ll_switchScanChannel(1, 1);
	}
	else if(bltParam.blt_state == BLS_LINK_STATE_CONN)
	{
		if(ll_irq_systemTick_conn_cb){
			ll_irq_systemTick_conn_cb();     //irq_slave_system_timer    //irq_master_system_timer
		}
	}
#else
	if(bltParam.blt_state == BLS_LINK_STATE_CONN)
	{
		if(ll_irq_systemTick_conn_cb){
			ll_irq_systemTick_conn_cb();     //irq_slave_system_timer    //irq_master_system_timer
		}
	}
#endif
}




_attribute_ram_code_  void irq_blt_sdk_handler(void)
{
	log_task_irq(BLE_IRQ_DBG_EN, SL01_irq, 1);


	//when adv sending in mainloop ,other irq trigger irq_handler, neglect TX/RX irq
	if(bltParam.adv_scanReq_connReq){  // && src_rf
		return;
	}


	u16  src_rf = reg_rf_irq_status;



	if(src_rf & FLD_RF_IRQ_RX)
	{

		log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_rx);
		DBG_CHN2_TOGGLE;
		irq_blc_ll_rx ();

		if( blc_rf_pa_cb && (bltParam.ble_state == BLE_STATE_BRX_S || bltParam.ble_state == BLE_STATE_BTX_S) ){
			blc_rf_pa_cb(PA_TYPE_TX_ON);
		}
	}



	if(src_rf & FLD_RF_IRQ_TX)
	{
		reg_rf_irq_status = FLD_RF_IRQ_TX;


		if( blc_rf_pa_cb && (bltParam.ble_state == BLE_STATE_BRX_S || bltParam.ble_state == BLE_STATE_BTX_S) ){
			blc_rf_pa_cb(PA_TYPE_RX_ON);
		}

		if(ll_irq_tx_cb && bltParam.tx_irq_proc_en){  //tx_irq_proc_en: to save run time in slave
			ll_irq_tx_cb();	// irq_blc_slave_tx or irq_blm_tx
		}
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		if(bltParam.blt_state == BLS_LINK_STATE_CONN){
			int sw_fifo_n = (blt_txfifo.wptr - blt_txfifo.rptr) & 31;
			if(sw_fifo_n){
				int hw_fifo_n = (reg_dma_tx_wptr - reg_dma_tx_rptr) & 31;
				if (hw_fifo_n < 3 && hw_fifo_n > 0){
					reg_dma_tx_wptr += sw_fifo_n;
					blt_txfifo.rptr += sw_fifo_n;
					blt_save_dma_tx_ptr();
					log_b16_irq (TX_FIFO_DBG_EN, SL16_tf_hw_TX, reg_dma_tx_wptr<<8 | reg_dma_tx_rptr);
					log_b16_irq (TX_FIFO_DBG_EN, SL16_tf_sw_TX, blt_txfifo.wptr<<8 | blt_txfifo.rptr);
				}
			}
		}

		log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_tx);
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		if(bltParam.blt_state == BLS_LINK_STATE_CONN){
			int n = (reg_dma_tx_wptr - reg_dma_tx_rptr) & 15;
			if (n > 0){
				while (blt_txfifo.rptr != blt_txfifo.wptr && n < 7){
					reg_dma_tx_fifo = (u16)(u32)(blt_txfifo_b + (blt_txfifo.rptr++ & (blt_txfifo.num-1)) * blt_txfifo.size);
					n++;
				}
			}
		}
	#endif
	}




	if (src_rf & FLG_RF_CONN_DONE)
	{
		if (src_rf & FLD_RF_IRQ_CMD_DONE){
			log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_cmddone);

		}
		else if (src_rf & FLD_RF_IRQ_RX_TIMEOUT){
			log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_rxTmt);

		}
		else if(src_rf & FLD_RF_IRQ_FIRST_TIMEOUT){
			log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_rxFirstTmt);
		}
		else if (src_rf & FLD_RF_IRQ_FSM_TIMEOUT){
			log_tick_irq(BLE_IRQ_DBG_EN, SLEV_irq_fsmTmt);
		}


		reg_rf_irq_status = FLG_RF_CONN_DONE;

		if (bltParam.ble_state == BLE_STATE_BRX_S)   //slave brx
		{
			systimer_set_irq_capture( clock_time () + 200);	//trigger system timer IRQ in 200 tick
		}
	#if (BLT_CONN_MASTER_EN)
		else if (bltParam.ble_state == BLE_STATE_BTX_S)  //master btx
		{

			bltParam.blm_btx_busy = 0;
			reg_rf_irq_mask = FLD_RF_IRQ_RX | FLD_RF_IRQ_TX;
			systimer_set_irq_capture( clock_time () + 200 * SYSTEM_TIMER_TICK_1US);	//trigger system timer IRQ in 200 tick
		}
	#endif

	}

	if(systimer_get_irq_status())
	{
		log_task_irq (BLE_IRQ_DBG_EN, SL01_sysTimer, 1);

		systimer_clr_irq_status();

		irq_ll_system_timer();

		log_task_irq (BLE_IRQ_DBG_EN, SL01_sysTimer, 0);
	}



	//need check
#if (MCU_CORE_TYPE == MCU_CORE_827x)
	if ((u32)(systimer_get_irq_capture() - (clock_time() +160)) > BIT(30) && ( bltParam.blt_state & (BLS_LINK_STATE_SCAN|BLS_LINK_STATE_INIT|BLS_LINK_STATE_CONN) ) )
	{
#elif( MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_9518)
	if ((u32)(systimer_get_irq_capture() - (clock_time())) > BIT(30) && ( bltParam.blt_state & (BLS_LINK_STATE_SCAN|BLS_LINK_STATE_INIT|BLS_LINK_STATE_CONN) ) )
	{
#endif
		// 8278 Need ensure reg system tick irq BIT<0:2> is 0;
		systimer_set_irq_capture( clock_time () + 2000);  //TODO: 2000 is too big
	}


	log_task_irq(BLE_IRQ_DBG_EN, SL01_irq, 0);
}




_attribute_ram_code_
int blt_sdk_main_loop(void)
{

	//Notice that phytest must before all other operation, and return
	if(blc_main_loop_phyTest_cb && bltParam.phy_en){
		blc_main_loop_phyTest_cb();
		return 0;
	}

	while (blt_rxfifo.rptr != blt_rxfifo.wptr)
	{
		wd_clear(); //clear watch dog
		
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			u8 *raw_pkt = (u8 *) (blt_rxfifo.p_base + blt_rxfifo.size * (blt_rxfifo.rptr & blt_rxfifo.mask));
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			u8 *raw_pkt = (u8 *) (blt_rxfifo_b + blt_rxfifo.size * (blt_rxfifo.rptr & (blt_rxfifo.num-1)));
		#endif
		
		if ( raw_pkt[2] )
		{
			if(blc_main_loop_data_cb){  //slave: blt_slave_main_loop_data     master: blt_master_main_loop_data
				blc_main_loop_data_cb(raw_pkt);
			}
		}
	#if BLT_SCAN_EN
		else if(raw_pkt[2] == 0)
		{
			if(blc_ll_procScanDatCb){
				blc_ll_procScanDatCb(raw_pkt);  // blc_ll_procScanData
			}
		}
	#endif
		blt_rxfifo.rptr++; //handle rx data overflow in irq to prevent rx_fifo.rptr from re-entering
	}



	// blc_procPendingEvent : slave and master shared event, write in ll
	// bls_procPendingEvent : slave event only ,write in ll_slave
	// blm_procPendingEvent : master event only, write in ll_master
	if(blc_tlkEvent_pending & (EVENT_MASK_RX_DATA_ABANDOM | EVENT_MASK_PHY_UPDATE | EVENT_MASK_CHN_SELECTION_ALGOTITHM | EVENT_MASK_DATA_LEN_UPDATE) ){
		blc_procPendingEvent();
	}


	if(bltParam.blt_state == BLS_LINK_STATE_CONN){
		blt_ll_conn_main_loop_post();  //for both slave and master
	}



	if(blc_main_loop_post_cb){
		blc_main_loop_post_cb();  // blt_slave_main_loop_post		blt_master_main_loop_post
	}

	if(ll_host_main_loop_cb){
		ll_host_main_loop_cb();  // blc_gap_peripheral_mainloop
	}

#if (LL_FEATURE_ENABLE_LL_PRIVACY)
	if(ll_rpa_tmo_main_loop_cb){
		ll_rpa_tmo_main_loop_cb();
	}
#endif



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

	//check if 32k pad clk stable
	if(pm_check_32k_clk_stable && !blt_miscParam.pm_enter_en){
		pm_check_32k_clk_stable();
	}



	extern ll_module_pm_callback_t  ll_module_pm_cb;
#if( FREERTOS_ENABLE )
	if( !x_freertos_on ){
#endif
	if(bltParam.sdk_mainLoop_run_flag && ll_module_pm_cb && blt_miscParam.pm_enter_en && (bltParam.blt_state & (BLS_LINK_STATE_ADV | BLS_LINK_STATE_CONN)) )
	{
		if ( !bltParam.blt_busy && !blc_tlkEvent_pending && !bltPm.conn_no_suspend && (blt_rxfifo.rptr == blt_rxfifo.wptr) && (bltPm.suspend_mask != SUSPEND_DISABLE) )
		{
			systimer_irq_disable();

			if(!bltParam.blt_busy){
				ll_module_pm_cb();  // blt_brx_sleep()
			}

			if(bltParam.blt_state == BLS_LINK_STATE_CONN){
				systimer_irq_enable();
			}
		}

	}
#if( FREERTOS_ENABLE )
	}
#endif


	if(bltParam.blt_state == BLS_LINK_STATE_ADV)
	{

		extern u32 blt_advExpectTime;
		if(	ll_module_adv_cb && (u32)(clock_time() - blt_advExpectTime) < BIT(31) && !blc_tlkEvent_pending  )     //(SYSTEM_TIMER_TICK_1US<<22): 4000000 * SYSTEM_TIMER_TICK_1US
		{

			log_task (BLE_ADV_DBG_EN, SL01_adv, 1);
			DBG_CHN1_TOGGLE;
			#if (MCU_CORE_TYPE == MCU_CORE_9518)
				bltParam.blc_state = BLE_STATE_ADV;
			#endif
			ll_module_adv_cb();  //blt_send_adv() blt_ext_adv_proc()
			log_task (BLE_ADV_DBG_EN, SL01_adv, 0);

			if(bltParam.blt_state == BLS_LINK_STATE_ADV){ // in case connect_req
				if(blttcon.conn_update){
					blttcon.conn_update = 0;
				}
				if(blts.scan_extension_mask & BLS_FLAG_SCAN_IN_ADV_MODE){
					reg_rf_irq_mask |= FLD_RF_IRQ_RX;
					#if (MCU_CORE_TYPE == MCU_CORE_9518)
						bltParam.blc_state = BLE_STATE_SCAN_IN_ADV;
					#endif
					blc_ll_switchScanChannel(0, 0);
				}
				else{
					reg_rf_irq_mask &= ~FLD_RF_IRQ_RX;
				}
			}
			else if(bltParam.blt_state == BLS_LINK_STATE_CONN){
				if( bltParam.adv_extension_mask & BLS_FLAG_ADV_IN_SLAVE_MODE )
				{
					#if (MCU_CORE_TYPE == MCU_CORE_9518)
						bltParam.blc_state = BLE_STATE_ADV_IN_SLAVE;
					#endif
					ll_module_advSlave_cb();
				}
			}

			bltParam.blt_busy = 0;
		}

	}


	mcu_oscillator_crystal_calibration();




	bltParam.sdk_mainLoop_run_flag = 1; //set flag at the end of this function

    return 0;
}




#if (BLC_PM_DEEP_RETENTION_MODE_EN)
_attribute_ram_code_
#endif
void blc_ll_recoverDeepRetention(void)
{

#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
	bltPHYs.cur_llPhy = BLE_PHY_1M;
#endif

#if (BLS_USER_TIMER_WAKEUP_ENABLE)
	if( pm_is_deepPadWakeup() ){
		extern void blc_pm_procGpioPadEarlyWakeup(u16 en, u8 *wakeup_src);
		
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			blc_pm_procGpioPadEarlyWakeup(bltPm.latency_en, (u8 *)&g_pm_status_info.wakeup_src);
			bltParam.blt_busy = 1;			//timer wake up may be check for pad wake up,A1 delete todo
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			blc_pm_procGpioPadEarlyWakeup(bltPm.latency_en, (u8 *)&pmParam.wakeup_src);
		#endif
		bltPm.appWakeup_loop_noLatency = 0;
	}
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	else if(g_pm_status_info.wakeup_src & WAKEUP_STATUS_TIMER)
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	else if(pmParam.wakeup_src & WAKEUP_STATUS_TIMER)
#endif
	{
		if(bltPm.appWakeup_flg){
			bltPm.appWakeup_loop_noLatency = 1;
			bltPm.timer_wakeup = 1;
		}
		else{
			bltPm.appWakeup_loop_noLatency = 0;
			bltParam.blt_busy = 1;
		}
	} 

	bltPm.appWakeup_flg = 0;
#else
	if(	pm_is_deepPadWakeup() ){
		extern void blc_pm_procGpioPadEarlyWakeup(u16 en, u8 *wakeup_src);
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			blc_pm_procGpioPadEarlyWakeup(bltPm.latency_en, (u8 *)&g_pm_status_info.wakeup_src);
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			blc_pm_procGpioPadEarlyWakeup(bltPm.latency_en, (u8 *)&pmParam.wakeup_src);
		#endif
	}
	else{
		bltParam.blt_busy = 1;
	}
#endif



	bltPm.timing_synced = 0;
	bltPm.wakeup_src = 0;
	bltPm.user_latency = 0xffff;


	if (bltParam.blt_state == BLS_LINK_STATE_ADV)
	{
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			blt_bb.blt_dma_tx_rptr = 0;
			blt_bb.blt_dma_tx_wptr = 0;
		#endif
	}
	else if(bltParam.blt_state == BLS_LINK_STATE_CONN) //enter conn state
	{

		//rf baseband set
		reset_sn_nesn();

		reg_rf_irq_mask = 0;
		CLEAR_ALL_RFIRQ_STATUS;
		reg_rf_irq_mask = FLD_RF_IRQ_RX | FLD_RF_IRQ_TX | FLG_RF_CONN_DONE;
		
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			blt_restore_dma_tx_ptr();
		#endif
		
		systimer_clr_irq_status();
		systimer_irq_enable();
		extern u32 blt_next_event_tick;
		systimer_set_irq_capture(blt_next_event_tick);//need check

	}

	bltParam.sdk_mainLoop_run_flag = 0;
	#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		blt_bb.blt_dma_tx_rptr = 0;
		pmParam.is_deepretn_back = 0;
	#elif (MCU_CORE_TYPE == MCU_CORE_9518)
		g_pm_status_info.mcu_status &= ~MCU_STATUS_DEEPRET_BACK;
	#endif
}

// 1 busy
bool blc_ll_isBrxWindowBusy(void)
{
	return (bltParam.ble_state != BLE_STATE_BRX_E);
}

bool blc_ll_isBrxBusy (void)
{
#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
	return (bltParam.btxbrx_status == BTXBRX_BUSY);
#else
	return bltParam.blt_busy;
#endif
}

void  blc_ll_set_CustomedAdvScanAccessCode(u32 accss_code)
{
	bltParam.custom_access_code = accss_code;
}



#if (MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_ram_code_ u8  blc_ll_getTxFifoNumber (void)
{
	u32 r = irq_disable();

	int hw_fifo_num = (reg_dma_tx_wptr - reg_dma_tx_rptr) & 31;
	u8 total_fifo_num = hw_fifo_num  +  ( (blt_txfifo.wptr - blt_txfifo.rptr) & 31 ) ;

	if(hw_fifo_num == 0){
		total_fifo_num += 1; //leave one space for inserting empty packet
	}

	irq_restore(r);

	return  total_fifo_num;
}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
_attribute_ram_code_ u8  blc_ll_getTxFifoNumber (void)
{
	u32 r = irq_disable();

	u8 fifo_num = ((reg_dma_tx_wptr - reg_dma_tx_rptr) & 15 )  +  ( (blt_txfifo.wptr - blt_txfifo.rptr) & 31 ) ;

	irq_restore(r);

	return  fifo_num;
}
#endif

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	_attribute_ram_code_ u8  blt_ll_getRealTxFifoNumber (void)
	{
		u32 r = irq_disable();

		u8 fifo_num = ((reg_dma_tx_wptr - reg_dma_tx_rptr) & 31 )  +  ( (blt_txfifo.wptr - blt_txfifo.rptr) & 31 ) ;

		irq_restore(r);

		return  fifo_num;
	}

#endif



#if (MCU_CORE_TYPE == MCU_CORE_9518)
	_attribute_ram_code_
	void blc_ll_initBasicMCU (void)
	{
		systimer_set_irq_capture(clock_time() + BIT(31));

		systimer_set_irq_mask();
		//systimer_clr_irq_status();
		zb_rt_irq_enable();

		#if(SUPPORT_PFT_ARCH)
			plic_set_priority(IRQ15_ZB_RT, 2);
			plic_set_priority(IRQ1_SYSTIMER,2);
			reg_irq_threshold = 0;
		#endif


		//reg_rf_ll_ctrl_1 = FLD_RF_FSM_TIMEOUT_EN | FLD_RF_RX_TIMEOUT_EN | FLD_RF_CRC_2_EN;

		#if 0 //TODO: SiHui
		reg_rf_irq_mask = FLD_RF_IRQ_RX | FLD_RF_IRQ_TX | BLMS_FLG_RF_CONN_DONE;



		/* enable:  RX_FIRST_TIMEOUT_EN + RX_TIMEOUT_EN + CRC_2_EN
		 * disable: FSM_TIMEOUT_EN */
		reg_rf_ll_ctrl_1 = FLD_RF_RX_FIRST_TIMEOUT_EN | FLD_RF_RX_TIMEOUT_EN | FLD_RF_CRC_2_EN;

		//rf_ble_set_rx_timeout(0x00F9); //default value is f9, no need set
		#endif

		ble_tx_dma_config();

		ble_rx_dma_config();

	}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	_attribute_ram_code_
	void blc_ll_initBasicMCU (void)
	{
		reg_dma_rf_rx_addr = (u16)(u32)(blt_rxfifo_b + (blt_rxfifo.wptr & (blt_rxfifo.num-1)) * blt_rxfifo.size);
		reg_dma_rf_rx_size = (blt_rxfifo.size>>4);   // rf rx buffer enable & size
		reg_dma_rf_rx_mode = FLD_DMA_WR_MEM;

		systimer_set_irq_capture( clock_time() + BIT(31)); //set to a big value, avoid irq happens unnormally
	#if (MCU_CORE_TYPE == MCU_CORE_825x)
		reg_system_tick_mode |= FLD_SYSTEM_TICK_IRQ_EN;
	#elif(MCU_CORE_TYPE == MCU_CORE_827x)
		reg_system_irq_mask |= BIT(2);
	#endif
		reg_irq_mask |= FLD_IRQ_ZB_RT_EN;
	}
#endif









ble_sts_t  		blc_hci_reset(void)
{

	bltParam.adv_hci_cmd  = 0;
	bltParam.scan_hci_cmd = 0;

#if (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING)
	if(bltParam.adv_version == ADV_EXTENDED_MASK){
		blt_ll_reset_ext_adv();
	}
#endif

	if(bltParam.adv_version == ADV_LEGACY_MASK)
	{
		if(bltParam.blt_state == BLS_LINK_STATE_ADV){
			bls_ll_setAdvEnable(BLC_ADV_DISABLE);
		}
	}

	bls_hci_reset();
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	blm_hci_reset();
#endif


	ll_whiteList_reset();


	#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
		bltParam.btxbrx_status = BTXBRX_NONE;
		bltParam.conn_role = LL_ROLE_NONE;
	#endif

	return BLE_SUCCESS;
}





ble_sts_t	blc_hci_receiveHostACLData(u16 connHandle, u8 PB_Flag, u8 BC_Flag, u8 *pData )
{
	u8 len = pData[0];  //core42 dataLen  max 251

	int n = min(len, bltData.connEffectiveMaxTxOctets);
	pData[0] = PB_Flag == HCI_CONTINUING_PACKET ? LLID_DATA_CONTINUE : LLID_DATA_START;         //llid
	pData[1] = n;
	while(!ll_push_tx_fifo_handler (connHandle, pData));

	for (int i=n; i<len; i+=bltData.connEffectiveMaxTxOctets)
	{
		n = len - i > bltData.connEffectiveMaxTxOctets ? bltData.connEffectiveMaxTxOctets : len - i;
		pData[0] = LLID_DATA_CONTINUE; //llid, fregment pkt
		pData[1] = n;
		smemcpy (pData + 2, pData + 2 + i, n);
		while(!ll_push_tx_fifo_handler (connHandle, pData));
	}

	return BLE_SUCCESS;
}



u8	  blc_ll_getCurrentState(void)
{
	return bltParam.blt_state;
}




#if (MCU_CORE_TYPE == MCU_CORE_9518)
ble_sts_t	blc_controller_check_appBufferInitialization(void)
{
	bltParam.controller_stack_param_check = 1;

	if(!bltParam.maxRxOct){
		bltParam.maxRxOct = MAX_OCTETS_DATA_LEN_27;
	}

	if(!bltParam.maxTxOct){
		bltParam.maxTxOct = MAX_OCTETS_DATA_LEN_27;
	}


	if(bltParam.ll_aclRxFifo_set){
		if(blt_rxfifo.num == 0 || blt_rxfifo.size == 0){
			return LL_ACL_RX_BUF_PARAM_INVALID;
		}
		else if(blt_rxfifo.size < bltParam.maxRxOct + 21){
			return LL_ACL_RX_BUF_SIZE_NOT_MEET_MAX_RX_OCT;
		}
	}
	else{ //ACL RX buffer not set
		if(bltParam.acl_master_en || bltParam.acl_slave_en){  //ACL master or slave module init_d but ACL RX buffer not set
			return LL_ACL_RX_BUF_NO_INIT;
		}
	}

	if(bltParam.ll_aclTxFifo_set){
		if(blt_txfifo.num == 0 || blt_txfifo.size == 0){
			return LL_ACL_TX_BUF_PARAM_INVALID;
		}
		else if(blt_txfifo.num * blt_txfifo.size >= 4096 ){
			return LL_ACL_TX_BUF_SIZE_MUL_NUM_EXCEED_4K;
		}
		else if(blt_txfifo.size < bltParam.maxTxOct + 10){
			return LL_ACL_TX_BUF_SIZE_NOT_MEET_MAX_TX_OCT;
		}
	}
	else{
		if(bltParam.acl_master_en || bltParam.acl_slave_en){  //ACL master module init_d but ACL TX buffer not set
			return LL_ACL_TX_BUF_NO_INIT;
		}
	}




	bltData.connInitialMaxTxOctets =   MAX_OCTETS_DATA_LEN_27;
	bltData.connEffectiveMaxTxOctets = MAX_OCTETS_DATA_LEN_27;
	bltData.connEffectiveMaxRxOctets = MAX_OCTETS_DATA_LEN_27;

	bltData.supportedMaxRxOctets =  bltData.connMaxRxOctets = bltParam.maxRxOct;
	bltData.supportedMaxTxOctets =  bltData.connMaxTxOctets = bltParam.maxTxOct;

	bltData.connRxDiff100 = 0;
	bltData.connTxDiff100 = 0;



	return BLE_SUCCESS;
}
#endif

#if(FREERTOS_ENABLE)
int blc_ll_allow_block(){
	if ((!(bltParam.blt_state & BLS_LINK_STATE_CONN)) || (!bltParam.blt_busy && !blc_tlkEvent_pending && (blt_rxfifo.rptr == blt_rxfifo.wptr)) ){
		return 1;
	}
	return 0;
}

extern u32 blt_next_event_tick;
_attribute_ram_code_ u32 blc_ll_cal_connwakeuptick(){

	u32 tick_next_event_tick = bltc.connExpectTime - bltc.conn_tolerance_time;//blt_next_event_tick;//bltc.connExpectTime - bltc.conn_tolerance_time;
	u32 tick_connExpectTime = bltc.connExpectTime;
	u16 sys_latency = bltPm.sys_latency;
	u16 latency_en = bltPm.latency_en;

	extern u32 ble_actual_conn_interval_tick;
	extern u8 conn_new_interval_flag;
	u32 tmpble_actual_conn_interval_tick = ble_actual_conn_interval_tick;
	u8  tmpconn_new_interval_flag		= conn_new_interval_flag;

	u8 long_suspend = bltc.long_suspend;

	int conn_tolerance_time = bltc.conn_tolerance_time;
	u32 conn_duration  = bltc.conn_duration ;

	if( !bltPm.appWakeup_loop_noLatency && (!bltPm.padWakeupCnt)){
		if (bltParam.blt_state == BLS_LINK_STATE_CONN && bltPm.suspend_mask & (SUSPEND_CONN )){
			sys_latency = 0;
			u8 no_latency = !bltc.conn_latency || blt_ll_getRealTxFifoNumber() || bltc.tick_1st_rx==0 || \
					!bltc.conn_rcvd_ack_pkt || bltc.conn_terminate_pending || \
				   (blttcon.conn_update && bltc.master_not_ack_slaveAckUpReq);

			if(!no_latency && blttcon.conn_update){
				s16 rest_interval = blttcon.conn_inst_next - blttcon.conn_inst - 1;
				if(rest_interval > 0){
					if(rest_interval < bltc.conn_latency){
						sys_latency = rest_interval;
					}
				}
				else{
					no_latency = 1;
				}
			}

			if(!no_latency && !sys_latency ){ //
				sys_latency = bltc.conn_latency;
			}

			u16 valid_latency = min(bltPm.user_latency, sys_latency);

			u16 latency_use = valid_latency;


			if ( (latency_use + 1) * bltc.conn_interval > 100000 * SYSTEM_TIMER_TICK_1US){
				latency_en = latency_use + 1;
			}

			if(blt_miscParam.pad32k_en)
			{
				tick_connExpectTime += latency_use * bltc.conn_interval + bltc.conn_interval_adjust * latency_en;
			}
			else
			{
				if(bltc.tick_1st_rx){
					extern u32 ble_first_rx_tick_last;
					extern u32 ble_first_rx_tick_pre;
					if(ble_first_rx_tick_last != ble_first_rx_tick_pre){
						tmpble_actual_conn_interval_tick = 0;
						tmpconn_new_interval_flag = 0;
					}
				}

				tmpble_actual_conn_interval_tick += latency_use * bltc.conn_interval;
				tick_connExpectTime += latency_use * bltc.conn_interval;
			}

			if(blt_miscParam.pad32k_en)
			{
				if(bltc.tick_1st_rx){  //synced
					long_suspend = 0;
				}

				u32 tick_delta = (u32)(tick_connExpectTime - clock_time());

				if(tick_delta < BIT(30)){
					if( tick_delta > BIT(25) ){ //2096 mS
						long_suspend = 3;
					}
					else if( tick_delta > BIT(24) ){ //1048 mS
						long_suspend = 2;
					}
					else if( tick_delta > BIT(23) ){ //16 * 1024 * 512 tick = (1<<23) tick = 524 ms
						long_suspend = 1;
					}
				}

				if(blt_miscParam.pad32k_en){ // if use external 32k crystal
					if(long_suspend){

						#if (TRY_FIX_ERR_BY_ADD_BRX_WAIT)//ll_slave.c: must enable MICRO: TRY_FIX_ERR_BY_ADD_BRX_WAIT
							u32 widen_ticks = (long_suspend - 1) * blc_pm_get_brx_early_time();
						#else
							u32 widen_ticks = (long_suspend - 1) * 500 * SYSTEM_TIMER_TICK_1US; //us
						#endif

						conn_tolerance_time += widen_ticks;
						conn_duration  += (widen_ticks << 1);
					}
				}
				else
				{
					if(long_suspend == 3){
						conn_tolerance_time += 500 * SYSTEM_TIMER_TICK_1US;
						conn_duration  += 1000 * SYSTEM_TIMER_TICK_1US;
					}
					else if(long_suspend == 2){
						conn_tolerance_time += 300 * SYSTEM_TIMER_TICK_1US;
						conn_duration  += 600 * SYSTEM_TIMER_TICK_1US;
					}
					else if(long_suspend == 1){
						conn_tolerance_time += 200 * SYSTEM_TIMER_TICK_1US;
						conn_duration  += 400 * SYSTEM_TIMER_TICK_1US;
					}
				}
//				#if(FREERTOS_ENABLE)
				if( x_freertos_on ){
					conn_tolerance_time += 100*SYSTEM_TIMER_TICK_1US;
					conn_duration	+= 200 * SYSTEM_TIMER_TICK_1US;
				}
//				#endif

				#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
				if(blttcon.conn_chnsel)
				{
					conn_tolerance_time += 20*SYSTEM_TIMER_TICK_1US;
				}
				#endif

				#if (LL_FEATURE_ENABLE_LE_CODED_PHY || LL_FEATURE_ENABLE_LE_2M_PHY)
				if(blt_conn_phy.conn_cur_phy != BLE_PHY_1M)
				{
					conn_tolerance_time += 10*SYSTEM_TIMER_TICK_1US;
				}
				#endif

				conn_tolerance_time += 30*SYSTEM_TIMER_TICK_1US; //actually 12us added from SDK3.3.1 to SDK3.4.0
			}
			else{

			}

			tick_next_event_tick = tick_connExpectTime - conn_tolerance_time;
		}
	}
	return tick_next_event_tick;
}

void blc_ll_set_freertos_en(u8 en)
{
	x_freertos_on = en;
	return;
}
#endif





u8* 	blc_ll_get_macAddrRandom(void)
{
	return bltMac.macAddress_random;
}

u8* 	blc_ll_get_macAddrPublic(void)
{
	return bltMac.macAddress_public;
}




