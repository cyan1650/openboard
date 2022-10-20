/********************************************************************************************************
 * @file	smp_peripheral.c
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
#include "stack/ble/host/ble_host.h"
#include "stack/ble/controller/ble_controller.h"




typedef void (*smp_key_handler_t)(void);


/**********************************************************************
 * LOCAL VARIABLES
 */



_attribute_data_retention_	smp_secSigInfo_t 	smp_sign_info;


/*
 *
 * */

extern int bls_smp_param_saveBondingInfo (smp_param_save_t*);











_attribute_data_retention_	_attribute_aligned_(4) secReq_ctl_t	blc_SecReq_ctrl  = {
		(SecReq_IMM_SEND<<4) | SecReq_IMM_SEND,  // <7:4> reConn   <3:0> newConn
		0,
		0,
};




/*
 * smp Pairing request
 * */
#if 0
smp_paring_req_rsp_t smpPairingReq_const = {
		0x01 ,   //code
		0x03 ,
		0x00,
		{1 },
		0x10,
		{0,1,0,0},
};
#endif





#if (DEBUG_PAIRING_ENCRYPTION)

u8 AA_paring_tk1[16];
u8 AA_smp_const[16];
u8 AA_paringBuf_1[16];
u8 AA_paringBuf_2[16];
u8 AA_paringRandom[16];

u8 AA_paring_tk2[16];
u8 AA_paring_peer_rand[16];
u8 AA_paring_conf[16];

u8 AA_smp_Stk_temp[16];
#endif







/**********************************************************************
 * GLOBAL VARIABLES
 */


extern u32	smp_phase_record; // SMP Phase stage double check

#define 	SMP_PHASE_STAGE1		BIT(24)
#define 	SMP_PHASE_STAGE2		BIT(25)
#define 	SMP_PHASE_STAGE2_ENC 	BIT(26)
#define 	SMP_PHASE_STAGE3		BIT(27)


//Not used now, define for KEY MASK
#define 	KEY_MASK_ENC			BIT(0)
#define 	KEY_MASK_IDENTITY 		BIT(1)
#define 	KEY_MASK_SIGN			BIT(2)


_attribute_data_retention_	u8 smp_bound_mask;
void blc_smp_MaskBoundInfo(unsigned char mask)
{
	smp_bound_mask = mask;
}
/**********************************************************************
 * LOCAL FUNCTIONS
 */


//paring end will trigger by: paring success & paring fail
void blc_smp_procParingEnd(u8 err_reason)  // 0 is OK; other for fail
{
	//When a Pairing process completes or fail, the Security Manager Timer shall be stopped.
	blc_smp_setCertTimeoutTick(0);

	blc_smp_setParingBusy (0);
	blc_ll_setEncryptionBusy (0);

	blc_smpMng.key_distribute = 0;
	smpDistirbuteKeyOrder = SMP_TRANSPORT_SPECIFIC_KEY_END;
	//printf("pairing %s end, clr distribut flg\n", err_reason? "failed":"succ");
	smp_DistributeKeyInit.keyIni = 0;
	smp_DistributeKeyResp.keyIni = 0;

	smp_phase_record = 0; //need clear when pairing failed

	if(err_reason && (gap_eventMask & GAP_EVT_MASK_SMP_PAIRING_FAIL) ){
		u8 param_evt[4];
		gap_smp_paringFailEvt_t *pEvt = (gap_smp_paringFailEvt_t *)param_evt;
		pEvt->connHandle = BLS_CONN_HANDLE;
		pEvt->reason = err_reason;
		blc_gap_send_event ( GAP_EVT_SMP_PAIRING_FAIL, param_evt, sizeof(gap_smp_paringFailEvt_t) );
	}

}



void blc_smp_certTimeoutLoopEvt (void)
{
	if(smp_timeout_start_tick)
	{
		if(clock_time_exceed(smp_timeout_start_tick, 30*1000*1000))
		{
			//printf("delta:%dms\n", (u32)(clock_time() - smp_timeout_start_tick)/SYSTEM_TIMER_TICK_1MS);
			// send terminate command
			//Here no longer send terminate directly, if the user wants to disconnect, register a 'GAP_EVT_SMP_PAIRING_FAIL' call back.
//			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			smp_timeout_start_tick = 0;
			//printf("smp timeout\n");

			blc_smp_procParingEnd(PAIRING_FAIL_REASON_PAIRING_TIEMOUT);  //paring timeout

		}
	}
}


void blc_smp_checkSecurityReqeustSending(u32 connStart_tick)
{
	if(clock_time_exceed(connStart_tick, blc_SecReq_ctrl.pending_ms * 1000) )
	{
		blc_SecReq_ctrl.secReq_pending = 0;
		blc_smp_sendSecurityRequest ();
	}
}













int bls_smp_encryption_done(u16 connHandle)
{
	blc_smpMng.key_distribute = 1;

	/*
	 * Legacy Pairing:
	 * ......
	 * (omit)
	 * M->S LL_ENC_REQ: Encryption begin
	 * S->M LL_START_ENC_REQ:
	 * M->S LL_START_ENC_RSP:
	 * S->M LL_START_ENC_RSP: Encryption done
	 */
	if(smp_phase_record & SMP_PHASE_STAGE2_ENC){
		smp_phase_record |= SMP_PHASE_STAGE3;
	}

	int reConnect = SMP_STANDARD_PAIR;
	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	if(tbl_bondDevice.keyIndex < tbl_bondDevice.cur_bondNum) //auto connect when key match
	{

		reConnect = SMP_FAST_CONNECT;

		#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
				//key match in device table, but not the latest paired one, auto connected to previous paired device
				if(tbl_bondDevice.index_update_method == Index_Update_by_Connect_Order)
				{
					/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
					if( tbl_bondDevice.keyIndex != (tbl_bondDevice.cur_bondNum - 1) )  //not the last one
					{
						blc_smp_param_updateToNearestByIndex(tbl_bondDevice.keyIndex);  //update to nearest   //@@@ do later @@@
					}
				}
		#else

				//do device index update like above   or not do, it's up to you

		#endif
	}




	if(gap_eventMask & GAP_EVT_MASK_SMP_CONN_ENCRYPTION_DONE){
		u8 param_evt[4];
		gap_smp_connEncDoneEvt_t *pEvt = (gap_smp_connEncDoneEvt_t *)param_evt;
		pEvt->connHandle = BLS_CONN_HANDLE;
		pEvt->re_connect = reConnect;

		blc_gap_send_event ( GAP_EVT_SMP_CONN_ENCRYPTION_DONE, param_evt, sizeof(gap_smp_paringBeginEvt_t) );
	}

	if((gap_eventMask & GAP_EVT_MASK_SMP_SECURITY_PROCESS_DONE) && reConnect == SMP_FAST_CONNECT){
		u8 param_evt[4];
		gap_smp_securityProcessDoneEvt_t *pEvt = (gap_smp_securityProcessDoneEvt_t *)param_evt;
		pEvt->connHandle = BLS_CONN_HANDLE;
		pEvt->re_connect = SMP_FAST_CONNECT;

		blc_gap_send_event( GAP_EVT_SMP_SECURITY_PROCESS_DONE, param_evt, sizeof(gap_smp_securityProcessDoneEvt_t) );
	}

	return 0;
}


