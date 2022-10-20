/********************************************************************************************************
 * @file	ll_resolvlist.c
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
#include <stack/ble/ble_common.h>
#include "tl_common.h"
#include "stack/ble/controller/ble_controller.h"
#include "stack/ble/host/ble_host.h"

#include "ll_resolvlist.h"

#if (LL_FEATURE_ENABLE_LL_PRIVACY)

_attribute_data_retention_	ll_ResolvingListTbl_t	ll_resolvingList_tbl;

_attribute_data_retention_ u32 ll_resolvRpaTmrTick = 0;
_attribute_data_retention_ u32 rpaTmrCntBased1sUnit = 0;


bool blt_ll_resolvIrkIsNonzero(const u8 *irk)
{
    int rc = 0;

    for(int i = 0; i < 16; i++){
        if (irk[i] != 0) {
            rc = 1;
            break;
        }
    }

    return rc;
}


/**
 * This command shall not be used when address resolution is enabled in the Controller and:
 * (1)Advertising (other than periodic advertising) is enabled,
 * (2)Scanning is enabled, or an HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection,
 *   or HCI_LE_Periodic_Advertising_Create_Sync command is outstanding.
 * This command may be used at any time when address resolution is disabled in the Controller.
 * @return int 0: not allowed. 1: allowed.
 */
bool blt_ll_resolvChgIsAllowed(void)
{
    int rc = 1;

    if(ll_resolvingList_tbl.addrRlEn &&  \
      (bltParam.adv_en || blts.scan_en || blm_create_connection)){
        rc = 0;
    }

    return rc;
}


void	blt_ll_resolvGenRpa(ll_resolv_list_t *rl, u8 local)
{
	u8* irk = NULL;
	u8* addr = NULL;
	u8* prand = NULL;

    if(local){
        addr = rl->rlLocalRpa;
        irk = rl->rlLocalIrk;
//        printf("get local irk: %x...%x\n",rl->rlLocalIrk[0],rl->rlLocalIrk[15]);
    }
    else{
        addr = rl->rlPeerRpa;
        irk = rl->rlPeerIrk;
//        printf("get peer irk: %x...%x\n",rl->rlLocalIrk[0],rl->rlLocalIrk[15]);
    }

//    array_printf(irk, 16);

    //need re-check
	u8 const_u8_16_zero[16]= {0};
	if(!memcmp(irk, const_u8_16_zero, 16)){
//		printf("err ah\n");
		return;
	}

    prand = addr + 3;

	// Resolvable private address:
	// LSB                                                     MSB
	// +--------------------------+----------------------+---+---+
	// |                          | Random part of prand | 1 | 0 |
	// +--------------------------+----------------------+---+---+
	// <--------+ hash +---------> <-----------+ prand +--------->
	//		    (24 bits)                     (24 bits)

    u32 irq = irq_disable();

    blt_crypto_alg_prand(prand); //Get prand
    blt_crypto_alg_ah(irk, prand, addr); //Calculate hash, hash = ah(local IRK, prand)

    irq_restore(irq);

//	printf("Created RPA: %x:%x:%x:%x:%x:%x\n", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

#if 0 //just test use
	if(blc_ll_resolvIsAddrResolved(irk , addr)){
		printf("Created RPA is resolved\n");
	}
	else{
		printf("Created RPA is non-resolved\n");
	}
#endif
}

void blt_ll_resolvRpaTmrCb(void)
{
	ll_resolv_list_t* rl = NULL;

	for(int idx = 0; idx < ll_resolvingList_tbl.rlCnt; idx++){
		rl = &ll_resolvingList_tbl.rlList[idx];

		if(rl->rlHasLocalRpa){
			blt_ll_resolvGenRpa(rl, 1); //Generate RPA for local device
		}

		if(rl->rlHasPeerRpa){
			blt_ll_resolvGenRpa(rl, 0); //Generate RPA for peer device
		}
	}

	//reset RPA timeout tick
	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmr1sChkTick = clock_time();
	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit = 0;

	//Mark RPA as timed out so we get a new RPA for Adv
	blt_ll_advSetRpaTmoFlg();
}


