/********************************************************************************************************
 * @file	app_att.c
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
#include "application/audio/audio_config.h"
#include "stack/ble/ble.h"
#include "app_audio.h"

#if (TEST_MODE == AMAZON_RCU_TEST)

#define SERVICE_UUID_SCAN_PARAMETE						0X1813
#define	CHARACTERISTIC_UUID_SCAN_PARAMETE				0x2a4f

#define HID_REPORT_ID_AUDIO_DATA_INPUT             		0xF0   //!< audio data report ID
#define HID_REPORT_ID_AUDIO_CONFIG_INPUT             	0xF1   //!< audio config report ID
#define HID_REPORT_ID_AUDIO_STATE_OUTPUT             	0xF2   //!< audio state report ID
#define HID_REPORT_ID_DIAGNOSTIC_OUTPUT             	0xF3   //!< diagnostic report ID

#define HID_REPORT_ID_BATTERY_INPUT                     3   //!< Battery input report ID

typedef struct
{
  /** Minimum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
  u16 intervalMin;
  /** Maximum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
  u16 intervalMax;
  /** Number of LL latency connection events (0x0000 - 0x03e8) */
  u16 latency;
  /** Connection Timeout (0x000A - 0x0C80 * 10 ms) */
  u16 timeout;
} gap_periConnectParams_t;

static const u16 clientCharacterCfgUUID = GATT_UUID_CLIENT_CHAR_CFG;

static const u16 extReportRefUUID = GATT_UUID_EXT_REPORT_REF;

static const u16 reportRefUUID = GATT_UUID_REPORT_REF;

static const u16 userdesc_UUID	= GATT_UUID_CHAR_USER_DESC;

static const u16 serviceChangeUUID = GATT_UUID_SERVICE_CHANGE;

static const u16 my_primaryServiceUUID = GATT_UUID_PRIMARY_SERVICE;

static const u16 my_characterUUID = GATT_UUID_CHARACTER;

static const u16 my_devServiceUUID = SERVICE_UUID_DEVICE_INFORMATION;

static const u16 my_PnPUUID = CHARACTERISTIC_UUID_PNP_ID;

static const u16 my_devNameUUID = GATT_UUID_DEVICE_NAME;

static const u16 my_gapServiceUUID = SERVICE_UUID_GENERIC_ACCESS;

static const u16 my_appearanceUIID = GATT_UUID_APPEARANCE;

static const u16 my_periConnParamUUID = GATT_UUID_PERI_CONN_PARAM;

static const u16 my_appearance = GAP_APPEARE_UNKNOWN;

static const u16 my_gattServiceUUID = SERVICE_UUID_GENERIC_ATTRIBUTE;

static const gap_periConnectParams_t my_periConnParameters = {16, 16, 49, 800};//{20, 40, 0, 1000};

static u16 serviceChangeVal[2] = {0,0};

static u8 serviceChangeCCC[2] = {0,0};

static const u8 my_devName[] = {'A','R'}; //{'A', 'm', 'a', 'z', 'o', 'n', ' ', 'R', 'e', 'm', 'o', 't', 'e'};



static const u8 PROP_READ = CHAR_PROP_READ;
static const u8 PROP_READ_NOTIFY = CHAR_PROP_READ | CHAR_PROP_NOTIFY;
static const u8 PROP_READ_WRITE_WRITENORSP = CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RSP;


//////////////////////// Device Information /////////////////////////////////////////////////
#if 0
static const u8 	my_PnPtrs[] = {0x02, 0x71, 0x01, 0x13, 0x04, 0x00, 0x00};
#else
static const u16 my_SYSTEM_ID_UUID = CHARACTERISTIC_UUID_SYSTEM_ID;
static const u16 my_MODEL_NUM_STRING_UUID = CHARACTERISTIC_UUID_MODEL_NUM_STRING;
static const u16 my_SERIAL_NUM_STRING_UUID = CHARACTERISTIC_UUID_SERIAL_NUM_STRING;
static const u16 my_FW_REVISION_STRING_UUID = CHARACTERISTIC_UUID_FW_REVISION_STRING;
static const u16 my_HW_REVISION_STRING_UUID = CHARACTERISTIC_UUID_HW_REVISION_STRING;
static const u16 my_SW_REVISION_STRING_UUID = CHARACTERISTIC_UUID_SW_REVISION_STRING;
static const u16 my_MANU_NAME_STRING_UUID = CHARACTERISTIC_UUID_MANU_NAME_STRING;
static const u16 my_PnP_UUID = CHARACTERISTIC_UUID_PNP_ID;

