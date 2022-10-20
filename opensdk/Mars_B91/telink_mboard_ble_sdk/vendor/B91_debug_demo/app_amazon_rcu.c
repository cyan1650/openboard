/********************************************************************************************************
 * @file	app_amazon_rcu.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
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
#include "drivers.h"
#include "tl_common.h"
#include "stack/ble/ble.h"

#include "application/keyboard/keyboard.h"

#include "vendor/common/blt_led.h"
#include "vendor/common/blt_soft_timer.h"
#include "vendor/common/blt_common.h"

#include "app_config.h"
#include "app_buffer.h"
#include "app_ui.h"

#if (TEST_MODE == AMAZON_RCU_TEST)

#define 	ADV_IDLE_ENTER_DEEP_TIME			60  //60 s
#define 	CONN_IDLE_ENTER_DEEP_TIME			60  //60 s


#if AMAZON_PARAMETER_ENABLE
#define 	MY_DIRECT_ADV_TMIE					1280000
#define 	MY_DIRECT_ADV_COUNT_SHORT			1// Actual 1 * 1.28 s = 1.28 s
#define 	MY_DIRECT_ADV_COUNT_MEDIUM			2// Actual 2 * 1.28 s = 2.56 s
#define 	MY_DIRECT_ADV_COUNT_LONG			4// Actual 4 * 1.28 s = 5.12 s

#define     MY_APP_ADV_CHANNEL					BLT_ENABLE_ADV_ALL
#define 	MY_ADV_INTERVAL_MIN					ADV_INTERVAL_20MS
#define 	MY_ADV_INTERVAL_MAX					ADV_INTERVAL_20MS
#else
#define 	MY_DIRECT_ADV_TMIE					2000000

#define     MY_APP_ADV_CHANNEL					BLT_ENABLE_ADV_ALL
#define 	MY_ADV_INTERVAL_MIN					ADV_INTERVAL_30MS
#define 	MY_ADV_INTERVAL_MAX					ADV_INTERVAL_35MS
#endif


#define		MY_RF_POWER_INDEX					RF_POWER_INDEX_P2p79dBm

#define		BLE_DEVICE_ADDRESS_TYPE 			BLE_DEVICE_ADDRESS_PUBLIC

_attribute_data_retention_	own_addr_type_t 	app_own_address_type = OWN_ADDRESS_PUBLIC;


//////////////////////////////////////////////////////////////////////////////
//	 Adv Packet, Response Packet
//////////////////////////////////////////////////////////////////////////////
#if AMAZON_PARAMETER_ENABLE
const u8	tbl_advData[] = {
	 0x02, 0x01, 0x05, 							// BLE limited discoverable mode and BR/EDR not supported
	 0x03, 0x03, 0x12, 0x18,					// incomplete list of service class UUIDs (0x1812)
	 0x03, 0x19, 0x80, 0x01, 					// 384, Generic Remote Control, Generic category
	 0x03, 0x09, 'A', 'R',
	 0x05, 0xFF, 0x71, 0x01, 0x04, 0x13,
};

const u8	tbl_scanRsp [] = {
//		 0x03, 0x09, 'A', 'R',
	};
#else
const u8	tbl_advData[] = {
	 0x05, 0x09, 'e', 'h', 'i', 'd',
	 0x02, 0x01, 0x05, 							// BLE limited discoverable mode and BR/EDR not supported
	 0x03, 0x19, 0x80, 0x01, 					// 384, Generic Remote Control, Generic category
	 0x05, 0x02, 0x12, 0x18, 0x0F, 0x18,		// incomplete list of service class UUIDs (0x1812, 0x180F)
};

const u8	tbl_scanRsp [] = {
		 0x08, 0x09, 'E', 'a', 'g', 'l', 'e', 'r', 'c',
	};
#endif


_attribute_data_retention_	int device_in_connection_state;

_attribute_data_retention_	u32 advertise_begin_tick;

_attribute_data_retention_	u32	interval_update_tick;

_attribute_data_retention_	u8	sendTerminate_before_enterDeep = 0;

_attribute_data_retention_	u32	latest_user_event_tick;

_attribute_data_retention_	u32	lowBattDet_tick   = 0;

_attribute_data_retention_	static u8	getCurrentState_old = 0xAA;

_attribute_data_retention_	static u32	idle_state_enter_suspend_tick = 0;


u8				app_switch_to_direct_adv_count = 0;


//////////////////// function ///////////////////////
extern void	terminateConn_factory_reset(void);
extern void	terminateConn_advertisingInConnSlave(void);
extern void	establishConn_advertisingInDisconnPaired(void);




#if (UI_KEYBOARD_ENABLE)


	void proc_keyboard (u8 e, u8 *p, int n);

#elif (UI_BUTTON_ENABLE)

	extern int button_not_released;
	extern void proc_button (u8 e, u8 *p, int n);

#endif



void 	app_switch_to_indirect_adv(u8 e, u8 *p, int n)
{
#if AMAZON_PARAMETER_ENABLE
///////////////////// USER application initialization ///////////////////
	bls_ll_setAdvData( (u8 *)tbl_advData, sizeof(tbl_advData) );
	bls_ll_setScanRspData( (u8 *)tbl_scanRsp, sizeof(tbl_scanRsp));

	if(app_switch_to_direct_adv_count > 1)
	{
		app_switch_to_direct_adv_count--;

		u8 bond_number = blc_smp_param_getCurrentBondingDeviceNumber();  //get bonded device number
		smp_param_save_t  bondInfo;
		if(bond_number)   //at least 1 bonding device exist
		{
			bls_smp_param_loadByIndex( bond_number - 1, &bondInfo);  //get the latest bonding device (index: bond_number-1 )
		}

		if(bond_number)   //set direct adv
		{
			//set direct adv
			u8 status = bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
											ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY, app_own_address_type,
											bondInfo.peer_addr_type,  bondInfo.peer_addr,
											MY_APP_ADV_CHANNEL,
											ADV_FP_NONE);
			if(status != BLE_SUCCESS) {;}  //debug: adv setting err

			//it is recommended that direct adv only last for 1.28 seconds, then switch to app_switch_to_indirect_adv
			bls_app_registerEventCallback (BLT_EV_FLAG_ADV_DURATION_TIMEOUT, &app_switch_to_indirect_adv);
		}
		else
		{
			app_switch_to_direct_adv_count = 0;

			bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
								ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type,
								0,  NULL,
								MY_APP_ADV_CHANNEL,
								ADV_FP_NONE);
		}
	}
	else
	{
		app_switch_to_direct_adv_count = 0;

		bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
							ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type,
							0,  NULL,
							MY_APP_ADV_CHANNEL,
							ADV_FP_NONE);
	}

	bls_ll_setAdvEnable(BLC_ADV_ENABLE);  //must: set adv enable
#else
	bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
						ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type,
						0,  NULL,
						MY_APP_ADV_CHANNEL,
						ADV_FP_NONE);

	bls_ll_setAdvEnable(BLC_ADV_ENABLE);  //must: set adv enable

#endif
}


void triggerAdvertisingInConnSlaveRole(void)
{
	blc_ll_addAdvertisingInConnSlaveRole();  //adv in conn slave role
	blc_ll_setAdvParamInConnSlaveRole(  (u8 *)tbl_advData, sizeof(tbl_advData), \
										(u8 *)tbl_scanRsp, sizeof(tbl_scanRsp), \
										ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type, MY_APP_ADV_CHANNEL, ADV_FP_NONE);

	bls_ll_setAdvEnable(BLC_ADV_ENABLE);  //must: set adv enable
}


void triggerAdvertisingInDisconnPaired(void)
{
///////////////////// USER application initialization ///////////////////
	bls_ll_setAdvData( (u8 *)tbl_advData, sizeof(tbl_advData) );
	bls_ll_setScanRspData( (u8 *)tbl_scanRsp, sizeof(tbl_scanRsp));

	bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
						ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type,
						0,  NULL,
						MY_APP_ADV_CHANNEL,
						ADV_FP_NONE);

	advertise_begin_tick = clock_time();
	bls_ll_setAdvEnable(BLC_ADV_ENABLE);  //must: set adv enable

	/*triggerAdvertisingInDisconnPaired*/
}


