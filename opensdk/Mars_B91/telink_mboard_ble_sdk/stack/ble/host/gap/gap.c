/********************************************************************************************************
 * @file	gap.c
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
#include "stack/ble/ble.h"



_attribute_data_retention_	host_ota_mian_loop_callback_t    		host_ota_main_loop_cb = NULL;
_attribute_data_retention_	host_ota_terminate_callback_t    		host_ota_terminate_cb = NULL;


int blc_gap_peripheral_conn_complete_handler(u16 connHandle, u8 *p)
{
	para_upReq.connParaUpReq_pending = 0;

	blt_indicate_handle = 0;

	bltAtt.pPendingPkt = 0;  //slave must clear,master not process in controller.

	#if(DATA_NO_INIT_EN)
		blt_buff_process_pending = 0;   //clear. can enter deep/deepRetention
	#endif

	if(func_smp_init){
		func_smp_init(connHandle, p + 4 );   // +4: dma_len(4 byte),  raw_pkt -> header
	}

	//kite/vulture need to keep compatibility with previous version,so disable exchange MTU automatically.
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	if(bltAtt.init_MTU != ATT_MTU_SIZE){
		bltAtt.mtu_exchange_pending = 1;
	}
#endif


	return 0;
}


int blc_gap_peripheral_conn_terminate_handler(u16 connHandle, u8 *p)
{

	bltAtt.pPendingPkt = 0;  //must clear

	#if(DATA_NO_INIT_EN)
		blt_buff_process_pending = 0;   //clear. can enter deep/deepRetention
	#endif

	/////// SMP clear
	if(blc_SecReq_ctrl.secReq_pending){
		blc_SecReq_ctrl.secReq_pending = 0;
	}


	if(blc_smpMng.paring_busy){
		blc_smp_procParingEnd(PAIRING_FAIL_REASON_CONN_DISCONNECT);
	}

	blc_smp_setCertTimeoutTick(0);  //clear the Security Manager Timer, must


	blt_att_resetEffectiveMtuSize(connHandle);

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	bltAtt.mtu_exchange_pending = 0;
#endif


	////// OTA //////
	if(host_ota_terminate_cb){
		host_ota_terminate_cb(connHandle);
	}

	return 0;
}





void blt_process_pendingPkt(u16 connHandle)
{
	rf_packet_l2cap_t *l2cap_pkt = (rf_packet_l2cap_t*)(bltAtt.pPendingPkt);
	if(l2cap_pkt->chanId ==L2CAP_CID_ATTR_PROTOCOL)
	{
		ble_sts_t ret =  blc_l2cap_pushData_2_controller (connHandle | HANDLE_STK_FLAG, l2cap_pkt->chanId, &l2cap_pkt->opcode, 1, l2cap_pkt->data, l2cap_pkt->l2capLen-1);
		if((ret == BLE_SUCCESS) || (ret == LL_ERR_CONNECTION_NOT_ESTABLISH))
		{
			bltAtt.pPendingPkt = NULL;
			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~BLT_ATT_PKT_PENDING; //pending packet has been completed and clear the relevant bit.
			#endif
		}
	}
}


int blc_gap_peripheral_mainloop(void)
{

	if(para_upReq.connParaUpReq_pending){
		blt_update_parameter_request ();
	}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	if(bltAtt.mtu_exchange_pending){
		blt_att_sendMtuRequest(BLS_CONN_HANDLE);
	}
#endif


	if(bltAtt.pPendingPkt)
	{
		blt_process_pendingPkt(BLS_CONN_HANDLE);
	}

	if (blc_smpMng.security_level != No_Authentication_No_Encryption)
	{

		if(blc_SecReq_ctrl.secReq_pending || blc_smpMng.paring_busy){
			bls_smp_peripheral_paring_loop();
		}



		//in case that smp informations exceed one flash sector, so if not flash media, no need do this
		#if (SMP_DATABASE_INFO_SOURCE == SMP_INFO_STORAGE_IN_FLASH)
				if( bond_device_flash_cfg_idx >= SMP_PARAM_LOOP_CLEAR_MAGIN_ADDR )
				{
					if(blc_ll_getCurrentState() != BLS_LINK_STATE_CONN)
					{
						#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
							if(mlDevMng.mldev_en)
							{
								bls_smp_multi_device_param_Cleanflash();
							}
							else
						#endif
							{
								bls_smp_param_Cleanflash ();
							}
					}
				}
		#endif

	}


	////// OTA //////
	if(host_ota_main_loop_cb){
		host_ota_main_loop_cb();
	}


	return 0;
}





void blc_gap_peripheral_init(void)
{

	blc_ll_registerConnectionCompleteHandler (blc_gap_peripheral_conn_complete_handler);
	blc_ll_registerConnectionTerminateHandler(blc_gap_peripheral_conn_terminate_handler);
	blc_ll_registerConnectionEncryptionDoneCallback(bls_smp_encryption_done);

#if(LL_PAUSE_ENC_FIX_EN)
	blc_ll_registerConnectionEncryptionPauseCallback(bls_smp_encryption_pause);
#endif

	blc_ll_registerHostMainloopCallback(blc_gap_peripheral_mainloop);

	blt_att_resetEffectiveMtuSize(BLS_CONN_HANDLE); //dft: 23
	blt_att_resetRxMtuSize(BLS_CONN_HANDLE); 			//dft: 23
	

	//for 512K Flash, SMP_PARAM_NV_ADDR_START equals to 0x74000(kite/vulture)
	//for 1M  Flash, SMP_PARAM_NV_ADDR_START equals to 0xFC000 (kite/vulture/eagle)
	//for 2M  Flash, SMP_PARAM_NV_ADDR_START equals to 0x1FC000(eagle)
	int flash_cap = flash_get_capacity();
	if(flash_cap == FLASH_SIZE_512K){      ///just kite/vulture;
		bls_smp_configParingSecurityInfoStorageAddr(0x74000);
	}
	else if(flash_cap == FLASH_SIZE_1M){   ///kite/vulture/eagle
		bls_smp_configParingSecurityInfoStorageAddr(0xFC000);
	}
	else if(flash_cap == FLASH_SIZE_2M){   ///just eagle
		bls_smp_configParingSecurityInfoStorageAddr(0x1FC000);
	}
	else if(flash_cap == FLASH_SIZE_4M){
		bls_smp_configParingSecurityInfoStorageAddr(0x3FC000);
	}
}









/*	This code in RF irq and system irq put in RAM by force
 * Because of the flash resource contention problem, when the
 * flash access is interrupted by a higher priority interrupt,
 * the interrupt processing function cannot operate the flash
*/
#if (STACK_IRQ_CODE_IN_SRAM_DUE_TO_FLASH_OPERATION)
_attribute_ram_code_
#endif
int blc_gap_central_conn_complete_handler(u16 connHandle, u8 *p)
{

	#if(DATA_NO_INIT_EN)
		blt_buff_process_pending = 0;   //clear. can enter deep/deepRetention
	#endif

	if (func_smp_init)
	{
		func_smp_init (connHandle, p);
	}


	return 0;
}



