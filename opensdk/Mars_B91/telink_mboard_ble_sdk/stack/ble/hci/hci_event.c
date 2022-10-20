/********************************************************************************************************
 * @file	hci_event.c
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
#include "stack/ble/controller/ble_controller.h"


void hci_disconnectionComplete_evt(u8 status, u16 connHandle, u8 reason)
{
	u8 result[4];
	hci_disconnectionCompleteEvt_t *pEvt = (hci_disconnectionCompleteEvt_t *)result;

	pEvt->status = status;
	pEvt->connHandle = connHandle;
	pEvt->reason = reason;

	blc_hci_send_event ( HCI_FLAG_EVENT_BT_STD | HCI_EVT_DISCONNECTION_COMPLETE, result, 4);
}


int hci_cmdComplete_evt(u8 numHciCmds, u8 opCode_ocf, u8 opCode_ogf, u8 paraLen, u8 *para, u8 *result)
{
	hci_cmdCompleteEvt_t *pEvt = (hci_cmdCompleteEvt_t *)result;

	pEvt->numHciCmds = 1;
	pEvt->opCode_OCF = opCode_ocf;
	pEvt->opCode_OGF = opCode_ogf;
	memcpy(pEvt->returnParas, para, paraLen);

	return (paraLen + 3);
}

void hci_cmdStatus_evt(u8 numHciCmds, u8 opCode_ocf, u8 opCode_ogf, u8 status, u8 *result)
{
	hci_cmdStatusEvt_t *pEvt = (hci_cmdStatusEvt_t *)result;

	pEvt->status = status;
	pEvt->numHciCmds = numHciCmds;
	pEvt->opCode_OCF = opCode_ocf;
	pEvt->opCode_OGF = opCode_ogf;
}



void hci_le_connectionComplete_evt(u8 status, u16 connHandle, u8 role, u8 peerAddrType, u8 *peerAddr,
                                   u16 connInterval, u16 slaveLatency, u16 supervisionTimeout, u8 masterClkAccuracy)
{
	u8 result[20];

	hci_le_connectionCompleteEvt_t *pEvt = (hci_le_connectionCompleteEvt_t *)result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_CONNECTION_COMPLETE;
	pEvt->status = status;
	pEvt->connHandle = connHandle;
	pEvt->role = role;
	pEvt->peerAddrType = peerAddrType;
	memcpy(pEvt->peerAddr, peerAddr, BLE_ADDR_LEN);
	pEvt->connInterval = connInterval;
	pEvt->slaveLatency = slaveLatency;
	pEvt->supervisionTimeout = supervisionTimeout;
	pEvt->masterClkAccuracy = masterClkAccuracy;

	blc_hci_send_event ( HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 19);
}


void hci_le_connectionUpdateComplete_evt(u8 status, u16 connHandle, u16 connInterval,
        									u16 connLatency, u16 supervisionTimeout)
{
	u8 result[12];
	hci_le_connectionUpdateCompleteEvt_t *pEvt = (hci_le_connectionUpdateCompleteEvt_t *)result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_CONNECTION_UPDATE_COMPLETE;
	pEvt->status = status;
	pEvt->connHandle = connHandle;
	pEvt->connInterval = connInterval;
	pEvt->connLatency = connLatency;
	pEvt->supervisionTimeout = supervisionTimeout;


	blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 10);
}


void hci_le_readRemoteFeaturesComplete_evt(u8 status, u16 connHandle, u8 * feature)
{
	u8 result[12];
	hci_le_readRemoteFeaturesCompleteEvt_t *pEvt = (hci_le_readRemoteFeaturesCompleteEvt_t *)result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_READ_REMOTE_USED_FEATURES_COMPLETE;
	pEvt->status = status;
	pEvt->connHandle = connHandle;
	memcpy(pEvt->feature, feature, LL_FEATURE_SIZE);

	blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 12);
}


#if  (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
void hci_le_phyUpdateComplete_evt(u16 connhandle,u8 status, u8 new_phy)
{
	u8 result[8];
	hci_le_phyUpdateCompleteEvt_t *pEvt = (hci_le_phyUpdateCompleteEvt_t *)result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_PHY_UPDATE_COMPLETE;
	pEvt->status = status;
	pEvt->connHandle = connhandle;
	pEvt->tx_phy = new_phy;
	pEvt->rx_phy = new_phy;

	blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 6);
}
#endif

int hci_le_longTermKeyRequest_evt(u16 connHandle, u8* random, u16 ediv, u8* result)
{
	hci_le_longTermKeyRequestEvt_t* pEvt = (hci_le_longTermKeyRequestEvt_t*) result;// = (hci_le_longTermKeyRequestEvt_t *)ev_buf_allocate(MEDIUM_BUFFER);
	int paramLen = 13;

	pEvt->subEventCode = HCI_SUB_EVT_LE_LONG_TERM_KEY_REQUESTED;
	pEvt->connHandle = connHandle;
	memcpy(pEvt->random, random, 8);
	pEvt->ediv = ediv;

	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, paramLen);
}

int hci_le_readLocalP256KeyComplete_evt(u8* localP256Key, u8 status)
{
	u8 result[66];
	hci_le_readLocalP256KeyCompleteEvt_t *pEvt = (hci_le_readLocalP256KeyCompleteEvt_t*) result;
	int paramLen = 66;

	pEvt->status = status ? BLE_SUCCESS : HCI_ERR_UNSPECIFIED_ERROR;
	pEvt->subEventCode = HCI_SUB_EVT_LE_READ_LOCAL_P256_KEY_COMPLETE;
	memcpy(pEvt->localP256Key, localP256Key, 64);

	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, paramLen);
}

int hci_le_generateDHKeyComplete_evt(u8* DHkey, u8 status)
{
	u8 result[34];
	hci_le_generateDHKeyCompleteEvt_t* pEvt = (hci_le_generateDHKeyCompleteEvt_t*) result;
	int paramLen = 34;

	pEvt->subEventCode = HCI_SUB_EVT_LE_GENERATE_DHKEY_COMPLETE;
	pEvt->status = status ? BLE_SUCCESS : HCI_ERR_INVALID_HCI_CMD_PARAMS;
	memcpy(pEvt->DHKey, DHkey, 32);

	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, paramLen);
}

#if (LL_FEATURE_ENABLE_LL_PRIVACY)
void hci_le_enhancedConnectionComplete_evt(u8 status, u16 connHandle, u8 role, u8 peerAddrType, u8 *peerAddr, u8 *loaclRpa, u8 *peerRpa,
                                           u16 connInterval, u16 connLatency, u16 supervisionTimeout, u8 masterClkAccuracy)
{
	u8 result[32];

	hci_le_enhancedConnCompleteEvt_t *pEvt = (hci_le_enhancedConnCompleteEvt_t *)result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_ENHANCED_CONNECTION_COMPLETE;
	pEvt->status = status;
	pEvt->connHandle = connHandle;
	pEvt->role = role;
	pEvt->PeerAddrType = peerAddrType;
	memcpy(pEvt->PeerAddr, peerAddr, BLE_ADDR_LEN);
	memcpy(pEvt->localRslvPrivAddr, loaclRpa, BLE_ADDR_LEN);
	memcpy(pEvt->Peer_RslvPrivAddr, peerRpa, BLE_ADDR_LEN);
	pEvt->connInterval = connInterval;
	pEvt->conneLatency = connLatency;
	pEvt->supervisionTimeout = supervisionTimeout;
	pEvt->masterClkAccuracy = masterClkAccuracy;

	blc_hci_send_event ( HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, sizeof(hci_le_enhancedConnCompleteEvt_t));
}
#endif

int hci_le_encryptChange_evt(u16 connhandle,  u8 encrypt_en)
{
	u8 result[4] = {0};
	hci_le_encryptEnableEvt_t* pEvt = (hci_le_encryptEnableEvt_t*) result;

	pEvt->status = BLE_SUCCESS;
	pEvt->connHandle = connhandle;
	pEvt->encryption_enable = encrypt_en;

	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_ENCRYPTION_CHANGE, result, 4);
}

int hci_le_encryptKeyRefresh_evt(u16 connhandle)
{
	u8 result[3] = {0};
	hci_le_encryptKeyRefreshEvt_t* pEvt = (hci_le_encryptKeyRefreshEvt_t*) result;

	pEvt->status = BLE_SUCCESS;
	pEvt->connHandle = connhandle;

	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_ENCRYPTION_KEY_REFRESH, result, 3);
}


void hci_le_chennel_selection_algorithm_evt(u16 connhandle, u8 channel_selection_alg)
{
	u8 result[4] = {0};
	hci_le_chnSelectAlgorithmEvt_t*	 pEvt = (hci_le_chnSelectAlgorithmEvt_t*) result;
	pEvt->subEventCode = HCI_SUB_EVT_LE_CHANNEL_SELECTION_ALGORITHM;
	pEvt->connHandle = connhandle;
	pEvt->channel_selection_algotihm = channel_selection_alg;
	blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 4);
}


void hci_le_data_len_update_evt(u16 connhandle, u16 effTxOctets, u16 effRxOctets, u16 maxtxtime, u16 maxrxtime)
{
	u8 result[12];
	hci_le_dataLengthChangeEvt_t *pEvt = (hci_le_dataLengthChangeEvt_t *) result;

	pEvt->subEventCode = HCI_SUB_EVT_LE_DATA_LENGTH_CHANGE;
	pEvt->connHandle = connhandle;
	pEvt->maxTxOct = effTxOctets;
	pEvt->maxTxtime	= maxtxtime;
	pEvt->maxRxOct = effRxOctets;
	pEvt->maxRxtime	= maxrxtime;

	blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	result, 11);
}


#if 0
void hci_le_advertising_set_terminated_evt(u8 status, u8 advertising_handle, u16 connction_handle, u8 num_complete_extended_adv_events)
{
	u8 result[6] = {0};
	hci_le_advSetTerminatedEvt_t*	 pEvt = (hci_le_advSetTerminatedEvt_t*) result;
	pEvt->subEventCode = HCI_SUB_EVT_LE_ADVERTISING_SET_TERMINATED;
	pEvt->status = status;
	pEvt->advHandle = advertising_handle;
	pEvt->connHandle = connction_handle;
	pEvt->num_compExtAdvEvt = num_complete_extended_adv_events;
	blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META, result, 6);
}
#endif


int hci_remoteNateReqComplete_evt (u8* bd_addr)
{
	u8 result[8] = {0}; // not standard
	result[0] = BLE_SUCCESS;
	memcpy (result + 1, bd_addr, 6 );
	result[7] = 0;
	return blc_hci_send_event (HCI_FLAG_EVENT_BT_STD | HCI_EVT_REMOTE_NAME_REQ_COMPLETE, result, 8);

}


#if (LL_FEATURE_ENABLE_ISO)



int hci_le_cisEstablished_evt(u8 status, u16 cisHandle, u8 cigSyncDly[3], u8 cisSyncDly[3], u8 transLaty_m2s[3], u8 transLaty_s2m[3], u8 phy_m2s,
		                      u8 phy_s2m, u8 nse, u8 bn_m2s, u8 bn_s2m, u8 ft_m2s, u8 ft_s2m, u16 maxPDU_m2s, u16 maxPDU_s2m, u16 isoIntvl )
{
	hci_le_cisEstablishedEvt_t Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_CIS_ESTABLISHED;
	Evt.status = status;
	Evt.cisHandle = cisHandle;

	memcpy(Evt.cigSyncDly, cigSyncDly, 3);
	memcpy(Evt.cisSyncDly, cisSyncDly, 3);
	memcpy(Evt.transLaty_m2s, transLaty_m2s, 3);
	memcpy(Evt.transLaty_s2m, transLaty_s2m, 3);

	Evt.phy_m2s = phy_m2s;
	Evt.phy_s2m = phy_s2m;
	Evt.nse = nse;
	Evt.bn_m2s = bn_m2s;
	Evt.ft_m2s = ft_m2s;
	Evt.ft_s2m = ft_s2m;
	Evt.maxPDU_m2s = maxPDU_m2s;
	Evt.maxPDU_s2m = maxPDU_s2m;
	Evt.isoIntvl = isoIntvl;

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_cisEstablishedEvt_t));
}

int hci_le_cisReq_evt(u16 aclHandle, u16 cisHandle, u8 cigId, u8 cisId)
{
	hci_le_cisReqEvt_t  Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_CIS_REQUESTED;
	Evt.aclHandle = aclHandle;
	Evt.cisHandle = cisHandle;
	Evt.cigId = cigId;
	Evt.cisId = cisId;

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_cisReqEvt_t));
}

int hci_le_craeteBigComplete_evt(u8 staus, u8 bigHandle, u8 bigSyncDly[3], u8 transLatyBig[3], u8 phy, u8 nse,
								 u8 bn, u8 pto, u8 irc, u16 maxPDU, u16 isoIntvl,  u8 numBis, u16 bisHandles[BIS_IN_BIG_NUM_MAX])
{
	//TODO: must check param: numBis <= BIS_IN_BIG_NUM_MAX

	hci_le_createBigCompleteEvt_t Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_CREATE_BIG_COMPLETE;
	Evt.staus = staus;
	Evt.bigHandle = bigHandle;
	memcpy(Evt.bigSyncDly, bigSyncDly, 3);
	memcpy(Evt.transLatyBig, transLatyBig, 3);
	Evt.phy = phy;
	Evt.nse = nse;
	Evt.bn = bn;
	Evt.pto = pto;
	Evt.irc = irc;
	Evt.maxPDU = maxPDU;
	Evt.isoIntvl = isoIntvl;
	Evt.numBis = numBis;

//	u8 i = 0;
//	while(numBis--){
//		Evt.bisHandles[i] = bisHandles[i];
//		i++;
//	}
	memcpy((u8*)Evt.bisHandles, (u8*)bisHandles, 2*numBis);

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_createBigCompleteEvt_t)-2*(BIS_IN_BIG_NUM_MAX-numBis));
}

int hci_le_terminateBigComplete_evt(u8 bigHandle, u8 reason)
{
	hci_le_terminateBigCompleteEvt_t  Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_TERMINATE_BIG_COMPLETE;
	Evt.bigHandle = bigHandle;
	Evt.reason = reason;

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_terminateBigCompleteEvt_t));
}

int hci_le_bigSyncEstablished_evt(u8 staus, u8 bigHandle, u8 transLatyBig[3], u8 nse, u8 bn, u8 pto, u8 irc,
		                          u16 maxPDU, u16 isoIntvl,  u8 numBis, u16 bisHandles[BIS_IN_BIG_NUM_MAX])
{
	//TODO: must check param: numBis <= BIS_IN_BIG_NUM_MAX

	hci_le_bigSyncEstablishedEvt_t Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_BIG_SYNC_ESTABLILSHED;
	Evt.staus = staus;
	Evt.bigHandle = bigHandle;
	memcpy(Evt.transLatyBig, transLatyBig, 3);
	Evt.nse = nse;
	Evt.bn = bn;
	Evt.pto = pto;
	Evt.irc = irc;
	Evt.maxPDU = maxPDU;
	Evt.isoIntvl = isoIntvl;
	Evt.numBis = numBis;

//	u8 i = 0;
//	while(numBis--){
//		Evt.bisHandles[i] = bisHandles[i];
//		i++;
//	}
	memcpy((u8*)Evt.bisHandles, (u8*)bisHandles, 2*numBis);

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_bigSyncEstablishedEvt_t)-2*(BIS_IN_BIG_NUM_MAX-numBis));
}


int hci_le_bigSyncLost_evt(u8 bigHandle, u8 reason)
{
	hci_le_bigSyncLostEvt_t  Evt;
	Evt.subEventCode = HCI_SUB_EVT_LE_BIG_SYNC_LOST;
	Evt.bigHandle = bigHandle;
	Evt.reason = reason;

	return blc_hci_send_event(HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META,	(u8*)&Evt, sizeof(hci_le_bigSyncLostEvt_t));
}


#endif