_attribute_ram_code_ void	task_suspend_exit (u8 e, u8 *p, int n)
{
	rf_set_power_level_index (MY_RF_POWER_INDEX);
}


void 	ble_remote_terminate(u8 e,u8 *p, int n) //*p is terminate reason
{
	device_in_connection_state = 0;


	if(*p == HCI_ERR_CONN_TIMEOUT){

	}
	else if(*p == HCI_ERR_REMOTE_USER_TERM_CONN){  //0x13

	}
	else if(*p == HCI_ERR_CONN_TERM_MIC_FAILURE){

	}
	else{

	}



#if (BLE_REMOTE_PM_ENABLE)
	 //user has push terminate pkt to ble TX buffer before deepsleep
	if(sendTerminate_before_enterDeep == 1){
		sendTerminate_before_enterDeep = 2;
	}
#endif


#if (BLE_AUDIO_ENABLE)
	if(ui_mic_enable){
		ui_enable_mic (0);
	}
#endif

#if (UI_LED_ENABLE)
	gpio_write(GPIO_LED_RED, !LED_ON_LEVAL);  //yellow light off
#endif

	terminateConn_factory_reset();


	terminateConn_advertisingInConnSlave();


//	advertise_begin_tick = clock_time();

}


void	task_connect (u8 e, u8 *p, int n)
{
#if AMAZON_PARAMETER_ENABLE
	bls_l2cap_requestConnParamUpdate (16, 16, 49, 800);  //interval=20ms latency=49 timeout=8s
#else
	bls_l2cap_requestConnParamUpdate (8, 8, 99, 400);  //interval=10ms latency=99 timeout=4s
#endif
	bls_l2cap_setMinimalUpdateReqSendingTime_after_connCreate(1000);


	latest_user_event_tick = clock_time();

	ui_mtu_size_exchange_req = 1;

	device_in_connection_state = 1;//

#if (UI_LED_ENABLE)
	gpio_write(GPIO_LED_RED, LED_ON_LEVAL);
#endif

	interval_update_tick = clock_time() | 1; //none zero
	
	bls_ll_setAdvEnable(BLC_ADV_DISABLE);   //disable Advertising, amazon

	establishConn_advertisingInDisconnPaired();



}


