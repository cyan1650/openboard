/********************************************************************************************************
 * @file	app_ui.c
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
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "app_config.h"

#include "application/keyboard/keyboard.h"
#include "application/usbstd/usbkeycode.h"
#include "../common/blt_led.h"
#include "../common/blt_soft_timer.h"
#if (TEST_MODE == AMAZON_RCU_TEST)

#if (UI_KEYBOARD_ENABLE)

/////////////////// macro definition /////////////////////////////////
#define MTU_SIZE_REQUEST_MAX	0x00AB	// 0x00AB--->171, 0x009e--->158
#define AMAZON_AUDIO_START		0x01
#define AMAZON_AUDIO_STOP		0x00

#define KEY_LONG_PRESS_TIME_MAX		8	// unit second
#define ADV_IN_CONN_SLAVE_TIME_MAX	60	// unit second
#define ADV_IN_DISCONN_PAIRED_TIME_MAX	59	// unit second



//////////////////// key type ///////////////////////
#define IDLE_KEY	   			0
#define CONSUMER_KEY   	   		1
#define KEYBOARD_KEY   	   		2
#define IR_KEY   	   			3
#define KEYBOARD_NUMBER_KEY		4

_attribute_data_retention_	u8 		key_type;
_attribute_data_retention_	u8 		key_not_released;


_attribute_data_retention_	u8 		ota_is_working = 0;


_attribute_data_retention_	u8     ui_mtu_size_exchange_req = 0;

_attribute_data_retention_	u8 	ir_not_released;
_attribute_data_retention_	u8 		user_key_mode;
u8      ir_hw_initialed = 0;   //note: can not be retention variable


_attribute_data_retention_	u8		ui_mic_enable = 0;
_attribute_data_retention_	u8 		key_voice_press = 0;


//////////////////// variable ///////////////////////
_attribute_data_retention_	u8 		amazon_voice_trigger_flag = 0;

_attribute_data_retention_	u8 		app_disable_slave_latency_sustaintime_second = 0;
_attribute_data_retention_	u32 	app_disable_slave_latency_tick = 0;

_attribute_data_retention_	u8 		key_long_press_type = 0;
_attribute_data_retention_	u8 		advertisingInConnSlave_flag = 0;
_attribute_data_retention_	u8 		advertisingInDisconnPaired_flag = 0;

_attribute_data_retention_	u32		key_long_press_tick;

_attribute_data_retention_	u8 		factory_reset_inConnect_enable_flag = 0;



extern u32	latest_user_event_tick;
extern u8	buffer_mic_pkt_wptr;
extern u8	buffer_mic_pkt_rptr;

extern u8	device_in_connection_state;

extern u32	advertise_begin_tick;



//////////////////// function ///////////////////////
int		amazon_0xF2_client_to_server_receivedata(rf_packet_att_write_t *p);
void	amazon_audio_start(void);
void	app_disable_slave_latency(u8 time_second);
s32 	advertisingInConnSlave_stop_delay(void);
s32 	advertisingInDisconnPaired_stop_delay(void);
s32 	noneBondingDevice_keyPress_ledShine_delay(void);
void	terminateConn_factory_reset(void);
s32 	terminateConn_factory_reset_delete_bondDevice_delay(void);
void	terminateConn_advertisingInConnSlave(void);
void	establishConn_advertisingInDisconnPaired(void);



extern void triggerAdvertisingInConnSlaveRole(void);
extern void triggerAdvertisingInDisconnPaired(void);



//////////////////// enum ///////////////////////
enum{
	KEY_LONG_PRESS_NONE = 0,
	KEY_LONG_PRESS_HOME,	//1
	KEY_LONG_PRESS_LEFT_BACK_MENU,	//2
};

#define MKEY_NONE	0x0000	//Amazon

//static const u16 vk_consumer_map[16] = {
//		MKEY_VOL_UP,
//		MKEY_VOL_DN,
//		MKEY_MUTE,
//		MKEY_CHN_UP,
//
//		MKEY_CHN_DN,
//		MKEY_POWER,
//		MKEY_AC_SEARCH,
//		MKEY_RECORD,
//
//		MKEY_PLAY,
//		MKEY_PAUSE,
//		MKEY_STOP,
//		MKEY_FAST_FORWARD,  //can not find fast_backword in <<HID Usage Tables>>
//
//		MKEY_FAST_FORWARD,
//		MKEY_AC_HOME,
//		MKEY_AC_BACK,
//		MKEY_MENU,
//};
static const u16 vk_consumer_map[11] = {
		MKEY_NONE,		// 0
		MKEY_NONE,		// 1

		MKEY_VOL_UP,	// 2 ---> 0xf2
		MKEY_VOL_DN,
		MKEY_MUTE,
		MKEY_AC_SEARCH,

		MKEY_PLAY_PAUSE,
		MKEY_REWIND,
		MKEY_FAST_FORWARD,
		MKEY_MENU,

		MKEY_AC_HOME,
};

typedef struct
{
	u8 keytype;
	u32 tick;
	u8 KeyValue[4];
}key_fifo_t;

#define  KEY_FIFO_NUM_MAX	8
key_fifo_t key_fifo[KEY_FIFO_NUM_MAX];
u32 key_wptr,key_rptr;

u8 key_fifo_init(void)
{
	key_wptr = key_rptr =0;
	return 0;
}

u8 key_fifo_add(u8 type, u8*key)
{
	key_fifo[key_wptr&(KEY_FIFO_NUM_MAX-1)].tick = clock_time();
	key_fifo[key_wptr&(KEY_FIFO_NUM_MAX-1)].keytype = type;
	memcpy(key_fifo[key_wptr&(KEY_FIFO_NUM_MAX-1)].KeyValue, key, KEY_FIFO_NUM_MAX);

	key_wptr++;
	return 0;
}


/////////////////////////// led management /////////////////////
#if (BLT_APP_LED_ENABLE)

	enum{
		LED_POWER_ON = 0,
		LED_AUDIO_ON,
		LED_AUDIO_OFF,
		LED_SHINE_SLOW,
		LED_SHINE_FAST,
		LED_SHINE_KEY,
		LED_SHINE_OTA,
		LED_SHINE_ADV_IND,
		LED_OFF_HIGH_PRIORITY,
	};

	const led_cfg_t led_cfg[] = {
			{50,      0,      1,      0x00,	 },    //power-on, 50ms on
			{100,	  0 ,	  0xff,	  0x02,  },    //audio on, long on
			{0,	      100 ,   0xff,	  0x02,  },    //audio off, long off
			{500,	  500 ,   2,	  0x01,	 },    //1Hz for 2 seconds
			{250,	  250 ,   4,	  0x01,  },    //2Hz for 2 seconds
			{200,	  800 ,   3,	  0x01,  },    //1Hz for 3 seconds
			{250,	  250 ,   200,	  0x08,  },    //2Hz for 100 seconds
			{200,	  800 ,   60,	  0x08,  },    //1Hz for 60 seconds
			{0,	      1 ,	  1,	  0x40,  },    //LED off, high priority
	};

#endif





#if (BLE_AUDIO_ENABLE)

#include "op_audio.h"

#define AUDIO_DEBUG			1

#if (AUDIO_DEBUG)

	dma_chain_config_t rx_dma_list_config[2];

	dma_chain_config_t tx_dma_list_config[2];
	extern u8 audio_tx_dma_chn;
	extern u8 audio_rx_dma_chn;

	volatile u8 amic_enable;
	void audio_amic_init(void)
	{
		audio_set_codec_supply();

		core_interrupt_enable();
		audio_set_rx_fifo_h_lvl1_th((TL_MIC_BUFFER_SIZE>>2)-2);// set rx fifo high level 1 threshold ,h_level=0x3fe,when max_rx_wptr=0x3ff(full). max_rx_rptr=0,(max_rx_wptr-max_rx_rptr)>h_level ,produce  interrupt
		audio_set_irq_mask(FLD_AUDIO_IRQ_RXFIFO_H_L1_EN);
		plic_interrupt_enable(IRQ20_DFIFO);

		audio_init(AMIC_IN_ONLY ,AUDIO_16K,STEREO_BIT_16);
		audio_rx_dma_chain_init(DMA2,(u16*)&buffer_mic,TL_MIC_BUFFER_SIZE);
//		audio_tx_dma_chain_init (DMA3,(u16*)&buffer_mic,AUDIO_BUFF_SIZE);
	}
	void audio_dmic_init()
	{
		audio_set_codec_supply();

		core_interrupt_enable();
		audio_set_rx_fifo_h_lvl1_th((TL_MIC_BUFFER_SIZE>>2)-2);// set rx fifo high level 1 threshold ,h_level=0x3fe,when max_rx_wptr=0x3ff(full). max_rx_rptr=0,(max_rx_wptr-max_rx_rptr)>h_level ,produce  interrupt
		audio_set_irq_mask(FLD_AUDIO_IRQ_RXFIFO_H_L1_EN);
		plic_interrupt_enable(IRQ20_DFIFO);

		audio_set_dmic_pin(DMIC_GROUPB_B2_DAT_B3_B4_CLK);
		audio_init(DMIC_IN_ONLY ,AUDIO_16K,MONO_BIT_16);

		audio_rx_dma_chain_init(DMA2,(u16*)&buffer_mic,TL_MIC_BUFFER_SIZE);
//		audio_tx_dma_chain_init (DMA3,(u16*)&buffer_mic,TL_MIC_BUFFER_SIZE);
	}
	void audio_mic_off()//
	{
		audio_clk_en(0,0);
		audio_rx_dma_dis();
//		audio_tx_dma_dis();
	}
#endif

	u32 	key_voice_pressTick = 0;

	void ui_enable_mic (int en)
	{
		ui_mic_enable = en;

		#if (BLT_APP_LED_ENABLE)
			device_led_setup(led_cfg[en ? LED_AUDIO_ON : LED_AUDIO_OFF]);
		#endif
		gpio_write(GPIO_LED_RED,en);
		if(en){  //audio on
			///////////////////// AUDIO initialization///////////////////
			#if (BLE_DMIC_ENABLE)  //Dmic config
				audio_dmic_init();
			#else  //Amic config
				audio_amic_init();
			#endif
			opus_related_init();
		}
		else{  //audio off
			#if (BLE_DMIC_ENABLE)  //Dmic config
			audio_mic_off();
			#else  //audio off
				audio_mic_off();
			#endif
		}

		#if (BATT_CHECK_ENABLE)
			battery_set_detect_enable(!en);
		#endif
	}


	void voice_press_proc(void)
	{
		key_voice_press = 0;
		ui_enable_mic (1);
#if 1
		if(ui_mtu_size_exchange_req && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN){
			ui_mtu_size_exchange_req = 0;
			blc_att_requestMtuSizeExchange(BLS_CONN_HANDLE, 0x009e);
		}
#endif
	}

	void task_audio (void)
	{
		static u32 audioProcTick = 0;
		if(clock_time_exceed(audioProcTick, 500)){
			audioProcTick = clock_time();
		}
		else{
			return;
		}
		proc_mic_encoder ();

		if (blc_ll_getTxFifoNumber() < 9)
		{
			int *p = mic_encoder_data_buffer ();
			if (p)
			{
				if( BLE_SUCCESS == blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_AMAZON_AUDIO_DATA_REPORT_INPUT_DP_H, (u8*)p, ADPCM_PACKET_LEN )){
					mic_encoder_data_read_ok();
					DBG_CHN5_TOGGLE;
				}
			}
		}
	}



	void blc_checkConnParamUpdate(void)
	{
		extern u32 interval_update_tick;

		if(	 interval_update_tick && clock_time_exceed(interval_update_tick,5*1000*1000) && \
			 blc_ll_getCurrentState() == BLS_LINK_STATE_CONN &&  bls_ll_getConnectionInterval()!= 8 )
		{
			interval_update_tick = clock_time() | 1;
			bls_l2cap_requestConnParamUpdate (8, 8, 99, 400);
		}
	}
#endif





#if (REMOTE_IR_ENABLE)
	//ir key
	#define TYPE_IR_SEND			1
	#define TYPE_IR_RELEASE			2

	///////////////////// key mode //////////////////////
	#define KEY_MODE_BLE	   		0    //ble key
	#define KEY_MODE_IR        		1    //ir  key


	static const u8 kb_map_ble[30] = 	KB_MAP_BLE;
	static const u8 kb_map_ir[30] = 	KB_MAP_IR;


	void ir_dispatch(u8 type, u8 syscode ,u8 ircode){

		if(!ir_hw_initialed){
			ir_hw_initialed = 1;
			rc_ir_init();
		}

		if(type == TYPE_IR_SEND){
			ir_nec_send(syscode,~(syscode),ircode);

		}
		else if(type == TYPE_IR_RELEASE){
			ir_send_release();
		}
	}


#endif



#if (BLE_REMOTE_OTA_ENABLE)
	void app_enter_ota_mode(void)
	{
		ota_is_working = 1;
		#if (BLT_APP_LED_ENABLE)
			device_led_setup(led_cfg[LED_SHINE_OTA]);
		#endif
		bls_ota_setTimeout(15 * 1000 * 1000); //set OTA timeout  15 seconds
	}



	void app_debug_ota_result(int result)
	{

		#if(0 && BLT_APP_LED_ENABLE)  //this is only for debug

			gpio_set_output_en(GPIO_LED, 1);

			if(result == OTA_SUCCESS){  //led for debug: OTA success
				gpio_write(GPIO_LED, 1);
				sleep_us(500000);
				gpio_write(GPIO_LED, 0);
				sleep_us(500000);
				gpio_write(GPIO_LED, 1);
				sleep_us(500000);
				gpio_write(GPIO_LED, 0);
				sleep_us(500000);
			}
			else{  //OTA fail

				#if 0 //this is only for debug,  can not use this in application code
					irq_disable();
					WATCHDOG_DISABLE;

					while(1){
						gpio_write(GPIO_LED, 1);
						sleep_us(200000);
						gpio_write(GPIO_LED, 0);
						sleep_us(200000);
					}
				#endif

			}

			gpio_set_output_en(GPIO_LED, 0);
		#endif
	}
#endif






/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////


//This function process ...
void deep_wakeup_proc(void)
{

#if(DEEPBACK_FAST_KEYSCAN_ENABLE)
	//if deepsleep wakeup is wakeup by GPIO(key press), we must quickly scan this
	//press, hold this data to the cache, when connection established OK, send to master
	//deepsleep_wakeup_fast_keyscan
	if(analog_read_reg8(USED_DEEP_ANA_REG) & CONN_DEEP_FLG){
		if(kb_scan_key (KB_NUMLOCK_STATUS_POWERON, 1) && kb_event.cnt){
			deepback_key_state = DEEPBACK_KEY_CACHE;
			key_not_released = 1;
			memcpy(&kb_event_cache,&kb_event,sizeof(kb_event));
		}

		analog_write_reg8(USED_DEEP_ANA_REG, analog_read_reg8(USED_DEEP_ANA_REG) & (~CONN_DEEP_FLG));
	}
#endif
}





void deepback_pre_proc(int *det_key)
{
#if (DEEPBACK_FAST_KEYSCAN_ENABLE)
	// to handle deepback key cache
	if(!(*det_key) && deepback_key_state == DEEPBACK_KEY_CACHE && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN \
			&& clock_time_exceed(bls_ll_getConnectionCreateTime(), 25000)){

		memcpy(&kb_event,&kb_event_cache,sizeof(kb_event));
		*det_key = 1;

		if(key_not_released || kb_event_cache.keycode[0] == VOICE){  //no need manual release
			deepback_key_state = DEEPBACK_KEY_IDLE;
		}
		else{  //need manual release
			deepback_key_tick = clock_time();
			deepback_key_state = DEEPBACK_KEY_WAIT_RELEASE;
		}
	}
#endif
}

void deepback_post_proc(void)
{
#if (DEEPBACK_FAST_KEYSCAN_ENABLE)
	//manual key release
	if(deepback_key_state == DEEPBACK_KEY_WAIT_RELEASE && clock_time_exceed(deepback_key_tick,150000)){
		key_not_released = 0;
		u8 		key_buf[8] = {0};
		key_buf[2] = 0;
		blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8); //release
		deepback_key_state = DEEPBACK_KEY_IDLE;
	}
#endif
}

void key_change_proc(void)
{

	latest_user_event_tick = clock_time();  //record latest key change time


	u8 key0 = kb_event.keycode[0];
	u8 key_buf[8] = {0,0,0,0,0,0,0,0};

	key_not_released = 1;
	if (kb_event.cnt == 2)   //two key press, do  not process
	{

	}
	else if(kb_event.cnt == 1)
	{
		if(key0 >= CR_VOL_UP )  //volume up/down
		{
			gpio_write(GPIO_LED_GREEN,1);

			key_type = CONSUMER_KEY;
//			u16 consumer_key;
//			if(key0 == CR_VOL_UP){  	//volume up
//				consumer_key = MKEY_VOL_UP;
//			}
//			else if(key0 == CR_VOL_DN){ //volume down
//				consumer_key = MKEY_VOL_DN;
//			}
//			blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE, HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 2);

			u32 consumer_key = vk_consumer_map[key0 & 0x0f];
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 4)!=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&consumer_key);
			}
		}
		else
		{
			gpio_write(GPIO_LED_BLUE,1);

			key_type = KEYBOARD_KEY;

//			key_buf[2] = key0;
//			if(key0 == VK_1)
//			{
//			}
//			else if(key0 == VK_2)
//			{
//			}
//			blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE, HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8);


			key_buf[0] = key0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE, HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 3) !=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&key_buf);
			}
		}

	}
	else   //kb_event.cnt == 0,  key release
	{
		gpio_write(GPIO_LED_BLUE,0);
		gpio_write(GPIO_LED_GREEN,0);

		key_not_released = 0;

//		if(key_type == CONSUMER_KEY)
//		{
//			u16 consumer_key = 0;
//
//			blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE, HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 2);
//		}
//		else if(key_type == KEYBOARD_KEY)
//		{
//			key_buf[2] = 0;
//
//			blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE, HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8); //release
//		}

		if(key_type == CONSUMER_KEY)
		{
			u32 consumer_key = 0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 4)!=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&consumer_key);
			}
		}
		else if(key_type == KEYBOARD_KEY)
		{
			key_buf[0] = 0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 3) !=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&key_buf);
			}
		}

	}
}


void key_change_proc_2(void)
{

	latest_user_event_tick = clock_time();  //record latest key change time

	if(key_voice_press){  //clear voice key press flg
		key_voice_press = 0;
	}

	u8 key0 = kb_event.keycode[0];
	u8 key1 = kb_event.keycode[1];
	u8 key2 = kb_event.keycode[2];
	u8 key_value;
	u8 key_buf[8] = {0,0,0,0,0,0,0,0};



	key_not_released = 1;
	if (kb_event.cnt == 3)   //three key press
	{
		if ((key0 == VK_BACK) && (key1 == VK_LEFT) && (key2 == CR_MENU))
		{
			key_long_press_type = KEY_LONG_PRESS_LEFT_BACK_MENU;
			key_long_press_tick = clock_time();
			//LEFT_BACK_MENU
		}
	}
	else if (kb_event.cnt == 2)   //two key press, do  not process
	{
#if (BLE_PHYTEST_MODE != PHYTEST_MODE_DISABLE)  //"enter + back" trigger PhyTest
		//notice that if IR enable, trigger keys must be defined in key map
		if ( (key0 == VK_ENTER && key1 == VK_0) || (key0 == VK_0 && key1 == VK_ENTER))
		{
			extern void app_phytest_init(void);
			extern void app_trigger_phytest_mode(void);
			app_phytest_init();
			app_trigger_phytest_mode();

			device_led_setup(led_cfg[LED_SHINE_FAST]);
		}
#endif
	}
	else if(kb_event.cnt == 1)
	{
		if(device_in_connection_state == 0)// none connection
		{
			if(!blc_smp_param_getCurrentBondingDeviceNumber())   //none bonding device exist
			{
				device_led_setup(led_cfg[LED_OFF_HIGH_PRIORITY]);
				blt_soft_timer_add(&noneBondingDevice_keyPress_ledShine_delay, Timing_CLOCK_100mS);
			}
		}

		if(key0 == KEY_MODE_SWITCH)
		{
			user_key_mode = !user_key_mode;

			#if (REMOTE_IR_ENABLE)
				if(user_key_mode == KEY_MODE_BLE){
					analog_write_reg8(USED_DEEP_ANA_REG, analog_read_reg8(USED_DEEP_ANA_REG)&(~IR_MODE_DEEP_FLG));
				}
				else{
					analog_write_reg8(USED_DEEP_ANA_REG, analog_read_reg8(USED_DEEP_ANA_REG)|IR_MODE_DEEP_FLG);
				}
			#endif

			#if (BLT_APP_LED_ENABLE)
				device_led_setup(led_cfg[LED_SHINE_SLOW + user_key_mode]);
			#endif
		}
#if (BLE_AUDIO_ENABLE)
		else if (key0 == VOICE)
		{
			if(ui_mic_enable){  //if voice on, voice off
				ui_enable_mic (0);
			}
			else{ //if voice not on, mark voice key press tick
				key_voice_press = 1;
				key_voice_pressTick = clock_time();
			}
		}
#endif

#if (REMOTE_IR_ENABLE)
		else if(user_key_mode == KEY_MODE_BLE)
		{

			key_value = kb_map_ble[key0];
			if(key_value >= 0xf0 ){
				key_type = CONSUMER_KEY;
				u16 consumer_key = vk_consumer_map[key_value & 0x0f];
				blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 2);
			}
			else
			{
				key_type = KEYBOARD_KEY;
				key_buf[2] = key_value;
				blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8);
			}

		}
		else if(user_key_mode == KEY_MODE_IR)
		{  //IR mode
			key_value = kb_map_ir[key0];
			key_type = IR_KEY;
			if(!ir_not_released){
				ir_dispatch(TYPE_IR_SEND, 0x88, key_value);
				ir_not_released = 1;
			}
		}
		else
		{
			key_type = IDLE_KEY;
		}
#else
		else
		{
			key_value = key0;
			if(key_value >= CR_VOL_UP){
				key_type = CONSUMER_KEY;
				u32 consumer_key = vk_consumer_map[key_value & 0x0f];
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 4)!=BLE_SUCCESS)
				{
					key_fifo_add(key_type, (u8 *)&consumer_key);
				}

				if(key_value == CR_HOME)
				{
					key_long_press_type = KEY_LONG_PRESS_HOME;
					key_long_press_tick = clock_time();
				}
			}
			else if((key_value >= VK_W) && (key_value <= VK_0))
			{
				key_type = KEYBOARD_NUMBER_KEY;
				key_buf[2] = key_value;
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8) !=BLE_SUCCESS)
				{
					key_fifo_add(key_type, (u8 *)&key_buf);
				}
			}
			else
			{
				key_type = KEYBOARD_KEY;
				key_buf[0] = key_value;
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 3) !=BLE_SUCCESS)
				{
					key_fifo_add(key_type, (u8 *)&key_buf);
				}
			}
		}
#endif
	}
	else   //kb_event.cnt == 0,  key release
	{
		key_not_released = 0;
		key_long_press_type = KEY_LONG_PRESS_NONE;

		if(key_type == CONSUMER_KEY)
		{
			u32 consumer_key = 0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, (u8 *)&consumer_key, 4)!=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&consumer_key);
			}
		}
		else if(key_type == KEYBOARD_KEY)
		{
			key_buf[0] = 0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 3) !=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&key_buf);
			}
		}
		else if(key_type == KEYBOARD_NUMBER_KEY)
		{
			key_buf[2] = 0;
			if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, key_buf, 8) !=BLE_SUCCESS)
			{
				key_fifo_add(key_type, (u8 *)&key_buf);
			}
		}
#if (REMOTE_IR_ENABLE)
		else if(key_type == IR_KEY)
		{
			if(ir_not_released){
				ir_not_released = 0;
				ir_dispatch(TYPE_IR_RELEASE, 0, 0);  //release
			}
		}
#endif
	}
}


#define GPIO_WAKEUP_KEYPROC_CNT				3


_attribute_data_retention_	static int gpioWakeup_keyProc_cnt = 0;
_attribute_data_retention_		static u32 keyScanTick = 0;

_attribute_ram_code_
void proc_keyboard (u8 e, u8 *p, int n)
{
	if(clock_time_exceed(keyScanTick, 8000)){
		keyScanTick = clock_time();
	}
	else{
		return;
	}

	kb_event.keycode[0] = 0;
	int det_key = kb_scan_key (0, 1);



	if (det_key){
		key_change_proc();
	}
}


void proc_keyboard_2 (u8 e, u8 *p, int n)
{


	//when key press gpio wakeup suspend, proc keyscan at least GPIO_WAKEUP_KEYPROC_CNT times
	//regardless of 8000 us interval
	if(e == BLT_EV_FLAG_GPIO_EARLY_WAKEUP){
		gpioWakeup_keyProc_cnt = GPIO_WAKEUP_KEYPROC_CNT;
	}
	else if(gpioWakeup_keyProc_cnt){
		gpioWakeup_keyProc_cnt --;
	}


	if(gpioWakeup_keyProc_cnt || clock_time_exceed(keyScanTick, 8000)){
		keyScanTick = clock_time();
	}
	else{
		return;
	}


	kb_event.keycode[0] = 0;
	int det_key = kb_scan_key (0, 1);


#if(DEEPBACK_FAST_KEYSCAN_ENABLE)
	if(deepback_key_state != DEEPBACK_KEY_IDLE){
		deepback_pre_proc(&det_key);
	}
#endif
	if((key_wptr&(KEY_FIFO_NUM_MAX-1)) != (key_rptr&(KEY_FIFO_NUM_MAX-1)))
	{
		u32 tick = key_fifo[key_rptr&(KEY_FIFO_NUM_MAX-1)].tick;
		u8 *pkey = key_fifo[key_rptr&(KEY_FIFO_NUM_MAX-1)].KeyValue;

		if(clock_time_exceed(tick, 100000))
		{
			key_rptr++;
		}
		else
		{
			if(key_fifo[key_rptr&(KEY_FIFO_NUM_MAX-1)].keytype == CONSUMER_KEY)
			{
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_CONSUME_REPORT_INPUT_DP_H, pkey, 4)==BLE_SUCCESS)
					key_rptr++;
			}
			else if(key_fifo[key_rptr&(KEY_FIFO_NUM_MAX-1)].keytype == KEYBOARD_KEY)
			{
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, pkey, 3) ==BLE_SUCCESS)
					key_rptr++;
			}
			else if(key_fifo[key_rptr&(KEY_FIFO_NUM_MAX-1)].keytype == KEYBOARD_NUMBER_KEY)
			{
				if(blc_gatt_pushHandleValueNotify (BLS_CONN_HANDLE,HID_NORMAL_KB_REPORT_INPUT_DP_H, pkey, 8) ==BLE_SUCCESS)
					key_rptr++;
			}
		}
	}


	if (det_key){
		key_change_proc();
	}


#if (BLE_AUDIO_ENABLE)
	 //long press voice 1 second
	if(key_voice_press && !ui_mic_enable && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN && \
		clock_time_exceed(key_voice_pressTick, 1000000)){

		voice_press_proc();
	}
#endif


#if(DEEPBACK_FAST_KEYSCAN_ENABLE)
	if(deepback_key_state != DEEPBACK_KEY_IDLE){
		deepback_post_proc();
	}
#endif
}

#endif


void app_ui_init_normal(void)
{
	/////////// keyboard gpio wakeup init ////////
//	u32 pin[] = KB_DRIVE_PINS;
//	for (int i=0; i<(sizeof (pin)/sizeof(*pin)); i++)
//	{
//		cpu_set_gpio_wakeup (pin[i], Level_High,1);  //drive pin pad high wakeup deepsleep
//	}
//
//	bls_app_registerEventCallback (BLT_EV_FLAG_GPIO_EARLY_WAKEUP, &proc_keyboard);


#if (BLT_APP_LED_ENABLE)
	device_led_init(GPIO_LED, LED_ON_LEVAL);  //LED initialization
	device_led_setup(led_cfg[LED_POWER_ON]);
#endif


#if (REMOTE_IR_ENABLE)
	user_key_mode = analog_read_reg8(USED_DEEP_ANA_REG) & IR_MODE_DEEP_FLG ? KEY_MODE_IR : KEY_MODE_BLE;
#endif



#if (BLT_TEST_SOFT_TIMER_ENABLE)
	blt_soft_timer_init();
#endif

	key_fifo_init();

}



void app_ui_init_deepRetn(void)
{
	/////////// keyboard gpio wakeup init ////////
//	u32 pin[] = KB_DRIVE_PINS;
//	for (int i=0; i<(sizeof (pin)/sizeof(*pin)); i++)
//	{
//		cpu_set_gpio_wakeup (pin[i], Level_High, 1);  //drive pin pad high wakeup deepsleep
//	}




#if (BLT_APP_LED_ENABLE)
	device_led_init(GPIO_LED, 1);  //LED initialization
#endif

#if (REMOTE_IR_ENABLE)
	user_key_mode = analog_read_reg8(USED_DEEP_ANA_REG) & IR_MODE_DEEP_FLG ? KEY_MODE_IR : KEY_MODE_BLE;
#endif
}


int amazon_0xF2_client_to_server_receivedata(rf_packet_att_write_t *p)
{
	u8 len = p->l2capLen - 3;

	if(len > 0)
	{
		u8 rx_data[len];
		memcpy(rx_data, &p->value, len);

		if(rx_data[0] == AMAZON_AUDIO_START)//0x01
		{
			if(!ui_mic_enable)
			{
				app_disable_slave_latency(1);// unit second
				amazon_voice_trigger_flag = 1;
			}
		}
		else if(rx_data[0] == AMAZON_AUDIO_STOP)//0x00
		{
			if(ui_mic_enable)
			{  //if voice on, voice off
				ui_enable_mic (0);
			}
		}
	}

	return 0;
}

int amazon_diag_client_to_server_receivedata(void *p)
{
}

void amazon_audio_start(void)
{
#if (BLE_AUDIO_ENABLE)
	voice_press_proc();
#endif
}


void app_disable_slave_latency(u8 time_second)
{
	app_disable_slave_latency_sustaintime_second = time_second;
	app_disable_slave_latency_tick = clock_time()|1;
	printf("SL OFF:%d\n", app_disable_slave_latency_sustaintime_second);
}


void ui_loop(void)
{
	// Manual Slave_Latency
	if(app_disable_slave_latency_sustaintime_second)
	{
		if(app_disable_slave_latency_tick && clock_time_exceed(app_disable_slave_latency_tick, app_disable_slave_latency_sustaintime_second * Timing_CLOCK_1S))
		{  // Max 15 Second
			app_disable_slave_latency_tick = 0;
			app_disable_slave_latency_sustaintime_second = 0;
			//SL ON
		}
		else
		{
			bls_pm_setManualLatency(0);
		}

		if(amazon_voice_trigger_flag == 1)
		{
			amazon_voice_trigger_flag = 0;
			amazon_audio_start();
		}
	}

	if(key_not_released)
	{
		if(key_long_press_type == KEY_LONG_PRESS_HOME)
		{
			if(clock_time_exceed(key_long_press_tick, KEY_LONG_PRESS_TIME_MAX * Timing_CLOCK_1S))
			{
				key_long_press_type = KEY_LONG_PRESS_NONE;

				if(device_in_connection_state == 1)	// connection
				{
					advertisingInConnSlave_flag = 1;
					triggerAdvertisingInConnSlaveRole();
		        	blt_soft_timer_add(&advertisingInConnSlave_stop_delay, ADV_IN_CONN_SLAVE_TIME_MAX * Timing_CLOCK_1S);
		    		device_led_setup(led_cfg[LED_SHINE_ADV_IND]);
					//AdvertisingInConnSlave_start
				}
				else
				{
					if(blc_smp_param_getCurrentBondingDeviceNumber())   //at least 1 bonding device exist
					{
						advertisingInDisconnPaired_flag = 1;
						triggerAdvertisingInDisconnPaired();
			        	blt_soft_timer_add(&advertisingInDisconnPaired_stop_delay, ADV_IN_DISCONN_PAIRED_TIME_MAX * Timing_CLOCK_1S);
			    		device_led_setup(led_cfg[LED_SHINE_ADV_IND]);
					}
				}
			}
		}
		else if(key_long_press_type == KEY_LONG_PRESS_LEFT_BACK_MENU)
		{
			if(clock_time_exceed(key_long_press_tick, KEY_LONG_PRESS_TIME_MAX * Timing_CLOCK_1S))
			{
				key_long_press_type = KEY_LONG_PRESS_NONE;
				//Factory Reset

				if(device_in_connection_state == 0)// none connection
				{
					bls_ll_setAdvEnable(BLC_ADV_DISABLE);  //must: set adv Disable

					if(blc_smp_param_getCurrentBondingDeviceNumber())   //at least 1 bonding device exist
					{
						bls_smp_eraseAllParingInformation();
						//None Connect Delete bond device info
					}
					else
					{
						//None Connect No bond device info
					}

					triggerAdvertisingInDisconnPaired();
				}
				else	// connection
				{
					bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN); //push terminate cmd into ble TX buffer
					factory_reset_inConnect_enable_flag = 1;
				}
			}
		}
	}
}


s32 advertisingInConnSlave_stop_delay(void)
{
	advertisingInConnSlave_flag = 0;
	bls_ll_setAdvEnable(BLC_ADV_DISABLE);   //disable adv
	blc_ll_removeAdvertisingFromConnSLaveRole();
	//AdvertisingInConnSlave_stop

	return -1;
}


s32 advertisingInDisconnPaired_stop_delay(void)
{
	advertisingInDisconnPaired_flag = 0;
	bls_ll_setAdvEnable(BLC_ADV_DISABLE);   //disable adv
	printf("AdvertisingInDisconnPaired_stop\n");

	return -1;
}


s32 noneBondingDevice_keyPress_ledShine_delay(void)
{
	device_led_setup(led_cfg[LED_SHINE_KEY]);

	return -1;
}


void terminateConn_factory_reset(void)
{
	if(factory_reset_inConnect_enable_flag == 1)
	{
		factory_reset_inConnect_enable_flag = 0;
    	blt_soft_timer_add(&terminateConn_factory_reset_delete_bondDevice_delay, Timing_CLOCK_100mS);
	}
}


s32 terminateConn_factory_reset_delete_bondDevice_delay(void)
{
	bls_smp_eraseAllParingInformation();
	//TerminateConnect Delete bond device info

	triggerAdvertisingInDisconnPaired();

	return -1;
}


void terminateConn_advertisingInConnSlave(void)
{
	if(advertisingInConnSlave_flag == 1)
	{
		advertisingInConnSlave_flag = 0;
		device_led_setup(led_cfg[LED_OFF_HIGH_PRIORITY]);
		blt_soft_timer_delete(&advertisingInConnSlave_stop_delay);
		triggerAdvertisingInDisconnPaired();
	}
}


void establishConn_advertisingInDisconnPaired(void)
{
	if(advertisingInDisconnPaired_flag == 1)
	{
		advertisingInDisconnPaired_flag = 0;
		device_led_setup(led_cfg[LED_OFF_HIGH_PRIORITY]);
		blt_soft_timer_delete(&advertisingInDisconnPaired_stop_delay);
	}
}
#endif