static const u8	my_System_ID[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x00};//{'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', ' '};
static const u8	my_Model_Number_String[] = {'M', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
static const u8	my_Serial_Number_String[] = {'G', '0', 'G', '0', 'U', '4', '0', '8', '9', '4', '0','2','0','Q','T','P'};
static const u8	my_Firmware_Revision_String[] = {'5', '2', '5', '1', '2', '9', '0', '1'};
static const u8	my_Hardware_Revision_String[] = {'P', 'V', 'T'};
static const u8	my_Software_Revision_String[] = {'1', '2', '3'};
static const u8	my_Manufacturer_Name_String[] = {'A', 'm', 'a', 'z', 'o', 'n', ' ', 'R', 'e', 'm', 'o', 't', 'e'};
static const u8 my_PnPtrs[] = {0x02, 0x71, 0x01, 0x13, 0x04, 0x00, 0x00};
#endif


//////////////////////// Battery /////////////////////////////////////////////////
static const u16 my_batServiceUUID        = SERVICE_UUID_BATTERY;
static const u16 my_batCharUUID			  = CHARACTERISTIC_UUID_BATTERY_LEVEL;
static u8 batteryValueInCCC[2];
static u8 my_batVal[1] 					  = {80};

//////////////////////// scan /////////////////////////////////////////////////
static const u16 my_scanParaUUID        = SERVICE_UUID_SCAN_PARAMETE;
static const u16 my_scanCharUUID			  = CHARACTERISTIC_UUID_SCAN_PARAMETE;



//////////////////////// HID /////////////////////////////////////////////////////

static const u16 my_hidServiceUUID        = SERVICE_UUID_HUMAN_INTERFACE_DEVICE;

static const u16 hidProtocolModeUUID      = CHARACTERISTIC_UUID_HID_PROTOCOL_MODE;
static const u16 hidReportUUID            = CHARACTERISTIC_UUID_HID_REPORT;
static const u16 hidReportMapUUID         = CHARACTERISTIC_UUID_HID_REPORT_MAP;
static const u16 hidbootKeyInReportUUID   = CHARACTERISTIC_UUID_HID_BOOT_KEY_INPUT;
static const u16 hidbootKeyOutReportUUID  = CHARACTERISTIC_UUID_HID_BOOT_KEY_OUTPUT;
static const u16 hidinformationUUID       = CHARACTERISTIC_UUID_HID_INFORMATION;
static const u16 hidCtrlPointUUID         = CHARACTERISTIC_UUID_HID_CONTROL_POINT;
static const u16 hidIncludeUUID           = GATT_UUID_INCLUDE;

static u8 protocolMode 			  		  = DFLT_HID_PROTOCOL_MODE;

// Key in Report characteristic variables
static u8 reportKeyIn[8];
static u8 reportKeyInCCC[2];
// HID Report Reference characteristic descriptor, key input
static const u8 reportRefKeyIn[2] =
             { HID_REPORT_ID_KEYBOARD_INPUT, HID_REPORT_TYPE_INPUT };

// Key out Report characteristic variables
static u8 reportKeyOut[1];
static const u8 reportRefKeyOut[2] =
             { HID_REPORT_ID_KEYBOARD_INPUT, HID_REPORT_TYPE_OUTPUT };

// Consumer Control input Report
static u8 reportConsumerControlIn[2];
static u8 reportConsumerControlInCCC[2];
static const u8 reportRefConsumerControlIn[2] =
			 { HID_REPORT_ID_CONSUME_CONTROL_INPUT, HID_REPORT_TYPE_INPUT };

static const u8 reportRefBatteryControlIn[2] =
			 { HID_REPORT_ID_BATTERY_INPUT, HID_REPORT_TYPE_INPUT };

// Boot Keyboard Input Report
static u8 bootKeyInReport;
static u8 bootKeyInReportCCC[2];

// Boot Keyboard Output Report
static u8 bootKeyOutReport;

// HID Information characteristic
static const u8 hidInformation[] =
{
  U16_LO(0x0100), U16_HI(0x0100),             // bcdHID (USB HID version)
  0x00,                                       // bCountryCode
  0x00                                        // Flags
};

// HID Control Point characteristic
static u8 controlPoint;

// HID Report Map characteristic
// Keyboard report descriptor (using format for Boot interface descriptor)
#if 0
static const u8 reportMap[] =
{
	//keyboard report in
	0x05, 0x01,     // Usage Pg (Generic Desktop)
	0x09, 0x06,     // Usage (Keyboard)
	0xA1, 0x01,     // Collection: (Application)
	0x85, HID_REPORT_ID_KEYBOARD_INPUT,     // Report Id (keyboard)
				  //
	0x05, 0x07,     // Usage Pg (Key Codes)
	0x19, 0xE0,     // Usage Min (224)  VK_CTRL:0xe0
	0x29, 0xE7,     // Usage Max (231)  VK_RWIN:0xe7
	0x15, 0x00,     // Log Min (0)
	0x25, 0x01,     // Log Max (1)
				  //
				  // Modifier byte
	0x75, 0x01,     // Report Size (1)   1 bit * 8
	0x95, 0x08,     // Report Count (8)
	0x81, 0x02,     // Input: (Data, Variable, Absolute)
				  //
				  // Reserved byte
	0x95, 0x01,     // Report Count (1)
	0x75, 0x08,     // Report Size (8)
	0x81, 0x01,     // Input: (static constant)

	//keyboard output
	//5 bit led ctrl: NumLock CapsLock ScrollLock Compose kana
	0x95, 0x05,    //Report Count (5)
	0x75, 0x01,    //Report Size (1)
	0x05, 0x08,    //Usage Pg (LEDs )
	0x19, 0x01,    //Usage Min
	0x29, 0x05,    //Usage Max
	0x91, 0x02,    //Output (Data, Variable, Absolute)
	//3 bit reserved
	0x95, 0x01,    //Report Count (1)
	0x75, 0x03,    //Report Size (3)
	0x91, 0x01,    //Output (static constant)

	// Key arrays (6 bytes)
	0x95, 0x06,     // Report Count (6)
	0x75, 0x08,     // Report Size (8)
	0x15, 0x00,     // Log Min (0)
	0x25, 0xF1,     // Log Max (241)
	0x05, 0x07,     // Usage Pg (Key Codes)
	0x19, 0x00,     // Usage Min (0)
	0x29, 0xf1,     // Usage Max (241)
	0x81, 0x00,     // Input: (Data, Array)

	0xC0,            // End Collection

	//consumer report in
	0x05, 0x0C,   // Usage Page (Consumer)
	0x09, 0x01,   // Usage (Consumer Control)
	0xA1, 0x01,   // Collection (Application)
	0x85, HID_REPORT_ID_CONSUME_CONTROL_INPUT,   //     Report Id
	0x75,0x10,     //global, report size 16 bits
	0x95,0x01,     //global, report count 1
	0x15,0x01,     //global, min  0x01
	0x26,0x8c,0x02,  //global, max  0x28c
	0x19,0x01,     //local, min   0x01
	0x2a,0x8c,0x02,  //local, max    0x28c
	0x81,0x00,     //main,  input data varible, absolute
	0xc0,        //main, end collection

};
#else
static const u8 reportMap[] =
{
	//keyboard report in
	0x05, 0x01,     // Usage Pg (Generic Desktop)
	0x09, 0x06,     // Usage (Keyboard)
	0xA1, 0x01,     // Collection: (Application)
	0x05, 0x07,     // Usage Pg (Key Codes)
	0x85, HID_REPORT_ID_KEYBOARD_INPUT,     // Report Id (keyboard)
	0x95, 0x03,     // Report Count (3)
	0x75, 0x08,     // Report Size (8)
	0x15, 0x00,     // Log Min (0)
	0x25, 0xFF,     // Log Max (1)
	0x19, 0x00,     // Usage Min (224)  VK_CTRL:0xe0
	0x29, 0xFF,     // Usage Max (231)  VK_RWIN:0xe7
	0x81, 0x00,     // Input: (Data, Variable, Absolute)
	0xC0,            // End Collection

	//consumer report in
	0x05, 0x0C,   // Usage Page (Consumer)
	0x09, 0x01,   // Usage (Consumer Control)
	0xA1, 0x01,   // Collection (Application)
	0x85, HID_REPORT_ID_CONSUME_CONTROL_INPUT,   //     Report Id
	0x95, 0x02,     //global, report count 2
	0x75, 0x10,     //global, report size 16 bits
	0x15, 0x00,     //global, min  0x00
	0x26, 0x9C, 0x02,  //global, max  0x28c
	0x19, 0x00,     //local, min   0x00
	0x2A, 0x9C, 0x02,  //local, max    0x28c
	0x81, 0x00,     //main,  input data varible, absolute
	0xC0,        //main, end collection

	//audio data report, 0xF0
	0x06, 0x00, 0xFF,  //
	0x09, 0x00,   // Usage (Consumer Control)
	0xA1, 0x01,   // Collection (Application)
	0x85, HID_REPORT_ID_AUDIO_DATA_INPUT,   //     Report Id
	0x95, 0x50,     //global, report count 2
	0x75, 0x08,     //global, report size 16 bits
	0x15, 0x00,     //global, min  0x00
	0x25, 0xFF,     //
	0x81, 0x00,     //main,  input data varible, absolute

	//audio config, 0xF1
	0x85, HID_REPORT_ID_AUDIO_CONFIG_INPUT,   //     Report Id
	0x95, 0x03,     //global, report count 2
	0x75, 0x08,     //global, report size 16 bits
	0x09, 0x00,     //
	0x81, 0x02,     //main,  input data varible, absolute

	//audio state, 0xF2
	0x85, HID_REPORT_ID_AUDIO_STATE_OUTPUT,   //     Report Id
	0x95, 0x01,     //global, report count 2
	0x75, 0x08,     //global, report size 16 bits
	0x09, 0x00,     //
	0x91, 0x02,     //main,  output data varible, absolute

	//diagnostic, 0xF3
	0x85, HID_REPORT_ID_DIAGNOSTIC_OUTPUT,   //     Report Id
	0x95, 0x0A,     //global, report count 2
	0x75, 0x08,     //global, report size 16 bits
	0x09, 0x00,     //
	0x91, 0x02,     //main,  output data varible, absolute
	0xC0,        //main, end collection

	//Battery report
	0x05, 0x0C,   // Usage Page (Consumer)
	0x09, 0x01,   // Usage (Consumer Control)
	0xA1, 0x01,   // Collection (Application)
	0x85, HID_REPORT_ID_BATTERY_INPUT,   //     Report Id
	0x05, 0x01,     //
	0x09, 0x06,     //
	0xA1, 0x02,     //
	0x05, 0x06,     //
	0x09, 0x20,     //
	0x15, 0x00,     //global, min  0x00
	0x26, 0x64, 0x00,  //global, max  0x28c
	0x75, 0x08,     //
	0x95, 0x01,     //
	0x81, 0x02,     //
	0xC0,        //main, end collection
	0xC0,        //main, end collection
};
#endif


// HID External Report Reference Descriptor for report map
static u16 extServiceUUID=0x2a19; //battery level


/////////////////////////////////////////////////////////
static const u8 my_AudioUUID[16]   	= TELINK_AUDIO_UUID_SERVICE;
static const u8 my_MicUUID[16]			= TELINK_MIC_DATA;
static const u8 my_SpeakerUUID[16]		= TELINK_SPEAKER_DATA;
static const u8 my_OtaUUID[16]			= TELINK_SPP_DATA_OTA;
static const u8 my_OtaServiceUUID[16]	= TELINK_OTA_UUID_SERVICE;


static u8 micDataCCC[2];
static u8 my_MicData 		= 0x80;
static u8 my_SpeakerData 	= 0x81;
static u8 my_OtaData 		= 0x00;

static const u8  my_MicName[] = {'M', 'i', 'c'};
static const u8  my_SpeakerName[] = {'S', 'p', 'e', 'a', 'k', 'e', 'r'};
static const u8  my_OtaName[] = {'O', 'T', 'A'};


// Include attribute (Battery service)
static const u16 include[3] = {BATT_PS_H, BATT_LEVEL_INPUT_CCB_H, SERVICE_UUID_BATTERY};


//// GAP attribute values
static const u8 my_devNameCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(GenericAccess_DeviceName_DP_H), U16_HI(GenericAccess_DeviceName_DP_H),
	U16_LO(GATT_UUID_DEVICE_NAME), U16_HI(GATT_UUID_DEVICE_NAME)
};
static const u8 my_appearanceCharVal[5] = {
	CHAR_PROP_READ,
	U16_LO(GenericAccess_Appearance_DP_H), U16_HI(GenericAccess_Appearance_DP_H),
	U16_LO(GATT_UUID_APPEARANCE), U16_HI(GATT_UUID_APPEARANCE)
};
static const u8 my_periConnParamCharVal[5] = {
	CHAR_PROP_READ,
	U16_LO(CONN_PARAM_DP_H), U16_HI(CONN_PARAM_DP_H),
	U16_LO(GATT_UUID_PERI_CONN_PARAM), U16_HI(GATT_UUID_PERI_CONN_PARAM)
};