#if(LL_PAUSE_ENC_FIX_EN)
	int bls_smp_encryption_pause(u16 connHandle)
	{
		blc_smpMng.key_distribute = 0;

		return 0;
	}
#endif


void bls_smp_pairing_success(void)
{
	//TODO:test if bux fix OK
	if(blc_smpMng.save_key_flag){
		return;
	}
	blc_smpMng.save_key_flag = 1;

	/*
	 * Legacy Pairing:
	 * ......
	 * (omit)
	 * M->S LL_ENC_REQ: Encryption begin
	 * S->M LL_START_ENC_REQ:
	 * M->S LL_START_ENC_RSP:
	 * S->M LL_START_ENC_RSP: Encryption done
	 *
	 * Key Distribution(smp phase3)
	 * Pairing Phase1\2\3 all finised: clear all marks
	 */
	if(smp_phase_record & SMP_PHASE_STAGE3){
		smp_phase_record = 0;
	}

	smpDistirbuteKeyOrder = SMP_TRANSPORT_SPECIFIC_KEY_END;
	//printf("pairing succ, clr distribut flg\n");

	int save_result = 0;
	if(blc_smpMng.bonding_enable){


#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
		smp_param_save_t smp_param_save;

		smp_param_save.peer_addr_type = smp_param_peer.peer_addr_type;
		memcpy (smp_param_save.peer_addr, smp_param_peer.peer_addr, 6);

		smp_param_save.cflg_union.cflg_pack = 0;
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en){
				smp_param_save.cflg_union.device_idx = smp_param_own.devc_idx;
			}
		#endif

		smp_param_save.peer_id_adrType = smp_param_peer.peer_id_address_type;
		memcpy (smp_param_save.peer_id_addr, smp_param_peer.peer_id_address, 6);

		memcpy (smp_param_save.local_irk, smp_param_own.local_irk, 16);
		memcpy (smp_param_save.peer_irk, smp_param_peer.peer_irk, 16);
		memcpy (smp_param_save.own_ltk, smp_param_own.own_ltk, 16);

		//core5.0 Vol3, Part H
		//The authentication requirements include the type of bonding and man-in-the-middle protection (MITM) requirements
		#if (1)// ?? unsure
			tbl_bondDevice.paring_status = (blc_smpMng.stk_method == JustWorks) ? Unauthenticated_LTK : (blc_smpMng.secure_conn ? Authenticated_LTK_Secure_Connection : Authenticated_LTK_Legacy_Paring);
		#else
			tbl_bondDevice.paring_status = blc_smpMng.secure_conn ? Authenticated_LTK_Secure_Connection : (blc_smpMng.stk_method == JustWorks ? Unauthenticated_LTK : Authenticated_LTK_Legacy_Paring);
		#endif

#if (SIMPLE_MULTI_MAC_EN)
		extern u8 flag_smp_param_save_base;
		smp_param_save.flag = (flag_smp_param_save_base | tbl_bondDevice.paring_status<<4);
#else
		smp_param_save.flag = (FLAG_SMP_PARAM_SAVE_BASE | tbl_bondDevice.paring_status<<4);
#endif

		save_result = bls_smp_param_saveBondingInfo(&smp_param_save);

#else

		// add paring info save interface here
#endif
	}




	//paring end successful
	blc_smp_procParingEnd(0);   //paring success
 	if(gap_eventMask & GAP_EVT_MASK_SMP_PAIRING_SUCCESS){
		u8 param_evt[4];
		gap_smp_paringSuccessEvt_t *pEvt = (gap_smp_paringSuccessEvt_t *)param_evt;
		pEvt->connHandle = BLS_CONN_HANDLE;
		pEvt->bonding = blc_smpMng.bonding_enable;
		pEvt->bonding_result = save_result ? 1:0;

		blc_gap_send_event ( GAP_EVT_SMP_PAIRING_SUCCESS, param_evt, sizeof(gap_smp_paringSuccessEvt_t) );
	}

	if(gap_eventMask & GAP_EVT_MASK_SMP_SECURITY_PROCESS_DONE){
		u8 param_evt[4];
		gap_smp_securityProcessDoneEvt_t *pEvt = (gap_smp_securityProcessDoneEvt_t *)param_evt;
		pEvt->connHandle = BLS_CONN_HANDLE;
		pEvt->re_connect = SMP_STANDARD_PAIR;

		blc_gap_send_event( GAP_EVT_SMP_SECURITY_PROCESS_DONE, param_evt, sizeof(gap_smp_securityProcessDoneEvt_t) );
	}
}



/**************************************************
 * 	used for distribute key in order.
 */

u8 * bls_smp_sendInfo (u16 connHandle)
{

	if (smpDistirbuteKeyOrder == SMP_TRANSPORT_SPECIFIC_KEY_START)   //distribute LTK
	{
		smpDistirbuteKeyOrder = SMP_OP_ENC_INFO;
		if(smp_DistributeKeyResp.encKey)
			return bls_smp_pushPkt (SMP_OP_ENC_INFO);
	}

	if (smpDistirbuteKeyOrder == SMP_OP_ENC_INFO) //distribute RAND & EDIV
	{
		smpDistirbuteKeyOrder = SMP_OP_ENC_IINFO;
		if(smp_DistributeKeyResp.encKey){
			smp_DistributeKeyResp.encKey = 0;   //clear encKey after pushFifo OK
			return bls_smp_pushPkt (SMP_OP_ENC_IDX);
		}

	}

	if (smpDistirbuteKeyOrder == SMP_OP_ENC_IINFO) //distribute IRK
	{
		smpDistirbuteKeyOrder = SMP_OP_ENC_IADR;
		if(smp_DistributeKeyResp.idKey)
			return bls_smp_pushPkt (SMP_OP_ENC_IINFO);
	}

	if (smpDistirbuteKeyOrder == SMP_OP_ENC_IADR) //distribute ADDR
	{
		smpDistirbuteKeyOrder = SMP_OP_ENC_SIGN;
		if(smp_DistributeKeyResp.idKey){
			smp_DistributeKeyResp.idKey = 0;   //clear idKey after pushFifo OK
			return bls_smp_pushPkt (SMP_OP_ENC_IADR);
		}

	}


	int send_signKey = 0;
	if (smpDistirbuteKeyOrder == SMP_OP_ENC_SIGN) //distribute CSRK
	{
		smpDistirbuteKeyOrder = SMP_TRANSPORT_SPECIFIC_KEY_END;
		//printf("end, clr distribut flg\n");

		if(smp_DistributeKeyResp.sign){
			smp_DistributeKeyResp.sign = 0;   //clear sign after pushFifo OK
			send_signKey = 1; //
		}

		//if sending key and receiving key all completed, process paring end
		if(smp_DistributeKeyResp.keyIni==0 && smp_DistributeKeyInit.keyIni==0) {
			bls_smp_pairing_success();
		}
	}


	if(send_signKey){
		return bls_smp_pushPkt (SMP_OP_ENC_SIGN);
	}


	return 0;
}



