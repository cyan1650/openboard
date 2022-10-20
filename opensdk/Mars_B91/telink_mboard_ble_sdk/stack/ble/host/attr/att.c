/********************************************************************************************************
 * @file	att.c
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
#include "stack/ble/service/uuid.h"


/* temporary reservation for device name*/

///////////////////////////  ATT  protocol  ///////////////////////////

#define		ATT_MTU_SIZE_MAX_LEN	(l2cap_buff.max_rx_size - 6)

#if(DATA_NO_INIT_EN) //only slave use these variable.
									//prepare write buffer (3byte rsvd to save length info)
_attribute_data_no_init_	u8		blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX] = {0};//dft useful fifo size: 253
#else
									//prepare write buffer (3byte rsvd to save length info)
_attribute_data_retention_	u8		blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX] = {0};//dft useful fifo size: 253
#endif

_attribute_data_retention_	u8		*pAppPrepareWriteBuff = NULL;
_attribute_data_retention_	u16		pAppPrepareWrite_max_len = 0;
_attribute_data_retention_	u16		prepare_pkt_current_len = 0;


_attribute_data_retention_	u32 	att_service_discover_tick = 0;


_attribute_data_retention_
att_para_t bltAtt = {
   .init_MTU = ATT_MTU_SIZE,
   .effective_MTU = ATT_MTU_SIZE,   //default min 23 byte
   .pPendingPkt = NULL,
   .Data_pending_time = 30,   //300ms default
   .mtu_exchange_pending = 0,
};



#define  MAX_DEV_NAME_LEN 				18

#ifndef DEV_NAME
	#define DEV_NAME                        "tModule"
#endif


_attribute_data_retention_	attribute_t* gAttributes = 0;


_attribute_data_retention_	u16	blt_indicate_handle;

_attribute_data_retention_	u8  ble_devName[MAX_DEV_NAME_LEN] = DEV_NAME;


void bls_att_setAttributeTable (u8 *p)
{
	if (p) {
		gAttributes = (attribute_t *)p;
	}
}



//u8 *pDevice2MasterData = SppDataClient2ServerData;

/////////////////////////////////////////////////////////////////////////

#define LL_RF_RESERVED_LEN                          4


// LL Header Bit Mask
#define LL_HDR_LLID_MASK                            0x03
#define LL_HDR_NESN_MASK                            0x04
#define LL_HDR_SN_MASK                              0x08
#define LL_HDR_MD_MASK                              0x10

#define LL_PDU_HDR_LLID_RESERVED                    0
#define LL_PDU_HDR_LLID_DATA_PKT_NEXT               1
#define LL_PDU_HDR_LLID_DATA_PKT_FIRST              2
#define LL_PDU_HDR_LLID_CONTROL_PKT                 3

// Macro to judgement the LL data type
#define IS_PACKET_LL_DATA(p)                        ((p & LL_HDR_LLID_MASK) != LL_PDU_HDR_LLID_CONTROL_PKT)
#define IS_PACKET_LL_CTRL(p)                        ((p & LL_HDR_LLID_MASK) == LL_PDU_HDR_LLID_CONTROL_PKT)
#define IS_PACKET_LL_INVALID(p)                     ((p & LL_HDR_LLID_MASK) == LL_PDU_HDR_LLID_RESERVED)


_attribute_data_retention_
rf_packet_att_errRsp_t pkt_errRsp = {
	0x02,									// type
	sizeof(rf_packet_att_errRsp_t) - 2,		// rf_len
	sizeof(rf_packet_att_errRsp_t) - 6,   	// l2cap_len
	4, 										// chanId
	ATT_OP_ERROR_RSP,						// opcode
	0,										// errOpcode
	0,										// errHandle
	ATT_ERR_ATTR_NOT_FOUND, // errReason
};



_attribute_data_retention_
rf_packet_att_mtu_t pkt_mtu_rsp = {		//  spec 4.1 ,  3.4.7.1 Handle Value Notification
	0x02,										// type
	sizeof(rf_packet_att_mtu_t) - 2,			// rf_len
	sizeof(rf_packet_att_mtu_t) - 6,			// l2cap_len
	4,											// chanId
	ATT_OP_EXCHANGE_MTU_RSP,
	{ATT_MTU_SIZE, 0}
};

_attribute_data_retention_ u8 *hid_reportmap_psrc=NULL;
_attribute_data_retention_ int hid_reportmap_len=0;

static inline int uuid_match(u8 uuidLen, u8* uuid1, u8* uuid2){
	if(2 == uuidLen  && uuid1[0] == uuid2[0] && uuid1[1] == uuid2[1]){
		return 1;
	}

	u32* uuid1_temp = (u32*)uuid1;//todo if uuid len of att small than 128bit
	u32* uuid2_temp = (u32*)uuid2;

	if(16 == uuidLen  && (uuid1_temp[0] == uuid2_temp[0]) && (uuid1_temp[1] == uuid2_temp[1])&&(uuid1_temp[2] == uuid2_temp[2]) &&(uuid1_temp[3] == uuid2_temp[3])){
		return 1;
	}
	return 0;
}

attribute_t* l2cap_att_search(u16 sh, u16 eh, u8 *attUUID, u16 *h){
	if(sh > gAttributes[0].attNum) return 0; // change from "=" to ">", find by tainxian, review qinghua,yafei
	
	eh = eh < gAttributes[0].attNum ? eh : gAttributes[0].attNum;
	while(sh <= eh)
	{
		attribute_t* pAtt = &gAttributes[sh];
		if(uuid_match(pAtt->uuidLen, pAtt->uuid, attUUID))
		{
			*h = sh;
			return pAtt;
		}
		++sh;
	}
	return 0;
}


u16 att_Find_end_group(u16 sh, u16 eh, u16 uuid)
{
	u16 s = sh+1;
	if(s >= gAttributes[0].attNum) return 0;
	eh = eh < gAttributes[0].attNum ? eh : gAttributes[0].attNum;

	while(s <= eh)
	{
		attribute_t* pAtt = &gAttributes[s];

		if(uuid==GATT_UUID_CHARACTER)
		{
			if((*(u16*)pAtt->uuid== GATT_UUID_CHARACTER) || (*(u16*)pAtt->uuid== GATT_UUID_SECONDARY_SERVICE) \
					|| (*(u16*)pAtt->uuid== GATT_UUID_PRIMARY_SERVICE))
			{
				return (s-1);
			}
		}
		else
		{
			if((*(u16*)pAtt->uuid== GATT_UUID_SECONDARY_SERVICE) || (*(u16*)pAtt->uuid== GATT_UUID_PRIMARY_SERVICE))
			{
				return (s-1);
			}
		}
		++s;
	}

	return gAttributes[0].attNum;//return the handle of end att

}

attribute_t* att_find_by_type_value_search(u16 sh, u16 eh, u8 *attUUID, u8 *value, u16 len, u16 *ret_fh, u16 *ret_geh){

	if(sh > gAttributes[0].attNum) return 0;// change from "=" to ">", find by tainxian, review qinghua,yafei
	eh = eh < gAttributes[0].attNum ? eh : gAttributes[0].attNum;

	while(sh <= eh)
	{
		attribute_t* pAtt = &gAttributes[sh];

		if(uuid_match(2, pAtt->uuid, attUUID))// attribute type 2bytes
		{
			if(len<= pAtt->attrLen)
			{
				if(!memcmp(value, pAtt->pAttrValue, len))
				{
					if((*(u16*)attUUID == GATT_UUID_PRIMARY_SERVICE) || (*(u16*)attUUID == GATT_UUID_SECONDARY_SERVICE)\
							||(*(u16*)attUUID == GATT_UUID_CHARACTER))
					{
						*ret_geh   = att_Find_end_group(sh, gAttributes[0].attNum, *(u16*)attUUID);
					}
					else
					{
						*ret_geh = sh;
					}

					*ret_fh = sh;
					return pAtt;
				}
			}

		}
		++sh;
	}
	return 0;
}