//TODO: gap central conn_terminate handler  (	blt_att_resetEffectiveMtuSize() )  NOTE: add this in blm_host.c   disconnect callBack

static u32 blm_idle_tick = 0;
int blc_gap_central_mainloop(void)
{

	if(bltAtt.pPendingPkt)
	{
		blt_process_pendingPkt(BLM_CONN_HANDLE);
	}

	if (blc_smpMng.security_level != No_Authentication_No_Encryption)
	{

#if 1
		extern smp_trigger_cb_t		func_smp_trigger;
		extern u32 master_connecting_tick_flag;
		if(func_smp_trigger && master_connecting_tick_flag && clock_time_exceed(master_connecting_tick_flag, 30000)){
			master_connecting_tick_flag = 0;
			func_smp_trigger(BLM_CONN_HANDLE);
		}
#endif


		//in case that smp informations exceed one flash sector, so if not flash media, no need do this


		if( bltParam.blt_state &  (BLS_LINK_STATE_CONN | BLS_LINK_STATE_INIT) ){
			blm_idle_tick = clock_time() | 1;
		}
		else{
		}

		if(blm_idle_tick && clock_time_exceed(blm_idle_tick, 1000000)){
			blm_idle_tick = clock_time();
			bond_slave_flash_clean();
		}
	}





	return 0;

}



void blc_gap_central_init(void)
{
	blc_ll_registerConnectionCompleteHandler(blc_gap_central_conn_complete_handler);

	blc_ll_registerHostMainloopCallback(blc_gap_central_mainloop);
	
	blt_att_resetEffectiveMtuSize(BLS_CONN_HANDLE); //dft: 23
	blt_att_resetRxMtuSize(BLS_CONN_HANDLE); 		//dft: 23
}