void bls_smp_peripheral_paring_loop(void)
{

	if(blc_SecReq_ctrl.secReq_pending && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN){
		//blc_smp_checkSecurityReqeustSending(blc_rcvd_connReq_tick);  //blc_rcvd_connReq_tick in ll_adv.c
		if(clock_time_exceed(blc_rcvd_connReq_tick, blc_SecReq_ctrl.pending_ms * 1000) )
		{
			blc_SecReq_ctrl.secReq_pending = 0;
			blc_smp_sendSecurityRequest ();
		}
	}



	if(blc_smpMng.tk_status & TK_ST_CONFIRM_PENDING){
		if( blc_smpMng.tk_status & TK_ST_UPDATE ){
			blc_smpMng.tk_status = 0;
			u8* pr = bls_smp_pushPkt (SMP_OP_PAIRING_CONFIRM);
			bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
		}
		else{
			//smp timeout, process in: blc_smp_certTimeoutLoopEvt
		}
	}

	if(blc_smpMng.tk_status & TK_ST_NUMERIC_COMPARE)
	{
		if( blc_smpMng.tk_status & TK_ST_NUMERIC_CHECK_YES){
			if(blc_smpMng.tk_status & TK_ST_NUMERIC_DHKEY_FAIL_PENDING){
				blc_smpMng.tk_status = 0;


				//paring fail event to tell upper layer
				blc_smp_procParingEnd(PAIRING_FAIL_REASON_DHKEY_CHECK_FAIL);  //paring end with failure

				u8* pr = blc_smp_pushParingFailed (PAIRING_FAIL_REASON_DHKEY_CHECK_FAIL);
				bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
			}
			else if(blc_smpMng.tk_status & TK_ST_NUMERIC_DHKEY_SUCC_PENDING){
				blc_smpMng.tk_status = 0;

				u8* pr = bls_smp_pushPkt (SMP_OP_PAIRING_DHKEY);
				bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
			}
		}
		else if( blc_smpMng.tk_status & TK_ST_NUMERIC_CHECK_NO){
			blc_smpMng.tk_status = 0;

			//paring fail event to tell upper layer
			blc_smp_procParingEnd(PAIRING_FAIL_REASON_NUMUERIC_FAILED);  //paring end with failure

			//See the Core_v5.0(Vol 3/Part H/3.5.5/Pairing Failed) for more information.
			//NOTICE: test by smart phone, master send unspecified reason when press "NO" button!
			u8* pr = blc_smp_pushParingFailed (PAIRING_FAIL_REASON_NUMUERIC_FAILED); // unsure??
			bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
		}
		else{
			//smp timeout, process in: blc_smp_certTimeoutLoopEvt
		}
	}


	if(blc_smpMng.key_distribute){

	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		if( blt_ll_getRealTxFifoNumber () == 0 )
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		if( blc_ll_getTxFifoNumber () == 0 )
	#endif
		{
			if(func_smp_info){
				u8 *pr = (u8 *)func_smp_info(BLS_CONN_HANDLE); //send encryption info
				if (pr)
				{
					bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
				}
			}
		}
	}


	//If the Security Manager Timer reaches 30 seconds, the procedure shall be
	//considered to have failed, and the local higher layer shall be notified.
	blc_smp_certTimeoutLoopEvt();

}






/**************************************************
 * 	used for save parameter in paring buffer
 */
int bls_smp_setAddress (u16 connHandle, u8 *p)
{

	smp_param_peer.peer_addr_type = p[0] & BIT(6) ? 1 : 0;
	memcpy (smp_param_peer.peer_addr, p + 2, 6);							//initiate address

	smp_param_own.own_conn_type = p[0] & BIT(7) ? 1 : 0;
	memcpy (smp_param_own.own_conn_addr, p + 8, 6);							//slave address


	#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
		if(mlDevMng.mldev_en){
			smp_param_own.devc_idx = mlDevMng.cur_dev_idx;
			/* for MLA, very important: "cur bondNum" match to mldCur bondNum[device index], can use "cur bondNum" later */
			tbl_bondDevice.cur_bondNum = tbl_bondDevice.mldCur_bondNum[mlDevMng.cur_dev_idx];
		}
	#endif

#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
	//if connection peer address is in bonding flash, in: index,  not in: 0xFF
	u32 flash_addr = bls_smp_searchBondingDevice_in_Flash_by_Address(smp_param_peer.peer_addr_type, smp_param_peer.peer_addr);
	if(flash_addr){
		tbl_bondDevice.addrIndex = bls_smp_param_getIndexByFLashAddr(flash_addr);

		u8 bondFlg = bls_smp_param_getBondFlag_by_flashAddress(flash_addr);
		//tbl_bondDevice.paring_status = (bondFlg == FLAG_SMP_PARAM_SAVE_UNANTHEN ? Unauthenticated_LTK : (bondFlg == FLAG_SMP_PARAM_SAVE_AUTHEN ? Authenticated_LTK_Legacy_Paring : Authenticated_LTK_Secure_Connection) );
		tbl_bondDevice.paring_status = (bondFlg & FLAG_SMP_PAIRING_STATUS_MASK)>>4;  // BIT<5:4> of bondFlag
	}
	else{
		tbl_bondDevice.addrIndex = ADDR_NOT_BONDED;
		tbl_bondDevice.paring_status = No_Bonded_No_LTK;
	}
#else
	//search current connected device in bonding device table by address
	//if bonded device, tbl_bondDevice.addrIndex is a small value,
	//if not bonded, tbl_bondDevice.addrIndex = ADDR_NOT_BONDED;

	//set tbl_bondDevice.paring_status
#endif



	smp_phase_record = 0;


	tbl_bondDevice.keyIndex = KEY_FLAG_IDLE;  	//if keyIndex used, must clear it to "0xFF" here


	blc_smpMng.save_key_flag = 0;

	//if use SC_passkey entry, global variable is used to count 20 repetitions over SMP.
	smpPkShftCnt = 0;

	//clear all peer key info
	//memset (smp_param_peer.paring_peer_rand, 0, 16);  //no need, to save code run time
	memset (smp_param_peer.peer_irk, 0xFF, 16);
	memset (smp_param_own.local_irk, 0xFF, 16);


#if (SECURE_CONNECTION_ENABLE)
	//init slave's own_ltk and sc_flg to zero
	blc_smpMng.secure_conn = 0;
	//memset(smp_param_own.own_ltk, 0,16); //currently, needless
#endif


/**********************************************************************************************************
process security request sending according to: if device bonded previously  & SecReq sending strategy configed by user
**********************************************************************************************************/
	u8 secReq_cfg;  //new device connect
	if(tbl_bondDevice.addrIndex != ADDR_NOT_BONDED){  //bonded device re-connect
		secReq_cfg = blc_SecReq_ctrl.secReq_conn & 0xF0;   //see <7:4>
	}
	else{   //new device connect
		secReq_cfg = blc_SecReq_ctrl.secReq_conn & 0x0F;   //see <3:0>
	}

	if( secReq_cfg & (SecReq_IMM_SEND<<4 | SecReq_IMM_SEND) ){
		blc_SecReq_ctrl.secReq_pending = 0;
		blc_smp_sendSecurityRequest ();
	}
	else if( secReq_cfg & (SecReq_PEND_SEND<<4 | SecReq_PEND_SEND)){
		blc_SecReq_ctrl.secReq_pending = 1;
	}
/*********************************************************************************************************/

	return 1;
}