//static
u16 lacap_att_service_search(u16 handle)
{
	u16 ser_handle = handle;
	u16 primaryServiceUUID = GATT_UUID_PRIMARY_SERVICE;
	if(handle == 0||!memcmp(gAttributes[handle].uuid,&primaryServiceUUID,2)) return 0;
	while(ser_handle > 0)
	{
		attribute_t* pAtt = &gAttributes[ser_handle];
		if(pAtt->uuidLen==2)
		{
			if(uuid_match(pAtt->uuidLen, pAtt->uuid, (u8*)(&primaryServiceUUID)))
			{
				return ser_handle;
			}
		}
		ser_handle--;
	}

	return 0;
}

attribute_t* att_read_by_group_type_request_search(u16 sh, u16 eh, u8 *attUUID, u8 attUUID_len, u16 *ret_fh, u16 *ret_geh){

	if(sh >= gAttributes[0].attNum) return 0;
	eh = eh < gAttributes[0].attNum ? eh : gAttributes[0].attNum;

	while(sh <= eh)
	{
		attribute_t* pAtt = &gAttributes[sh];

		if(uuid_match(attUUID_len, pAtt->uuid, attUUID))// TODO type 2bytes if pAtt->uuidLen <16
		{
			*ret_geh   = att_Find_end_group(sh, gAttributes[0].attNum, *(u16*)attUUID);

			*ret_fh = sh;
			return pAtt;

		}
		++sh;
	}
	return 0;
}


u8 * l2cap_att_handler(u16 connHandle, u8 * p)
{
	if (!gAttributes)
		return 0;

	rf_packet_l2cap_req_t * req = (rf_packet_l2cap_req_t *)p;

	u8 * r = 0;


	pkt_errRsp.errReason = ATT_ERR_ATTR_NOT_FOUND;
	u8 *ptx_buff = l2cap_buff.tx_p;

	if(ptx_buff==NULL)
	{
		return NULL;
	}

	((rf_packet_att_data_t*)ptx_buff)->type = 2; //ll data
	((rf_packet_att_data_t*)ptx_buff)->chanid = 4; //att

	switch(req->opcode){
    case ATT_OP_READ_BY_GROUP_TYPE_REQ: {

    	attribute_t* pAtt;
		rf_packet_att_readByType_t *p = (rf_packet_att_readByType_t*)req;
		rf_packet_att_readByGroupTypeRsp_t *rsp = (rf_packet_att_readByGroupTypeRsp_t*)ptx_buff;


#if(HUAWEI_ATTACK)
		u16 sh = p->startingHandle, eh = p->endingHandle;
		u16 attUUID = p->attType[0]| (p->attType[1]<<8);
		u16 groupEndHanle = sh;
		u16 i = 0;
		u32 attrLen = 0;
		u8 uuid_len=0;

		//for huawei attack #161  :l2cap len need to check

		if(p->l2capLen != 0x15 && p->l2capLen != 0x07)
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_PDU;
			pkt_errRsp.errOpcode = ATT_OP_READ_BY_GROUP_TYPE_REQ;
			pkt_errRsp.errHandle = sh;
			r = (u8 *)(&pkt_errRsp);
			break;
		}


		att_service_discover_tick = clock_time() | 1;
#else
		att_service_discover_tick = clock_time() | 1;

		u16 sh = p->startingHandle, eh = p->endingHandle;
		u16 attUUID = p->attType[0]| (p->attType[1]<<8);
		u16 groupEndHanle = sh;
		u16 i = 0;
		u32 attrLen = 0;
		u8 uuid_len=0;
#endif

		if((sh!=0) && (sh<=eh))
		{
			if((attUUID==GATT_UUID_PRIMARY_SERVICE) || (attUUID==GATT_UUID_SECONDARY_SERVICE)\
					||(attUUID==GATT_UUID_CHARACTER))
			{
				u16 total_pkt = 2;

				if(p->l2capLen==21)
					uuid_len = 16;
				else if(p->l2capLen==7)
					uuid_len = 2;
				if(uuid_len )
				{
					while((pAtt = att_read_by_group_type_request_search(sh, eh, (u8*)&attUUID, uuid_len, &sh, &groupEndHanle))){

						if(attrLen && attrLen != pAtt->attrLen)
							break;

						attrLen = pAtt->attrLen;
						if(total_pkt > bltAtt.effective_MTU - (4 + attrLen))
							break;


						rsp->data[i++] = sh & 0xff;
						rsp->data[i++] = (sh>>8) & 0xff;
						rsp->data[i++] = groupEndHanle & 0xff;
						rsp->data[i++] = (groupEndHanle>>8) & 0xff;

						memcpy(&rsp->data[i], pAtt->pAttrValue, pAtt->attrLen);

						total_pkt += (4 + attrLen);

						i += (pAtt->attrLen);
						sh  = groupEndHanle + 1;
						if(sh > eh){
							break;
						}
					}
				}
				else
				{
					pkt_errRsp.errReason = ATT_ERR_INVALID_ATTR_VALUE_LEN;
				}
			}
			else
			{
				pkt_errRsp.errReason = ATT_ERR_UNSUPPORTED_GRP_TYPE;
			}
		}
		else
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
		}

		if(i > 0){
			rsp->l2capLen = 2 + i;
			rsp->rf_len = rsp->l2capLen + 4;
			rsp->opcode = ATT_OP_READ_BY_GROUP_TYPE_RSP;
			rsp->datalen = attrLen + 4;
			r = (u8 *)(rsp);
		}else{
			pkt_errRsp.errOpcode = ATT_OP_READ_BY_GROUP_TYPE_REQ;
			pkt_errRsp.errHandle = p->startingHandle;
//			pkt_errRsp.errReason = ATT_ERR_ATTR_NOT_FOUND;
			r = (u8 *)&pkt_errRsp;
		}
	}
	break;
    case ATT_OP_FIND_BY_TYPE_VALUE_REQ:{

    		attribute_t* pAtt;
			rf_packet_att_findByTypeReq_t *p = (rf_packet_att_findByTypeReq_t *)req ;
			rf_packet_att_findByTypeRsp_t *rsp = (rf_packet_att_findByTypeRsp_t *)ptx_buff ;
//			pkt_errRsp.errReason = 0;

			att_service_discover_tick = clock_time() | 1;


			u16 sh = p->startingHandle, eh = p->endingHandle;
			u16 findByType_startHandle = sh;
			u16 groupEndHanle = sh;
			u16 attUUID = (p->attType[1] << 8) | p->attType[0];
			u16 i = 0;

			if(sh!=0 && sh<=eh)
			{
				while((pAtt=att_find_by_type_value_search(sh, eh, (u8*)(&attUUID), p->attValue, p->l2capLen-7,&sh, &groupEndHanle)))
				{
				   if((i*2) + 1 > bltAtt.effective_MTU-4) // opcode + sh + eh == 5  todo MTU
							   break;
					rsp->data[i++] = sh;
					rsp->data[i++] = groupEndHanle;

					sh = groupEndHanle+1;
					if(sh > eh)
						break;
				}
			}
			else
			{
				pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			}

		 	if(i > 0){
				rsp->l2capLen = (i*2) + 1;
				rsp->rf_len = rsp->l2capLen + 4;
				rsp->opcode = ATT_OP_FIND_BY_TYPE_VALUE_RSP;
				r = (u8 *)(rsp);
			}else{
				pkt_errRsp.errOpcode = ATT_OP_FIND_BY_TYPE_VALUE_REQ;

				//pkt_errRsp.errHandle = sh;
				pkt_errRsp.errHandle = findByType_startHandle;  //

	//			pkt_errRsp.errReason = ATT_ERR_ATTR_NOT_FOUND;
				r = (u8 *)(&pkt_errRsp);
			}
    	}
    	break;
	case ATT_OP_READ_BY_TYPE_REQ: {
		rf_packet_att_readByType_t *p = (rf_packet_att_readByType_t*)req;
		rf_packet_att_readByTypeRsp_t *rsp = (rf_packet_att_readByTypeRsp_t*)ptx_buff;
		attribute_t* pAtt;

		att_service_discover_tick = clock_time() | 1;

		u16 sh = p->startingHandle, eh = p->endingHandle;
		u16 i = 0;
		u8 uuidLen = 0;
		u8 uuidReqLen = 0;
#if (HUAWEI_ATTACK)
		//for huawei attack #1348  :l2cap len need to check

		if(p->l2capLen != 0x15 && p->l2capLen != 0x07)
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_PDU;
			pkt_errRsp.errOpcode = ATT_OP_READ_BY_TYPE_REQ;
			pkt_errRsp.errHandle = sh;
			r = (u8 *)(&pkt_errRsp);
			break;
		}
