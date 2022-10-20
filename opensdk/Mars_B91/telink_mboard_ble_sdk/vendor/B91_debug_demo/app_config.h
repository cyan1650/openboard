/********************************************************************************************************
 * @file	app_config.h
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
#pragma once



#define LED_TEST							1
#define PRINT_TEST							2
#define RF_TEST								3
#define USB_LOG_TEST						4
#define AMAZON_RCU_TEST						5
#define IRQ_PRIORITY_TEST					6

#define TEST_MODE							IRQ_PRIORITY_TEST






#if (TEST_MODE == USB_LOG_TEST)

	#define PA5_FUNC						AS_USB_DM
	#define PA6_FUNC						AS_USB_DP
	#define PA5_INPUT_ENABLE				1
	#define PA6_INPUT_ENABLE				1

#elif (TEST_MODE == AMAZON_RCU_TEST)

	#define BLE_REMOTE_PM_ENABLE				0
	#define PM_DEEPSLEEP_RETENTION_ENABLE		0
	#define BLE_REMOTE_SECURITY_ENABLE      	1
	#define BLE_REMOTE_OTA_ENABLE				0
	#define REMOTE_IR_ENABLE					0
	#define BATT_CHECK_ENABLE       			0   //must enable
	#define BLE_AUDIO_ENABLE					1
	#define BLT_APP_LED_ENABLE					1
	#define BLT_TEST_SOFT_TIMER_ENABLE			1

	#define AMAZON_PARAMETER_ENABLE				1

	#define	UI_KEYBOARD_ENABLE					1



	#if (BLT_TEST_SOFT_TIMER_ENABLE)
	#define BLT_SOFTWARE_TIMER_ENABLE		1
	#endif


	////////////////////////// AUDIO CONFIG (RCU board) /////////////////////////////
	#if (BLE_AUDIO_ENABLE)
		#define BLE_DMIC_ENABLE					1  //0: Amic   1: Dmic
#if BLE_DMIC_ENABLE
		#define PB2_FUNC						AS_DMIC_DI
		#define PB3_FUNC						AS_DMIC_SCK
		#define PB4_FUNC						AS_DMIC_SCK

		#define PB2_INPUT_ENABLE				1
		#define PB3_INPUT_ENABLE				1
		#define PB4_INPUT_ENABLE				1
#endif
		#define	ADPCM_PACKET_LEN				80	// 160 + 4
		#define TL_MIC_ADPCM_UNIT_SIZE			320
		#define	TL_MIC_BUFFER_SIZE				1280

	#endif


	//////////////////////////// KEYSCAN/MIC  GPIO //////////////////////////////////
	#define	MATRIX_ROW_PULL					PM_PIN_PULLDOWN_100K
	#define	MATRIX_COL_PULL					PM_PIN_PULLUP_10K

	#define	KB_LINE_HIGH_VALID				0   //dirve pin output 0 when keyscan, scanpin read 0 is valid
	#define DEEPBACK_FAST_KEYSCAN_ENABLE	0   //proc fast scan when deepsleep back trigged by key press, in case key loss
	#define LONG_PRESS_KEY_POWER_OPTIMIZE	1   //lower power when pressing key without release

	//stuck key
	#define STUCK_KEY_PROCESS_ENABLE		0
	#define STUCK_KEY_ENTERDEEP_TIME		60  //in s

	//repeat key
	#define KB_REPEAT_KEY_ENABLE			0
	#define	KB_REPEAT_KEY_INTERVAL_MS		200
	#define KB_REPEAT_KEY_NUM				1
	#define KB_MAP_REPEAT					{VK_1, }

	#define			CR_VOL_UP				0xf2  ////
	#define			CR_VOL_DN				0xf3
	#define			CR_VOL_MUTE				0xf4
	#define			CR_RECORD				0xf5
	#define			CR_PLAY_PAUSE			0xf6  ////
	#define			CR_FAST_BACKWARD		0xf7
	#define			CR_FAST_FORWARD			0xf8  ////
	#define			CR_MENU					0xf9
	#define			CR_HOME					0xfa

	//special key
	#define		 	VOICE					0xc0
	#define 		KEY_MODE_SWITCH			0xc1
	#define		 	PHY_TEST				0xc2

	#define 		VK_POWER1				0x66	//Amazon
	#define 		VK_BACK					0xF1	//Amazon

	#if (UI_KEYBOARD_ENABLE)   // if test pure power, kyeScan GPIO setting all disabled
		//---------------  KeyMatrix PB2/PB3/PB4/PB5 -----------------------------
		#define	MATRIX_ROW_PULL					PM_PIN_PULLDOWN_100K
		#define	MATRIX_COL_PULL					PM_PIN_PULLUP_10K

		#define	KB_LINE_HIGH_VALID				0   //dirve pin output 0 when keyscan, scanpin read 0 is valid

		#define		KB_MAP_NORMAL	{	{CR_HOME,		VK_LEFT},	 \
										{CR_RECORD,		VK_RIGHT}, }

		//////////////////// KEY CONFIG (EVK board) ///////////////////////////
		#define  KB_DRIVE_PINS  {GPIO_PC2, GPIO_PC0}
		#define  KB_SCAN_PINS   {GPIO_PC3, GPIO_PC1}

		//drive pin as gpio
		#define	PC2_FUNC				AS_GPIO
		#define	PC0_FUNC				AS_GPIO

		//drive pin need 100K pulldown
		#define	PULL_WAKEUP_SRC_PC2		MATRIX_ROW_PULL
		#define	PULL_WAKEUP_SRC_PC0		MATRIX_ROW_PULL

		//drive pin open input to read gpio wakeup level
		#define PC2_INPUT_ENABLE		1
		#define PC0_INPUT_ENABLE		1

		//scan pin as gpio
		#define	PC3_FUNC				AS_GPIO
		#define	PC1_FUNC				AS_GPIO

		//scan  pin need 10K pullup
		#define	PULL_WAKEUP_SRC_PC3		MATRIX_COL_PULL
		#define	PULL_WAKEUP_SRC_PC1		MATRIX_COL_PULL

		//scan pin open input to read gpio level
		#define PC3_INPUT_ENABLE		1
		#define PC1_INPUT_ENABLE		1
	#endif



	#define		HID_BOOT_ATT_EN		0

	///////////////////////////////////// ATT  HANDLER define ///////////////////////////////////////
	typedef enum
	{
		ATT_H_START = 0,


		//// gatt ////
		/**********************************************************************************************/
		GenericAttribute_PS_H,					//UUID: 2800, 	VALUE: uuid 1801
		GenericAttribute_ServiceChanged_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Indicate
		GenericAttribute_ServiceChanged_DP_H,   //UUID:	2A05,	VALUE: service change
		GenericAttribute_ServiceChanged_CCB_H,	//UUID: 2902,	VALUE: serviceChangeCCC

		//// Gap ////
		/**********************************************************************************************/
		GenericAccess_PS_H, 					//UUID: 2800, 	VALUE: uuid 1800
		GenericAccess_DeviceName_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		GenericAccess_DeviceName_DP_H,			//UUID: 2A00,   VALUE: device name
		GenericAccess_Appearance_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read
		GenericAccess_Appearance_DP_H,			//UUID: 2A01,	VALUE: appearance
		CONN_PARAM_CD_H,						//UUID: 2803, 	VALUE:  			Prop: Read
		CONN_PARAM_DP_H,						//UUID: 2A04,   VALUE: connParameter





		//// device information ////
		/**********************************************************************************************/
	#if 0
		DeviceInformation_PS_H,					//UUID: 2800, 	VALUE: uuid 180A
		DeviceInformation_pnpID_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_pnpID_DP_H,			//UUID: 2A50,	VALUE: PnPtrs
	#else
		DeviceInformation_PS_H,								//UUID: 2800, 	VALUE: uuid 180A
		DeviceInformation_System_ID_CD_H,					//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_System_ID_DP_H,					//UUID: 2A23,	VALUE:
		DeviceInformation_Model_Number_String_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Model_Number_String_DP_H,			//UUID: 2A24,	VALUE:
		DeviceInformation_Serial_Number_String_CD_H,		//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Serial_Number_String_DP_H,		//UUID: 2A25,	VALUE:
		DeviceInformation_Firmware_Revision_String_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Firmware_Revision_String_DP_H,	//UUID: 2A26,	VALUE:
		DeviceInformation_Hardware_Revision_String_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Hardware_Revision_String_DP_H,	//UUID: 2A27,	VALUE:
		DeviceInformation_Software_Revision_String_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Software_Revision_String_DP_H,	//UUID: 2A28,	VALUE:
		DeviceInformation_Manufacturer_Name_String_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_Manufacturer_Name_String_DP_H,	//UUID: 2A29,	VALUE:
		DeviceInformation_pnpID_CD_H,						//UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_pnpID_DP_H,						//UUID: 2A50,	VALUE: PnPtrs
	#endif

		//// battery service ////
		/**********************************************************************************************/
		BATT_PS_H, 								//UUID: 2800, 	VALUE: uuid 180f
		BATT_LEVEL_INPUT_CD_H,					//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		BATT_LEVEL_INPUT_DP_H,					//UUID: 2A19 	VALUE: batVal
		BATT_LEVEL_INPUT_CCB_H,					//UUID: 2902, 	VALUE: batValCCC
		BATT_LEVEL_INPUT_CTRL_IN_H,

		//// scan para service ////
		/**********************************************************************************************/
		SCAN_PS_H, 								//UUID: 2800, 	VALUE: uuid 180f
		SCAN_INPUT_CD_H,					//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		SCAN_INPUT_DP_H,					//UUID: 2A19 	VALUE: batVal

		//// HID ////
		/**********************************************************************************************/
		HID_PS_H, 								//UUID: 2800, 	VALUE: uuid 1812

		//include
		HID_INCLUDE_H,							//UUID: 2802, 	VALUE: include


		//control point
		HID_CONTROL_POINT_CD_H,					//UUID: 2803, 	VALUE:  			Prop: write_without_rsp
		HID_CONTROL_POINT_DP_H,					//UUID: 2A4C 	VALUE: controlPoint

		//hid information
		HID_INFORMATION_CD_H,					//UUID: 2803, 	VALUE:  			Prop: read
		HID_INFORMATION_DP_H,					//UUID: 2A4A 	VALUE: hidInformation

		// report map
		HID_REPORT_MAP_CD_H,					//UUID: 2803, 	VALUE:  			Prop: Read
		HID_REPORT_MAP_DP_H,					//UUID: 2A4B, 	VALUE: reportKeyIn
		HID_REPORT_MAP_EXT_REF_H,				//UUID: 2907 	VALUE: extService


		//keyboard report in
		HID_NORMAL_KB_REPORT_INPUT_CD_H,		//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_NORMAL_KB_REPORT_INPUT_DP_H,		//UUID: 2A4D, 	VALUE: reportKeyIn
		HID_NORMAL_KB_REPORT_INPUT_CCB_H,		//UUID: 2902, 	VALUE: reportKeyInInCCC
		HID_NORMAL_KB_REPORT_INPUT_REF_H, 		//UUID: 2908    VALUE: REPORT_ID_KEYBOARD, TYPE_INPUT

	//	//keyboard report out
	//	HID_NORMAL_KB_REPORT_OUTPUT_CD_H,		//UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
	//	HID_NORMAL_KB_REPORT_OUTPUT_DP_H,  		//UUID: 2A4D, 	VALUE: reportKeyOut
	//	HID_NORMAL_KB_REPORT_OUTPUT_REF_H, 		//UUID: 2908    VALUE: REPORT_ID_KEYBOARD, TYPE_OUTPUT


	#if(HID_BOOT_ATT_EN)
		//protocol
		HID_PROTOCOL_MODE_CD_H,					//UUID: 2803, 	VALUE:  			Prop: read | write_without_rsp
		HID_PROTOCOL_MODE_DP_H,					//UUID: 2A4E,	VALUE: protocolMode

		//boot keyboard input report
		HID_BOOT_KB_REPORT_INPUT_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_BOOT_KB_REPORT_INPUT_DP_H,			//UUID: 2A22, 	VALUE: bootKeyInReport
		HID_BOOT_KB_REPORT_INPUT_CCB_H,			//UUID: 2902, 	VALUE: bootKeyInReportCCC

		//boot keyboard output report
		HID_BOOT_KB_REPORT_OUTPUT_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
		HID_BOOT_KB_REPORT_OUTPUT_DP_H,		    //UUID: 2A32, 	VALUE: bootKeyOutReport
	#endif
		//consume report in
		HID_CONSUME_REPORT_INPUT_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_CONSUME_REPORT_INPUT_DP_H,			//UUID: 2A4D, 	VALUE: reportConsumerIn
		HID_CONSUME_REPORT_INPUT_CCB_H,			//UUID: 2902, 	VALUE: reportConsumerInCCC
		HID_CONSUME_REPORT_INPUT_REF_H, 		//UUID: 2908    VALUE: REPORT_ID_CONSUMER, TYPE_INPUT


	#if 1
		//amazon_report audio data In
		HID_AMAZON_AUDIO_DATA_REPORT_INPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_AMAZON_AUDIO_DATA_REPORT_INPUT_DP_H,	//UUID: 2A4D, 	VALUE: amazon_report_audio_data_In
		HID_AMAZON_AUDIO_DATA_REPORT_INPUT_CCB_H,	//UUID: 2902, 	VALUE: amazon_report_audio_data_InCCC
		HID_AMAZON_AUDIO_DATA_REPORT_INPUT_REF_H, 	//UUID: 2908    VALUE: amazon_reportRefaudiodataIn, TYPE_INPUT

		//amazon_report audio config In7
		HID_AMAZON_AUDIO_CONFIG_REPORT_INPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_AMAZON_AUDIO_CONFIG_REPORT_INPUT_DP_H,	//UUID: 2A4D, 	VALUE: amazon_report_audio_config_In
		HID_AMAZON_AUDIO_CONFIG_REPORT_INPUT_CCB_H,	//UUID: 2902, 	VALUE: amazon_report_audio_config_InCCC
		HID_AMAZON_AUDIO_CONFIG_REPORT_INPUT_REF_H, //UUID: 2908    VALUE: amazon_reportRefaudioconfigIn, TYPE_INPUT

		//amazon_report audio state Out
		HID_AMAZON_AUDIO_STATE_REPORT_OUNPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
		HID_AMAZON_AUDIO_STATE_REPORT_OUNPUT_DP_H,  	//UUID: 2A4D, 	VALUE: amazon_report_audio_state_Out
		HID_AMAZON_AUDIO_STATE_REPORT_OUNPUT_REF_H, 	//UUID: 2908    VALUE: amazon_reportRefaudiostateOut, TYPE_OUTPUT
	#endif



		//amazon_report diag state Out
		HID_AMAZON_DIAG_STATE_REPORT_OUNPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
		HID_AMAZON_DIAG_STATE_REPORT_OUNPUT_DP_H,  	//UUID: 2A4D, 	VALUE: amazon_report_audio_state_Out
		HID_AMAZON_DIAG_STATE_REPORT_OUNPUT_REF_H, 	//UUID: 2908    VALUE: amazon_reportRefaudiostateOut, TYPE_OUTPUT










		//// Ota ////
		/**********************************************************************************************/
		OTA_PS_H, 								//UUID: 2800, 	VALUE: telink ota service uuid
		OTA_CMD_OUT_CD_H,						//UUID: 2803, 	VALUE:  			Prop: read | write_without_rsp
		OTA_CMD_OUT_DP_H,						//UUID: telink ota uuid,  VALUE: otaData
		OTA_CMD_OUT_DESC_H,						//UUID: 2901, 	VALUE: otaName



	#if (BLE_AUDIO_ENABLE)
		//// Audio ////
		/**********************************************************************************************/
		AUDIO_PS_H, 							//UUID: 2800, 	VALUE: telink audio service uuid

		//mic
		AUDIO_MIC_INPUT_CD_H,					//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		AUDIO_MIC_INPUT_DP_H,					//UUID: telink mic uuid,  VALUE: micData
		AUDIO_MIC_INPUT_CCB_H,					//UUID: 2A19 	VALUE: micDataCCC
		AUDIO_MIC_INPUT_DESC_H,					//UUID: 2901, 	VALUE: micName

		//speaker
		AUDIO_SPEAKER_OUT_CD_H,					//UUID: 2803, 	VALUE:  			Prop: write_without_rsp
		AUDIO_SPEAKER_OUT_DP_H,					//UUID: telink speaker uuid,  VALUE: speakerData
		AUDIO_SPEAKEROUT_DESC_H,				//UUID: 2901, 	VALUE: speakerName
	#endif



		ATT_END_H,

	}ATT_HANDLE;