/**************************************************
 * 	used for handle link layer callback (ltk event callback), packet LL_ENC_request .
 *
 *  this function should pass ltk to controller with API: "blc_hci_ltkRequestReply" or "blc_hci_ltkRequestNegativeReply"
 */

int bls_smp_getLtkReq (u16 connHandle, u8 * random, u16 ediv)
{

	//notice that: "blc_SecReq_ctrl.secReq_pending = 0" execute in this function, for host need do this upon ll_enc_req is received
	blc_SecReq_ctrl.secReq_pending = 0;  //clear security request pending flag





	//own ediv: ltk(0~1);   random(2~9)
	u8 randParam[10] = {0};
	u8 const_u8_10_zero[10] = {0};
	memcpy (randParam, random, 8);
	randParam[8] = ediv;
	randParam[9] = ediv >> 8;

	//Legacy Paring, master new enc_req   or  Secure Connection: Master new & old enc_req
	if (memcmp (const_u8_10_zero, randParam, 10) == 0)
	{
		u8 smp_Stk[16] = {0};
		u8 smp_Stk_temp[16] = {0};
		u8 max_key_size = smp_param_own.paring_req.maxEncrySize ;
		//check length Not bigger then 16
		max_key_size = max_key_size > 16 ? 16 : max_key_size;

#if (SECURE_CONNECTION_ENABLE)   //Secure Connection enc_req process
		if(blc_smpMng.secure_conn)
		{
			/*
			 * Secure Connection Pairing:
			 * ......
			 * (omit)
			 * M->S Pairing DHKey Check: none
			 * S->M Pairing DHKey Check: Pairing DHKey Check marked
			 * M->S LL_ENC_REQ: Encryption begin
			 */
			if(smp_phase_record & BIT(SMP_OP_PAIRING_DHKEY)){

				smp_phase_record |= SMP_PHASE_STAGE2_ENC;

				//////////////////// secure connections 1st //////////////////

				memcpy(smp_Stk, smp_param_own.own_ltk, max_key_size);
				blc_hci_ltkRequestReply(connHandle,  smp_Stk);
			}
			else{
				smp_phase_record = 0;
				// ltk get error : hci cmd ltk negative reply command.
				blc_hci_ltkRequestNegativeReply(connHandle);
			}
		}
		//Legacy Paring, master new enc_req   or  Secure Connection: old enc_req
		else{
			/*
			 * there are two cases:
			 * case1: legacy paring 1st ( smp_phase_record & SMP_OP_PAIRING_RANDOM must be True)
			 * case2: secure connections back
			 */
			if((smp_phase_record & BIT(SMP_OP_PAIRING_RANDOM)) == 0)//We think this is the condition of case 2.
			{
				//////////////////// secure connections back //////////////////////

				#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
					u32 flash_addr = bls_smp_loadParamVsAddr (smp_param_peer.peer_addr_type , smp_param_peer.peer_addr );
					if(flash_addr && (!smp_bound_mask)){

						blc_hci_ltkRequestReply(connHandle, smp_param_own.own_ltk);

						tbl_bondDevice.keyIndex = bls_smp_param_getIndexByFLashAddr(flash_addr);
					}
					else{
						// ltk get error : hci cmd ltk negative reply command.
						blc_hci_ltkRequestNegativeReply(connHandle);
						if(smp_bound_mask)
							return 0;
						tbl_bondDevice.keyIndex = KEY_FLAG_FAIL;//FD

						/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
						if(tbl_bondDevice.addrIndex < tbl_bondDevice.cur_bondNum)
						{
							bls_smp_param_deleteByIndex(tbl_bondDevice.addrIndex); //local device index is "smp param own.devc idx" for MLD application
							tbl_bondDevice.addrIndex = ADDR_DELETE_BOND; //update
						}
					}
				#else
					// find keys in your storage media
				#endif
			}
			else
#endif
			{
				//////////////////// legacy paring 1st ////////////////////
				/*
				 * Legacy Pairing:
				 * M->S Pairing Confirm: phase2 begin
				 * S->M Pairing Confirm: Pairing Confirm marked
				 * M->S Pairing Random:
				 * S->M Pairing Random: Pairing Random marked
				 * M->S LL_ENC_REQ: Encryption begin
				 */
				if(smp_phase_record & BIT(SMP_OP_PAIRING_RANDOM)){

					smp_phase_record |= SMP_PHASE_STAGE2_ENC;

					
					//STK = s1(TK, Srand, Mrand)

					blt_smp_alg_s1 (smp_Stk_temp, smp_param_own.paring_tk, smp_param_peer.paring_peer_rand, smpOwnRand_const);

#if (DEBUG_PAIRING_ENCRYPTION)
					memcpy(AA_smp_Stk_temp, smp_Stk_temp, 16);
#endif

					//generate session key
					memcpy(smp_Stk, smp_Stk_temp, max_key_size);

					blc_hci_ltkRequestReply(connHandle,  smp_Stk);

					tbl_bondDevice.keyIndex = KEY_FLAG_NEW;  //mark that STK generated in paring procedure, not previous existed keys

					/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
					if(tbl_bondDevice.addrIndex < tbl_bondDevice.cur_bondNum)
					{
						bls_smp_param_deleteByIndex(tbl_bondDevice.addrIndex); //local device index is "smp param own.devc idx" for MLD application
						tbl_bondDevice.addrIndex = ADDR_NEW_BONDED; //update
					}
				}
				else{
					smp_phase_record = 0;
					// ltk get error : hci cmd ltk negative reply command.
					blc_hci_ltkRequestNegativeReply(connHandle);
				}
			}
#if (SECURE_CONNECTION_ENABLE)
		}
#endif
	}
	else
	{
		//////////////////// legacy paring back ////////////////////

		//Legacy Paring: Master uses previously assigned key
		#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
			u32 flash_addr = bls_smp_loadParamVsRand (ediv, random);

			if(flash_addr && (!smp_bound_mask))
			{
				blc_hci_ltkRequestReply(connHandle,  smp_param_own.own_ltk);

				tbl_bondDevice.keyIndex = bls_smp_param_getIndexByFLashAddr(flash_addr);  //mark that LTK is previous distributed key,  re_connect process
			}
			else
			{
				#if (LL_PAUSE_ENC_FIX_EN)

					bool needSpecialProc = FALSE;

					if(smp_phase_record & SMP_PHASE_STAGE3){
						needSpecialProc = TRUE;
					}

					//During key distribution, ediv rand is obtained by negating the first 10 bytes of ltk, saving ram and flash space
					u8 rand[10];
					rand[0] = ~ediv;
					rand[1] = ~(ediv >> 8);
					for(u8 i=0; i<8; i++){
						rand[i+2] = ~random[i];
					}
					if(needSpecialProc && !memcmp(rand, smp_param_own.own_ltk, 10)){
						blc_hci_ltkRequestReply(connHandle,  smp_param_own.own_ltk);
						tbl_bondDevice.keyIndex = bls_smp_param_getIndexByFLashAddr(flash_addr);  //mark that LTK is previous distributed key,  re_connect process
					}
					else
				#endif
				{
					// ltk get error : hci cmd ltk negative reply command.
					blc_hci_ltkRequestNegativeReply(connHandle);
					if(smp_bound_mask)
						return 0;

					tbl_bondDevice.keyIndex = KEY_FLAG_FAIL;  // mark that key not found in a re_connect process

					/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
					if(tbl_bondDevice.addrIndex < tbl_bondDevice.cur_bondNum)
					{
						bls_smp_param_deleteByIndex(tbl_bondDevice.addrIndex); //local device index is "smp param own.devc idx" for MLD application
						tbl_bondDevice.addrIndex = ADDR_DELETE_BOND; //update
					}
				}
			}
		#else
				// find keys in your storage media
		#endif
	}
	return 1;
}