//// GATT attribute values
static const u8 my_serviceChangeCharVal[5] = {
	CHAR_PROP_INDICATE,
	U16_LO(GenericAttribute_ServiceChanged_DP_H), U16_HI(GenericAttribute_ServiceChanged_DP_H),
	U16_LO(GATT_UUID_SERVICE_CHANGE), U16_HI(GATT_UUID_SERVICE_CHANGE)
};


//// device Information  attribute values
static const u8 my_PnCharVal[5] = {
	CHAR_PROP_READ,
	U16_LO(DeviceInformation_pnpID_DP_H), U16_HI(DeviceInformation_pnpID_DP_H),
	U16_LO(CHARACTERISTIC_UUID_PNP_ID), U16_HI(CHARACTERISTIC_UUID_PNP_ID)
};

#if(HID_BOOT_ATT_EN)
//// HID attribute values
static const u8 my_hidProtocolModeCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RSP,
	U16_LO(HID_PROTOCOL_MODE_DP_H), U16_HI(HID_PROTOCOL_MODE_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_PROTOCOL_MODE), U16_HI(CHARACTERISTIC_UUID_HID_PROTOCOL_MODE)
};


static const u8 my_hidbootKeyInReporCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(HID_BOOT_KB_REPORT_INPUT_DP_H), U16_HI(HID_BOOT_KB_REPORT_INPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_BOOT_KEY_INPUT), U16_HI(CHARACTERISTIC_UUID_HID_BOOT_KEY_INPUT)
};
static const u8 my_hidbootKeyOutReporCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RSP,
	U16_LO(HID_BOOT_KB_REPORT_OUTPUT_DP_H), U16_HI(HID_BOOT_KB_REPORT_OUTPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_BOOT_KEY_OUTPUT), U16_HI(CHARACTERISTIC_UUID_HID_BOOT_KEY_OUTPUT)
};
#endif