#endif
		uuidReqLen = (p->l2capLen == 0x0015)?16:2;
		u8 attUUID[16];
		memcpy(attUUID, (u8*)(p->attType), uuidReqLen);

		pkt_errRsp.errReason = (p->startingHandle >(p->endingHandle))?(ATT_ERR_INVALID_HANDLE):0x00;
		if(pkt_errRsp.errReason == 0)
		{
			if((uuidReqLen==2) && (attUUID[0]==U16_LO(GATT_UUID_CHARACTER))&&(attUUID[1]==U16_HI(GATT_UUID_CHARACTER)))
			{
				while((pAtt = l2cap_att_search(sh, eh, (u8*)(attUUID), &sh)))
				{
					if(uuidLen && uuidLen != (pAtt+1)->uuidLen)
						break;
					if(i + uuidLen + 5 > bltAtt.effective_MTU - 2)
						break;
					rsp->data[i++] = sh++;
					rsp->data[i++] = 0;
					rsp->data[i++] = pAtt->pAttrValue[0];
					++pAtt;
					rsp->data[i++] = sh++;
					rsp->data[i++] = 0;
					memcpy(&rsp->data[i], pAtt->uuid, pAtt->uuidLen);
					uuidLen = pAtt->uuidLen;
					i += pAtt->uuidLen;
					uuidLen = pAtt->uuidLen;
				}
				rsp->datalen = uuidLen + 5;
			}
			else
			{
				if((pAtt = l2cap_att_search(sh, eh, (u8*)(attUUID), &sh)))
				{
					if(uuidReqLen && uuidReqLen != pAtt->uuidLen)
					{
						break;
					}
					u8 read_perm = gAttributes[sh].perm & (ATT_PERMISSIONS_AUTHOR_READ | ATT_PERMISSIONS_AUTHEN_READ | ATT_PERMISSIONS_ENCRYPT_READ | ATT_PERMISSIONS_READ);
					if(read_perm == 0)
					{
						pkt_errRsp.errReason = ATT_ERR_READ_NOT_PERMITTED;
						pkt_errRsp.errOpcode = ATT_OP_READ_BY_TYPE_REQ;
						pkt_errRsp.errHandle = sh;
						r = (u8 *)(&pkt_errRsp);
						break;
					}
					rsp->data[0] = sh;
					rsp->data[1] = sh>>8;
					/*
					 * ATT_MTU_SIZE - Opcode(1B) - length(1B) - Handle(2B) = AttUserData(19B)
					 */
					u16 avaAttUsrLen = (bltAtt.effective_MTU-4);
					u8 pAtt_used_len = pAtt->attrLen > avaAttUsrLen ? avaAttUsrLen : pAtt->attrLen;

					memcpy(&rsp->data[2], pAtt->pAttrValue, pAtt_used_len);
					i = 2 + pAtt_used_len;
				}
				rsp->datalen = i;
			}
		}

		if(i > 0)
		{
			rsp->l2capLen = i + 2;
			rsp->rf_len = rsp->l2capLen + 4;
			rsp->opcode = ATT_OP_READ_BY_TYPE_RSP;
			r = (u8 *)(rsp);
		}
		else
		{
			if(!pkt_errRsp.errReason)
			{
				pkt_errRsp.errReason = ATT_ERR_ATTR_NOT_FOUND;
			}
			pkt_errRsp.errOpcode = ATT_OP_READ_BY_TYPE_REQ;
			pkt_errRsp.errHandle = sh;
			r = (u8 *)(&pkt_errRsp);
		}
	}
	break;
	case ATT_OP_FIND_INFO_REQ: {
		rf_packet_att_readByType_t *p = (rf_packet_att_readByType_t*)req;
		rf_packet_att_readByTypeRsp_t *rsp = (rf_packet_att_readByTypeRsp_t*)ptx_buff;
		attribute_t* pAtt;

		att_service_discover_tick = clock_time() | 1;

		u16 sh = p->startingHandle;
		u16 eh = p->endingHandle < gAttributes[0].attNum ? p->endingHandle : gAttributes[0].attNum;
		int i = 0;
		u8  tem_attLen = 18;
		u8 format = 1;
		u8 last_uuidLen = 0;
		while (sh <= eh ){
			if(i + tem_attLen > bltAtt.effective_MTU-2)
				break;
			pAtt = &gAttributes[sh];
			/*************In a single RSP uuid len should same*****************/
			if(!last_uuidLen)
				last_uuidLen = pAtt->uuidLen;
			if(last_uuidLen != pAtt->uuidLen)
				break;
			last_uuidLen = pAtt->uuidLen;

			rsp->data[i++] = sh;
			rsp->data[i++] = sh>>8;
			if(pAtt->uuidLen == 2){
				rsp->data[i++] = pAtt->uuid[0];
				rsp->data[i++] = pAtt->uuid[1];
				tem_attLen = 4;
			}
			else{ //modified by june
				memcpy(&rsp->data[i],pAtt->uuid,pAtt->uuidLen);
				i += 16;
				tem_attLen = 18;
				format = 2;
			}
			sh++;
		}
		if (i) {
			rsp->l2capLen = i + 2;
			rsp->rf_len = rsp->l2capLen + 4;
			rsp->opcode = ATT_OP_FIND_INFO_RSP;
			rsp->datalen = format;
			r = (u8 *)(rsp);
		}
		else{
			pkt_errRsp.errOpcode = ATT_OP_FIND_INFO_REQ;
			pkt_errRsp.errHandle = sh;
//			pkt_errRsp.errReason = ATT_ERR_ATTR_NOT_FOUND;
			r = (u8 *)(&pkt_errRsp);
		}
	}
	break;
	case ATT_OP_WRITE_CMD:
	case ATT_OP_WRITE_REQ:{
		rf_packet_att_write_t *p = (rf_packet_att_write_t*)req;
		u16 h = p->handle;
		attribute_t *pAtt = &gAttributes[h];
		pkt_errRsp.errOpcode = req->opcode;
		pkt_errRsp.errHandle = p->handle;
#if(HUAWEI_ATTACK)
		//for huawei attack #2175:error handle
		if(h==0)
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			r = (u8 *)(&pkt_errRsp);
			break;
		}