void	task_conn_update_req (u8 e, u8 *p, int n)
{

}


void	task_conn_update_done (u8 e, u8 *p, int n)
{

}


int app_conn_param_update_response(u8 id, u16  result)
{
	if(result == CONN_PARAM_UPDATE_ACCEPT){

	}
	else if(result == CONN_PARAM_UPDATE_REJECT){

	}

	return 0;
}


_attribute_ram_code_
void blt_pm_proc(void)
{
#if(BLE_REMOTE_PM_ENABLE)
	if(ui_mic_enable || advertisingInConnSlave_flag)
	{
		bls_pm_setSuspendMask (SUSPEND_DISABLE);
	}
	#if(REMOTE_IR_ENABLE)
		else if( ir_send_ctrl.is_sending){
			bls_pm_setSuspendMask(SUSPEND_DISABLE);
		}
	#endif
	#if (BLE_PHYTEST_MODE != PHYTEST_MODE_DISABLE)
		else if( blc_phy_isPhyTestEnable() )
		{
			bls_pm_setSuspendMask(SUSPEND_DISABLE);  //phy test can not enter suspend
		}
	#endif
	else
	{
		if((blc_ll_getCurrentState() == BLS_LINK_STATE_IDLE) && (getCurrentState_old != BLS_LINK_STATE_IDLE))
		{
			idle_state_enter_suspend_tick = clock_time()|1;
		}
		getCurrentState_old = blc_ll_getCurrentState();

		if ((blc_ll_getCurrentState() == BLS_LINK_STATE_IDLE) && (!sendTerminate_before_enterDeep))// && (!DEVICE_LED_BUSY))
		{// IDLE State, Enter into suspend
			if(DEVICE_LED_BUSY)
			{
				bls_pm_setWakeupSource(PM_WAKEUP_TIMER);  // PM_WAKEUP_TIMER wakeup suspend
				cpu_sleep_wakeup(SUSPEND_MODE, PM_WAKEUP_TIMER, clock_time() + 100 * SYSTEM_TIMER_TICK_1MS); // suspend

			}
			else if(idle_state_enter_suspend_tick && clock_time_exceed(idle_state_enter_suspend_tick, Timing_CLOCK_500mS))
			{
				idle_state_enter_suspend_tick = 0;
				/*IDLE_Enter_DEEPSLEEP*/
				bls_pm_setWakeupSource(PM_WAKEUP_PAD);  //gpio pad wakeup suspend/deepsleep
				cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);  //deepsleep
			}
		}
		else
		{
			#if (PM_DEEPSLEEP_RETENTION_ENABLE)
				bls_pm_setSuspendMask (SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
			#else
				bls_pm_setSuspendMask (SUSPEND_ADV | SUSPEND_CONN);
			#endif

			int user_task_flg = ota_is_working || scan_pin_need || key_not_released || DEVICE_LED_BUSY;

			if(user_task_flg){

				bls_pm_setSuspendMask (SUSPEND_ADV | SUSPEND_CONN);

				#if (LONG_PRESS_KEY_POWER_OPTIMIZE)
					extern int key_matrix_same_as_last_cnt;
					if(!ota_is_working && key_matrix_same_as_last_cnt > 5){  //key matrix stable can optize
						bls_pm_setManualLatency(3);
					}
					else{
						bls_pm_setManualLatency(0);  //latency off: 0
					}
				#else
					bls_pm_setManualLatency(0);
				#endif
			}

			#if 1 //deepsleep
			if(sendTerminate_before_enterDeep == 1){ //sending Terminate and wait for ack before enter deepsleep
				if(user_task_flg){  //detect key Press again,  can not enter deep now
					sendTerminate_before_enterDeep = 0;
					bls_ll_setAdvEnable(BLC_ADV_ENABLE);   //enable adv again
				}
			}
			else if(sendTerminate_before_enterDeep == 2){  //Terminate OK
				analog_write_reg8(USED_DEEP_ANA_REG, analog_read_reg8(USED_DEEP_ANA_REG) | CONN_DEEP_FLG);
				cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);  //deepsleep
			}

			if(  !blc_ll_isControllerEventPending() ){  //no controller event pending
				//adv 60s, deepsleep
				if( blc_ll_getCurrentState() == BLS_LINK_STATE_ADV && !sendTerminate_before_enterDeep && \
					clock_time_exceed(advertise_begin_tick , ADV_IDLE_ENTER_DEEP_TIME * 1000000))
				{
					/*ADV_Timeout_Enter_DEEPSLEEP*/
					cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);  //deepsleep
				}
				#if BLE_LONG_CONNECT_ENTER_DEEPSLEEP_ENABLE
				//conn 60s no event(key/voice/led), enter deepsleep
				else if( device_in_connection_state && !user_task_flg && \
						clock_time_exceed(latest_user_event_tick, CONN_IDLE_ENTER_DEEP_TIME * 1000000) )
				{
					bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN); //push terminate cmd into ble TX buffer
					bls_ll_setAdvEnable(BLC_ADV_DISABLE);   //disable adv
					sendTerminate_before_enterDeep = 1;
				}
				#endif
			}
			#endif
		}
	}

