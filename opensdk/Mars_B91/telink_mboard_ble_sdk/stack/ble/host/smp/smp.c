/********************************************************************************************************
 * @file	smp.c
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



/**************************************************
 * 	used for distribute key in order.
 */
_attribute_data_retention_	u8 smpDistirbuteKeyOrder = 0;
_attribute_data_retention_	smp_keyDistribution_t smp_DistributeKeyInit ;
_attribute_data_retention_	smp_keyDistribution_t smp_DistributeKeyResp ;


_attribute_data_retention_	u32 smp_timeout_start_tick = 0;

_attribute_data_retention_  u8  smpPkShftCnt = 0;//if SC_PK, is used to count 20 repetitions over SMP.

_attribute_data_retention_	smp_init_handler_t		 func_smp_init = NULL;
_attribute_data_retention_	smp_info_handler_t		 func_smp_info = NULL;
//_attribute_data_retention_	smp_bond_clean_handler_t  func_bond_check_clean = NULL;

_attribute_data_retention_	smp_check_handler_t		 func_smp_check = NULL;




#if (SECURE_CONNECTION_ENABLE)
_attribute_data_retention_  smp_sc_cmd_handler_t 	 func_smp_sc_proc = NULL;
_attribute_data_retention_	smp_sc_pushPkt_handler_t func_smp_sc_pushPkt_proc = NULL;
#endif


_attribute_data_retention_	_attribute_aligned_(4) smp_mng_t  blc_smpMng = {
	Unauthenticated_Paring_with_Encryption,   // security_level
	SMP_BONDING_DEVICE_MAX_NUM, 			  // bonding_maxNum
	Bondable_Mode, 							  // bonding_mode
	IO_CAPABILITY_NO_IN_NO_OUT,         	  // io capability
	non_debug_mode,                           // ecdh keys use non-debug mode if use SC
};


/*
 *	smp Pairing own random
 * */
//
#if (DEBUG_PAIRING_ENCRYPTION)
_attribute_data_retention_	u8	smpOwnRand_const[16] = {0x28,0xCF,0x90,0x14, 0xDB,0xE2,0x9C,0x7A, 0x35,0x3B,0xF5,0x67, 0x48,0x6E,0x84,0xC1};
#else
_attribute_data_retention_	u8	smpOwnRand_const[16] = {0};  
#endif

/*
 *	smp Pairing own confirm
 * */
_attribute_data_retention_	u8	smpOwnParingConfirm[16] = {0};

/*
 *	smp   Pairing own IRK , save in little endian format.
 * */
_attribute_data_retention_
u8 smpParingOwnIRK[16] = {0x97, 0x74, 0x24, 0x67, 0x62, 0x42, 0x81, 0x14, 0x57, 0x20, 0x42, 0x53, 0x32, 0x37, 0x32, 0x74};

/*
 *	smp   Pairing own CSK
 * */
_attribute_data_retention_
u8 smpParingOwnCSRK[16] = {0x33, 0x21, 0x12, 0x34, 0x29, 0x78, 0x64, 0x54, 0x56, 0x07, 0x82, 0x58, 0x09, 0x79, 0x86, 0x19};

/*
 *  SMP Phase stage double check
 */
_attribute_data_retention_	u32	smp_phase_record;


//_attribute_data_retention_   //TODO: when need smp_ir, put it in retention area
u8 smp_ir[16]  = {0x4c, 0x87, 0xD3, 0x34, 0xE4, 0x49, 0x7B, 0x6A, 0x5F, 0x57, 0x03, 0x70, 0x99, 0x84, 0x56, 0xDE};

//_attribute_data_retention_     //TODO: when need smp_er, put it in retention area
u8 smp_er[16]  = {0x6A, 0x42, 0x7D, 0xBE, 0x06, 0x7B, 0x65, 0xC6, 0x1F, 0xBC, 0xA1, 0x26, 0xA7, 0x72, 0x39, 0x43};