static const u8 my_hidReportCCinCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(HID_CONSUME_REPORT_INPUT_DP_H), U16_HI(HID_CONSUME_REPORT_INPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_REPORT), U16_HI(CHARACTERISTIC_UUID_HID_REPORT)
};
static const u8 my_hidReportKEYinCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(HID_NORMAL_KB_REPORT_INPUT_DP_H), U16_HI(HID_NORMAL_KB_REPORT_INPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_REPORT), U16_HI(CHARACTERISTIC_UUID_HID_REPORT)
};
//static const u8 my_hidReportKEYoutCharVal[5] = {
//	CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RSP,
//	U16_LO(HID_NORMAL_KB_REPORT_OUTPUT_DP_H), U16_HI(HID_NORMAL_KB_REPORT_OUTPUT_DP_H),
//	U16_LO(CHARACTERISTIC_UUID_HID_REPORT), U16_HI(CHARACTERISTIC_UUID_HID_REPORT)
//};
static const u8 my_hidReportMapCharVal[5] = {
	CHAR_PROP_READ,
	U16_LO(HID_REPORT_MAP_DP_H), U16_HI(HID_REPORT_MAP_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_REPORT_MAP), U16_HI(CHARACTERISTIC_UUID_HID_REPORT_MAP)
};
static const u8 my_hidinformationCharVal[5] = {
	CHAR_PROP_READ,
	U16_LO(HID_INFORMATION_DP_H), U16_HI(HID_INFORMATION_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_INFORMATION), U16_HI(CHARACTERISTIC_UUID_HID_INFORMATION)
};
static const u8 my_hidCtrlPointCharVal[5] = {
	CHAR_PROP_WRITE_WITHOUT_RSP,
	U16_LO(HID_CONTROL_POINT_DP_H), U16_HI(HID_CONTROL_POINT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_HID_CONTROL_POINT), U16_HI(CHARACTERISTIC_UUID_HID_CONTROL_POINT)
};