#endif
		if(h <= gAttributes[0].attNum)
		{
			u8 gatt_perm = gAttributes[h].perm;
			if( !(gatt_perm & ATT_PERMISSIONS_WRITE))
			{
				pkt_errRsp.errReason = ATT_ERR_WRITE_NOT_PERMITTED;

				/*
				 * For ATT_WRITE_CMD, No ATT_ERROR_RSP or ATT_WRITE_RSP PDUs shall be sent in response
				 * to this command. If the server cannot write this attribute for any reason the
				 * command shall be ignored
				 */
				r = (req->opcode==ATT_OP_WRITE_CMD)? NULL: (u8 *)(&pkt_errRsp);
				break;
			}


			if(gatt_perm & ATT_PERMISSIONS_SECURITY){
				u8 att_err = blc_gatt_requestServiceAccess(BLS_CONN_HANDLE, gatt_perm);
				if(att_err){
					pkt_errRsp.errReason = att_err;
					/*
					 * For ATT_WRITE_CMD, No ATT_ERROR_RSP or ATT_WRITE_RSP PDUs shall be sent in response
					 * to this command. If the server cannot write this attribute for any reason the
					 * command shall be ignored
					 */
					r = (req->opcode==ATT_OP_WRITE_CMD)? NULL: (u8 *)(&pkt_errRsp);
					break;
				}
			}


			if(ATT_OP_WRITE_REQ == req->opcode)
			{
//				r = (u8 *)(&pkt_writeRsp);

				rf_packet_att_writeRsp_t *rsp = (rf_packet_att_writeRsp_t*)ptx_buff;

				rf_packet_att_writeRsp_t pkt_writeRsp = {
					0x02,										// type
					sizeof(rf_packet_att_writeRsp_t) - 2,		// rf_len
					sizeof(rf_packet_att_writeRsp_t) - 6,		// l2cap_len
					4,											// chanId
					ATT_OP_WRITE_RSP,
				};

				memcpy(rsp, &pkt_writeRsp, sizeof(rf_packet_att_writeRsp_t));
				r = (u8*)rsp;
			}
			if(pAtt->w){
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					int errReason=pAtt->w(connHandle, p);
					if(errReason)
					{
						pkt_errRsp.errReason = errReason;
						r = (req->opcode==ATT_OP_WRITE_CMD)? NULL: (u8 *)(&pkt_errRsp);
						break;
					}
				#elif(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					int errReason=pAtt->w(p);
					if(errReason)
					{
						pkt_errRsp.errReason = errReason;
						r = (req->opcode==ATT_OP_WRITE_CMD)? NULL: (u8 *)(&pkt_errRsp);
						break;
					}
				#endif
			}else{
				if(p->l2capLen >= 3){
					u16 len = p->l2capLen - 3;
					//pAtt->attrLen = len;
					if(len > pAtt->attrLen)
					{
						pkt_errRsp.errReason = ATT_ERR_INVALID_ATTR_VALUE_LEN;
//						r = (u8 *)(&pkt_errRsp);

						r = (req->opcode==ATT_OP_WRITE_CMD)? NULL: (u8 *)(&pkt_errRsp);
						break;
					}
					memcpy(pAtt->pAttrValue, &p->value, len);
				}
			}
		}
		else
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			r = (u8 *)(&pkt_errRsp);
		}
	}
	break;

	case ATT_OP_PREPARE_WRITE_REQ:
	{
		rf_packet_att_write_t *p = (rf_packet_att_write_t*)req;
		u16 h = p->handle;
		pkt_errRsp.errOpcode = req->opcode;
		pkt_errRsp.errHandle = p->handle;
		if(h==0)
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			r = (u8 *)(&pkt_errRsp);
			break;
		}

#if(HUAWEI_ATTACK)
		//for huawei attack #103 & #2183 :wrong l2cap len
		u16 recv_l2cap_len = p->l2capLen;
		if(recv_l2cap_len <5 || recv_l2cap_len > bltAtt.effective_MTU) //at least 5byte opcode(1byte) handle(2byte) offet(2byte)but not larger mtu size
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_PDU;
			r = (u8 *)(&pkt_errRsp);
			break;
		}
