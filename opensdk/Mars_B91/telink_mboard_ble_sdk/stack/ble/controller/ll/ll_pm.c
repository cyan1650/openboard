/********************************************************************************************************
 * @file	ll_pm.c
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



_attribute_data_retention_ u32 blt_next_event_tick;

#if(DATA_NO_INIT_EN)
_attribute_data_retention_ u8  blt_buff_process_pending = 0; //prepare write & l2cap rx buff & l2cap tx buff
#endif


extern blt_event_callback_t		blt_p_event_callback ;


extern _attribute_aligned_(4) st_ll_conn_slave_t bltc;

_attribute_data_retention_ _attribute_aligned_(4) st_ll_pm_t  bltPm;

void blc_pm_proc_no_suspend(void);

void	blt_brx_sleep (void);

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	#if PM_32k_RC_CALIBRATION_ALGORITHM_EN

		_attribute_data_retention_ 	u32 ble_first_rx_tick_last = 0;
		_attribute_data_retention_ 	u32 ble_first_rx_tick_pre = 0;
		_attribute_data_retention_ 	u32 ble_actual_conn_interval_tick = 0;

	#endif
#endif

///// pm module ////
_attribute_data_retention_	ll_module_pm_callback_t  ll_module_pm_cb = NULL;

void blc_ll_initPowerManagement_module(void)
{
	ll_module_pm_cb = blt_brx_sleep;

	bltPm.user_latency = 0xffff;

	bltPm.deepRet_advThresTick = bltPm.deepRet_connThresTick = 100 * SYSTEM_TIMER_TICK_1MS;

	bltPm.deepRet_earlyWakeupTick = 500 * SYSTEM_TIMER_TICK_1US;
	
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	bltPm.deepRet_type = (u8)DEEPSLEEP_MODE_RET_SRAM_LOW16K;
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
	#if(FREERTOS_ENABLE)
		bltPm.deepRet_type = (u8)(DEEPSLEEP_MODE_RET_SRAM_LOW64K);
	#else
		bltPm.deepRet_type = (u8)(DEEPSLEEP_MODE_RET_SRAM_LOW32K);
	#endif
#endif

}









void	bls_pm_setWakeupSource (u8 src)
{
	bltPm.wakeup_src = src;
}

void bls_pm_setManualLatency(u16 latency)
{
	bltPm.user_latency = latency;
}

u32 bls_pm_getSystemWakeupTick(void)
{
	return	bltPm.current_wakeup_tick;
}

u32 bls_pm_getNexteventWakeupTick(void)
{
	return	blt_next_event_tick;
}


/////////////////////////////////////////////
void 	bls_pm_setSuspendMask (u8 mask)
{
	bltPm.suspend_mask = mask;
}

u8 	bls_pm_getSuspendMask (void)
{
	return bltPm.suspend_mask;
}






void bls_pm_procGpioEarlyWakeup(u16 en, u8 *wakeup_src)
{

	blt_p_event_callback (BLT_EV_FLAG_GPIO_EARLY_WAKEUP, wakeup_src, 1);
	bltPm.padWakeupCnt ++;

	if(bltParam.blt_state == BLS_LINK_STATE_CONN){  //conn state
		u32 tn = blt_next_event_tick - clock_time () - 3500 * SYSTEM_TIMER_TICK_1US;
		while ((u32)(tn - bltc.conn_interval) < BIT(30))
		{
			blt_next_event_tick -= bltc.conn_interval;

		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			if(blt_miscParam.pad32k_en)
			{
				if (en) {
					blt_next_event_tick -= bltc.conn_interval_adjust ;
				}
			}
			else
			{
				ble_actual_conn_interval_tick -= bltc.conn_interval;
			}
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			if (en) {
				blt_next_event_tick -= bltc.conn_interval_adjust ;
			}
		#endif

			tn -= bltc.conn_interval;

			if(blttcon.chn_idx){
				blttcon.chn_idx--;
			}
			else{  // 0->36
				blttcon.chn_idx = 36;
			}
			blttcon.conn_inst--;
		}

		systimer_set_irq_capture(blt_next_event_tick);//need check
		bltc.connExpectTime = blt_next_event_tick + bltc.conn_tolerance_time;
	}

}



void blc_pm_procGpioPadEarlyWakeup(u16 en, u8 *wakeup_src)
{

	blt_p_event_callback (BLT_EV_FLAG_GPIO_EARLY_WAKEUP, wakeup_src, 1);
	bltPm.padWakeupCnt ++;

	if(bltParam.blt_state == BLS_LINK_STATE_CONN){  //conn state
		u32 tn = blt_next_event_tick - clock_time () - 3500 * SYSTEM_TIMER_TICK_1US;
		while ((u32)(tn - bltc.conn_interval) < BIT(30))
		{
			blt_next_event_tick -= bltc.conn_interval;
			tn -= bltc.conn_interval;
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			if(blt_miscParam.pad32k_en)
			{
				if (en) {
					blt_next_event_tick -= bltc.conn_interval_adjust ;
				}
			}
			else
			{
				ble_actual_conn_interval_tick -= bltc.conn_interval;
			}
		#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
			if (en) {
				blt_next_event_tick -= bltc.conn_interval_adjust ;
			}
		#endif

			if(blttcon.chn_idx){
				blttcon.chn_idx--;
			}
			else{  // 0->36
				blttcon.chn_idx = 36;
			}
			blttcon.conn_inst--;
		}

		bltc.connExpectTime = blt_next_event_tick + bltc.conn_tolerance_time;
	}

}



#if (BLS_USER_TIMER_WAKEUP_ENABLE)

	_attribute_data_retention_	pm_appWakeupLowPower_callback_t  pm_appWakeupLowPowerCb = NULL;

	//#define	(SYSTEM_TIMER_TICK_1US<<23)
	void bls_pm_setAppWakeupLowPower(u32 wakeup_tick, u8 enable)
	{
		bltPm.appWakeup_tick = wakeup_tick;
		bltPm.appWakeup_en = enable;
	}

	void bls_pm_registerAppWakeupLowPowerCb(pm_appWakeupLowPower_callback_t cb)
	{
		pm_appWakeupLowPowerCb = cb;
	}


	void bls_appWakeupLowPower_procSuspendConn(void)
	{
		for(int i=0; i<bltPm.valid_latency; i++){
			if( (u32)(bltPm.appWakeup_tick - blt_next_event_tick) < BIT(30) ){
				blt_next_event_tick += bltc.conn_interval;
				bltPm.latency_use ++;
			}
			else{
				bltPm.appWakeup_flg = 1;
				break;
			}
		}
	}
#endif



void blc_pm_setDeepsleepRetentionThreshold(u32 adv_thres_ms, u32 conn_thres_ms)
{
	bltPm.deepRet_advThresTick = adv_thres_ms * SYSTEM_TIMER_TICK_1MS;
	bltPm.deepRet_connThresTick = conn_thres_ms * SYSTEM_TIMER_TICK_1MS;
}



void blc_pm_setDeepsleepRetentionEarlyWakeupTiming(u32 earlyWakeup_us)
{
	bltPm.deepRet_earlyWakeupTick = earlyWakeup_us * SYSTEM_TIMER_TICK_1US;
}

void blc_pm_setDeepsleepRetentionType(SleepMode_TypeDef sleep_type)
{
	bltPm.deepRet_type = sleep_type;
}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	#if PM_32k_RC_CALIBRATION_ALGORITHM_EN
		extern u8 	conn_new_interval_flag ;
	#endif
	
	_attribute_data_retention_  u32   conn_rx_offset_limit = 0;
#endif

_attribute_ram_code_ void	blt_brx_sleep (void)
{

	bltPm.pm_border_flag = 0;

#if (BLS_USER_TIMER_WAKEUP_ENABLE)
	if(bltPm.appWakeup_en){
		//  -4S  ~  +1550us
		if( (u32)( clock_time() + 1550 * SYSTEM_TIMER_TICK_1US - bltPm.appWakeup_tick  ) < BIT(26)){  // EMPTYRUN_TIME_US + 50
			return;
		}
	}
#endif


	bltPm.latency_en = 0;
	bltPm.deepRt_en = 0;
	u32 deepRet_thresTick = BIT(29);

	if(bltParam.blt_state == BLS_LINK_STATE_ADV)
	{
		extern u32 blt_advExpectTime;
		blt_next_event_tick = blt_advExpectTime;

		if(bltPm.suspend_mask & DEEPSLEEP_RETENTION_ADV){
			deepRet_thresTick = bltPm.deepRet_advThresTick;
			bltPm.deepRt_en = 1;
//			bltPm.deepRt_en = (deepretn_suspend_switch_flg++)&1;
		}
	}
	else
	{
		blt_next_event_tick = bltc.connExpectTime - bltc.conn_tolerance_time;

		if(bltPm.suspend_mask & DEEPSLEEP_RETENTION_CONN){
			deepRet_thresTick = bltPm.deepRet_connThresTick;
			bltPm.deepRt_en = 1;
//			bltPm.deepRt_en = (deepretn_suspend_switch_flg++)&1;
		}
	}


	u32 current_wakeup_tick;


#if (BLS_USER_TIMER_WAKEUP_ENABLE)
	if( !bltPm.appWakeup_loop_noLatency && (!bltPm.padWakeupCnt))
	{
#else
	if(!bltPm.padWakeupCnt)
	{
#endif

		if (bltParam.blt_state == BLS_LINK_STATE_CONN && (bltPm.suspend_mask & SUSPEND_CONN) )
		{
			////////////////// Calculate real latency use  ////////////////////
			// step 1. sys_latency, maybe 3 case :  0 / conn latency / update latency(rest_interval)
			bltPm.sys_latency = 0;

	#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
			bltPm.no_latency = bltParam.wirte_sonos_flash_req || \
							!bltc.conn_latency || blt_ll_getRealTxFifoNumber() || bltc.tick_1st_rx==0 || \
							!bltc.conn_rcvd_ack_pkt || bltc.conn_terminate_pending || \
						   (blttcon.conn_update && bltc.master_not_ack_slaveAckUpReq);
	#else
			bltPm.no_latency = !bltc.conn_latency || blt_ll_getRealTxFifoNumber() || bltc.tick_1st_rx==0 || \
							!bltc.conn_rcvd_ack_pkt || bltc.conn_terminate_pending || \
						   (blttcon.conn_update && bltc.master_not_ack_slaveAckUpReq);
	#endif


			//IC can't enter deep until data in tx fifo have send over.
//			if(bltPm.deepRt_en && blt_ll_getRealTxFifoNumber()){
//				bltPm.deepRt_en = 0;
//			}


			if(!bltPm.no_latency && blttcon.conn_update){  //update req not processed
				s16 rest_interval = blttcon.conn_inst_next - blttcon.conn_inst - 1;
				if(rest_interval > 0){
					if(rest_interval < bltc.conn_latency){
						bltPm.sys_latency = rest_interval;
					}
				}
				else{
					bltPm.no_latency = 1;
				}
			}


			if(!bltPm.no_latency && !bltPm.sys_latency ){ //
				bltPm.sys_latency = bltc.conn_latency;
			}


			// step 2:  compare sys latency & user latency, use smaller one
			bltPm.valid_latency = min(bltPm.user_latency, bltPm.sys_latency);



			// step 4. compare blt_next_event_tick & user app timer wakeup tick
			bltPm.latency_use = bltPm.valid_latency;



			
			if ( (bltPm.latency_use + 1) * bltc.conn_interval > 100000 * SYSTEM_TIMER_TICK_1US){
				bltPm.latency_en = bltPm.latency_use + 1;
			}

			if(blt_miscParam.pad32k_en)
			{
				bltc.connExpectTime += bltPm.latency_use * bltc.conn_interval + bltc.conn_interval_adjust * bltPm.latency_en;
			}
			else
			{
				if(bltc.tick_1st_rx){
					if(ble_first_rx_tick_last != ble_first_rx_tick_pre){
						ble_first_rx_tick_last = ble_first_rx_tick_pre ;
						int ble_conn_offset_tick = 0;
						if(!conn_new_interval_flag){
							ble_conn_offset_tick = (int)( ble_actual_conn_interval_tick + ble_first_rx_tick_last - bltc.tick_1st_rx);
							//NOte: If the system timer is updated, it may return to a point in time in the past, which may be risky for the application.
//							bltc.connExpectTime += ble_conn_offset_tick;
//							reg_system_tick = reg_system_tick + ble_conn_offset_tick;
//							bltc.tick_1st_rx += ble_conn_offset_tick;

							if(abs(ble_conn_offset_tick) < conn_rx_offset_limit){
								pm_cal_32k_rc_offset (ble_conn_offset_tick);
							}
						}
						ble_actual_conn_interval_tick = 0;
						conn_new_interval_flag = 0;

					}
					ble_first_rx_tick_pre = bltc.tick_1st_rx;
				}

				ble_actual_conn_interval_tick += bltPm.latency_use * bltc.conn_interval;
				bltc.connExpectTime += bltPm.latency_use * bltc.conn_interval;
			}
			blttcon.chn_idx = (blttcon.chn_idx + bltPm.latency_use)%37;
			blttcon.conn_inst += bltPm.latency_use;
			if(blt_miscParam.pad32k_en)
			{
		#if (1)  //process long suspend
				if(bltc.tick_1st_rx){  //synced
					bltc.long_suspend = 0;
				}

				u32 tick_delta = (u32)(bltc.connExpectTime - clock_time());
				if(tick_delta < BIT(30)){
					if( tick_delta > BIT(25) ){ //2096 mS
						bltc.long_suspend = 3;
					}
					else if( tick_delta > BIT(24) ){ //1048 mS
						bltc.long_suspend = 2;
					}
					else if( tick_delta > BIT(23) ){ //16 * 1024 * 512 tick = (1<<23) tick = 524 ms
						bltc.long_suspend = 1;
					}
				}

		#if 1
				if(blt_miscParam.pad32k_en){ // if use external 32k crystal
					if(bltc.long_suspend){

						#if (TRY_FIX_ERR_BY_ADD_BRX_WAIT)//ll_slave.c: must enable MICRO: TRY_FIX_ERR_BY_ADD_BRX_WAIT
							u32 widen_ticks = (bltc.long_suspend - 1) * blc_pm_get_brx_early_time();
						#else
							u32 widen_ticks = (bltc.long_suspend - 1) * 500 * SYSTEM_TIMER_TICK_1US; //us
						#endif

						bltc.conn_tolerance_time += widen_ticks;
						bltc.conn_duration  += (widen_ticks << 1);
					}
				}
				else
		#endif
				{ // if use internal 32k RC
					if(bltc.long_suspend == 3){
						bltc.conn_tolerance_time += 500 * SYSTEM_TIMER_TICK_1US;
						bltc.conn_duration  += 1000 * SYSTEM_TIMER_TICK_1US;
					}
					else if(bltc.long_suspend == 2){
						bltc.conn_tolerance_time += 300 * SYSTEM_TIMER_TICK_1US;
						bltc.conn_duration  += 600 * SYSTEM_TIMER_TICK_1US;
					}
					else if(bltc.long_suspend == 1){
						bltc.conn_tolerance_time += 200 * SYSTEM_TIMER_TICK_1US;
						bltc.conn_duration  += 400 * SYSTEM_TIMER_TICK_1US;
					}
				}
		#if(FREERTOS_ENABLE)
				extern u8 x_freertos_on;
				if( x_freertos_on ){
					bltc.conn_tolerance_time += 100*SYSTEM_TIMER_TICK_1US;
					bltc.conn_duration	+= 200 * SYSTEM_TIMER_TICK_1US;
				}
		#endif
				//system IRQ timing for brx_start should be earlier for 3 problem:
				// 1. timing from irq_handler to start_brx is longer than old SDK, cause code changes a lot for BLE5.0
				// 2. CSA2 cost about 23uS for 16M system clock
				// 3. PHY switch cost about   uS for 16M system clock
		#if (LL_FEATURE_ENABLE_CHANNEL_SELECTION_ALGORITHM2)
				if(blttcon.conn_chnsel)
				{
					bltc.conn_tolerance_time += 20*SYSTEM_TIMER_TICK_1US;
				}
		#endif

		#if (LL_FEATURE_ENABLE_LE_CODED_PHY || LL_FEATURE_ENABLE_LE_2M_PHY)
				if(blt_conn_phy.conn_cur_phy != BLE_PHY_1M)
				{
					bltc.conn_tolerance_time += 10*SYSTEM_TIMER_TICK_1US;
				}
		#endif

				bltc.conn_tolerance_time += 30*SYSTEM_TIMER_TICK_1US; //actually 12us added from SDK3.3.1 to SDK3.4.0
		#endif
			}
			else
			{
				u32 tick_delta = (u32)(bltc.connExpectTime - clock_time());
				conn_rx_offset_limit = 0;
				if(tick_delta < BIT(30)){
					tick_delta = (tick_delta>>8);//1s : 3828 us
					conn_rx_offset_limit =  (tick_delta < 5*1000*SYSTEM_TIMER_TICK_1US) ? 5*1000*SYSTEM_TIMER_TICK_1US : tick_delta;
				}
			}
			reg_system_tick_irq = blt_next_event_tick = bltc.connExpectTime - bltc.conn_tolerance_time;

		}
		else if(bltParam.blt_state != BLS_LINK_STATE_CONN) {

			if(!blt_miscParam.pad32k_en){
					ble_first_rx_tick_last = 0;
					ble_first_rx_tick_pre = 0;
					ble_actual_conn_interval_tick = 0;
			}

		}

#if (BLS_USER_TIMER_WAKEUP_ENABLE)
	}//note: don't delete

	if(bltPm.appWakeup_en ){
		if( (u32)(blt_next_event_tick - bltPm.appWakeup_tick ) < BIT(30) ){
			bltPm.appWakeup_flg = 1;

			if(bltParam.blt_state == BLS_LINK_STATE_CONN){
				bltPm.timing_miss = 1;
			}
		}
	}

	if(bltPm.appWakeup_flg){
		bltPm.appWakeup_loop_noLatency = 1;
		current_wakeup_tick = bltPm.appWakeup_tick;
	}
	else{
		bltPm.appWakeup_loop_noLatency = 0;
		current_wakeup_tick = blt_next_event_tick;
	}
#if 0
#if !PM_32k_RC_CALIBRATION_ALGORITHM_EN
	if(bltParam.blt_state == BLS_LINK_STATE_CONN){
		if(bltPm.timing_synced){
			bltPm.timing_miss = 0;
			bltPm.pm_border_flag |= PM_TIM_RECOVER_START;
		}

		if(bltPm.timing_miss && !bltPm.timing_synced){
			bltPm.pm_border_flag |= PM_TIM_RECOVER_END;
		}
	}
	else{
		bltPm.timing_miss = 0;
	}
#endif
#endif

#else
	current_wakeup_tick = blt_next_event_tick;
#endif


	if( (bltParam.blt_state == BLS_LINK_STATE_ADV  && (bltPm.suspend_mask & SUSPEND_ADV) ) || \
		(bltParam.blt_state == BLS_LINK_STATE_CONN && (bltPm.suspend_mask & SUSPEND_CONN)) ){

		bltPm.current_wakeup_tick = current_wakeup_tick;

		blt_p_event_callback (BLT_EV_FLAG_SUSPEND_ENTER, NULL, 0);


		SleepMode_TypeDef  sleep_M =  SUSPEND_MODE;//SUSPEND_MODE TODO

		if(0xff == g_chip_version){//A0
			if((u32)(current_wakeup_tick - clock_time() - 1147*16)  < BIT(30)){//g_pm_early_wakeup_time_us.sleep_min_time_us
				current_wakeup_tick -= bltPm.deepRet_earlyWakeupTick;
			}
			sleep_M = (SleepMode_TypeDef)bltPm.deepRet_type;

		}
		else{
			if( bltPm.deepRt_en && (u32)(current_wakeup_tick - clock_time() - deepRet_thresTick) < BIT(30) ){
	//			bltParam.blt_busy = 1;   //move to blc_ll_recoverDeepRetention
				current_wakeup_tick -= bltPm.deepRet_earlyWakeupTick;
				sleep_M = (SleepMode_TypeDef)bltPm.deepRet_type;
			}
		}

		#if(DATA_NO_INIT_EN)
			if(blt_buff_process_pending){
				sleep_M = SUSPEND_MODE;
			}
		#endif

		u32 wakeup_src = cpu_sleep_wakeup (sleep_M, (PM_WAKEUP_TIMER | bltPm.wakeup_src | bltPm.pm_border_flag), current_wakeup_tick);

		reg_embase_addr = 0xc0000000;//default is 0xc0200000;

		if(wakeup_src & STATUS_ENTER_SUSPEND){
			wakeup_src &= (~STATUS_ENTER_SUSPEND);
#if (BLS_USER_TIMER_WAKEUP_ENABLE)
			bltPm.timing_synced = 0;
#endif
		}


#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		rf_ble_1m_param_init();//need check
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
	rf_drv_ble_init();//need check
#endif
#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
		bltPHYs.cur_llPhy = BLE_PHY_1M;
#endif


		blt_p_event_callback (BLT_EV_FLAG_SUSPEND_EXIT, (u8 *)&wakeup_src, 1);

		#if (BLS_USER_TIMER_WAKEUP_ENABLE)
			//if ( (u32)(blt_next_event_tick - clock_time () - 800 *  SYSTEM_TIMER_TICK_1US) < BIT(30) )
			if ( (wakeup_src & WAKEUP_STATUS_TIMER_PAD ) == WAKEUP_STATUS_PAD || wakeup_src == STATUS_GPIO_ERR_NO_ENTER_PM)  //pad, no timer
			{
				bls_pm_procGpioEarlyWakeup(bltPm.latency_en, (u8 *)&wakeup_src);

				bltPm.appWakeup_loop_noLatency = 0;

				if(wakeup_src == STATUS_GPIO_ERR_NO_ENTER_PM){
					bltParam.blt_busy = 1;
				}
			}
			else if(wakeup_src & WAKEUP_STATUS_TIMER){
				if(bltPm.appWakeup_flg){
					if(pm_appWakeupLowPowerCb){
						pm_appWakeupLowPowerCb(1);  //CALLBACK_ENTRY
					}

					bltPm.appWakeup_loop_noLatency = 1;
					bltPm.timer_wakeup = 1;
				}
				else{
					bltPm.appWakeup_loop_noLatency = 0;
					bltParam.blt_busy = 1;
				}
			}
		#else
			//if ( (u32)(blt_next_event_tick - clock_time () - 800 *  SYSTEM_TIMER_TICK_1US) < BIT(30) )
			if ( (wakeup_src & WAKEUP_STATUS_TIMER_PAD ) == WAKEUP_STATUS_PAD || wakeup_src == STATUS_GPIO_ERR_NO_ENTER_PM)  //pad, no timer
			{
				bls_pm_procGpioEarlyWakeup(bltPm.latency_en, (u8 *)&wakeup_src);

				if(wakeup_src == STATUS_GPIO_ERR_NO_ENTER_PM){
					bltParam.blt_busy = 1;
				}
			}
			else{
				bltParam.blt_busy = 1;
			}
		#endif
	}
	else{  //empty run to consume time
		blc_pm_proc_no_suspend();  //save ramcode 48 byte
		//bltPm.current_wakeup_tick = 0; save ramcode 4 byte
	}


	bltPm.appWakeup_flg = 0;
	bltPm.wakeup_src = 0;
	bltPm.user_latency = 0xffff;
}




void blc_pm_proc_no_suspend(void)
{

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	if(bltPm.suspend_mask == MCU_STALL){
		reg_mcu_wakeup_mask |= FLD_IRQ_SYSTEM_TIMER;//timer1 mask enable
		write_reg8(0x6f,0x80);//stall mcu
		asm("tnop");
		asm("tnop");
	}


	bltParam.blt_busy = 1;
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
#endif
}