int blt_ll_resolvRpaTmoLoopEvt (void)
{
	if(ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn){
		if(clock_time_exceed(ll_resolvingList_tbl.rpaTmoCtrl.rpaTmr1sChkTick , 1*1000*1000)){  // 1s
			ll_resolvingList_tbl.rpaTmoCtrl.rpaTmr1sChkTick = clock_time();
			ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit++;
		}

		if(ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit >= ll_resolvingList_tbl.rpaTmoCtrl.rpaTimeoutXs){
			ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit = 0;

			blt_ll_resolvRpaTmrCb();
		}
	}

	return 0;
}


/**
 * Used to determine if the device is on the resolving list.
 *
 * @param addr
 * @param addr_type Public address (0) or random address (1)
 *
 * @return -1: device is not on resolving list; otherwise the return value is the 'position' of the device in the resolving list
 */
int blt_ll_resolvIsAddrOnRl(const u8 *addr, u8 addrType)
{
	ll_resolv_list_t* rl = NULL;

	for(int idx = 0; idx < ll_resolvingList_tbl.rlCnt; idx++){
		rl = &ll_resolvingList_tbl.rlList[idx];
        if((rl->rlIdAddrType == addrType) && (!memcmp(&rl->rlIdAddr, addr, BLE_ADDR_LEN))){
            return idx;
        }
    }

    return -1;
}


/**
 * Used to determine if the device is on the resolving list.
 *
 * @param addr
 * @param addr_type Public address (0) or random address (1)
 *
 * @return Pointer to resolving list entry or NULL if no entry found.
 */
ll_resolv_list_t* blt_ll_resolvFindRlEntry(const u8* addr, u8 addrType)
{
	ll_resolv_list_t* rl = NULL;

	for(int idx = 0; idx < ll_resolvingList_tbl.rlCnt; idx++){
		rl = &ll_resolvingList_tbl.rlList[idx];
        if((rl->rlIdAddrType == addrType) && (!memcmp(&rl->rlIdAddr, addr, BLE_ADDR_LEN))){
            return rl;
        }
    }

    return NULL;
}


//ble_sts_t		blc_ll_resolvListAdd(u8 peerIdAddrType, u8 *peerIdAddr, u8 *peer_irk, u8 *local_irk);
//ble_sts_t		blc_ll_resolvListRemove(u8 peerIdAddrType, u8 *peerIdAddr);
//ble_sts_t		blc_ll_resolvListClear(void);
//ble_sts_t		blc_ll_resolvListReadSize(u8 *Size);
//ble_sts_t		blc_ll_resolvReadPeerRpa(u8 peerIdAddrType, u8* peerIdAddr, u8* peerResolvableAddr);
//ble_sts_t		blc_ll_resolvingReadLocalRpa(u8 peerIdAddrType, u8* peerIdAddr, u8* LocalResolvableAddr);
//ble_sts_t		blc_ll_resolvSetAddrResolutionEn(u8 resolutionEn);
//ble_sts_t		blc_ll_resolvSetRpaTmo(u16 timeout_s);
//ble_sts_t		blc_ll_resolvSetPrivMode(u8 peerIdAddrType, u8* peerIdAddr, u8 privMode);


/*
 * @brief
 *
 * @param[in]	peer_irk  - The IRKs are stored in little endian format
 * @param[in]	local_irk - The addr are stored in little endian format
 *
 * @return
 * */