/**************************************************
 * 	used for set smp responder packet data and return .
 */
u8 *bls_smp_pushPkt (int type)
{
	//The SMP Timer shall be reset when an L2CAP SMP command is queued for transmission.

	switch(type){


		case SMP_OP_PAIRING_RSP :
		{
			blc_ll_setEncryptionBusy (1);

			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x07;      //l2cap len
			memcpy (&smpResSignalPkt.opcode, (u8*)&smp_param_own.paring_rsp, 7);
		}
		break;

		case SMP_OP_PAIRING_CONFIRM :
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			//make smp paring confirm
#if (SECURE_CONNECTION_ENABLE)
			if(blc_smpMng.secure_conn)
			{
				if(func_smp_sc_pushPkt_proc){
					func_smp_sc_pushPkt_proc(type);
				}
			}
			else
#endif
			{
				//Legacy Pairing: S->M Pairing Confirm: mark (M->S Pairing Confirm: phase2 begin)
				if(smp_phase_record & SMP_PHASE_STAGE2){
					smp_phase_record |= BIT(SMP_OP_PAIRING_CONFIRM);
				}

				u8* ia = smp_param_peer.peer_addr;      //initiate address
				u8 iat = smp_param_peer.peer_addr_type; //initial address type
				u8* ra = smp_param_own.own_conn_addr;   //slave address
				u8 rat = smp_param_own.own_conn_type;   //slave address type

#if (DEBUG_PAIRING_ENCRYPTION)
				memcpy(AA_paring_tk1, smp_param_own.paring_tk, 16);
				memcpy(AA_smp_const, smpOwnRand_const, 16);
				memcpy(AA_paringBuf_1, paring_buf1, 16);
				memcpy(AA_paringBuf_2, paring_buf2, 16);
#endif

				/* Calculate Sconfirm
				 *    Sconfirm = c1(TK, Srand,Pairing Request command, Pairing Response command,
				 *					initiating device address type, initiating device address,
				 *					responding device address type, responding device address)
				 **/
				//aes_ll_encryption in blt_smp_alg_c1/blt_smp_alg_s1 need critical data 4B aligned
				blt_smp_alg_c1(smpOwnParingConfirm, smp_param_own.paring_tk, smpOwnRand_const,
							  (u8*)&smp_param_own.paring_rsp, (u8*)&smp_param_own.paring_req,
							  iat, ia, rat, ra);

#if (DEBUG_PAIRING_ENCRYPTION)
				memcpy(AA_paringRandom, smpOwnParingConfirm, 16);
#endif
			}

			memcpy( smpResSignalPkt.data, smpOwnParingConfirm, 16);
			smpResSignalPkt.l2capLen = 0x11;      //l2cap len
		}
		break;

		case SMP_OP_PAIRING_RANDOM :
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

#if (SECURE_CONNECTION_ENABLE)
			if(blc_smpMng.secure_conn)
			{
				if(func_smp_sc_pushPkt_proc){
					func_smp_sc_pushPkt_proc(type);
				}
			}
			else
#endif
			{
				/*
				 * Legacy Pairing:
				 * M->S Pairing Confirm: phase2 begin
				 * S->M Pairing Confirm: Pairing Confirm marked
				 * M->S Pairing Random: none
				 * S->M Pairing Random: Pairing Random marked
				 */
				if(smp_phase_record & BIT(SMP_OP_PAIRING_CONFIRM)){
					smp_phase_record |= BIT(SMP_OP_PAIRING_RANDOM);
				}

				memcpy(smpResSignalPkt.data, smpOwnRand_const, 16);  // send random in legacy paring and send Nonces in secure connection.

				//smp4.0, after exchange smp random, then transport specific keys distribution
				smpDistirbuteKeyOrder = SMP_TRANSPORT_SPECIFIC_KEY_START;
				//printf("smp4.0, set distribut flg\n");
			}

			smpResSignalPkt.l2capLen = 0x11;      //l2cap len
		}
			break;

		case SMP_OP_ENC_INFO :  //distribute LTK
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x11;      //l2cap len
			memcpy(smpResSignalPkt.data, smp_param_own.own_ltk, 16);
		}
			break;

		case SMP_OP_ENC_IDX :	 //distribute ediv addr
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x0b;      //l2cap len
			u8 ediv_rand[10];
			for(int i=0;i<10;i++){
				ediv_rand[i] = ~smp_param_own.own_ltk[i];
			}
			memcpy(smpResSignalPkt.data , ediv_rand, 10);
		}
			break;

		case SMP_OP_ENC_IINFO :	  //distribute IRK
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x11;      //l2cap len

			#if 1//walk around win10 bug