#endif




#if 1
#define UART_PRINT_DEBUG_ENABLE			0
/////////////////////////////////////// PRINT DEBUG INFO ///////////////////////////////////////
#if (UART_PRINT_DEBUG_ENABLE)
	//the baud rate should not bigger than 1M(system timer clock is constant 16M)
	#define PRINT_BAUD_RATE             					1000000		//1000000
	#define DEBUG_INFO_TX_PIN           					GPIO_PA0
	#define PULL_WAKEUP_SRC_PA0         					PM_PIN_PULLUP_10K
	#define PA0_OUTPUT_ENABLE         						1
	#define PA0_DATA_OUT                                    1 			//must

	#include "../../drivers/B91/gpio.h"
	static inline void uart_io_init(void){
		gpio_set_gpio_en(DEBUG_INFO_TX_PIN);
		gpio_set_output_en(DEBUG_INFO_TX_PIN, 1);
		gpio_set_up_down_res(DEBUG_INFO_TX_PIN, PULL_WAKEUP_SRC_PA0);
		gpio_write(DEBUG_INFO_TX_PIN,PA0_DATA_OUT);
	}

#else
	static inline void uart_io_init(void){}
#endif
#endif




// Hardware board select
#define HW_C1T219A20_V1_0_64_EVB		1
#define HW_C1T217A20_V1_0_48_EVB		2
#define HW_C1T213A20_V1_0_80_EVB		3
#define HARDWARE_BOARD_SELECT			HW_C1T213A20_V1_0_80_EVB