#endif  //END of  BLE_REMOTE_PM_ENABLE
}



_attribute_ram_code_ void  ble_remote_set_sleep_wakeup (u8 e, u8 *p, int n)
{
	if( blc_ll_getCurrentState() == BLS_LINK_STATE_CONN && ((u32)(bls_pm_getSystemWakeupTick() - clock_time())) > 80 * SYSTEM_TIMER_TICK_1MS){  //suspend time > 30ms.add gpio wakeup
		bls_pm_setWakeupSource(PM_WAKEUP_PAD);  //gpio pad wakeup suspend/deepsleep
	}
}


void user_init_normal()
{
	//random number generator must be initiated here( in the beginning of user_init_nromal)
	//when deepSleep retention wakeUp, no need initialize again
	random_generator_init();

	////////////////// BLE stack initialization ////////////////////////////////////
	u8  mac_public[6];
	u8  mac_random_static[6];

	//for 1M   Flash, flash_sector_mac_address equals to 0xFF000
	blc_initMacAddress(flash_sector_mac_address, mac_public, mac_random_static);


#if(BLE_DEVICE_ADDRESS_TYPE == BLE_DEVICE_ADDRESS_PUBLIC)
	app_own_address_type = OWN_ADDRESS_PUBLIC;
#elif(BLE_DEVICE_ADDRESS_TYPE == BLE_DEVICE_ADDRESS_RANDOM_STATIC)
	app_own_address_type = OWN_ADDRESS_RANDOM;
	blc_ll_setRandomAddr(mac_random_static);
#endif

	////// Controller Initialization  //////////
	blc_ll_initBasicMCU();                      //mandatory
	blc_ll_initStandby_module(mac_public);				//mandatory
	blc_ll_initLegacyAdvertising_module(); 		//legacy advertising module: mandatory for BLE slave
	blc_ll_initConnection_module();				//connection module  mandatory for BLE slave/master
	blc_ll_initSlaveRole_module();				//slave module: 	 mandatory for BLE slave,
//	blc_ll_initPowerManagement_module();        //pm module:      	 optional

	blc_ll_setAclConnMaxOctetsNumber(ACL_CONN_MAX_RX_OCTETS, ACL_CONN_MAX_TX_OCTETS);

	blc_ll_initAclConnTxFifo(app_acl_txfifo, ACL_TX_FIFO_SIZE, ACL_TX_FIFO_NUM);
	blc_ll_initAclConnRxFifo(app_acl_rxfifo, ACL_RX_FIFO_SIZE, ACL_RX_FIFO_NUM);

	u8 check_status = blc_controller_check_appBufferInitialization();
	if(check_status != BLE_SUCCESS){
		/* here user should set some log to know which application buffer incorrect */
		write_log32(0x88880000 | check_status);
		while(1);
	}

	////// Host Initialization  //////////
	blc_gap_peripheral_init();    //gap initialization
	extern void my_att_init ();
	my_att_init (); //gatt initialization
	blc_l2cap_register_handler (blc_l2cap_packet_receive);  	//l2cap initialization

	//Smp Initialization may involve flash write/erase(when one sector stores too much information,
	//   is about to exceed the sector threshold, this sector must be erased, and all useful information
	//   should re_stored) , so it must be done after battery check
	#if (BLE_REMOTE_SECURITY_ENABLE)
		blc_smp_peripheral_init();

		blc_smp_param_setBondingDeviceMaxNumber(1);

		// Hid device on android7.0/7.1 or later version
		// New paring: send security_request immediately after connection complete
		// reConnect:  send security_request 1000mS after connection complete. If master start paring or encryption before 1000mS timeout, slave do not send security_request.
		blc_smp_configSecurityRequestSending(SecReq_IMM_SEND, SecReq_PEND_SEND, 1000); //if not set, default is:  send "security request" immediately after link layer connection established(regardless of new connection or reconnection)
	#else
		blc_smp_setSecurityLevel(No_Security);
	#endif

	////////////////// config adv packet /////////////////////
	#if (BLE_REMOTE_SECURITY_ENABLE)
		u8 bond_number = blc_smp_param_getCurrentBondingDeviceNumber();  //get bonded device number
		smp_param_save_t  bondInfo;
		if(bond_number)   //at least 1 bonding device exist
		{
			bls_smp_param_loadByIndex( bond_number - 1, &bondInfo);  //get the latest bonding device (index: bond_number-1 )

		}

		if(bond_number)   //set direct adv
		{
			//set direct adv
			u8 status = bls_ll_setAdvParam( MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
											ADV_TYPE_CONNECTABLE_DIRECTED_HIGH_DUTY, app_own_address_type,
											bondInfo.peer_addr_type,  bondInfo.peer_addr,
											MY_APP_ADV_CHANNEL,
											ADV_FP_NONE);
			if(status != BLE_SUCCESS) {}  //debug: adv setting err

			//it is recommended that direct adv only last for several seconds, then switch to indirect adv
//			bls_ll_setAdvDuration(MY_DIRECT_ADV_TMIE, 1);
			app_switch_to_direct_adv_count = MY_DIRECT_ADV_COUNT_MEDIUM;
			bls_app_registerEventCallback (BLT_EV_FLAG_ADV_DURATION_TIMEOUT, &app_switch_to_indirect_adv);

		}
		else   //set indirect adv
	#endif
		{
			u8 status = bls_ll_setAdvParam(  MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
											 ADV_TYPE_CONNECTABLE_UNDIRECTED, app_own_address_type,
											 0,  NULL,
											 MY_APP_ADV_CHANNEL,
											 ADV_FP_NONE);
			if(status != BLE_SUCCESS) {}  //debug: adv setting err
		}



	///////////////////// USER application initialization ///////////////////
	bls_ll_setAdvData( (u8 *)tbl_advData, sizeof(tbl_advData) );
	bls_ll_setScanRspData( (u8 *)tbl_scanRsp, sizeof(tbl_scanRsp));

	bls_ll_setAdvEnable(BLC_ADV_ENABLE);  //adv enable

	//set rf power index, user must set it after every suspend wakeup, cause relative setting will be reset in suspend
	rf_set_power_level_index (MY_RF_POWER_INDEX);
	bls_app_registerEventCallback (BLT_EV_FLAG_SUSPEND_EXIT, &task_suspend_exit);

	//ble event call back
	bls_app_registerEventCallback (BLT_EV_FLAG_CONNECT, &task_connect);
	bls_app_registerEventCallback (BLT_EV_FLAG_TERMINATE, &ble_remote_terminate);
	bls_app_registerEventCallback (BLT_EV_FLAG_CONN_PARA_REQ, &task_conn_update_req);
	bls_app_registerEventCallback (BLT_EV_FLAG_CONN_PARA_UPDATE, &task_conn_update_done);

	blc_l2cap_registerConnUpdateRspCb(app_conn_param_update_response);

	///////////////////// Power Management initialization///////////////////
	#if(BLE_REMOTE_PM_ENABLE)
		blc_ll_initPowerManagement_module();

		#if (PM_DEEPSLEEP_RETENTION_ENABLE)
			bls_pm_setSuspendMask (SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
			blc_pm_setDeepsleepRetentionThreshold(50, 30);
			blc_pm_setDeepsleepRetentionEarlyWakeupTiming(400);
		#else
			bls_pm_setSuspendMask (SUSPEND_ADV | SUSPEND_CONN);
		#endif

		bls_app_registerEventCallback (BLT_EV_FLAG_SUSPEND_ENTER, &ble_remote_set_sleep_wakeup);
	#else
		bls_pm_setSuspendMask (SUSPEND_DISABLE);
	#endif

#if (BLE_REMOTE_OTA_ENABLE)
	////////////////// OTA relative ////////////////////////
	bls_ota_clearNewFwDataArea(); //must
	bls_ota_registerStartCmdCb(app_enter_ota_mode);
	//bls_ota_registerResultIndicateCb(app_debug_ota_result);  //debug
#endif

	app_ui_init_normal();

	advertise_begin_tick = clock_time();
}


_attribute_ram_code_ void user_init_deepRetn(void)
{
#if (PM_DEEPSLEEP_RETENTION_ENABLE)

	blc_ll_initBasicMCU();   //mandatory
	rf_set_power_level_index (MY_RF_POWER_INDEX);

	blc_ll_recoverDeepRetention();

	DBG_CHN0_HIGH;    //debug

	irq_enable();

	#if (UI_KEYBOARD_ENABLE)
		/////////// keyboard gpio wakeup init ////////
		u32 pin[] = KB_DRIVE_PINS;
		for (int i=0; i<(sizeof (pin)/sizeof(*pin)); i++)
		{
			cpu_set_gpio_wakeup (pin[i], Level_High,1);  //drive pin pad high wakeup deepsleep
		}
	#endif
	
	app_ui_init_deepRetn();
#endif
}


/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
void main_loop (void)
{
	////////////////////////////////////// BLE entry /////////////////////////////////
	blt_sdk_main_loop();


	////////////////////////////////////// UI entry /////////////////////////////////
	static u32 t_sync = 0;

	if(clock_time_exceed(t_sync, 200000))
	{
		t_sync = clock_time();
//		gpio_toggle(GPIO_LED_WHITE);
	}


	#if (BLT_TEST_SOFT_TIMER_ENABLE)
	blt_soft_timer_process(MAINLOOP_ENTRY);
	#endif

	#if (BLE_AUDIO_ENABLE)
		//blc_checkConnParamUpdate();
		if(ui_mic_enable){
			task_audio();
		}
	#endif

	#if (BATT_CHECK_ENABLE)
		if(battery_get_detect_enable() && clock_time_exceed(lowBattDet_tick, 500000) ){
			lowBattDet_tick = clock_time();
			app_battery_power_check(VBAT_ALRAM_THRES_MV);  //2000 mV low battery
		}
	#endif


	#if (UI_KEYBOARD_ENABLE)
	proc_keyboard (0, 0, 0);
	#endif


	#if (BLT_APP_LED_ENABLE)
		device_led_process();
	#endif

	ui_loop();

	////////////////////////////////////// PM Process /////////////////////////////////
	blt_pm_proc();
}

#endif //end of  (TEST_MODE == AMAZON_RCU_TEST)


