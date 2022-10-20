/********************************************************************************************************
 * @file	app_ui.h
 *
 * @brief	This is the header file for BLE SDK
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
#ifndef APP_UI_H_
#define APP_UI_H_


void app_ui_init_normal(void);
void app_ui_init_deepRetn(void);

void proc_keyboard (u8 e, u8 *p, int n);

void task_audio (void);

void ui_enable_mic (int en);

#if (BLE_REMOTE_OTA_ENABLE)
	void app_enter_ota_mode(void);
	void app_debug_ota_result(int result);
#endif

void ui_loop (void);

//////////////////// variable ///////////////////////
extern 	u8 		key_type;
extern	u8 		key_not_released;
extern	u8		ui_mtu_size_exchange_req;
extern	u8 		ir_not_released;
extern	u8 		user_key_mode;
extern	u8      ir_hw_initialed;
extern	u8 		ota_is_working;
extern	u8		ui_mic_enable;
extern	u8 		key_voice_press;
//extern	u8		lowBatt_alarmFlag;
extern	u8 		advertisingInConnSlave_flag;




//////////////////// function ///////////////////////
extern void 	app_disable_slave_latency(u8 time_second);
extern void		amazon_audio_start(void);
extern void		ui_loop(void);







#endif /* APP_UI_H_ */