#if BLT_APP_LED_ENABLE
#define     UI_LED_ENABLE           1
#else
#define     UI_LED_ENABLE           1
#endif

#if UI_LED_ENABLE
	// LED GPIO
	#define GPIO_LED_BLUE		GPIO_PB4
	#define GPIO_LED_GREEN		GPIO_PB5
	#define GPIO_LED_WHITE		GPIO_PB6

	#define PB6_FUNC				AS_GPIO
	#define PB5_FUNC				AS_GPIO
//	#define PB4_FUNC				AS_GPIO

	#define	PB6_OUTPUT_ENABLE		1
	#define PB5_OUTPUT_ENABLE		1
//	#define	PB4_OUTPUT_ENABLE		1

	#if (HARDWARE_BOARD_SELECT == HW_C1T219A20_V1_0_64_EVB)
		#define GPIO_LED_RED		GPIO_PB7
		#define PB7_FUNC			AS_GPIO
		#define	PB7_OUTPUT_ENABLE		1
	#elif (HARDWARE_BOARD_SELECT == HW_C1T217A20_V1_0_48_EVB)
		#define GPIO_LED_RED		GPIO_PB2
		#define PB2_FUNC			AS_GPIO
		#define	PB2_OUTPUT_ENABLE		1
	#elif (HARDWARE_BOARD_SELECT == HW_C1T213A20_V1_0_80_EVB)
		#define GPIO_LED_RED		GPIO_PB7
		#define PB7_FUNC			AS_GPIO
		#define	PB7_OUTPUT_ENABLE		1
	#endif


	#define LED_ON_LEVAL 			1 		//gpio output high voltage to turn on led