//				 generateRandomNum(16,smpResSignalPkt.data);
				 u8 data[16]={0};

				 memcpy((u8*)data, (u8*)bltMac.macAddress_public, 6);
				 aes_ll_encryption(smpParingOwnIRK, data, smpResSignalPkt.data);

				#else
				 memcpy(smpResSignalPkt.data, (u8 *)(smpParingOwnIRK), 16);
			#endif

			/*
			 * Notice: air pkt little endian format ==>  big endian format
			 * AES use big endian format.
			 */
			#if (IRK_REVERT_TO_SAVE_AES_TMIE_ENABLE)
				 swapX(smpResSignalPkt.data, smp_param_own.local_irk, 16); //save use big endian format
			#else
				 memcpy(smp_param_own.local_irk, smpResSignalPkt.data, 16);
			#endif
		}
			break;
		case SMP_OP_ENC_IADR :    //distribute bd_addr
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x08;      //l2cap len

			/*
			 * core5.0 Vol6, PartB, page 2688
			 * If a device is using Resolvable Private Addresses Section 1.3.2.2, it shall also
			 * have an Identity Address that is either a Public or Random Static address type.
			 **/
			#if 0 // here, we use the conn_req's (slave) BDR maybe a mistake, if we(slave) use a RPA as its BDR.
				smpResSignalPkt.data[0] = smp_param_own.own_conn_type;   ////address type
				memcpy(smpResSignalPkt.data + 1, smp_param_own.own_conn_addr , 6);
			#else // here we use Public BDR as its Identity Addres

				u8 own_addr_type = blt_ll_getOwnAddrType();

				if(own_addr_type == OWN_ADDRESS_PUBLIC || own_addr_type == OWN_ADDRESS_RESOLVE_PRIVATE_PUBLIC){
					u8* own_pub_addr = blc_ll_get_macAddrPublic();
					smpResSignalPkt.data[0] = OWN_ADDRESS_PUBLIC;
					memcpy(smpResSignalPkt.data + 1, own_pub_addr, 6);
				}
				else if(own_addr_type == OWN_ADDRESS_RANDOM || own_addr_type == OWN_ADDRESS_RESOLVE_PRIVATE_RANDOM){
					u8* own_rnd_addr = blc_ll_get_macAddrRandom();
					smpResSignalPkt.data[0] = OWN_ADDRESS_RANDOM;
					memcpy(smpResSignalPkt.data + 1, own_rnd_addr, 6);
				}
			#endif
		}
			break;
		case SMP_OP_ENC_SIGN : //distribute csrk
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x11;
			memcpy(smpResSignalPkt.data, (u8 *)(smpParingOwnCSRK), 16);
		}
			break;
		case SMP_OP_PAIRING_FAIL :
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x02;      //l2cap len
			smpResSignalPkt.data[0] = 0x04; //confirm value failed
		}
			break;
		case SMP_OP_SEC_REQ:
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			smpResSignalPkt.l2capLen = 0x02;        //l2cap len
			*smpResSignalPkt.data = smp_param_own.auth_req.authType;     	//AuthReq
		}
			break;
		// -- core 4.2
#if (SECURE_CONNECTION_ENABLE)
		case SMP_OP_PAIRING_PUBLIC_KEY:
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			if(func_smp_sc_pushPkt_proc){
				func_smp_sc_pushPkt_proc(type);
			}

			return NULL; //must add
		}
		break;

		case SMP_OP_PAIRING_DHKEY:
		case SMP_OP_KEYPRESS_NOTIFICATION:
		{
			blc_smp_setCertTimeoutTick(clock_time()|1);

			if(func_smp_sc_pushPkt_proc){
				func_smp_sc_pushPkt_proc(type);
			}
		}
			break;
#endif
		default:
			break;
	}
	smpResSignalPkt.opcode = type;
	smpResSignalPkt.rf_len = smpResSignalPkt.l2capLen + 4;
	smpResSignalPkt.chanId = 0x06;

	return (u8*)&smpResSignalPkt;
}




/**************************************************
 * 	used for handle smp request data.
 */