#endif
		if(h <= gAttributes[0].attNum)
		{
			u8 gatt_perm = gAttributes[h].perm;
			if( !(gatt_perm & ATT_PERMISSIONS_WRITE))
			{
				pkt_errRsp.errReason = ATT_ERR_WRITE_NOT_PERMITTED;
				r = (u8 *)(&pkt_errRsp);
				break;
			}

			if(gatt_perm & ATT_PERMISSIONS_SECURITY){
				u8 att_err = blc_gatt_requestServiceAccess(BLS_CONN_HANDLE, gatt_perm);
				if(att_err){
					pkt_errRsp.errReason = att_err;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
			}

			int offset = p->value + *(&p->value + 1) * 256;
			unsigned short prepare_pkt_len = 0;

			if(pAppPrepareWriteBuff){
				if (offset == 0)
				{
					prepare_pkt_len = 9; //init prepare queue length
					prepare_pkt_current_len = prepare_pkt_len;
					memcpy (pAppPrepareWriteBuff, p, 9);
				}
				prepare_pkt_len = prepare_pkt_current_len;
				prepare_pkt_len += p->l2capLen - 5; //keep prepare queue length
				prepare_pkt_current_len = prepare_pkt_len;

				if(prepare_pkt_len > pAppPrepareWrite_max_len - 3){

					#if(DATA_NO_INIT_EN)
						blt_buff_process_pending &= ~PREPARE_WRITE_PENDING; //clear the relevant bit.
					#endif

					pkt_errRsp.errReason = ATT_ERR_PREPARE_QUEUE_FULL;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
			}
			else{
				if (offset == 0)
				{
					prepare_pkt_len = 9; //init prepare queue length
					blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-2] = U16_LO(prepare_pkt_len);
					blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-1] = U16_HI(prepare_pkt_len);
					memcpy (blt_buff_prepare_write, p, 9);
				}

				prepare_pkt_len = MAKE_U16(blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-1], blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-2]);
				prepare_pkt_len += p->l2capLen - 5; //keep prepare queue length
				blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-2] = U16_LO(prepare_pkt_len);
				blt_buff_prepare_write[L2CAP_RX_BUFF_LEN_MAX-1] = U16_HI(prepare_pkt_len);

				if(prepare_pkt_len > L2CAP_RX_BUFF_LEN_MAX - 3){

					#if(DATA_NO_INIT_EN)
						blt_buff_process_pending &= ~PREPARE_WRITE_PENDING; //clear the relevant bit.
					#endif

					pkt_errRsp.errReason = ATT_ERR_PREPARE_QUEUE_FULL;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
			}

			rf_packet_att_write_t *pw;
			if(pAppPrepareWriteBuff)
			{
				memcpy (pAppPrepareWriteBuff + 9 + offset, &p->value + 2, p->l2capLen - 5);
				pw = (rf_packet_att_write_t *)pAppPrepareWriteBuff;
			}
			else{
				memcpy (blt_buff_prepare_write + 9 + offset, &p->value + 2, p->l2capLen - 5);
				pw = (rf_packet_att_write_t *)blt_buff_prepare_write;
			}

			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending |= PREPARE_WRITE_PENDING; //clear the relevant bit.
			#endif

			pw->l2capLen = offset + p->l2capLen - 2;

			rf_packet_att_write_t* writePrepRsp = p;
			writePrepRsp->opcode = ATT_OP_PREPARE_WRITE_RSP;
			r = (u8 *)writePrepRsp;
		}
		else
		{
			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~PREPARE_WRITE_PENDING; //clear the relevant bit.
			#endif
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			r = (u8 *)(&pkt_errRsp);
		}
	}
	break;

	case ATT_OP_EXECUTE_WRITE_REQ:
	{
		rf_packet_att_executeWriteReq_t *rst = (rf_packet_att_executeWriteReq_t*)req;
		rf_packet_att_writeRsp_t *rsp = (rf_packet_att_writeRsp_t*)ptx_buff;

		rf_packet_att_writeRsp_t pkt_execute_writeRsp = {
			0x02,										// type
			sizeof(rf_packet_att_writeRsp_t) - 2,		// rf_len
			sizeof(rf_packet_att_writeRsp_t) - 6,		// l2cap_len
			4,											// chanId
			ATT_OP_EXECUTE_WRITE_RSP,
		};

		memcpy (rsp, &pkt_execute_writeRsp, sizeof(rf_packet_att_writeRsp_t));
		r = (u8 *)rsp;

		if(rst->flags == 0x01)//imm write all pending prepared values
		{
			rf_packet_att_write_t *p;
			if(pAppPrepareWriteBuff)
			{
				p = (rf_packet_att_write_t*)pAppPrepareWriteBuff;
			}
			else{
				p = (rf_packet_att_write_t*)blt_buff_prepare_write;
			}

			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~PREPARE_WRITE_PENDING; //clear the relevant bit.
			#endif

			u16 h = p->handle;
			attribute_t *pAtt = &gAttributes[h];
			pkt_errRsp.errOpcode = ATT_OP_EXECUTE_WRITE_REQ;
			pkt_errRsp.errHandle = p->handle;
			if (h <= gAttributes[0].attNum){
				if(pAtt->w){
					#if (MCU_CORE_TYPE == MCU_CORE_9518)
						pAtt->w(connHandle, p);
					#elif(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
						pAtt->w(p);
					#endif
				}
				else{
					if(p->l2capLen >= 3){
						u16 len = p->l2capLen - 3;
						//pAtt->attrLen = len;
						if(len > pAtt->attrLen)
						{
							pkt_errRsp.errReason = ATT_ERR_INVALID_ATTR_VALUE_LEN;
							r = (u8 *)(&pkt_errRsp);
							break;
						}

						memcpy(pAtt->pAttrValue, &p->value, len);
					}
				}
			}
			else
			{
				pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
				r = (u8 *)(&pkt_errRsp);
			}
		}
		else if(rst->flags == 0x00){ //cancel
			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~PREPARE_WRITE_PENDING; //clear the relevant bit.
			#endif
		}
	}
	break;

	case ATT_OP_SIGNED_WRITE_CMD:
	{
#if 0
		rf_packet_att_write_t *p = (rf_packet_att_write_t*)req;
		u16 h = p->handle;
		attribute_t *pAtt = &gAttributes[h];
		u16 len = p->l2capLen - 15;
		smp_secSigInfo_t rec_siginfo;
		if(h <= gAttributes[0].attNum)
		{
			memcpy(rec_siginfo,p->value+len,12);
			if(smp_sign_verify(rec_siginfo)==0)	//todo:smp_sign_verify to be implemented
			{
				if(p->l2capLen >= 3)
				{
					memcpy(pAtt->pAttrValue, &p->value, len);
				}
			}
		}
#endif
	}
	break;
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:{
		rf_packet_att_readBlob_t *p = (rf_packet_att_readBlob_t*)req;


		u16 h = p->handle;
		attribute_t *pAtt = &gAttributes[h];
		pkt_errRsp.errOpcode = req->opcode;
		pkt_errRsp.errHandle = p->handle;
		if(h <= gAttributes[0].attNum)
		{
			u8 gatt_perm = gAttributes[h].perm;
			if( !(gatt_perm & ATT_PERMISSIONS_READ) )  //no read permission
			{
				pkt_errRsp.errReason = ATT_ERR_READ_NOT_PERMITTED;
				r = (u8 *)(&pkt_errRsp);
				break;
			}



			if(gatt_perm & ATT_PERMISSIONS_SECURITY){
				u8 att_err = blc_gatt_requestServiceAccess(BLS_CONN_HANDLE, gatt_perm);
				if(att_err){
					pkt_errRsp.errReason = att_err;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
			}


			if(pAtt->r)
			{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					if(pAtt->r(connHandle, p)==1)
				#elif(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					if(pAtt->r(p)==1)
				#endif
					{
						return 0;
					}
			}

			rf_packet_att_readRsp_t *rsp = (rf_packet_att_readRsp_t*)ptx_buff;
			u8 *psrc = pAtt->pAttrValue;
			int len = pAtt->attrLen;
			if((u16)*(pAtt->uuid)==0x2A4B&&blt_att_getReportMap())//0x2A4B CHARACTERISTIC_UUID_HID_REPORT_MAP
			{
				psrc = blt_att_getReportMap();
				len = blt_att_getReportMapSize();
			}
			if (req->opcode == ATT_OP_READ_BLOB_REQ)
			{
				rsp->opcode = ATT_OP_READ_BLOB_RSP;

#if(HUAWEI_ATTACK)
				//for huawei attack #1693

				if(p->l2capLen != 5) // opcode(1byte) handle(2byte) offet(2byte)
				{
					pkt_errRsp.errReason = ATT_ERR_INVALID_PDU;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
#endif
				u16 offset = p->offset;
				if(offset > pAtt->attrLen)
				{
					pkt_errRsp.errReason = ATT_ERR_INVALID_OFFSET;
					r = (u8 *)(&pkt_errRsp);
					break;
				}
				len -= offset;
				psrc += offset;
			}
			else
			{
				rsp->opcode = ATT_OP_READ_RSP;
			}

			if (len > (bltAtt.effective_MTU - 1) )
			{
				len = bltAtt.effective_MTU - 1;
			}
			else if (len < 0)
			{
				len = 0;
			}

			memcpy(rsp->value, psrc, len);
			rsp->l2capLen = len + 1;
			rsp->rf_len = rsp->l2capLen + 4;
			r = (u8*)(rsp);

		}
		else
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_HANDLE;
			r = (u8 *)(&pkt_errRsp);
		}
	}
	break;

	case ATT_OP_EXCHANGE_MTU_REQ:
	case ATT_OP_EXCHANGE_MTU_RSP:
	{
		#if (MCU_CORE_TYPE == MCU_CORE_9518)
			bltAtt.mtu_exchange_pending = 0;
		#endif
		rf_packet_att_mtu_t *pMtu = (rf_packet_att_mtu_t*)req;

		if(pMtu->l2capLen != 3)
		{
			pkt_errRsp.errReason = ATT_ERR_INVALID_PDU;
			pkt_errRsp.errOpcode = pMtu->opcode;
			r = (u8 *)(&pkt_errRsp);
			break;
		}

		if(req->opcode ==  ATT_OP_EXCHANGE_MTU_REQ){
			pkt_mtu_rsp.mtu[0] = bltAtt.init_MTU &0xff;
			pkt_mtu_rsp.mtu[1] = (bltAtt.init_MTU>>8) & 0xff;
			r = (u8*)(&pkt_mtu_rsp);
		}


		u16 peer_mtu_size = (pMtu->mtu[0] | pMtu->mtu[1]<<8);

		bltAtt.effective_MTU = (peer_mtu_size < 23 ? 23 : min(bltAtt.init_MTU, peer_mtu_size));

	 	if(gap_eventMask & GAP_EVT_MASK_ATT_EXCHANGE_MTU){
			u8 param_evt[8];
			gap_gatt_mtuSizeExchangeEvt_t *pEvt = (gap_gatt_mtuSizeExchangeEvt_t *)param_evt;
			pEvt->connHandle = connHandle;
			pEvt->peer_MTU = peer_mtu_size;
			pEvt->effective_MTU = bltAtt.effective_MTU;

			blc_gap_send_event ( GAP_EVT_ATT_EXCHANGE_MTU, param_evt, sizeof(gap_gatt_mtuSizeExchangeEvt_t) );
		}

	}

	break;

	case ATT_OP_HANDLE_VALUE_CFM:
	{
		blt_indicate_handle = 0;

	 	if(gap_eventMask & GAP_EVT_MASK_GATT_HANDLE_VLAUE_CONFIRM){
			blc_gap_send_event ( GAP_EVT_GATT_HANDLE_VLAUE_CONFIRM, NULL, 0 );
		}


	}
	break;

    default:
	break;

	}
	return r;
}