#endif


#if (BLT_APP_LED_ENABLE)
	#define	GPIO_LED				GPIO_LED_WHITE
#endif







/////////////////// Timing_CLOCK  //////////////////////////////
#define	Timing_CLOCK_10uS		10
#define	Timing_CLOCK_500uS		500
#define	Timing_CLOCK_800uS		800
#define	Timing_CLOCK_1mS		1000
#define	Timing_CLOCK_10mS		10000
#define	Timing_CLOCK_50mS		50000
#define	Timing_CLOCK_100mS		100000
#define	Timing_CLOCK_150mS		150000
#define	Timing_CLOCK_200mS		200000
#define	Timing_CLOCK_300mS		300000
#define	Timing_CLOCK_400mS		400000
#define	Timing_CLOCK_500mS		500000
#define	Timing_CLOCK_800mS		800000
#define	Timing_CLOCK_1S			1000000
#define	Timing_CLOCK_1500mS		1500000
#define	Timing_CLOCK_2S			2000000
#define	Timing_CLOCK_3S			3000000





#define DEBUG_GPIO_ENABLE							0

#if(DEBUG_GPIO_ENABLE)


#if (HARDWARE_BOARD_SELECT == HW_C1T219A20_V1_0_64_EVB)



#elif (HARDWARE_BOARD_SELECT == HW_C1T217A20_V1_0_48_EVB)