// amazon audio data in Report characteristic variables
u8 amazon_report_audio_data_In[165];
u8 amazon_report_audio_data_InCCC[2];
// HID Report Reference characteristic descriptor, key input
static u8 amazon_reportRefaudiodataIn[2] =
			 { HID_REPORT_ID_AUDIO_DATA_INPUT, HID_REPORT_TYPE_INPUT };

// amazon audio config in Report characteristic variables
u8 amazon_report_audio_config_In[4];
u8 amazon_report_audio_config_InCCC[2];
// HID Report Reference characteristic descriptor, key input
static u8 amazon_reportRefaudioconfigIn[2] =
			 { HID_REPORT_ID_AUDIO_CONFIG_INPUT, HID_REPORT_TYPE_INPUT };

// amazon audio state out Report characteristic variables
u8 amazon_report_audio_state_Out[1];
static u8 amazon_reportRefaudiostateOut[2] =
			 { HID_REPORT_ID_AUDIO_STATE_OUTPUT, HID_REPORT_TYPE_OUTPUT };

u8 amazon_report_diagnostic_state_Out[1];
static u8 amazon_reportRefdiagnosticstateOut[2] =
			 { HID_REPORT_ID_DIAGNOSTIC_OUTPUT, HID_REPORT_TYPE_OUTPUT };