#if 0
u8 smp_sign_verify(u8* data)
{
	rf_packet_att_write_t* pData = data;
	u8 *payload = pData->value;
	u16 len = pData->l2capLen;
	extern smp_secSigInfo_t smp_sign_info;
	if(smp_sign_info.csrk == 0)
	{
		return ? ;  //add later
	}
	/* compare the signature counter */
	u32 counter = ((payload[len - 9]) << 24) | ((payload[len - 10]) << 16) | ((payload[len - 11]) << 8) | (payload[len - 12]);
	if(smp_sign_info.signCounter == counter)
	{
		return ?; //add later
	}

	/* update the signature counter */
	smp_sign_info.signCounter = counter;

	return smp_verifySignature(smp_sign_info.csrk, len, payload);
}
#endif




/*****************************************************************************************
CORE_4.2   Page2628  Data PDU Length Management
 * no data length extension      27
 *     pkt_notify_short:  llid(1) + rflen(1) + 27 = 29
 *     tx fifo: 		  dmalen(4) + llid(1) + rflen(1) + 27 + mic(4) = 27+10 =37
 *
 * with data length extension    251
 *     pkt_notify_short:  llid(1) + rflen(1) + 251 = 253
 *     tx fifo: 		  dmalen(4) + llid(1) + rflen(1) + 251 + mic(4) = 251+10 = 261
 *
 ****************************************************************************************/


#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	#define CONN_MAX_TX_OCTETS				bltData.connEffectiveMaxTxOctets
	#define MAX_TX_OCTETS_FISRT_PKT		    (bltData.connEffectiveMaxTxOctets - 7)
						 	  //llid rfLen   l2capLen     cid    opcode   handle      data[0..243]  mic[0..3]
	//u8 pkt_notify_short[240] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};

#else
	#define CONN_MAX_TX_OCTETS				27
	#define MAX_TX_OCTETS_FISRT_PKT			20
							 //llid rfLen   l2capLen     cid    opcode   handle      data[0..19]   mic[0..3]
	//u8 pkt_notify_short[36] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};

#endif



ble_sts_t	bls_att_pushIndicateData (u16 attHandle, u8 *p, int len)
{

	int n;

	int pktNum = 1 + (len + 6 )/CONN_MAX_TX_OCTETS;  //len - 20 + 26 = len + 6

	if (blt_indicate_handle){
		return GATT_ERR_PREVIOUS_INDICATE_DATA_HAS_NOT_CONFIRMED;
	}
	else if(blc_ll_getCurrentState() != BLS_LINK_STATE_CONN){
		return LL_ERR_CONNECTION_NOT_ESTABLISH;
	}
	else if( blc_smp_isParingBusy() || blc_ll_isEncryptionBusy()){
		return 	LL_ERR_ENCRYPTION_BUSY;   //TODO:  modify
	}
	else if(blc_ll_getTxFifoNumber() + pktNum >= blt_txfifo.num - BLE_STACK_USED_TX_FIFIO_NUM ){
		return LL_ERR_TX_FIFO_NOT_ENOUGH;
	}
	else if(att_service_discover_tick){  //NOTE: this branch must be the last one
		if(clock_time_exceed(att_service_discover_tick, bltAtt.Data_pending_time * 10000)){
			att_service_discover_tick = 0;
		}
		else{
			return 	GATT_ERR_DATA_PENDING_DUE_TO_SERVICE_DISCOVERY_BUSY;
		}
	}



#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	//257, align 4,use 260 here//llid rfLen   l2capLen     cid    opcode   handle      data[0..243]  mic[0..3]
	u8 pkt_notify_short[260] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};
#else
							 //llid rfLen   l2capLen     cid    opcode   handle      data[0..19]   mic[0..3]
	u8 pkt_notify_short[36] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};
#endif



	n = len < MAX_TX_OCTETS_FISRT_PKT ? len : MAX_TX_OCTETS_FISRT_PKT;
	pkt_notify_short[0] = 2;				//first data packet
	pkt_notify_short[1] = n + 7;

	*(u16*)(pkt_notify_short + 2) = len + 3;	//l2cap
	*(u16*)(pkt_notify_short + 4) = 0x04;	//chanid

	pkt_notify_short[6] = ATT_OP_HANDLE_VALUE_IND;
	pkt_notify_short[7] = U16_LO(attHandle);
	pkt_notify_short[8] = U16_HI(attHandle);

	memcpy (pkt_notify_short + 9, p, n);

	if( !bls_ll_pushTxFifo (BLS_CONN_HANDLE, pkt_notify_short) ){
		return LL_ERR_TX_FIFO_NOT_ENOUGH;
	}


	for (int i=n; i<len; i+=CONN_MAX_TX_OCTETS)
	{
		n = len - i > CONN_MAX_TX_OCTETS ? CONN_MAX_TX_OCTETS : len - i;
		pkt_notify_short[0] = 1;				//first data packet
		pkt_notify_short[1] = n;
		memcpy (pkt_notify_short + 2, p + i, n);


		if (!bls_ll_pushTxFifo (BLS_CONN_HANDLE, pkt_notify_short)){
			return LL_ERR_TX_FIFO_NOT_ENOUGH;
		}
	}


	blt_indicate_handle = attHandle;

	return BLE_SUCCESS;
}