#elif (HARDWARE_BOARD_SELECT == HW_C1T213A20_V1_0_80_EVB)
	#define GPIO_CHN0							GPIO_PE1
	#define GPIO_CHN1							GPIO_PE2
//	#define GPIO_CHN2							GPIO_PA0
	#define GPIO_CHN3							GPIO_PA4
	#define GPIO_CHN4							GPIO_PA3
	#define GPIO_CHN5							GPIO_PB0
	#define GPIO_CHN6							GPIO_PB2
	#define GPIO_CHN7							GPIO_PE0

	#define GPIO_CHN8							GPIO_PA2
	#define GPIO_CHN9							GPIO_PA1
	#define GPIO_CHN10							GPIO_PB1
	#define GPIO_CHN11							GPIO_PB3

	//PDx bug(baoyi)
	#define GPIO_CHN12							GPIO_PD7
	#define GPIO_CHN13							GPIO_PD6
	#define GPIO_CHN14							GPIO_PD5
	#define GPIO_CHN15							GPIO_PD4


	#define PE1_OUTPUT_ENABLE					1
	#define PE2_OUTPUT_ENABLE					1
//	#define PA0_OUTPUT_ENABLE					1
	#define PA4_OUTPUT_ENABLE					1
	#define PA3_OUTPUT_ENABLE					1
	#define PB0_OUTPUT_ENABLE					1
	#define PB2_OUTPUT_ENABLE					1
	#define PE0_OUTPUT_ENABLE					1

	#define PA2_OUTPUT_ENABLE					1
	#define PA1_OUTPUT_ENABLE					1
	#define PB1_OUTPUT_ENABLE					1
	#define PB3_OUTPUT_ENABLE					1