u8 * l2cap_smp_handler(u16 connHandle, u8 * p)
{
	rf_packet_l2cap_req_t * req = (rf_packet_l2cap_req_t *)p;
	u8 param_evt[8];

	u8 code = req->opcode;
	switch(code){
		case SMP_OP_PAIRING_REQ :
		{
			memcpy ((u8*)&smp_param_own.paring_req, &req->opcode, 7);

			blc_smpMng.secure_conn = 0;
			blc_smpMng.stk_method = 0;
			blc_smpMng.tk_status = 0;  //TK status clear
			blc_smpMng.peerKey_mask = 0;
			blc_smpMng.bonding_enable = 0;
			blc_smpMng.key_distribute = 0;

			blc_SecReq_ctrl.secReq_pending = 0;   //clear security request pending flag when paring_req is received

			if(gap_eventMask & GAP_EVT_MASK_SMP_PAIRING_BEGIN){
				gap_smp_paringBeginEvt_t *pEvt = (gap_smp_paringBeginEvt_t *)param_evt;
				pEvt->connHandle = BLS_CONN_HANDLE;
				pEvt->secure_conn = blc_smpMng.secure_conn;
				pEvt->tk_method = blc_smpMng.stk_method;

				blc_gap_send_event ( GAP_EVT_SMP_PAIRING_BEGIN, param_evt, sizeof(gap_smp_paringBeginEvt_t) );
			}

			//only this lowest security level, can not support paring
			if (blc_smpMng.security_level == No_Authentication_No_Encryption)
			{
				//paring fail event to tell upper layer
				blc_smp_procParingEnd(PAIRING_FAIL_REASON_PAIRING_NOT_SUPPORTED);  //paring end with failure

				return blc_smp_pushParingFailed (PAIRING_FAIL_REASON_PAIRING_NOT_SUPPORTED);
			}
			else if(smp_param_own.paring_req.maxEncrySize < ENCRYPRION_KEY_SIZE_MINIMUN)
			{
				//paring fail event to tell upper layer
				blc_smp_procParingEnd(PAIRING_FAIL_REASON_ENCRYPT_KEY_SIZE);  //paring end with failure

				return blc_smp_pushParingFailed (PAIRING_FAIL_REASON_ENCRYPT_KEY_SIZE);
			}
			else if(smp_param_own.paring_req.maxEncrySize > ENCRYPRION_KEY_SIZE_MAXINUM) //encryption key size: 7~16
			{
				return blc_smp_pushParingFailed (PAIRING_FAIL_REASON_INVAILD_PARAMETER);
			}

			//u8 peer_key_size = smp_param_own.paring_req.maxEncrySize;

			/*
			 * M->S Pairing Req: phase1 begin
			 * S->M Pairing Rsp:
			 */
			smp_phase_record = SMP_PHASE_STAGE1;
			blc_smpMng.bonding_enable = blc_smpMng.bonding_mode && smp_param_own.paring_req.authReq.bondingFlag;  // determine bonding final here
			blc_smpMng.secure_conn = (smp_param_own.paring_rsp.authReq.SC && smp_param_own.paring_req.authReq.SC)? 1:0;
			blc_smpMng.stk_method = blc_smp_getGenMethod(blc_smpMng.secure_conn);

			u8 level4only = ((blc_smpMng.security_level & LE_Security_Mode_1) == LE_Security_Mode_1_Level_4)? 1:0;

			if((!blc_smpMng.secure_conn) || (blc_smpMng.secure_conn && blc_smpMng.stk_method == JustWorks))
			{
				//if the local gap setting only support level4 only,we should response pairing failed
				if(level4only)
				{
					blc_smp_procParingEnd(PAIRING_FAIL_REASON_SUPPORT_NC_ONLY);  //paring end with failure
					return blc_smp_pushParingFailed (PAIRING_FAIL_REASON_UNSPECIFIED_REASON);
				}
			}

			/********************************************************************************
			 * Check parameters of both sides,  and determine :
			 * 1. which paring used:  legacy paring or Secure Connection
			 * 2. which stk generate methods used:  just works/OOB/pass_key entry/numeric comparison
			 *******************************************************************************/
			#if (SECURE_CONNECTION_ENABLE)
				//step 1:  if both sides support secure connection
				if(blc_smpMng.secure_conn)
				{
					blc_smp_setResponderKey(0, 1, 0); //TODO: when data signing OK later, add CSRK here

					// Generate ECDH keys
					if(func_smp_sc_proc){
						func_smp_sc_proc(connHandle, p);
					}
				}
				else
			#endif
				{
					#if 1 //TODO:re-init Key Distribution bits
						/*
						 * if 1st time use SC, then unpaired, and 2nd time(do not re-power) use LG, key distribution bit will be Err.
						 */
						blc_smp_setResponderKey(1, 1, 0);  //@@@ to do: when data signing OK later, add CSRK here
					#endif
				}

			/*******************************************************************************/

			//step 2:  calculate final stk generate methods
//			blc_smpMng.stk_method = blc_smp_getGenMethod(blc_smpMng.secure_conn);
//			blc_smpMng.bonding_enable = blc_smpMng.bonding_mode && smp_param_own.paring_req.authReq.bondingFlag;  // determine bonding final here


			/********************************************************************************
			 *Key Generate for a new Paring
			 *******************************************************************************/
			#if (NEW_FLASH_STORAGE_KEY_ENABLE)
				//dm(k, r) = e(k, r') mod 2^16,  Y = dm(DHK, Rand),  EDIV = Y xor DIV
				//EDIV = ( e(DHK, Rand || padding) mod 2^16)  xor 0xFFFF
			#else
				//Generate Srand
#if (DEBUG_PAIRING_ENCRYPTION)
				for(int i=0;i<16;i++){
					smpOwnRand_const[i] = i;
				}
#else

				generateRandomNum(16, smpOwnRand_const);
#endif
				if(!blc_smpMng.secure_conn){  //SC no need LTK here
					for(int i=0;i<16;i++){
						smp_param_own.own_ltk[i] = smpOwnRand_const[i] ^ 0x55;
					}
				}
			#endif

			/*******************************************************************************/

			smp_param_own.paring_rsp.initKeyDistribution.keyIni &= smp_param_own.paring_req.initKeyDistribution.keyIni;
			smp_param_own.paring_rsp.rspKeyDistribution.keyIni &= smp_param_own.paring_req.rspKeyDistribution.keyIni;

			smp_DistributeKeyInit.keyIni  = smp_param_own.paring_rsp.initKeyDistribution.keyIni;   //key receive
			smp_DistributeKeyResp.keyIni  =	smp_param_own.paring_rsp.rspKeyDistribution.keyIni;    //key send


			/********************************************************************************
			Paring begin
			 *******************************************************************************/
			blc_smp_setCertTimeoutTick(clock_time()|1);
			blc_smp_setParingBusy (1);

			/*******************************************************************************/


			/********************************************************************************
			Send corresponding event to upper layer according to TK generate methods
			 *******************************************************************************/
			if(blc_smpMng.stk_method == OOB_Authentication){
				//OOB, send TK request event to upper layer, expect upper layer call "blc_smp_setTK_by_OOB"
				// to set TK value
				blc_smpMng.tk_status = TK_ST_REQUEST;
				if(gap_eventMask & GAP_EVT_MASK_SMP_TK_REQUEST_OOB){
					blc_gap_send_event ( GAP_EVT_SMP_TK_REQUEST_OOB, NULL, 0);
				}
			}
			else if(blc_smpMng.stk_method == PK_Resp_Dsply_Init_Input){
				//Responder generate TK value(0~999999) , should notify upper layer to display this number,
				//then initiator input this number to set TK value upon watching the displayed value
				int tk_set= blc_smpMng.passKeyEntryDftTK&(~BIT(31));
				if( (blc_smpMng.passKeyEntryDftTK & BIT(31))&& (tk_set <= 999999) ){  //leave it for debug mode: manual set TK value callBack(Because RC remote do not have display equipment)
					memset(smp_param_own.paring_tk, 0, 16);
					smemcpy(smp_param_own.paring_tk, &tk_set, 4);
				}
				else{
					#if (1)
						generateRandomNum(4, (u8*)&tk_set);
						tk_set += 100000;
						tk_set &= 999999;//0~999999
						memset(smp_param_own.paring_tk, 0, 16);
						memcpy(smp_param_own.paring_tk, &tk_set, 4);
					#else
						tk_set = rand() & 0xFFFF;  // should be "x % 1000000", here "x & 0xFFFF" equal to "x % 65536",
												   // use this to save code size and code run time
					#endif
				}

				if(gap_eventMask & GAP_EVT_MASK_SMP_TK_DISPALY){
					blc_gap_send_event ( GAP_EVT_SMP_TK_DISPALY, (u8 *)&tk_set, 4);
				}
			}
			else if(blc_smpMng.stk_method == PK_BOTH_INPUT){
				// both sides should input TK value, here send TK request event to upper layer,
				// expect upper layer call "blc_smp_setTK_by_PasskeyEntry"  to set TK value
				blc_smpMng.tk_status = TK_ST_REQUEST;
				if(gap_eventMask & GAP_EVT_MASK_SMP_TK_REQUEST_PASSKEY){
					blc_gap_send_event ( GAP_EVT_SMP_TK_REQUEST_PASSKEY, NULL, 0);
				}
			}
			/*******************************************************************************/

			return bls_smp_pushPkt (SMP_OP_PAIRING_RSP);

		}
		break;

		case  SMP_OP_PAIRING_CONFIRM :
		{

			memcpy (smp_param_peer.paring_confirm, req->data, 16);

			if(!blc_smpMng.secure_conn && (smp_phase_record & SMP_PHASE_STAGE1)){
				/*
				 * Legacy Pairing:
				 * M->S Pairing Req: phase1 begin
				 * S->M Pairing Rsp:
				 * M->S Pairing Confirm: phase2 begin
				 */
				smp_phase_record |= SMP_PHASE_STAGE2;
			}

			// if use SC_PK, exec 20 times pairing confirm, avoid this ( exec only once!!!).
			if(blc_smpMng.stk_method == PK_Init_Dsply_Resp_Input && !smpPkShftCnt) //
			{
				// responder should input TK value, here send TK request event to upper layer,
				// expect upper layer call "blc_smp_setTK_by_PasskeyEntry"  to set TK value
				blc_smpMng.tk_status = TK_ST_REQUEST;
				if(gap_eventMask & GAP_EVT_MASK_SMP_TK_REQUEST_PASSKEY){
					blc_gap_send_event ( GAP_EVT_SMP_TK_REQUEST_PASSKEY, NULL, 0);
				}
			}


			int TK_ok = 0;
			if(blc_smpMng.tk_status & TK_ST_REQUEST){  //has send TK request event to upper layer
				if(blc_smpMng.tk_status & TK_ST_UPDATE){  //TK set by upper layer completed
					TK_ok = 1;
				}
				else{
					// check it in gap mainLoop, if TK set, send paring_confirm to peer device
					blc_smpMng.tk_status |= TK_ST_CONFIRM_PENDING;
				}
			}
			else{  //no need upper layer set TK
				TK_ok = 1;
			}

			if(TK_ok){
				return bls_smp_pushPkt (SMP_OP_PAIRING_CONFIRM);
			}
		}
		break;



		case SMP_OP_PAIRING_RANDOM :
		{

#if (SECURE_CONNECTION_ENABLE)
			if(blc_smpMng.secure_conn)
			{
				if(func_smp_sc_proc){
					return func_smp_sc_proc(connHandle, p);
				}
			}
			else
#endif			
			{
				memcpy (smp_param_peer.paring_peer_rand, req->data, 16);

				u8 pairingConfirm[16]  = {0};
				u8* ia = smp_param_peer.peer_addr;      //initiate address
				u8 iat = smp_param_peer.peer_addr_type; //initial address type
				u8* ra = smp_param_own.own_conn_addr;   //slave address
				u8 rat = smp_param_own.own_conn_type;   //slave address type

#if (DEBUG_PAIRING_ENCRYPTION)
				memcpy(AA_paring_tk2, smp_param_own.paring_tk, 16);
				memcpy(AA_paring_peer_rand, smp_param_peer.paring_peer_rand, 16);
#endif

				/* Checkout Mconfirm
				 *	Mconfirm = c1(TK, Mrand, Pairing Request command, Pairing Response command,
				 *				     initiating device address type, initiating device address,
				 *				     responding device address type, responding device address)
				 **/
				//aes_ll_encryption in blt_smp_alg_c1/blt_smp_alg_s1 need critical data 4B aligned
				blt_smp_alg_c1(pairingConfirm, smp_param_own.paring_tk, smp_param_peer.paring_peer_rand, (u8*)&smp_param_own.paring_rsp,
						      (u8*)&smp_param_own.paring_req, iat, ia, rat, ra);

#if (DEBUG_PAIRING_ENCRYPTION)
				memcpy(AA_paring_conf, pairing_conf, 16);
#endif


				if (memcmp (smp_param_peer.paring_confirm, pairingConfirm, 16) == 0)
				{
					return bls_smp_pushPkt (SMP_OP_PAIRING_RANDOM);
				}
				else
				{
					blc_smp_procParingEnd(PAIRING_FAIL_REASON_CONFIRM_FAILED);  //paring end with failure

					return blc_smp_pushParingFailed (PAIRING_FAIL_REASON_CONFIRM_FAILED);
				}

			}
		}
		break;

		case SMP_OP_ENC_INFO :  //LTK(16B) from peer device
		{

		}
		break;

		case SMP_OP_ENC_IDX:   	// Random(8B) & EDIV(2B) from peer device
		{
			smp_DistributeKeyInit.encKey = 0;

			blc_smpMng.peerKey_mask |= KEY_MASK_ENC;

			//if sending key and receiving key all completed, process paring end
			if(smp_DistributeKeyResp.keyIni==0 && smp_DistributeKeyInit.keyIni==0) {
				bls_smp_pairing_success();
			}
		}
		break;

		case SMP_OP_ENC_IINFO :  //IRK(16B) from peer device
		{
			#if (IRK_REVERT_TO_SAVE_AES_TMIE_ENABLE)
				for(int i=0; i<16; ++i){
					smp_param_peer.peer_irk[i] = req->data[15-i];
				}
			#else
				memcpy(smp_param_peer.peer_irk, req->data, 16);
			#endif


		}
		break;

		case SMP_OP_ENC_IADR :  //Identity Address (7B) from peer device
		{
			/*
			 * core5.0 Vol6, PartB, page 2688
			 * If a device is using Resolvable Private Addresses Section 1.3.2.2, it shall also
			 * have an Identity Address that is either a Public or Random Static address type.
			 **/

			smp_DistributeKeyInit.idKey = 0;
			//Real master address from master
			smp_param_peer.peer_id_address_type = req->data[0];
			memcpy (smp_param_peer.peer_id_address, req->data + 1, 6);


			//if sending key and receiving key all completed, process paring end
			if(smp_DistributeKeyResp.keyIni==0 && smp_DistributeKeyInit.keyIni==0) {
				bls_smp_pairing_success();
			}

		}
		break ;



		case SMP_OP_ENC_SIGN :   //CSRK(16B) from peer device
		{
			#if 0  // @@@ do later: process signCounter
				memcpy (smp_param_peer.peer_csrk, req->data , 16);
 				swap128(smp_sign_info.csrk, smp_param_peer.peer_csrk);
				smp_sign_info.signCounter = 0xffffffff;
			#endif
			smp_DistributeKeyInit.sign = 0;

			blc_smpMng.peerKey_mask |= KEY_MASK_SIGN;
			//if sending key and receiving key all completed, process paring end
			if(smp_DistributeKeyResp.keyIni==0 && smp_DistributeKeyInit.keyIni==0) {
				bls_smp_pairing_success();
			}
		}
		break;


#if (SECURE_CONNECTION_ENABLE)
		case SMP_OP_PAIRING_PUBLIC_KEY:
		case SMP_OP_PAIRING_DHKEY:
		case SMP_OP_KEYPRESS_NOTIFICATION:
		{
			if(blc_smpMng.secure_conn)
			{
				if(func_smp_sc_proc){
					return func_smp_sc_proc(connHandle, p);
				}
			}
			else
			{
				//paring fail event to tell upper layer
				blc_smp_procParingEnd(PAIRING_FAIL_REASON_CMD_NOT_SUPPORT);  //paring end with failure

				return blc_smp_pushParingFailed(PAIRING_FAIL_REASON_CMD_NOT_SUPPORT);
			}
		}
		break;
#endif



		case SMP_OP_PAIRING_FAIL :
		{
			//paring fail event to tell upper layer
			blc_smp_procParingEnd(req->data[0]);  //paring end with failure

		}
		break;



		default:
		break;

	}

	return 0;
}