ble_sts_t  ll_resolvingList_add(u8 peerIdAddrType, u8 *peerIdAddr, u8 *peer_irk, u8 *local_irk)
//ble_sts_t blc_ll_resolvListAdd(u8 peerIdAddrType, u8 *peerIdAddr, u8 *peer_irk, u8 *local_irk)
{
	if(!blt_ll_resolvChgIsAllowed()){
		return HCI_ERR_CMD_DISALLOWED;
	}

	if(ll_resolvingList_tbl.rlCnt > ll_resolvingList_tbl.rlSize){
		return HCI_ERR_MEM_CAP_EXCEEDED;
	}

    if(peerIdAddrType > 1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

    //Core spec is not clear on how to handle this but make sure host is aware that new keys are not used in that case
    if (blt_ll_resolvIsAddrOnRl(peerIdAddr, peerIdAddrType) != -1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

    ll_resolv_list_t* rl = &ll_resolvingList_tbl.rlList[ll_resolvingList_tbl.rlCnt];
	rl->rlIdAddrType = peerIdAddrType;
	smemcpy((char*)rl->rlIdAddr, (char*)peerIdAddr, BLE_ADDR_LEN);

	if(blt_ll_resolvIrkIsNonzero(peer_irk)){
		//printf("peer irk nonzero\n");
		rl->rlHasPeerRpa = 1;
		smemcpy(rl->rlPeerIrk, peer_irk, 16);
		blt_ll_resolvGenRpa(rl, 0); //Create peer RPA now, it will be updated by RPA timer when resolution is enabled
    }

	if(blt_ll_resolvIrkIsNonzero(local_irk)){
		//printf("local irk nonzero\n");
		rl->rlHasLocalRpa = 1;
		smemcpy(rl->rlLocalIrk, local_irk, 16);
		blt_ll_resolvGenRpa(rl, 1);
	}

    //By default use privacy network mode
    rl->rlPrivMode = PRIVACY_NETWORK_MODE;

    ll_resolvingList_tbl.rlCnt++;

    if(ll_resolvingList_tbl.rlCnt == 1){
    	//start RPA timer if this was first element added to RL
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn = 1;
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmr1sChkTick = clock_time();
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit = 0;
    }

	return BLE_SUCCESS;
}

ble_sts_t ll_resolvingList_delete(u8 peerIdAddrType, u8 *peerIdAddr)
//ble_sts_t blc_ll_resolvListRemove(u8 peerIdAddrType, u8 *peerIdAddr)
{
	if(!blt_ll_resolvChgIsAllowed()){
		return HCI_ERR_CMD_DISALLOWED;
	}

    if(peerIdAddrType > 1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

	int idx = blt_ll_resolvIsAddrOnRl(peerIdAddr, peerIdAddrType);

	//When a Controller cannot remove a device from the resolving list because it is not found
	if(idx != -1){
		return HCI_ERR_UNKNOWN_CONN_ID;
	}

	memcpy( &ll_resolvingList_tbl.rlList[idx], \
			&ll_resolvingList_tbl.rlList[idx+1], \
			(ll_resolvingList_tbl.rlCnt - idx -1) * sizeof(ll_resolv_list_t));

	ll_resolvingList_tbl.rlCnt--;

    if(ll_resolvingList_tbl.rlCnt == 0){
    	//stop RPA timer if RL is empty
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn = 0;
    }

	return BLE_SUCCESS;
}

ble_sts_t 	ll_resolvingList_reset(void)
//ble_sts_t blc_ll_resolvListClear(void)
{
	if(!blt_ll_resolvChgIsAllowed()){
		return HCI_ERR_CMD_DISALLOWED;
	}

	ll_resolvingList_tbl.rlCnt = 0;

	//stop RPA timer when clearing RL
	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn = 0;

	return BLE_SUCCESS;
}

ble_sts_t ll_resolvingList_getSize(u8 *Size)
//ble_sts_t blc_ll_resolvListReadSize(u8 *Size)
{
	*Size = MAX_RESOLVING_LIST_SIZE;
	return BLE_SUCCESS;
}

ble_sts_t ll_resolvingList_getPeerResolvableAddr(u8 peerIdAddrType, u8* peerIdAddr, u8* peerResolvableAddr)
//ble_sts_t blc_ll_resolvReadPeerRpa(u8 peerIdAddrType, u8* peerIdAddr, u8* peerResolvableAddr)
{
	ll_resolv_list_t* rl = blt_ll_resolvFindRlEntry(peerIdAddr, peerIdAddrType);

	if(!rl){
		smemset(peerResolvableAddr, 0, BLE_ADDR_LEN);
		return HCI_ERR_UNKNOWN_CONN_ID;
	}

    if(peerIdAddrType > 1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

	//TODO: if need check rl->rlHasPeerRpa == 1 ?
	smemcpy((char*)peerResolvableAddr, (char*)rl->rlPeerRpa, BLE_ADDR_LEN);

	return BLE_SUCCESS;
}

ble_sts_t ll_resolvingList_getLocalResolvableAddr(u8 peerIdAddrType, u8* peerIdAddr, u8* LocalResolvableAddr)
//ble_sts_t blc_ll_resolvingReadLocalRpa(u8 peerIdAddrType, u8* peerIdAddr, u8* LocalResolvableAddr)
{
	ll_resolv_list_t* rl = blt_ll_resolvFindRlEntry(peerIdAddr, peerIdAddrType);

	if(!rl){
		smemset(LocalResolvableAddr, 0, BLE_ADDR_LEN);
		return HCI_ERR_UNKNOWN_CONN_ID;
	}

    if(peerIdAddrType > 1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

	//TODO: if need check rl->rlLocalRpa == 1 ?
	smemcpy((char*)LocalResolvableAddr, (char*)rl->rlLocalRpa, BLE_ADDR_LEN);

	return BLE_SUCCESS;
}


/*
 * @brief 	API to  enable resolution of Resolvable Private Addresses in the Controller.
 * 			This causes the Controller to use the resolving list whenever the Controller
 * 			receives a local or peer Resolvable Private Address.
 *
 * */
ble_sts_t ll_resolvingList_setAddrResolutionEnable (u8 resolutionEn)
//ble_sts_t blc_ll_resolvSetAddrResolutionEn(u8 resolutionEn)
{
	if(!blt_ll_resolvChgIsAllowed()){
		return HCI_ERR_CMD_DISALLOWED;
	}

    if(resolutionEn > 1){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

    //refer to Core spec page2564: This command does not affect the generation of Resolvable Private Addresses.

    ll_resolvingList_tbl.addrRlEn = resolutionEn;

	return BLE_SUCCESS;
}


/*
 * @brief 	API to set the length of time the controller uses a Resolvable Private Address
 * 			before a new resolvable	private address is generated and starts being used.
 * 			This timeout applies to all addresses generated by the controller
 *
 * */
ble_sts_t  ll_resolvingList_setResolvablePrivateAddrTimer (u16 timeout_s)
//ble_sts_t  blc_ll_resolvSetRpaTmo(u16 timeout_s)
{
    if(!((timeout_s > 0) && (timeout_s <= 0x0E10))){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

    ll_resolvingList_tbl.rpaTmoCtrl.rpaTimeoutXs = timeout_s; //unit: s;

    if(ll_resolvingList_tbl.rlCnt){
    	//restart timer if there is something on RL
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn = 1;
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmr1sChkTick = clock_time();
    	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrCnt1sUnit = 0;
    }

	return BLE_SUCCESS;
}


/*
 * @brief 	API to used to allow the Host to specify the privacy mode to be used for a given
 * 			entry on the resolving list.
 *
 * */
ble_sts_t  ll_resolvingList_setPrivcyMode(u8 peerIdAddrType, u8* peerIdAddr, u8 privMode)
//ble_sts_t blc_ll_resolvSetPrivMode(u8 peerIdAddrType, u8* peerIdAddr, u8 privMode)
{
	if(!blt_ll_resolvChgIsAllowed()){
		return HCI_ERR_CMD_DISALLOWED;
	}

    if(peerIdAddrType > 1 || privMode > PRIVACY_DEVICE_MODE){
        return HCI_ERR_INVALID_HCI_CMD_PARAMS;
    }

	ll_resolv_list_t* rl = blt_ll_resolvFindRlEntry(peerIdAddr, peerIdAddrType);

    if (!rl) {
        return HCI_ERR_UNKNOWN_CONN_ID;
    }

    rl->rlPrivMode = privMode;

    return BLE_SUCCESS;
}


unsigned short blc_ll_resolvGetRpaTmo(void)
{
    return ll_resolvingList_tbl.rpaTmoCtrl.rpaTimeoutXs;
}


void blc_ll_resolvGetRpaByRlEntry(ll_resolv_list_t* rl, u8* addr, u8 local)
{
    if(local){
        smemcpy((char*)addr, (char*)rl->rlLocalRpa, BLE_ADDR_LEN);
    }
    else{
        smemcpy((char*)addr, (char*)rl->rlPeerRpa, BLE_ADDR_LEN);
    }
}


void blc_ll_resolvSetPeerRpaByIdx(u8 idx, u8 *rpa)
{
	ll_resolv_list_t* rl = &ll_resolvingList_tbl.rlList[idx];
    smemcpy((char*)rl->rlPeerRpa, (char*)rpa, BLE_ADDR_LEN);
}


void blc_ll_resolvSetLocalRpaByIdx(u8 idx, u8 *rpa)
{
	ll_resolv_list_t* rl = &ll_resolvingList_tbl.rlList[idx];
    smemcpy((char*)rl->rlLocalRpa, (char*)rpa, BLE_ADDR_LEN);
}


bool blc_ll_resolvGetRpaByAddr(u8* peerIdAddr, u8 peerIdAddrType, u8* rpa, u8 local)
{
	ll_resolv_list_t* rl = blt_ll_resolvFindRlEntry(peerIdAddr, peerIdAddrType);

    if(rl){
        if ((local && rl->rlHasLocalRpa) || (!local && rl->rlHasPeerRpa)) {
        	blc_ll_resolvGetRpaByRlEntry(rl, rpa, local);
            return 1;
        }
    }

    return 0;
}


/*
 * @brief 	Resolvable a Private Address
 *
 * @param[in]	irk  - The IRKs are stored in little endian format
 * @param[in]	addr - The addr are stored in little endian format
 *
 * @return      1: Address resolution succeeded; 0: Address resolution failed
 * */
#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
bool blc_ll_resolvIsAddrResolved(const u8* irk, const u8* addr)
{
	u8 hash[3];
	blt_crypto_alg_ah(irk, (u8*)(addr + 3), hash);

//	printf("hash:%x %x %x, add:%x %x %x\n ",hash[0],hash[1],hash[2],addr[0],addr[1],addr[2]);

	if(hash[0] == addr[0] && hash[1] == addr[1] && hash[2] == addr[2]){
		return 1;
	}

	return 0;
}

#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
int blc_ll_resolvPeerRpaResolvedAny(const u8* rpa)
{
    for(int i = 0; i < ll_resolvingList_tbl.rlCnt; i++){
        if(blc_ll_resolvIsAddrResolved(ll_resolvingList_tbl.rlList[i].rlPeerIrk, rpa)){
            return i;
        }
    }

    return -1;
}


void blc_ll_resolvListInit(void)
{
	ll_resolvingList_tbl.addrRlEn = 0; //disable private address resolution
	ll_resolvingList_tbl.rpaTmoCtrl.rpaTmrEn = 0; //close a soft timer for RPA

	//blc_ll_resolvListClear();
	ll_resolvingList_reset();

	ll_resolvingList_tbl.rlSize = MAX_RESOLVING_LIST_SIZE;
	ll_resolvingList_tbl.rpaTmoCtrl.rpaTimeoutXs = 900; //dft: 15min, unit: s

	blc_ll_registerRpaTmoMainloopCallback(blt_ll_resolvRpaTmoLoopEvt); //register a soft timer callback for RPA

}


/**
 * Returns whether or not address resolution is enabled.
 *
 * @return uint8_t
 */
#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
bool blc_ll_resolvIsAddrRlEnabled(void)
{
    return ll_resolvingList_tbl.addrRlEn;
}


//////////////////////////////////////////////////////////////////////////////////////////
//TODO: only support slave role/adv ll privacy1.2 feature, so this API is needed, we'll remove it in the future version
//According to the spec, RPA can also be placed in the white list. If the RPA used in the master's connect_req has not
//been updated, the white list can be searched immediately, which saves a lot of time. When the white list cannot be
//searched and it is RPA, then search the resolving list
/*********************************************************************
 * @fn      ll_searchAddr_in_WhiteList_and_ResolvingList
 *
 * @brief   API to search  list entry through specified address
 *
 * @param   type - Specified BLE address type
 *                addr - Specified BLE address
 *                table - Table to be searched
 *                tblSize - Table size
 *
 * @return  Pointer to the found entry. NULL means not found
 */
#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
u8 * blt_ll_searchAddr_in_WhiteList(u8 type, u8 *addr)
{
	//search in white list first
	for(int i=0; i<ll_whiteList_tbl.wl_addr_tbl_index; i++){
		if( type==ll_whiteList_tbl.wl_addr_tbl[i].type && MAC_MATCH8(ll_whiteList_tbl.wl_addr_tbl[i].address, addr) ){
			return (u8 *)(&(ll_whiteList_tbl.wl_addr_tbl[i]));
		}
	}

	return NULL;
}

#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
u8 * blt_ll_searchAddr_in_ResolvingList(u8 type, u8 *addr)
{
	//search in resolving list
	if( IS_RESOLVABLE_PRIVATE_ADDR(type, addr) && ll_resolvingList_tbl.addrRlEn){
		for(int i = 0; i < ll_resolvingList_tbl.rlCnt; i++) {
			if(smp_quickResolvPrivateAddr(ll_resolvingList_tbl.rlList[i].rlPeerIrk, addr)) {
				return (u8*)(ll_resolvingList_tbl.rlList[i].rlPeerIrk);
			}
		}
	}

	return NULL;
}

#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
u8 * ll_searchAddr_in_WhiteList_and_ResolvingList(u8 type, u8 *addr) //Old RL version, will remove latter
{
	u8 *r = NULL;
	//search in white list first
	if((r = blt_ll_searchAddr_in_WhiteList(type, addr)) == NULL){
		//if not in white list and is RPA, search in resolving list
		r = blt_ll_searchAddr_in_ResolvingList(type, addr);
	}

	return r;
}


#else

_attribute_data_retention_	ll_ResolvingListTbl_t	ll_resolvingList_tbl;

//-------------------------------------------------------------
ble_sts_t  ll_resolvingList_delete(u8 peerIdAddrType, u8 *peerIdAddr)
{
	for (int i=0; i<ll_resolvingList_tbl.idx; i++)
	{
		if ((ll_resolvingList_tbl.tbl[i].type == peerIdAddrType) &&
			(smemcmp (ll_resolvingList_tbl.tbl[i].address, peerIdAddr, BLE_ADDR_LEN) == 0))
		{
			smemcpy (&ll_resolvingList_tbl.tbl[i], &ll_resolvingList_tbl.tbl[i + 1],
					(ll_resolvingList_tbl.idx - i - 1) * sizeof (rl_addr_t));
			ll_resolvingList_tbl.idx --;
			return BLE_SUCCESS;
		}
	}

	return BLE_SUCCESS;  //Same as white list, delete a non-list device and return success
}


ble_sts_t  ll_resolvingList_add(u8 peerIdAddrType, u8 *peerIdAddr, u8 *peer_irk, u8 *local_irk)
{
	if (ll_resolvingList_tbl.idx < MAX_WHITE_IRK_LIST_SIZE)
	{
		ll_resolvingList_tbl.tbl[ll_resolvingList_tbl.idx].type = peerIdAddrType;
		smemcpy (ll_resolvingList_tbl.tbl[ll_resolvingList_tbl.idx].address, peerIdAddr, BLE_ADDR_LEN);
		smemcpy (ll_resolvingList_tbl.tbl[ll_resolvingList_tbl.idx].irk, peer_irk, 16);
		ll_resolvingList_tbl.idx ++;
		return BLE_SUCCESS;
	}

	return HCI_ERR_MEM_CAP_EXCEEDED;
}




u8 * ll_searchAddrInResolvingListTbl(u8 *addr)
{
	if(ll_resolvingList_tbl.en){
		for(int i = 0; i < ll_resolvingList_tbl.idx; i++) {
			if(smp_quickResolvPrivateAddr(ll_resolvingList_tbl.tbl[i].irk, addr)) {
				return (u8 *)(ll_resolvingList_tbl.tbl[i].irk);
			}
		}
	}

	return NULL;
}


ble_sts_t 	ll_resolvingList_reset(void)
{
	ll_resolvingList_tbl.idx = 0;
	return BLE_SUCCESS;
}

ble_sts_t ll_resolvingList_getSize(u8 *Size)
{
	*Size = MAX_WHITE_IRK_LIST_SIZE;
	return BLE_SUCCESS;
}



ble_sts_t ll_resolvingList_getPeerResolvableAddr(u8 peerIdAddrType, u8* peerIdAddr, u8* peerResolvableAddr)
{

	return HCI_ERR_UNKNOWN_CONN_ID;
}

ble_sts_t ll_resolvingList_getLocalResolvableAddr(u8 peerIdAddrType, u8* peerIdAddr, u8* LocalResolvableAddr)
{

	return BLE_SUCCESS;
}

/*
 * @brief 	API to  enable resolution of Resolvable Private Addresses in the Controller.
 * 			This causes the Controller to use the resolving list whenever the Controller
 * 			receives a local or peer Resolvable Private Address.
 *
 * */
ble_sts_t ll_resolvingList_setAddrResolutionEnable (u8 resolutionEn)
{
	ll_resolvingList_tbl.en = resolutionEn;

	return BLE_SUCCESS;
}


/*
 * @brief 	API to set the length of time the controller uses a Resolvable Private Address
 * 			before a new resolvable	private address is generated and starts being used.
 * 			This timeout applies to all addresses generated by the controller
 *
 * */
ble_sts_t  ll_resolvingList_setResolvablePrivateAddrTimer (u16 timeout_s)
{

	return BLE_SUCCESS;
}



/*********************************************************************
 * @fn      ll_searchAddr_in_WhiteList_and_ResolvingList
 *
 * @brief   API to search  list entry through specified address
 *
 * @param   type - Specified BLE address type
 *                addr - Specified BLE address
 *                table - Table to be searched
 *                tblSize - Table size
 *
 * @return  Pointer to the found entry. NULL means not found
 */
_attribute_ram_code_ u8 * blt_ll_searchAddr_in_WhiteList(u8 type, u8 *addr)
{
	for(int i=0; i<ll_whiteList_tbl.wl_addr_tbl_index; i++){
		if( type==ll_whiteList_tbl.wl_addr_tbl[i].type && MAC_MATCH8(ll_whiteList_tbl.wl_addr_tbl[i].address, addr) ){
			return (u8 *)(&(ll_whiteList_tbl.wl_addr_tbl[i]));
		}
	}

	return NULL;
}

_attribute_ram_code_ u8 * blt_ll_searchAddr_in_ResolvingList(u8 type, u8 *addr)
{
	if( IS_RESOLVABLE_PRIVATE_ADDR(type, addr) && ll_resolvingList_tbl.en){
		for(int i = 0; i < ll_resolvingList_tbl.idx; i++) {
			if(smp_quickResolvPrivateAddr(ll_resolvingList_tbl.tbl[i].irk, addr)) {
				return (u8 *)(ll_resolvingList_tbl.tbl[i].irk);
			}
		}
	}

	return NULL;
}

_attribute_ram_code_ u8 * ll_searchAddr_in_WhiteList_and_ResolvingList(u8 type, u8 *addr)
{
	u8 *r = NULL;
	//search in white list first
	if((r = blt_ll_searchAddr_in_WhiteList(type, addr)) == NULL){
		//if not in white list and is RPA, search in resolving list
		r = blt_ll_searchAddr_in_ResolvingList(type, addr);
	}

	return r;
}

#endif// The end of #if (LL_FEATURE_ENABLE_LL_PRIVACY)