#endif


	#define DBG_CHN0_LOW		gpio_write(GPIO_CHN0, 0)
	#define DBG_CHN0_HIGH		gpio_write(GPIO_CHN0, 1)
	#define DBG_CHN0_TOGGLE		gpio_toggle(GPIO_CHN0)
	#define DBG_CHN1_LOW		gpio_write(GPIO_CHN1, 0)
	#define DBG_CHN1_HIGH		gpio_write(GPIO_CHN1, 1)
	#define DBG_CHN1_TOGGLE		gpio_toggle(GPIO_CHN1)
	#define DBG_CHN2_LOW		//gpio_write(GPIO_CHN2, 0)
	#define DBG_CHN2_HIGH		//gpio_write(GPIO_CHN2, 1)
	#define DBG_CHN2_TOGGLE		//gpio_toggle(GPIO_CHN2)
	#define DBG_CHN3_LOW		gpio_write(GPIO_CHN3, 0)
	#define DBG_CHN3_HIGH		gpio_write(GPIO_CHN3, 1)
	#define DBG_CHN3_TOGGLE		gpio_toggle(GPIO_CHN3)
	#define DBG_CHN4_LOW		gpio_write(GPIO_CHN4, 0)
	#define DBG_CHN4_HIGH		gpio_write(GPIO_CHN4, 1)
	#define DBG_CHN4_TOGGLE		gpio_toggle(GPIO_CHN4)
	#define DBG_CHN5_LOW		gpio_write(GPIO_CHN5, 0)
	#define DBG_CHN5_HIGH		gpio_write(GPIO_CHN5, 1)
	#define DBG_CHN5_TOGGLE		gpio_toggle(GPIO_CHN5)
	#define DBG_CHN6_LOW		gpio_write(GPIO_CHN6, 0)
	#define DBG_CHN6_HIGH		gpio_write(GPIO_CHN6, 1)
	#define DBG_CHN6_TOGGLE		gpio_toggle(GPIO_CHN6)
	#define DBG_CHN7_LOW		gpio_write(GPIO_CHN7, 0)
	#define DBG_CHN7_HIGH		gpio_write(GPIO_CHN7, 1)
	#define DBG_CHN7_TOGGLE		gpio_toggle(GPIO_CHN7)
	#define DBG_CHN8_LOW		gpio_write(GPIO_CHN8, 0)
	#define DBG_CHN8_HIGH		gpio_write(GPIO_CHN8, 1)
	#define DBG_CHN8_TOGGLE		gpio_toggle(GPIO_CHN8)
	#define DBG_CHN9_LOW		gpio_write(GPIO_CHN9, 0)
	#define DBG_CHN9_HIGH		gpio_write(GPIO_CHN9, 1)
	#define DBG_CHN9_TOGGLE		gpio_toggle(GPIO_CHN9)
	#define DBG_CHN10_LOW		gpio_write(GPIO_CHN10, 0)
	#define DBG_CHN10_HIGH		gpio_write(GPIO_CHN10, 1)
	#define DBG_CHN10_TOGGLE	gpio_toggle(GPIO_CHN10)
	#define DBG_CHN11_LOW		gpio_write(GPIO_CHN11, 0)
	#define DBG_CHN11_HIGH		gpio_write(GPIO_CHN11, 1)
	#define DBG_CHN11_TOGGLE	gpio_toggle(GPIO_CHN11)