/**************************************************
 *  API used for slave start encryption.
 */
int blc_smp_sendSecurityRequest (void)
{
	u8* pr = bls_smp_pushPkt (SMP_OP_SEC_REQ);
	return bls_ll_pushTxFifo(BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
}

//This function is used by some special application(They no need change app code), so do not delete it even not used by stack
int bls_smp_startEncryption (void)
{
	u8* pr = bls_smp_pushPkt (SMP_OP_SEC_REQ);
	return bls_ll_pushTxFifo(BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
}


int blc_smp_peripheral_init (void)
{
	//only lowest security level "No_Authentication_No_Encryption", no need initialize any security paramters
	if(blc_smpMng.security_level != No_Authentication_No_Encryption)
	{

		//register some necessary functions

		func_smp_init = &bls_smp_setAddress;

		func_smp_info = &bls_smp_sendInfo;

		blc_ll_registerLtkReqEvtCb ( bls_smp_getLtkReq ); 		//register get ltk function in controller

#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
		//to get tbl_bondDevice data form flash
		bls_smp_param_initFromFlash();
#else
		// tbl_bondDevice data initialization from where your database stored
#endif



		//generate IR & ER for key distribution
		//blc_smp_generate_initial_keys();  //TODO: when adding own device RPA feature, IRK should be calculated.

		blc_smp_param_init();

		return 1;
	}

	return 0;
}






void blc_smp_configSecurityRequestSending( secReq_cfg newConn_cfg,  secReq_cfg reConn_cfg, u16 pending_ms)
{

	blc_SecReq_ctrl.secReq_conn = (reConn_cfg<<4) | newConn_cfg;
	blc_SecReq_ctrl.pending_ms = pending_ms;
}




paring_sts_t bls_smp_get_paring_statas(u16 connHandle)  //connHandle for future mutiConnection, now use direct BLS_CONN_HANDLE is OK
{
	return tbl_bondDevice.paring_status;
}

