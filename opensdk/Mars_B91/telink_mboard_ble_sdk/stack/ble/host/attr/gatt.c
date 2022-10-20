/********************************************************************************************************
 * @file	gatt.c
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






const u8 att_access_err_no_enc[4][4] = {
	{ATT_SUCCESS, 				 ATT_SUCCESS,					ATT_SUCCESS, 					ATT_SUCCESS,},
	{ATT_ERR_INSUFFICIENT_AUTH,  ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,},
	{ATT_ERR_INSUFFICIENT_AUTH,  ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,},
	{ATT_ERR_INSUFFICIENT_AUTH,  ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,	ATT_ERR_INSUFFICIENT_ENCRYPT,},
};

const u8 att_access_err_enc[4][4] = {
	{ATT_SUCCESS, 				 ATT_SUCCESS,					ATT_SUCCESS, 					ATT_SUCCESS,},
	{ATT_SUCCESS,  				 ATT_SUCCESS,					ATT_SUCCESS,					ATT_SUCCESS,},
	{ATT_SUCCESS,  				 ATT_ERR_INSUFFICIENT_AUTH,		ATT_SUCCESS,					ATT_SUCCESS,},
	{ATT_SUCCESS,  				 ATT_ERR_INSUFFICIENT_AUTH,		ATT_ERR_INSUFFICIENT_AUTH,		ATT_SUCCESS,},
};










u8 blc_gatt_requestServiceAccess(u16 connHandle, int gatt_perm)
{

	u8 access_require = 0;
	if(gatt_perm & ATT_PERMISSIONS_SECURE_CONN){
		access_require = 3;
	}
	else if( gatt_perm & ATT_PERMISSIONS_AUTHEN ){
		access_require = 2;
	}
	else if( gatt_perm & ATT_PERMISSIONS_ENCRYPT ){
		access_require = 1;
	}
	else{
		return ATT_SUCCESS;
	}


	u8 paring_status = (u8) bls_smp_get_paring_statas(BLS_CONN_HANDLE);

	if(bls_ll_isConnectionEncrypted(BLS_CONN_HANDLE)){
		return att_access_err_enc[access_require][paring_status];
	}
	else{
		return att_access_err_no_enc[access_require][paring_status];
	}

}





#if (HOST_CONTROLLER_DATA_FLOW_IMPROVE_EN)




ble_sts_t  blc_gatt_pushHandleValueNotify (u16 connHandle, u16 attHandle, u8 *p, int len)
{

	if( blc_smp_isParingBusy() ){
		return 	SMP_ERR_PAIRING_BUSY;
	}
	//If the attribute value is longer than (ATT_MTU-3) octets, peer device's host can not receive whole data
	//e.g. MTU=23, HandValueNotify format_len=3(opcode, attHandle), 20 bytes max
	else if(len > bltAtt.effective_MTU - 3){
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}
	else if(att_service_discover_tick){  //NOTE: this branch must be the last one
		if(clock_time_exceed(att_service_discover_tick, bltAtt.Data_pending_time * 10000)){
			att_service_discover_tick = 0;
		}
		else{
			return 	GATT_ERR_DATA_PENDING_DUE_TO_SERVICE_DISCOVERY_BUSY;
		}
	}



	//HandleValueNotify format
	u8 format[4];
	format[0] = ATT_OP_HANDLE_VALUE_NOTI;
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);

	return blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, p, len);

}



ble_sts_t  blc_gatt_pushHandleValueIndicate (u16 connHandle, u16 attHandle, u8 *p, int len)
{
	if( blc_smp_isParingBusy() ){
		return 	SMP_ERR_PAIRING_BUSY;
	}
	//If the attribute value is longer than (ATT_MTU-3) octets, peer device's host can not receive whole data
	//e.g. MTU=23, HandValueIndicate format_len=3(opcode, attHandle), 20 bytes max
	else if(len > bltAtt.effective_MTU - 3){
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}
	else if(blt_indicate_handle){
		return GATT_ERR_PREVIOUS_INDICATE_DATA_HAS_NOT_CONFIRMED;
	}
	else if(att_service_discover_tick){   //NOTE: this branch must be the last one
		if(clock_time_exceed(att_service_discover_tick, bltAtt.Data_pending_time * 10000)){
			att_service_discover_tick = 0;
		}
		else{
			return 	GATT_ERR_DATA_PENDING_DUE_TO_SERVICE_DISCOVERY_BUSY;
		}
	}



	//HandleValueNotify format
	u8 format[4];
	format[0] = ATT_OP_HANDLE_VALUE_IND;
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);

	u8 api_status = (u8)blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, p, len);
	if(api_status == BLE_SUCCESS){
		blt_indicate_handle = attHandle;
	}

	return api_status;

}








ble_sts_t 	blc_gatt_pushWriteComand (u16 connHandle, u16 attHandle, u8 *p, int len)
{
	if( blc_smp_isParingBusy() ){
		return 	SMP_ERR_PAIRING_BUSY;
	}
	//If the attribute value is longer than (ATT_MTU-3) octets, peer device's host can not receive whole data
	//e.g. MTU=23, WriteCommand format_len=3(opcode, attHandle), 20 bytes max
	else if(len > bltAtt.effective_MTU - 3){
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}
	//WriteCommand format
	u8 format[4];
	format[0] = ATT_OP_WRITE_CMD;
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);


	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, p, len);

	return api_status;
}


ble_sts_t 	blc_gatt_pushWriteRequest (u16 connHandle, u16 attHandle, u8 *p, int len)
{
	if( blc_smp_isParingBusy() ){
		return 	SMP_ERR_PAIRING_BUSY;
	}
	//If the attribute value is longer than (ATT_MTU-3) octets, peer device's host can not receive whole data
	//e.g. MTU=23, WriteRequest format_len=3(opcode, attHandle), 20 bytes max
	else if(len > bltAtt.effective_MTU - 3){
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}
	//WriteCommand format
	u8 format[4];
	format[0] = ATT_OP_WRITE_REQ;
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);


	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, p, len);

	return api_status;
}

ble_sts_t blc_gatt_pushFindInformationRequest(u16 connHandle, u16 start_attHandle, u16 end_attHandle)
{
	if( blc_smp_isParingBusy() ){
		return 	SMP_ERR_PAIRING_BUSY;
	}

	u8 format[12];
	format[0] = ATT_OP_FIND_INFO_REQ;//Payload
	format[1] = U16_LO(start_attHandle);
	format[2] = U16_HI(start_attHandle);
	format[3] = U16_LO(end_attHandle);
	format[4] = U16_HI(end_attHandle);

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 5, NULL, 0);

	return api_status;
}

ble_sts_t blc_gatt_pushFindByTypeValueRequest(u16 connHandle, u16 start_attHandle, u16 end_attHandle,
		                                      u16 uuid, u8 *attr_value, int len)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}
	else if(attr_value == NULL || len == 0)
	{
		return GATT_ERR_INVALID_PARAMETER;
	}
	else if(len > bltAtt.effective_MTU - 7){
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}

	u8 format[255];
	format[0] = ATT_OP_FIND_BY_TYPE_VALUE_REQ;//Payload
	format[1] = U16_LO(start_attHandle);
	format[2] = U16_HI(start_attHandle);
	format[3] = U16_LO(end_attHandle);
	format[4]= U16_HI(end_attHandle);
	format[5]= U16_LO(uuid);
	format[6]= U16_HI(uuid);

	for(int i=0; i<len; i++){
		format[7+i] = attr_value[i];
	}

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 7+len, NULL, 0);

	return api_status;
}

ble_sts_t blc_gatt_pushReadByTypeRequest(u16 connHandle, u16 start_attHandle, u16 end_attHandle,
		                                 u8 *uuid, int uuid_len)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}
	else if(uuid == NULL || uuid_len == 0)
	{
		return GATT_ERR_INVALID_PARAMETER;
	}

	u8 format[30];
	format[0] = ATT_OP_READ_BY_TYPE_REQ;//Payload
	format[1] = U16_LO(start_attHandle);
	format[2] = U16_HI(start_attHandle);
	format[3] = U16_LO(end_attHandle);
	format[4]= U16_HI(end_attHandle);

	for(int i=0; i<uuid_len; i++){
		format[5 + i] = uuid[i];
	}

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 5+uuid_len, NULL, 0);

	return api_status;
}


ble_sts_t 	blc_gatt_pushReadByGroupTypeRequest(u16 connHandle, u16 start_attHandle, u16 end_attHandle,
		                                        u8 *uuid, int uuid_len)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}
	else if(uuid == NULL || uuid_len == 0)
	{
		return GATT_ERR_INVALID_PARAMETER;
	}
	else if(uuid_len > bltAtt.effective_MTU - 5)
	{
		return GATT_ERR_DATA_LENGTH_EXCEED_MTU_SIZE;
	}

	u8 format[30];

	format[0] = ATT_OP_READ_BY_GROUP_TYPE_REQ;//Payload
	format[1] = U16_LO(start_attHandle);
	format[2] = U16_HI(start_attHandle);
	format[3] = U16_LO(end_attHandle);
	format[4]= U16_HI(end_attHandle);

	for(int i=0; i<uuid_len; i++){
		format[5 + i] = uuid[i];
	}
	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 5+uuid_len, NULL, 0);

	return api_status;
}

ble_sts_t blc_gatt_pushReadRequest(u16 connHandle, u16 attHandle)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}

	u8 format[10];
	format[0] = ATT_OP_READ_REQ;//Payload
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, NULL, 0);

	return api_status;
}

ble_sts_t blc_gatt_pushReadBlobRequest (u16 connHandle, u16 attHandle, u16 offset)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}
	u8 format[4];
	format[0] = ATT_OP_READ_BLOB_REQ;
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);

	u16 tempOffset = offset;

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 3, (u8*)&tempOffset, 2);

	return api_status;
}

ble_sts_t blc_gatt_pushPrepareWriteRequest(u16 connHandle, u16 attHandle, u16 valOffset,
		                                 u8 *data, int data_len)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}
	else if(data == NULL || data_len == 0)
	{
		return GATT_ERR_INVALID_PARAMETER;
	}

	u8 format[30];
	format[0] = ATT_OP_PREPARE_WRITE_REQ;//Payload
	format[1] = U16_LO(attHandle);
	format[2] = U16_HI(attHandle);
	format[3] = U16_LO(valOffset);
	format[4]= U16_HI(valOffset);

	for(int i=0; i<data_len; i++){
		format[5 + i] = data[i];
	}

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 5+data_len, NULL, 0);

	return api_status;
}

ble_sts_t blc_gatt_pushExecuteWriteRequest(u16 connHandle,u8 value)
{
	if(blc_smp_isParingBusy() )
	{
		return 	SMP_ERR_PAIRING_BUSY;
	}

	u8 format[2];
	format[0] = ATT_OP_EXECUTE_WRITE_REQ;//opcode
	format[1] = value;//value

	ble_sts_t api_status = blc_l2cap_pushData_2_controller(connHandle, L2CAP_CID_ATTR_PROTOCOL, format, 2, NULL, 0);

	return api_status;
}

#endif  //end of HOST_CONTROLLER_DATA_FLOW_IMPROVE_EN