#else
	#define DBG_CHN0_LOW
	#define DBG_CHN0_HIGH
	#define DBG_CHN0_TOGGLE
	#define DBG_CHN1_LOW
	#define DBG_CHN1_HIGH
	#define DBG_CHN1_TOGGLE
	#define DBG_CHN2_LOW
	#define DBG_CHN2_HIGH
	#define DBG_CHN2_TOGGLE
	#define DBG_CHN3_LOW
	#define DBG_CHN3_HIGH
	#define DBG_CHN3_TOGGLE
	#define DBG_CHN4_LOW
	#define DBG_CHN4_HIGH
	#define DBG_CHN4_TOGGLE
	#define DBG_CHN5_LOW
	#define DBG_CHN5_HIGH
	#define DBG_CHN5_TOGGLE
	#define DBG_CHN6_LOW
	#define DBG_CHN6_HIGH
	#define DBG_CHN6_TOGGLE
	#define DBG_CHN7_LOW
	#define DBG_CHN7_HIGH
	#define DBG_CHN7_TOGGLE
	#define DBG_CHN8_LOW
	#define DBG_CHN8_HIGH
	#define DBG_CHN8_TOGGLE
	#define DBG_CHN9_LOW
	#define DBG_CHN9_HIGH
	#define DBG_CHN9_TOGGLE
	#define DBG_CHN10_LOW
	#define DBG_CHN10_HIGH
	#define DBG_CHN10_TOGGLE
	#define DBG_CHN11_LOW
	#define DBG_CHN11_HIGH
	#define DBG_CHN11_TOGGLE
#endif  //end of DEBUG_GPIO_ENABLE

#include "../common/default_config.h"