//// Battery attribute values
static const u8 my_batCharVal[5] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(BATT_LEVEL_INPUT_DP_H), U16_HI(BATT_LEVEL_INPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_BATTERY_LEVEL), U16_HI(CHARACTERISTIC_UUID_BATTERY_LEVEL)
};

//// scan attribute values
static const u8 my_scanparaCharVal[5] = {
	CHAR_PROP_READ ,
	U16_LO(SCAN_INPUT_DP_H), U16_HI(SCAN_INPUT_DP_H),
	U16_LO(CHARACTERISTIC_UUID_SCAN_PARAMETE), U16_HI(CHARACTERISTIC_UUID_SCAN_PARAMETE)
};


//// OTA attribute values
static const u8 my_OtaCharVal[19] = {
	CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RSP,
	U16_LO(OTA_CMD_OUT_DP_H), U16_HI(OTA_CMD_OUT_DP_H),
	TELINK_SPP_DATA_OTA,
};


#if (BLE_AUDIO_ENABLE)
//// Audio attribute values
static const u8 my_MicCharVal[19] = {
	CHAR_PROP_READ | CHAR_PROP_NOTIFY,
	U16_LO(AUDIO_MIC_INPUT_DP_H), U16_HI(AUDIO_MIC_INPUT_DP_H),
	TELINK_MIC_DATA,
};
static const u8 my_SpeakerCharVal[19] = {
	CHAR_PROP_WRITE_WITHOUT_RSP,
	U16_LO(AUDIO_SPEAKER_OUT_DP_H), U16_HI(AUDIO_SPEAKER_OUT_DP_H),
	TELINK_SPEAKER_DATA,
};
#endif

extern int amazon_0xF2_client_to_server_receivedata(rf_packet_att_write_t *p);
extern int amazon_diag_client_to_server_receivedata(void *p);

// TM : to modify
static const attribute_t my_Attributes[] = {

	{ATT_END_H - 1, 0,0,0,0,0},	// total num of attribute



	// 0001 - 0004 gatt
	{4,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_gattServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_serviceChangeCharVal),(u8*)(&my_characterUUID), 		(u8*)(my_serviceChangeCharVal), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (serviceChangeVal), (u8*)(&serviceChangeUUID), 	(u8*)(&serviceChangeVal), 0},
	{0,ATT_PERMISSIONS_RDWR,2,sizeof (serviceChangeCCC),(u8*)(&clientCharacterCfgUUID), (u8*)(serviceChangeCCC), 0},


	// 0004 - 000B  gap  @ganeric access
	{7,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_gapServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_devNameCharVal),(u8*)(&my_characterUUID), (u8*)(my_devNameCharVal), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_devName), (u8*)(&my_devNameUUID), (u8*)(my_devName), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_appearanceCharVal),(u8*)(&my_characterUUID), (u8*)(my_appearanceCharVal), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_appearance), (u8*)(&my_appearanceUIID), 	(u8*)(&my_appearance), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_periConnParamCharVal),(u8*)(&my_characterUUID), (u8*)(my_periConnParamCharVal), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_periConnParameters),(u8*)(&my_periConnParamUUID), 	(u8*)(&my_periConnParameters), 0},





	// 000C - 001C  device Information Service
#if 0
	{3,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_devServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_PnCharVal),(u8*)(&my_characterUUID), (u8*)(my_PnCharVal), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_PnPtrs),(u8*)(&my_PnPUUID), (u8*)(my_PnPtrs), 0},
