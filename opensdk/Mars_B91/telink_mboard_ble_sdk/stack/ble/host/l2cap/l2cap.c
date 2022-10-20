/********************************************************************************************************
 * @file	l2cap.c
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


//////////////////////////////////////////////////////////////////////////////////
//	L2CAP Handler
//		1. ATT & SMP handler
//		2. HCI incoming packet handler
///////////////////////////////////////////////////////////////////////////////////
#if(DATA_NO_INIT_EN)
_attribute_data_no_init_	u8		blt_buff_ll_rx[L2CAP_RX_BUFF_LEN_MAX] = {0};//l2cap rx buffer
_attribute_data_no_init_	u8		blt_buff_att_tx[L2CAP_RX_BUFF_LEN_MAX] = {0}; //l2cap tx buffer
#else
_attribute_data_retention_	u8		blt_buff_ll_rx[L2CAP_RX_BUFF_LEN_MAX] = {0};//l2cap rx buffer
_attribute_data_retention_	u8		blt_buff_att_tx[L2CAP_RX_BUFF_LEN_MAX] = {0}; //l2cap tx buffer
#endif

_attribute_data_retention_	l2cap_handler_t att_client_handler = 0;
_attribute_data_retention_	l2cap_handler_t att_sig_hander = 0;
_attribute_data_retention_	l2cap_conn_update_rsp_callback_t	l2cap_connUpdateRsp_cb = 0;

_attribute_data_retention_	_attribute_aligned_(4) para_up_req_t	para_upReq;

_attribute_data_retention_	_attribute_aligned_(4) l2cap_buff_t 	l2cap_buff = { //dft l2cap buffer setting
	.rx_p = blt_buff_ll_rx,
	.tx_p = blt_buff_att_tx,
	.max_rx_size = L2CAP_RX_BUFF_LEN_MAX,
	.max_tx_size = L2CAP_RX_BUFF_LEN_MAX,
};


//This API should define in controller, and name should be "blc_ll_xxx" , not a l2cap layer  API.
//But consider that this API have been used for a long time, do not change, so still define here in l2cap.c(blc_l2cap_handler define in ll.c)
void blc_l2cap_register_handler (void *p)
{
	blc_l2cap_handler = p;
}

void blc_l2cap_reg_att_cli_hander(void *p)
{
	att_client_handler = p;
}


void blc_l2cap_reg_att_sig_hander(void *p)//signaling L2CAP proc
{
	att_sig_hander = p;
}


void blc_l2cap_registerConnUpdateRspCb(l2cap_conn_update_rsp_callback_t cb)
{
	l2cap_connUpdateRsp_cb = cb;
}


void blc_l2cap_initMtuBuffer(u8 *pMTU_rx_buff, u16 mtu_rx_size, u8 *pMTU_tx_buff, u16 mtu_tx_size)
{
	l2cap_buff.rx_p = pMTU_rx_buff;
	l2cap_buff.max_rx_size = mtu_rx_size;

	l2cap_buff.tx_p = pMTU_tx_buff;
	l2cap_buff.max_tx_size = mtu_tx_size;
}


///////////////////////////////////
// l2cap process
///////////////////////////////////
u8 * blt_ll_pack (u8 *p)
{
	rf_packet_l2cap_req_t *pl = (rf_packet_l2cap_req_t *)(p + 0); //p has been removed dma header 4B
	u8 type = pl->type & 3;

	if (type == 3)
	{
		return p;
	}
	else if ( type == 2)
	{
		if (pl->rf_len == (pl->l2capLen + 4))
		{
			return p;
		}
		else
		{
			if(l2cap_buff.rx_p == NULL)
			{
				return NULL;
			}

			if (pl->l2capLen + 4 > pl->rf_len)
			{
				u16 len = pl->rf_len + 2; //header
				pl->rf_len = pl->l2capLen + 4;

				memcpy(l2cap_buff.rx_p, p, len);
				U16_SET(l2cap_buff.rx_p, len);

				#if(DATA_NO_INIT_EN)
					blt_buff_process_pending |= L2CAP_PACK_PENDING;//set and can not enter deep/deepRetention
				#endif
			}
			else
			{
				U16_SET(l2cap_buff.rx_p, 0);
			}
		}

	}
	else if (type==1)
	{

		if((l2cap_buff.rx_p == NULL)  || (U16_GET(l2cap_buff.rx_p)==0))
		{
			return NULL;
		}

		u16 att_len = U16_GET(l2cap_buff.rx_p)- 2 - 4 + pl->rf_len;
		if(att_len > (l2cap_buff.max_rx_size-6))//excess MTU; //-6 confirmed with Yafei/Qinghua.
		{
			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~L2CAP_PACK_PENDING; //clear the relevant bit.
			#endif

			U16_SET(l2cap_buff.rx_p, 0);
			return (u8 *)(l2cap_buff.rx_p);
		}


		memcpy(l2cap_buff.rx_p + U16_GET(l2cap_buff.rx_p), p + 2, pl->rf_len);
		U16_SET(l2cap_buff.rx_p, U16_GET(l2cap_buff.rx_p) + pl->rf_len);
		rf_packet_l2cap_req_t *ps = (rf_packet_l2cap_req_t *)(l2cap_buff.rx_p);
		if (U16_GET(l2cap_buff.rx_p) >= ps->l2capLen + (2 + 4))
		{
			#if(DATA_NO_INIT_EN)
				blt_buff_process_pending &= ~L2CAP_PACK_PENDING;//pack complete and clear the relevant bit.
			#endif
			U16_SET(l2cap_buff.rx_p, 0);
			return (u8 *)(l2cap_buff.rx_p);
		}

	}
	return NULL;
}



int blc_l2cap_packet_receive (u16 connHandle, u8 * p)
{
	u8 * pp = blt_ll_pack (p);
	rf_packet_l2cap_t *ptrL2cap = (rf_packet_l2cap_t*)pp; //p has been removed dma header 4B

	if (pp)
	{
		if (ptrL2cap->chanId == L2CAP_CID_SMP){		//SMP
			u8 *pr = l2cap_smp_handler (connHandle, pp);
			if (pr)
			{
				bls_ll_pushTxFifo (BLS_CONN_HANDLE | HANDLE_STK_FLAG, pr);
			}
		}
		else if (ptrL2cap->chanId == L2CAP_CID_ATTR_PROTOCOL) //CID == ATT
		{		//ATT
			if( (!(ptrL2cap->opcode & 0x01)) || (ptrL2cap->opcode == ATT_OP_EXCHANGE_MTU_RSP))		//att server handler
			{
				u8 *pr = l2cap_att_handler (connHandle, pp);
				if (pr)
				{
					rf_packet_l2cap_t *l2cap_pkt = (rf_packet_l2cap_t*)pr;
					if(BLE_SUCCESS != blc_l2cap_pushData_2_controller (connHandle | HANDLE_STK_FLAG, l2cap_pkt->chanId, &l2cap_pkt->opcode, 1, l2cap_pkt->data, l2cap_pkt->l2capLen-1))
					{
						bltAtt.pPendingPkt = pr;
						#if(DATA_NO_INIT_EN)
							blt_buff_process_pending |= BLT_ATT_PKT_PENDING; //set and can not enter deep/deepRetention
						#endif
					}
				}
			}
			else						//att client handler
			{
				if(att_client_handler)
					att_client_handler(connHandle, pp);
			}
		}
		else if (ptrL2cap->chanId == L2CAP_CID_SIG_CHANNEL)
		{
			if(att_sig_hander){
				att_sig_hander(connHandle, pp+0);
			}

			if(ptrL2cap->opcode == L2CAP_CMD_CONN_UPD_PARA_RESP){

				rf_pkt_l2cap_sig_connParaUpRsp_t* pRsp = (rf_pkt_l2cap_sig_connParaUpRsp_t*)pp;
				if(l2cap_connUpdateRsp_cb && pRsp->l2capLen == 6 && pRsp->dataLen == 2){
					l2cap_connUpdateRsp_cb(pRsp->id, pRsp->result);
				}
			}
		}
	}
	return 0;
}






void bls_l2cap_requestConnParamUpdate (u16 min_interval, u16 max_interval, u16 latency, u16 timeout)
{
	para_upReq.connParaUpReq_minInterval = min_interval;
	para_upReq.connParaUpReq_maxInterval = max_interval;
	para_upReq.connParaUpReq_latency = latency;
	para_upReq.connParaUpReq_timeout = timeout;

	para_upReq.connParaUpReq_pending = 1;
}

_attribute_data_retention_	u32  blc_connUpdateSendTime_us = 1000000;
void   bls_l2cap_setMinimalUpdateReqSendingTime_after_connCreate(int time_ms)
{
	blc_connUpdateSendTime_us = time_ms*1000;
}


extern u32  blc_connUpdateSendTime_us;
ble_sts_t blt_update_parameter_request (void)
{
	if(!blc_ll_isEncryptionBusy() && !blc_smp_isParingBusy() && ((u32)(clock_time() - bls_ll_getConnectionCreateTime() ) > (SYSTEM_TIMER_TICK_1US * blc_connUpdateSendTime_us))){

		u8 connParaUpData[18];
		rf_packet_l2cap_connParaUpReq_t *pReq = (rf_packet_l2cap_connParaUpReq_t* )connParaUpData;
		pReq->llid = 2;
		pReq->rf_len = 16;
		pReq->l2capLen = 0x000c;
		pReq->chanId = 5;
		pReq->opcode = 0x12;
		pReq->id = 1;
		pReq->data_len = 8;
		pReq->min_interval = para_upReq.connParaUpReq_minInterval;
		pReq->max_interval = para_upReq.connParaUpReq_maxInterval;
		pReq->latency = para_upReq.connParaUpReq_latency;
		pReq->timeout = para_upReq.connParaUpReq_timeout;


		if (bls_ll_pushTxFifo(BLS_CONN_HANDLE | HANDLE_STK_FLAG, connParaUpData)) {
			para_upReq.connParaUpReq_pending = 0;
		}
		else{//push fifo error
			return LL_ERR_TX_FIFO_NOT_ENOUGH;
		}
	}
	else{//Conditions are not enough
		return LL_ERR_CURRENT_STATE_NOT_SUPPORTED_THIS_CMD;
	}
	return BLE_SUCCESS;
}


void  blc_l2cap_SendConnParamUpdateResponse(u16 connHandle, u8 req_id, conn_para_up_rsp result)
{
	u8 conn_update_rsp[16];  //12 + 4(mic)

	rf_packet_l2cap_connParaUpRsp_t *pRsp = (rf_packet_l2cap_connParaUpRsp_t *)conn_update_rsp;
	pRsp->llid = L2CAP_FIRST_PKT_C2H;
	pRsp->rf_len = 10;
	pRsp->l2capLen = 6;
	pRsp->chanId = L2CAP_CID_SIG_CHANNEL;
	pRsp->opcode = 0x13;
	pRsp->id = req_id;
	pRsp->data_len = 2;
	pRsp->result = result;

	ll_push_tx_fifo_handler (connHandle | HANDLE_STK_FLAG, conn_update_rsp);
}


#if (HOST_CONTROLLER_DATA_FLOW_IMPROVE_EN)




//_attribute_ram_code_   //save some RamCode
ble_sts_t  blc_l2cap_pushData_2_controller (u16 connHandle, u16 cid, u8 *format, int format_len, u8 *pDate, int data_len)
{

	if(blc_ll_getCurrentState() != BLS_LINK_STATE_CONN){
		return LL_ERR_CONNECTION_NOT_ESTABLISH;
	}
	else if( blc_ll_isEncryptionBusy() ){  //for master, it's 0 all time,  we will do it later
		return 	LL_ERR_ENCRYPTION_BUSY;
	}



	//Note: one question not consider now: if data comes from stack(HANDLE_STK_FLAG valid) and TX fifo not enough,
	//      data will drop without any buffer hold



/////////step 1, calculate TX FIFO number cost, if not enough, return error ////////////
	//u8 mtuSize = blc_att_getEffectiveMtuSize(connHandle);  //TELINK MTU no longer than 256, so 1 byte is enough
	int PDU_max = bltAtt.effective_MTU - format_len;  //e.g. MTU=23, HandValueNotify format_len=3(opcode, attHandle), 20 bytes max												 //     MTU=50, 47 bytes max
	int extraDateLen = ( 4 + format_len);  //l2cap_len: 2 byte,  channel ID: 2 byte
	int cur_dataLen;


	int conn_MAX_TX_OCTETS = blc_ll_get_connEffectiveMaxTxOctets();

	int pktNum = 0;

	if(data_len)
	{
		for(int i=0; i<data_len; i+=PDU_max){   // unpack raw data according to MTU  e.g. MTU = 50, data_len=128: 47 + 47 + 34
			cur_dataLen = (data_len - i) > PDU_max ? PDU_max : (data_len - i);

			// unpack MTU data according to connEffectiveMaxTxOctets
			pktNum += (cur_dataLen + format_len + 3)/conn_MAX_TX_OCTETS  + 1;   // (cur_dataLen + 4(l2cap_len+CID) + format_len - 1 )/CONN_MAX_TX_OCTETS + 1
		}
	}
	else
	{
		cur_dataLen = 0;
		// unpack MTU data according to connEffectiveMaxTxOctets
		pktNum = 1;
	}

	if(blc_ll_getTxFifoNumber() + pktNum >= ((connHandle & HANDLE_STK_FLAG) ? blt_txfifo.num : (blt_txfifo.num - BLE_STACK_USED_TX_FIFIO_NUM)) ){
		return LL_ERR_TX_FIFO_NOT_ENOUGH;
	}

/////////////// step 2, push data to TX fifo ////////////
	#if (LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION)
		u8 pkt_l2cap_data[256];
	#else
		u8 pkt_l2cap_data[36];
	#endif

	if(data_len)
	{
		int n;
		int max_TX_OCTETS_FISRT_PKT	= conn_MAX_TX_OCTETS - extraDateLen;
		for(int i=0; i<data_len; i+=PDU_max){   // unpack raw data according to MTU  e.g. MTU = 50, data_len=128: 47 + 47 + 34

			cur_dataLen = (data_len - i) > PDU_max ? PDU_max : (data_len - i);

			//push first data packet
			n = cur_dataLen < max_TX_OCTETS_FISRT_PKT ? cur_dataLen : max_TX_OCTETS_FISRT_PKT;
			pkt_l2cap_data[0] = LLID_DATA_START;  //first data packet
			pkt_l2cap_data[1] = n + extraDateLen; //rf_len
			*(u16*)(pkt_l2cap_data + 2) = cur_dataLen + format_len;	//l2cap_len
			*(u16*)(pkt_l2cap_data + 4) = cid;	//channel ID
			smemcpy((char *)(pkt_l2cap_data+6), (char *)format, format_len);  // format data
			smemcpy((char *)(pkt_l2cap_data+6+format_len), (char *)(pDate+i), n); // real data

			ll_push_tx_fifo_handler (connHandle, pkt_l2cap_data);


			//push rest data packet
			for (int j=n; j<cur_dataLen; j+=conn_MAX_TX_OCTETS)
			{
				n = (cur_dataLen - j) > conn_MAX_TX_OCTETS ? conn_MAX_TX_OCTETS : (cur_dataLen - j);
				pkt_l2cap_data[0] = LLID_DATA_CONTINUE;	//continue data packet
				pkt_l2cap_data[1] = n;  //rf_len
				smemcpy((char *)(pkt_l2cap_data + 2), (char *)(pDate+i+j), n); // real data

				ll_push_tx_fifo_handler (connHandle, pkt_l2cap_data);
			}

		}
	}
	else
	{
		pkt_l2cap_data[0] = LLID_DATA_START;  //first data packet
		pkt_l2cap_data[1] = extraDateLen; //rf_len
		*(u16*)(pkt_l2cap_data + 2) = format_len;	//l2cap_len
		*(u16*)(pkt_l2cap_data + 4) = cid;	//channel ID
		smemcpy((char *)(pkt_l2cap_data+6), (char *)format, format_len);  // format data
		ll_push_tx_fifo_handler (connHandle, pkt_l2cap_data);
	}


	return BLE_SUCCESS;

}







#endif