#if (MCU_CORE_TYPE == MCU_CORE_9518)

	_attribute_data_retention_	u32  blt_mtuReqSendTime_tick = 1200 * SYSTEM_TIMER_TICK_1MS;
	void   blc_att_setMtureqSendingTime_after_connCreate(int time_ms)
	{
		blt_mtuReqSendTime_tick = time_ms * SYSTEM_TIMER_TICK_1MS;
	}


	int	blt_att_sendMtuRequest (u16 connHandle)
	{
		if(!blc_ll_isEncryptionBusy() && !blc_smp_isParingBusy() && ((u32)(clock_time() - bls_ll_getConnectionCreateTime() ) > blt_mtuReqSendTime_tick)){

			u8 mtu_exchange[12]; //9, 4B align, no need consider MIC
			rf_packet_att_mtu_exchange_t *pReq = (rf_packet_att_mtu_exchange_t *)mtu_exchange;

			pReq->type = 2;  //LLID
			pReq->rf_len = 7;
			pReq->l2capLen = 0x0003;
			pReq->chanId = L2CAP_CID_ATTR_PROTOCOL;
			pReq->opcode = ATT_OP_EXCHANGE_MTU_REQ;
			pReq->mtu[0] = U16_LO(bltAtt.init_MTU);
			pReq->mtu[1] = U16_HI(bltAtt.init_MTU);


			if( ll_push_tx_fifo_handler (connHandle | HANDLE_STK_FLAG, mtu_exchange) ){
				bltAtt.mtu_exchange_pending = 0;
				return 1;
			}
		}

		return 0;
	}

#endif ///ending of "#if (MCU_CORE_TYPE == MCU_CORE_9518)"


/**
 * @brief	This function is used to request MTU size exchange
 * @param	connHandle - connect handle
 * @param	mtu_size - ATT MTU size
 * @return	0: success
 * 			other: fail
 */
ble_sts_t	blc_att_requestMtuSizeExchange (u16 connHandle, u16 mtu_size)
{

	if(!blc_att_setRxMtuSize(mtu_size)){

	#if(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		// this is special design, for TELINK old version(826x SDK or older/Kite SDK 1.0 or older) master dongle not ack mtuSizeRequest, but RC audio 128 data must send
		// so for slave, effective_MTU set to a value bigger than 128 + 3
		if(connHandle == BLS_CONN_HANDLE){
			bltAtt.effective_MTU = 158;
		}
	#endif

		u8 mtu_exchange[16];  // 9 + 4(mic)
		rf_packet_att_mtu_exchange_t *pReq = (rf_packet_att_mtu_exchange_t *)mtu_exchange;

		pReq->type = 2;  //llid
		pReq->rf_len = 7;
		pReq->l2capLen = 0x0003;
		pReq->chanId = L2CAP_CID_ATTR_PROTOCOL;
		pReq->opcode = ATT_OP_EXCHANGE_MTU_REQ;
		pReq->mtu[0] = U16_LO(mtu_size);
		pReq->mtu[1] = U16_HI(mtu_size);


		if( ll_push_tx_fifo_handler (connHandle | HANDLE_STK_FLAG, mtu_exchange) ){
			return BLE_SUCCESS;
		}
		else{
			return LL_ERR_TX_FIFO_NOT_ENOUGH;
		}
	}

	return GATT_ERR_INVALID_PARAMETER;
}


ble_sts_t  blc_att_setRxMtuSize(u16 mtu_size)
{
//	if( mtu_size < ATT_MTU_SIZE || mtu_size > ATT_MTU_SIZE_MAX_LEN)
	if( mtu_size < ATT_MTU_SIZE)
	{
		return GATT_ERR_INVALID_PARAMETER;
	}
	else
	{
		bltAtt.init_MTU = mtu_size;
		return BLE_SUCCESS;
	}

}


void  blc_att_setServerDataPendingTime_upon_ClientCmd(u8 num_10ms)
{
	bltAtt.Data_pending_time = num_10ms;
}

/**
 * @brief	This function is used to set prepare write buffer
 * @param	*p - the pointer of buffer
 * @param	len - the length of buffer
 * @return	none.
 */
void  blc_att_setPrepareWriteBuffer(u8 *p, u16 len){
	if(p){
		pAppPrepareWriteBuff = (u8 *)p;
		pAppPrepareWrite_max_len = len;
	}
}

/**
 * @brief	This function is used to reset effective ATT MTU size
 * @param	connHandle - connect handle
 * @return	none.
 */
void  blt_att_resetEffectiveMtuSize(u16 connHandle)
{
	bltAtt.effective_MTU = ATT_MTU_SIZE;
}

/**
 * @brief	This function is used to set effective ATT MTU size
 * @param	connHandle - connect handle
 * @param	effective_mtu - bltAtt.effective_MTU
 * @return	none.
 */
void  blt_att_setEffectiveMtuSize(u16 connHandle, u8 effective_mtu)
{
	bltAtt.effective_MTU = effective_mtu;
}

/**
 * @brief	This function is used to reset RX MTU size--23B
 * @param	connHandle - connect handle
 * @return	none
 */
void blt_att_resetRxMtuSize(u16 connHandle)
{
	bltAtt.init_MTU = ATT_MTU_SIZE;
}
/**
 * @brief   This function is used to get effective MTU size.
 * @param	connHandle - connect handle
 * @return  effective MTU value.
 */
u16  blc_att_getEffectiveMtuSize(u16 connHandle)
{
	return bltAtt.effective_MTU;
}

ble_sts_t blc_att_setHIDReportMap(u8* p,u32 len)
{
	hid_reportmap_psrc=p;
	hid_reportmap_len=len;
	return BLE_SUCCESS;
}

ble_sts_t blc_att_resetHIDReportMap()
{
	hid_reportmap_psrc=NULL;
	hid_reportmap_len=0;
	return BLE_SUCCESS;
}

#if(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)

ble_sts_t  bls_att_pushNotifyData (u16 attHandle, u8 *p, int len)
{


#if 0
	if(bltAtt.Data_permission_check)
	{
		if(!(gAttributes[attHandle + 1].pAttrValue[0] & 0x01)){
			return ATT_ERR_NOTIFY_INDICATION_NOT_PERMITTED;
		}
	}
#endif




	int n;

	int pktNum = 1 + (len + 6 )/CONN_MAX_TX_OCTETS;  //len - 20 + 26 = len + 6

	if(blc_ll_getCurrentState() != BLS_LINK_STATE_CONN){
		return LL_ERR_CONNECTION_NOT_ESTABLISH;
	}
	else if( blc_smp_isParingBusy() || blc_ll_isEncryptionBusy() ){
		return 	LL_ERR_ENCRYPTION_BUSY;
	}
	else if(blc_ll_getTxFifoNumber() + pktNum >= blt_txfifo.num - 2 ){
		return LL_ERR_TX_FIFO_NOT_ENOUGH;
	}
	else if(att_service_discover_tick){  //NOTE: this branch must be the last one
		if(clock_time_exceed(att_service_discover_tick, bltAtt.Data_pending_time * 10000)){
			att_service_discover_tick = 0;
		}
		else{
			return 	GATT_ERR_DATA_PENDING_DUE_TO_SERVICE_DISCOVERY_BUSY;
		}
	}


#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
	//253, align 4,use 256 here//llid rfLen   l2capLen     cid    opcode   handle      data[0..243]  mic[0..3]
	u8 pkt_notify_short[256] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};   //mic[0..3] on  blt_txfifo
#else
							 //llid rfLen   l2capLen     cid    opcode   handle      data[0..19]   mic[0..3]
	u8 pkt_notify_short[36] = {0x02,0x09, 0x05,0x00,  0x04,0x00, 0x1b,  0x0e,0x00,  0x00, 0x00, 0x00, 0x00};    //mic[0..3] on  blt_txfifo