#else
	{17,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_devServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_PnPtrs),(u8*)(&my_PnP_UUID), (u8*)(my_PnPtrs), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Manufacturer_Name_String),(u8*)(&my_MANU_NAME_STRING_UUID), (u8*)(my_Manufacturer_Name_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Software_Revision_String),(u8*)(&my_SW_REVISION_STRING_UUID), (u8*)(my_Software_Revision_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Hardware_Revision_String),(u8*)(&my_HW_REVISION_STRING_UUID), (u8*)(my_Hardware_Revision_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Firmware_Revision_String),(u8*)(&my_FW_REVISION_STRING_UUID), (u8*)(my_Firmware_Revision_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Serial_Number_String),(u8*)(&my_SERIAL_NUM_STRING_UUID), (u8*)(my_Serial_Number_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_Model_Number_String),(u8*)(&my_MODEL_NUM_STRING_UUID), (u8*)(my_Model_Number_String), 0},
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_System_ID),(u8*)(&my_SYSTEM_ID_UUID), (u8*)(my_System_ID), 0},
#endif


	////////////////////////////////////// Battery Service /////////////////////////////////////////////////////
	// 001D - 0021
	{5,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_batServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_batCharVal),(u8*)(&my_characterUUID), (u8*)(my_batCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_batVal),(u8*)(&my_batCharUUID), 	(u8*)(my_batVal), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(batteryValueInCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(batteryValueInCCC), 0},	//value
	{0,ATT_PERMISSIONS_READ,2,sizeof(reportRefBatteryControlIn),(u8*)(&reportRefUUID), 	(u8*)(reportRefBatteryControlIn), 0},	//


	////////////////////////////////////// scan paramete Service /////////////////////////////////////////////////////
	// 0022 - 0024
	{3,ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_scanParaUUID), 0},
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_scanparaCharVal),(u8*)(&my_characterUUID), (u8*)(my_scanparaCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_batVal),(u8*)(&my_scanCharUUID), 	(u8*)(my_batVal), 0},	//value


	/////////////////////////////////// 4. HID Service /////////////////////////////////////////////////////////
	// 0025
	//{27, ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_hidServiceUUID), 0},
	{HID_AMAZON_DIAG_STATE_REPORT_OUNPUT_REF_H - HID_PS_H + 1, ATT_PERMISSIONS_READ,2,2,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_hidServiceUUID), 0},

	// 0026  include battery service
	{0,ATT_PERMISSIONS_READ,2,sizeof(include),(u8*)(&hidIncludeUUID), 	(u8*)(include), 0},

	// 0026 - 0028 . control point: 2
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidCtrlPointCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidCtrlPointCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_WRITE,2, sizeof(controlPoint),(u8*)(&hidCtrlPointUUID), 	(u8*)(&controlPoint), 0},	//value

	// 0029 - 002A . hid information: 2
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidinformationCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidinformationCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2, sizeof(hidInformation),(u8*)(&hidinformationUUID), 	(u8*)(hidInformation), 0},	//value

	// 002B - 002D . report map: 3
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidReportMapCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidReportMapCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2,sizeof(reportMap),(u8*)(&hidReportMapUUID), 	(u8*)(reportMap), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(extServiceUUID),(u8*)(&extReportRefUUID), 	(u8*)(&extServiceUUID), 0},	//value


	// 002E - 0031 . keyboard report in : 4 (char-val-client-ref)
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidReportKEYinCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidReportKEYinCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2, sizeof(reportKeyIn),(u8*)(&hidReportUUID), 	(u8*)(reportKeyIn), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(reportKeyInCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(reportKeyInCCC), 0},	//value
	{0,ATT_PERMISSIONS_READ,2,sizeof(reportRefKeyIn),(u8*)(&reportRefUUID), 	(u8*)(reportRefKeyIn), 0},	//value

//	// 0020 - 0022 . keyboard report out: 3 (char-val-ref)
//	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidReportKEYoutCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidReportKEYoutCharVal), 0},				//prop
//	{0,ATT_PERMISSIONS_RDWR,2,sizeof(reportKeyOut),(u8*)(&hidReportUUID), 	(u8*)(reportKeyOut), 0},	//value
//	{0,ATT_PERMISSIONS_READ,2,sizeof(reportRefKeyOut),(u8*)(&reportRefUUID), 	(u8*)(reportRefKeyOut), 0},	//value

#if(HID_BOOT_ATT_EN)
	// 0011 - 0012  protocol mode
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidProtocolModeCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidProtocolModeCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_RDWR,2, sizeof(protocolMode),(u8*)(&hidProtocolModeUUID), 	(u8*)(&protocolMode), 0},	//value

	// 0013 - 0015  boot keyboard input report (char-val-client)
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidbootKeyInReporCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidbootKeyInReporCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2,sizeof(bootKeyInReport),(u8*)(&hidbootKeyInReportUUID), 	(u8*)(&bootKeyInReport), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(bootKeyInReportCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(bootKeyInReportCCC), 0},	//value

	// 0016 - 0017   boot keyboard output report (char-val)
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidbootKeyOutReporCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidbootKeyOutReporCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_RDWR,2, sizeof(bootKeyOutReport), (u8*)(&hidbootKeyOutReportUUID), 	(u8*)(&bootKeyOutReport), 0},	//value