//_attribute_data_retention_     //TODO: when need smp_irk, put it in retention area
u8 smp_irk[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


//_attribute_data_retention_     //TODO: when need smp_dhk, put it in retention area
u8 smp_dhk[16] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 *	smp responder signal packet.
 * */

_attribute_data_retention_
smp2llcap_type_t smpResSignalPkt = {
		0x02,		// type
		0x15,		//rf len
		0x0011, //l2cap len
		0x0006,  // l2chn id
};




_attribute_data_retention_	_attribute_aligned_(4) smp_param_peer_t   	smp_param_peer;
_attribute_data_retention_	_attribute_aligned_(4) smp_param_own_t		smp_param_own;



// H: Initiator Capabilities
// V: Responder Capabilities
// See the Core_v5.0(Vol 3/Part H/2.3.5.1) for more information.
static const stk_generationMethod_t gen_method_legacy[5 /*Responder*/][5 /*Initiator*/] = {
	{ JustWorks,      			JustWorks,       		  PK_Resp_Dsply_Init_Input, JustWorks, PK_Resp_Dsply_Init_Input },
	{ JustWorks,      			JustWorks,       		  PK_Resp_Dsply_Init_Input, JustWorks, PK_Resp_Dsply_Init_Input },
	{ PK_Init_Dsply_Resp_Input, PK_Init_Dsply_Resp_Input, PK_BOTH_INPUT,   			JustWorks, PK_Init_Dsply_Resp_Input },
	{ JustWorks,      			JustWorks,       		  JustWorks,      			JustWorks, JustWorks   			    },
	{ PK_Init_Dsply_Resp_Input, PK_Init_Dsply_Resp_Input, PK_Resp_Dsply_Init_Input, JustWorks, PK_Init_Dsply_Resp_Input },
};

#if SECURE_CONNECTION_ENABLE
static const stk_generationMethod_t gen_method_sc[5 /*Responder*/][5 /*Initiator*/] = {
	{ JustWorks,      			JustWorks,       		  PK_Resp_Dsply_Init_Input, JustWorks, PK_Resp_Dsply_Init_Input },
	{ JustWorks,      			Numric_Comparison,        PK_Resp_Dsply_Init_Input, JustWorks, Numric_Comparison },
	{ PK_Init_Dsply_Resp_Input, PK_Init_Dsply_Resp_Input, PK_BOTH_INPUT,   			JustWorks, PK_Init_Dsply_Resp_Input },
	{ JustWorks,      			JustWorks,       		  JustWorks,      			JustWorks, JustWorks   			    },
	{ PK_Init_Dsply_Resp_Input, Numric_Comparison, 		  PK_Resp_Dsply_Init_Input, JustWorks, Numric_Comparison },
};
#endif



/**************************************************
 * 	used for set smp responder packet data and return .
 */
// @@@@@  length can optimize to save ramcode
u8* blc_smp_pushParingFailed(u8 failReason)
{
	smpResSignalPkt.l2capLen = 0x02;  				//l2cap_len
	smpResSignalPkt.data[0] = failReason;			//confirm value failed
	smpResSignalPkt.opcode = SMP_OP_PAIRING_FAIL;
	smpResSignalPkt.rf_len = smpResSignalPkt.l2capLen + 4;
	return (u8*)&smpResSignalPkt;
}










int blc_smp_isWaitingToSetPasskeyEntry(void)
{
	u8 is_master = 0; //Only slave role supported, //connHandle & BLM_CONN_HANDLE;

	if((is_master && blc_smpMng.stk_method == PK_Resp_Dsply_Init_Input) || \
	  ((!is_master) && blc_smpMng.stk_method == PK_Init_Dsply_Resp_Input) || \
	  blc_smpMng.stk_method == PK_BOTH_INPUT){
		if(!(blc_smpMng.tk_status & TK_ST_UPDATE)){
			if (blc_smpMng.tk_status & TK_ST_REQUEST){
				return 1;
			}
		}
	}

	return 0;
}


int blc_smp_isWaitingToCfmNumericComparison(void)
{
	//Only slave role supported, //connHandle & BLM_CONN_HANDLE;
	if(blc_smpMng.stk_method == Numric_Comparison && \
	   (blc_smpMng.tk_status & TK_ST_NUMERIC_COMPARE) && \
	  (!(blc_smpMng.tk_status & TK_ST_NUMERIC_CHECK_YES)) && \
	  (!(blc_smpMng.tk_status & TK_ST_NUMERIC_CHECK_NO))){
		return 1;
	}

	return 0;
}


int  blc_smp_setTK_by_PasskeyEntry (u32 pinCodeInput)
{

	if(blc_smpMng.tk_status & TK_ST_REQUEST){
		memset(smp_param_own.paring_tk, 0, 16);
		if(pinCodeInput <= 999999){ //0~999999
			memcpy(smp_param_own.paring_tk, &pinCodeInput, 4);
			blc_smpMng.tk_status |= TK_ST_UPDATE;

			return 1;
		}
	}

	return 0;
}


void blc_smp_setTK_by_OOB (u8 *oobData)
{
	if(blc_smpMng.tk_status & TK_ST_REQUEST){
		memcpy(smp_param_own.paring_tk, oobData, 16);
		blc_smpMng.tk_status |= TK_ST_UPDATE;
	}
}


void blc_smp_setNumericComparisonResult(bool YES_or_NO)
{
	if(blc_smpMng.tk_status & TK_ST_NUMERIC_COMPARE){
		if(YES_or_NO){
			blc_smpMng.tk_status |= TK_ST_NUMERIC_CHECK_YES;
		}
		else{
			blc_smpMng.tk_status |= TK_ST_NUMERIC_CHECK_NO;
		}
	}
}





/*************************************************
 * 	used for set MAX key size,
Note: don not provide this API to user, We set all keySize to default 16.
 * */
void blc_smp_setMaxKeySize (u8 maxKeySize)
{
	if((maxKeySize > 6) && (maxKeySize < 17)){
		smp_param_own.paring_rsp.maxEncrySize = maxKeySize;
	}
	return ;
}

/*
 * API used for set distribute key enable.
 * */
//only for stack, so return value type can keep same as eagle.(eagle-void;kite/vul-smp_keyDistribution_t)
void blc_smp_setInitiatorKey (u8 LTK_distributeEn, u8 IRK_distributeEn, u8 CSRK_DistributeEn)
{
	smp_keyDistribution_t initKey ;
	initKey.keyIni = 0;
	initKey.encKey = LTK_distributeEn ? 1 : 0 ;
	initKey.idKey = IRK_distributeEn ? 1 : 0 ;
	initKey.sign = CSRK_DistributeEn ? 1 : 0 ;


	smp_param_own.paring_req.initKeyDistribution.keyIni = initKey.keyIni;
	smp_param_own.paring_req.rspKeyDistribution.keyIni = initKey.keyIni;
}

/*
 * API used for set distribute key enable.
 * */
//only for stack, so return value type can keep same as eagle.(eagle-void;kite/vul-smp_keyDistribution_t)
void blc_smp_setResponderKey (u8 LTK_distributeEn, u8 IRK_distributeEn, u8 CSRK_DistributeEn)
{
	smp_keyDistribution_t rspKey ;
	rspKey.keyIni = 0;
	rspKey.encKey = LTK_distributeEn ? 1 : 0 ;
	rspKey.idKey = IRK_distributeEn ? 1 : 0 ;
	rspKey.sign = CSRK_DistributeEn ? 1 : 0 ;

	smp_param_own.paring_rsp.initKeyDistribution.keyIni = rspKey.keyIni;
	smp_param_own.paring_rsp.rspKeyDistribution.keyIni = rspKey.keyIni;
}







void blc_smp_param_setBondingDeviceMaxNumber ( int device_num)
{
	blc_smpMng.bonding_maxNum = device_num > SMP_BONDING_DEVICE_MAX_NUM ? SMP_BONDING_DEVICE_MAX_NUM : device_num;
}




void blc_smp_setSecurityLevel(le_security_mode_level_t  mode_level)
{
	blc_smpMng.security_level = mode_level;
}


void blc_smp_setBondingMode(bonding_mode_t mode)
{
	blc_smpMng.bonding_mode = mode;
}



void blc_smp_setParingMethods (paring_methods_t  method)
{
#if (SECURE_CONNECTION_ENABLE)
	blc_smpMng.paring_method = method;

	if (method == LE_Secure_Connection) // if support secure connection feature
	{
		func_smp_sc_proc 		 = blc_smp_sc_handler;
		func_smp_sc_pushPkt_proc = blc_smp_sc_pushPkt_handler;
		blt_ecc_init(); //previous kite/vulture not exist this line code. eagle has that. 
	}
#endif
}


void blc_smp_enableOobAuthentication (int OOB_en)
{
	blc_smpMng.oob_enable = OOB_en;
}


void blc_smp_enableAuthMITM (int MITM_en)
{
	blc_smpMng.MITM_protetion = MITM_en;
}

/*************************************************
 * 	used for set IO capability
 * */
void blc_smp_setIoCapability (io_capability_t ioCapablility)
{
	blc_smpMng.IO_capability = ioCapablility;
}

/*************************************************
 * 	used for enable keypress flag
 */
void blc_smp_enableKeypress (int keyPress_en)
{
#if (SECURE_CONNECTION_ENABLE)
	blc_smpMng.keyPress_en = keyPress_en;
#endif
}

/*************************************************
 * 	used for ECDH debug mode select
 */
void blc_smp_setEcdhDebugMode(ecdh_keys_mode_t mode)
{
#if (SECURE_CONNECTION_ENABLE)
	blc_smpMng.ecdh_debug_mode = mode;
#endif
}

void blc_smp_setSecurityParameters (  bonding_mode_t mode,
		                             int MITM_en, int OOB_en, int keyPress_en,
									 io_capability_t 	ioCapablility)
{
	blc_smpMng.bonding_mode = mode;
	blc_smpMng.oob_enable = OOB_en;
	blc_smpMng.MITM_protetion = MITM_en;
	blc_smpMng.IO_capability = ioCapablility;
#if (SECURE_CONNECTION_ENABLE)
	blc_smpMng.keyPress_en = keyPress_en;
#endif
}




/****************************************************************************************************
 *    API below is only available for BLE stack, user can not call !!!!!
 ***************************************************************************************************/
//only for stack,so parameter type can be changed(from bonding_mode_t to eagle_u8), compatible
void blc_stack_smp_setBondingMode(u8 mode) //'mode' type: bonding_mode_t
{
	smp_param_own.auth_req.bondingFlag = mode;
	smp_param_own.paring_rsp.authReq.bondingFlag = mode;
	smp_param_own.paring_req.authReq.bondingFlag = mode;
}


//only for stack,so parameter type can be changed(from paring_methods_t to eagle_u8), compatible
void blc_stack_smp_setParingMethods (u8  method) //'method' type: paring_methods_t
{
#if (SECURE_CONNECTION_ENABLE)
	smp_param_own.auth_req.SC = method;
	smp_param_own.paring_rsp.authReq.SC = method;
	smp_param_own.paring_req.authReq.SC = method;
#endif


}


void blc_stack_smp_enableOobAuthentication (int OOB_en)
{
	smp_param_own.paring_req.oobDataFlag = OOB_en;
	smp_param_own.paring_rsp.oobDataFlag = OOB_en;
}


void blc_stack_smp_enableAuthMITM (int MITM_en)
{

	smp_param_own.auth_req.MITM = MITM_en;
	smp_param_own.paring_rsp.authReq.MITM = MITM_en;
	smp_param_own.paring_req.authReq.MITM = MITM_en;
}

/*************************************************
 * 	used for set IO capability
 * */
//only for stack,so parameter type can be changed(from io_capability_t to eagle_u8), compatible
void blc_stack_smp_setIoCapability (u8 ioCapablility) //'ioCapablility' type: io_capability_t
{
	smp_param_own.paring_req.ioCapablity = ioCapablility;
	smp_param_own.paring_rsp.ioCapablity = ioCapablility;
}

/*************************************************
 * 	used for enable keypress flag
 */
void blc_stack_smp_enableKeypress (int keyPress_en)
{
#if (SECURE_CONNECTION_ENABLE)
	smp_param_own.auth_req.keyPress = keyPress_en;

	smp_param_own.paring_rsp.authReq.keyPress = keyPress_en;
	smp_param_own.paring_req.authReq.keyPress = keyPress_en;
#endif
}
//only for stack,so parameter type can be changed(from paring_methods_t to eagle_u8), compatible
int 	blc_stack_smp_setSecurityParameters (u8 mode, u8 method, u8 MITM_en, u8 OOB_en, u8 ioCapablility, u8 keyPress_en)
{
/*
	u8 bondingFlag : 2;
	u8 MITM : 1;
	u8 SC	: 1;
	u8 keyPress: 1;
	u8 rsvd: 3;
*/
	u8 temp = mode | MITM_en<<2 | method<<3 | keyPress_en<<4;
	smp_param_own.auth_req.authType = temp;
	smp_param_own.paring_rsp.authReq.authType = temp;
	smp_param_own.paring_req.authReq.authType = temp;

	smp_param_own.paring_req.oobDataFlag = OOB_en;
	smp_param_own.paring_rsp.oobDataFlag = OOB_en;

	smp_param_own.paring_req.ioCapablity = ioCapablility;
	smp_param_own.paring_rsp.ioCapablity = ioCapablility;

	return 1;
}
/***************************************************************************************************/










/*
 * Return STK generate method.
 * See the Core_v5.0(Vol 3/Part H/2.3.5.1) for more information.
 * */
int blc_smp_getGenMethod (int SC_en)
{
	int responder_MITM = smp_param_own.paring_rsp.authReq.MITM;
	int responder_oob = smp_param_own.paring_rsp.oobDataFlag;
	int responder_iocap = smp_param_own.paring_rsp.ioCapablity;

	int initiator_MITM = smp_param_own.paring_req.authReq.MITM;
	int initiator_oob = smp_param_own.paring_req.oobDataFlag;
	int initiator_iocap = smp_param_own.paring_req.ioCapablity;

	if (SC_en + responder_oob + initiator_oob >= 2)  //simplify the judge: 2 of them is true, OOB is available
	{
		return OOB_Authentication;
	}

	if(!responder_MITM && !initiator_MITM)
	{
		return JustWorks;
	}

	if(responder_iocap > IO_CAPABILITY_KEYBOARD_DISPLAY || initiator_iocap > IO_CAPABILITY_KEYBOARD_DISPLAY)	//data[0] io cap
	{
		return JustWorks;
	}


#if (SECURE_CONNECTION_ENABLE)
	if(SC_en)
	{
		return gen_method_sc[responder_iocap][initiator_iocap];
	}
	else
#endif
	{
		return gen_method_legacy[responder_iocap][initiator_iocap];
	}

}






//IR & ER will be(and must be) same every time for a specific device(public address same)
void blc_smp_generate_initial_keys(void)
{

	u16 temp[16] = {0};

	//TODO: improvement: temp comes form flash UUID later, in case that public mac not exist  initial
	blc_ll_readBDAddr((u8 *)temp);

	aes_ll_encryption((u8 *)temp, smp_ir, smp_ir);  //16M clock,  95 us first call(no cache code)
	aes_ll_encryption((u8 *)temp, smp_er, smp_er); //16M clock,  48 us second call(some cache code)


/*********************************************************************************************************
	d1(k, d, r) = e(k, d')

	IRK = d1(IR, 1, 0)
	DHK = d1(IR, 3, 0)

    IRK: d' = padding || r || d = 0x00...00 0000 0001
    DHK: d' = padding || r || d = 0x00...00 0000 0011

	d is concatenated with r and padding to generate d', which is used as the 128-bit input parameter
plaintextData to security function e:
						d' = padding || r || d
	The least significant octet of d becomes the least significant octet of d' and the most significant
octet of padding becomes the most significant octet of d'.

    For example, if the 16-bit value d is 0x1234 and the 16-bit value r is 0xabcd,
then d' is 0x000000000000000000000000abcd1234.
 ********************************************************************************************************/

	aes_ll_encryption(smp_ir, smp_irk, smp_irk);
	aes_ll_encryption(smp_ir, smp_dhk, smp_dhk);

}






/*************************************************
 * 	@brief 		used for reset smp param to default value.
 */
int blc_smp_param_init (void)
{

	//memset (smp_param_own.paring_tk, 0, 16);  //default value is 0, no need do it to save code size


	smp_param_own.paring_req.code = SMP_OP_PAIRING_REQ;
	smp_param_own.paring_rsp.code = SMP_OP_PAIRING_RSP;


	smp_param_own.paring_req.maxEncrySize = 16;
	smp_param_own.paring_rsp.maxEncrySize = 16;

	blc_smp_setResponderKey(1, 1, 0);  //@@@ to do: when data signing OK later, add CSRK here
	blc_smp_setInitiatorKey(1, 1, 0);

	blc_smpMng.passKeyEntryDftTK=0;		//clear default TK

	// conFig all SMP parameters according to application's setting
	blc_stack_smp_setSecurityParameters( blc_smpMng.bonding_mode, blc_smpMng.paring_method, blc_smpMng.MITM_protetion,
										blc_smpMng.oob_enable,   blc_smpMng.IO_capability, blc_smpMng.keyPress_en);



	return 0;
}


/*************************************************
 *	When using the PasskeyEntry, set the default pincode
 *	displayed by our side and input by the other side.
 *	Note: If it is not the above method, setting the pincode is useless
 */
void blc_smp_setDefaultPinCode(u32 pinCodeInput)
{
	blc_smpMng.passKeyEntryDftTK=pinCodeInput|BIT(31);
}




#if 0

typedef enum {
	Smp_None		 = 0,
	Smp_Peripheral   = BIT(0),
	Smp_Central      = BIT(1),
}smp_role_t;

int blc_smp_init (smp_role_t  role)
{
	if(role & Smp_Peripheral){
		blc_smp_peripheral_init();
	}

	if(role & Smp_Central){
		blc_smp_central_init();
	}


	if(blc_smpMng.security_level != No_Authentication_No_Encryption)
	{
		//generate IR & ER for key distribution
		//blc_smp_generate_initial_keys();  //TODO: when adding own device RPA feature, IRK should be calculated.

		blc_smp_param_init();
	}
}
#endif