#endif


	n = len < MAX_TX_OCTETS_FISRT_PKT ? len : MAX_TX_OCTETS_FISRT_PKT;
	pkt_notify_short[0] = 2;				//first data packet
	pkt_notify_short[1] = n + 7;

	*(u16*)(pkt_notify_short + 2) = len + 3;	//l2cap
	*(u16*)(pkt_notify_short + 4) = 0x04;	//chanid

	pkt_notify_short[6] = ATT_OP_HANDLE_VALUE_NOTI;
	pkt_notify_short[7] = U16_LO(attHandle);
	pkt_notify_short[8] = U16_HI(attHandle);

	memcpy (pkt_notify_short + 9, p, n);


	bls_ll_pushTxFifo (BLS_CONN_HANDLE, pkt_notify_short);


	for (int i=n; i<len; i+=CONN_MAX_TX_OCTETS)
	{
		n = len - i > CONN_MAX_TX_OCTETS ? CONN_MAX_TX_OCTETS : len - i;
		pkt_notify_short[0] = 1;				//continue data packet
		pkt_notify_short[1] = n;
		memcpy (pkt_notify_short + 2, p + i, n);

		bls_ll_pushTxFifo (BLS_CONN_HANDLE, pkt_notify_short);
	}


	return BLE_SUCCESS;
}

ble_sts_t	blc_att_responseMtuSizeExchange (u16 connHandle, u16 mtu_size)
{
	{
		u8 mtu_exchange[16];  // 9 + 4(mic)
		rf_packet_att_mtu_exchange_t *pReq = (rf_packet_att_mtu_exchange_t *)mtu_exchange;

		pReq->type = 2;  //llid
		pReq->rf_len = 7;
		pReq->l2capLen = 0x0003;
		pReq->chanId = 0x0004;
		pReq->opcode = ATT_OP_EXCHANGE_MTU_RSP;
		pReq->mtu[0] = U16_LO(mtu_size);
		pReq->mtu[1] = U16_HI(mtu_size);

		if( ll_push_tx_fifo_handler (connHandle | HANDLE_STK_FLAG, mtu_exchange) ){
			return BLE_SUCCESS;
		}
		else{
			return LL_ERR_TX_FIFO_NOT_ENOUGH;
		}
	}

}

//--------------  ATT client function ---------------
volatile u32	att_req_busy = 0;


void att_req_read (u8 *p, u16 attHandle)
{
	p[0] = 2;
	p[1] = 7;
	p[2] = 3;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_READ_REQ;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
}

void att_req_read_blob (u8 *p, u16 attHandle, u16 offset)
{
	p[0] = 2;
	p[1] = 9;
	p[2] = 5;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_READ_BLOB_REQ;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
	p[9] = offset;
	p[10] = offset >> 8;
}

void att_req_read_multi (u8 *p, u16* h, u8 n)
{
	if(n>10)
		n = 10;
	p[0] = 2;
	p[1] = 7 + 2*n;
	p[2] = 3 + 2*n;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_READ_MULTI_REQ;
	for(u8 i = 0; i<n; i++)
	{
		p[7+i] = *h;
		p[8+i] = *h >> 8;
		h++;
	}
}

void att_req_write (u8 *p, u16 attHandle, u8 *buf, int len)
{
	if (len > 20)
		len = 20;
	p[0] = 2;
	p[1] = 7 + len;
	p[2] = 3 + len;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_WRITE_REQ;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
	memcpy (p + 9, buf, len);
}

void att_req_signed_write_cmd (u8 *p, u16 attHandle, u8 *pd, int n, u8 *sign)
{
	if (n > 8)
		n = 8;
	p[0] = 2;
	p[1] = 19 + n;
	p[2] = 15 + n;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_SIGNED_WRITE_CMD;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
	memcpy (p + 9, pd, n);
	memcpy (p+n+9, sign, 12);
}

void att_req_prep_write (u8 *p, u16 attHandle, u8 *pd, u16 offset, int len)
{
	if (len > 18)
		len = 18;
	p[0] = 2;
	p[1] = 7 + len;
	p[2] = 3 + len;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_PREPARE_WRITE_REQ;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
	p[9] = offset;
	p[10] = offset>>8;
	memcpy (p + 9, pd, len);
}

void att_req_write_cmd (u8 *p, u16 attHandle, u8 *buf, int len)
{
	if (len > 20)
		len = 20;
	p[0] = 2;
	p[1] = 7 + len;
	p[2] = 3 + len;
	p[3] = 0;
	p[4] = L2CAP_CID_ATTR_PROTOCOL;
	p[5] = 0;
	p[6] = ATT_OP_WRITE_CMD;
	p[7] = attHandle;
	p[8] = attHandle >> 8;
	memcpy (p + 9, buf, len);
}

void att_req_read_by_type (u8 *p, u16 start_attHandle, u16 end_attHandle, u8 *uuid, int uuid_len)
{
	p[0] = 2;
	p[1] = 9 + uuid_len;
	p[2] = 5 + uuid_len;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_READ_BY_TYPE_REQ;
	p[7] = start_attHandle;
	p[8] = start_attHandle >> 8;
	p[9] = end_attHandle;
	p[10] = end_attHandle >> 8;
	memcpy (p + 11, uuid, uuid_len);
}

void att_req_read_by_group_type (u8 *p, u16 start_attHandle, u16 end_attHandle, u8 *uuid, int uuid_len)
{
	if(uuid_len>16) uuid_len=16;
	p[0] = 2;
	p[1] = 9 + uuid_len;
	p[2] = 5 + uuid_len;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_READ_BY_GROUP_TYPE_REQ;
	p[7] = start_attHandle;
	p[8] = start_attHandle >> 8;
	p[9] = end_attHandle;
	p[10] = end_attHandle >> 8;
	memcpy (p + 11, uuid, uuid_len);
}

void att_req_find_info(u8 *p, u16 start_attHandle, u16 end_attHandle)
{
	p[0] = 2;
	p[1] = 9;
	p[2] = 5;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_FIND_INFO_REQ;
	p[7] = start_attHandle;
	p[8] = start_attHandle >> 8;
	p[9] = end_attHandle;
	p[10] = end_attHandle >> 8;
}

void att_req_find_by_type (u8 *p, u16 start_attHandle, u16 end_attHandle, u8 *uuid, u8* attr_value, int len)
{
	p[0] = 2;
	p[1] = 11 + len;
	p[2] = 7 + len;
	p[3] = 0;
	p[4] = 4;
	p[5] = 0;
	p[6] = ATT_OP_FIND_BY_TYPE_VALUE_REQ;
	p[7] = start_attHandle;
	p[8] = start_attHandle >> 8;
	p[9] = end_attHandle;
	p[10] = end_attHandle >> 8;
	memcpy (p + 11, uuid, 2);
	memcpy (p + 13, attr_value, len);
}


/**
 * @brief      set device name
 * @param[in]  p - the point of name
 * @param[in]  len - the length of name
 * @return     BLE_SUCCESS
 */
ble_sts_t bls_att_setDeviceName(u8* pName,u8 len)
{
	memset(ble_devName, 0, MAX_DEV_NAME_LEN );
	memcpy(ble_devName, pName, len < MAX_DEV_NAME_LEN ? len : MAX_DEV_NAME_LEN);

	return BLE_SUCCESS;
}


#endif ///ending of "#if(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)"