#endif

	// 0032 - 0035. consume report in: 4 (char-val-client-ref)
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_hidReportCCinCharVal),(u8*)(&my_characterUUID), (u8*)(my_hidReportCCinCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2, sizeof(reportConsumerControlIn),(u8*)(&hidReportUUID), 	(u8*)(reportConsumerControlIn), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(reportConsumerControlInCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(reportConsumerControlInCCC), 0},	//value
	{0,ATT_PERMISSIONS_READ,2,sizeof(reportRefConsumerControlIn),(u8*)(&reportRefUUID), 	(u8*)(reportRefConsumerControlIn), 0},	//value



#if 1
	// 0036 - 0039.			amazon_report audio data In
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ_NOTIFY), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2, sizeof(amazon_report_audio_data_In),(u8*)(&hidReportUUID), 	(u8*)(amazon_report_audio_data_In), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_report_audio_data_InCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(amazon_report_audio_data_InCCC), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_reportRefaudiodataIn),(u8*)(&reportRefUUID), 	(u8*)(amazon_reportRefaudiodataIn), 0},	//value

	// 003A - 003D.			amazon_report audio config In
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ_NOTIFY), 0},				//prop
	{0,ATT_PERMISSIONS_READ,2, sizeof(amazon_report_audio_config_In),(u8*)(&hidReportUUID), 	(u8*)(amazon_report_audio_config_In), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_report_audio_config_InCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(amazon_report_audio_config_InCCC), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_reportRefaudioconfigIn),(u8*)(&reportRefUUID), 	(u8*)(amazon_reportRefaudioconfigIn), 0},	//value

	// 003E - 0040.			amazon_report audio state Out
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ_WRITE_WRITENORSP), 0},				//prop
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_report_audio_state_Out),(u8*)(&hidReportUUID), 	(u8*)(amazon_report_audio_state_Out), amazon_0xF2_client_to_server_receivedata},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_reportRefaudiostateOut),(u8*)(&reportRefUUID), 	(u8*)(amazon_reportRefaudiostateOut), 0},	//value
#endif


	// 0041 - 0043.
	{0,ATT_PERMISSIONS_READ,2,1,(u8*)(&my_characterUUID), 		(u8*)(&PROP_READ_WRITE_WRITENORSP), 0},				//prop
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_report_audio_state_Out),(u8*)(&hidReportUUID), 	(u8*)(amazon_report_diagnostic_state_Out), amazon_diag_client_to_server_receivedata},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(amazon_reportRefaudiostateOut),(u8*)(&reportRefUUID), 	(u8*)(amazon_reportRefdiagnosticstateOut), 0},	//value




	////////////////////////////////////// OTA /////////////////////////////////////////////////////
	// 0044 - 0047
	{4,ATT_PERMISSIONS_READ, 2,16,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_OtaServiceUUID), 0},
	{0,ATT_PERMISSIONS_READ, 2, sizeof(my_OtaCharVal),(u8*)(&my_characterUUID), (u8*)(my_OtaCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_RDWR,16,sizeof(my_OtaData),(u8*)(&my_OtaUUID),	(&my_OtaData), &otaWrite, NULL},			//value
	{0,ATT_PERMISSIONS_READ, 2,sizeof (my_OtaName),(u8*)(&userdesc_UUID), (u8*)(my_OtaName), 0},


#if (BLE_AUDIO_ENABLE)
	////////////////////////////////////// Audio /////////////////////////////////////////////////////
	// 0048 Audio
	{8,ATT_PERMISSIONS_READ,2,16,(u8*)(&my_primaryServiceUUID), 	(u8*)(&my_AudioUUID), 0},

	// 0049 - 004C  MIC
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_MicCharVal),(u8*)(&my_characterUUID), 		(u8*)(my_MicCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_READ,16,sizeof(my_MicData),(u8*)(&my_MicUUID), 	(u8*)(&my_MicData), 0},	//value
	{0,ATT_PERMISSIONS_RDWR,2,sizeof(micDataCCC),(u8*)(&clientCharacterCfgUUID), 	(u8*)(micDataCCC), 0},	//value
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_MicName),(u8*)(&userdesc_UUID), (u8*)(my_MicName), 0},

	// 004D - 004F  SPEAKER
	{0,ATT_PERMISSIONS_READ,2,sizeof(my_SpeakerCharVal),(u8*)(&my_characterUUID), 		(u8*)(my_SpeakerCharVal), 0},				//prop
	{0,ATT_PERMISSIONS_WRITE,16,sizeof(my_SpeakerData),(u8*)(&my_SpeakerUUID), 	(u8*)(&my_SpeakerData), 0},//value
	{0,ATT_PERMISSIONS_READ,2,sizeof (my_SpeakerName),(u8*)(&userdesc_UUID), (u8*)(my_SpeakerName), 0},
#endif

};

void	my_att_init (void)
{
	bls_att_setAttributeTable ((u8 *)my_Attributes);
}
#endif //end of  (TEST_MODE == AMAZON_RCU_TEST)

