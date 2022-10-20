/********************************************************************************************************
 * @file	ll_whitelist.c
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
#include "stack/ble/controller/ble_controller.h"

_attribute_data_retention_	ll_whiteListTbl_t		ll_whiteList_tbl;

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
_attribute_ram_code_
bool smp_quickResolvPrivateAddr(u8 *key, u8 *addr) {

	//16M 46us, need irq disable protect(slave maybe decrypt conn_param_update and map_update using AES),prevent reentry
	u32 r = irq_disable();


	/*Set AES as encryption mode*/
	reg_aes_ctrl = 0x00;

	/*Set AES key*/
#if (IRK_REVERT_TO_SAVE_AES_TMIE_ENABLE)  //revert irk
	REG_ADDR32(aeskey_reg_start) = *((volatile u32 *) key);
	REG_ADDR32(aeskey_reg_start + 4) = *((volatile u32 *)(key + 4));
	REG_ADDR32(aeskey_reg_start + 8) = *((volatile u32 *)(key + 8));
	REG_ADDR32(aeskey_reg_start + 12) = *((volatile u32 *)(key + 12));
#else //standard irk
	for(int i=0;i<16;i++){
		REG_ADDR8(aeskey_reg_start + i) = key[15 -i];
	}
#endif


	/*Set AES data, after write four times, start to encrypt automatically*/
	reg_aes_data = 0x00;
	reg_aes_data = 0x00;
	reg_aes_data = 0x00;
	reg_aes_data = (addr[3] << 24) + (addr[4] << 16) + (addr[5] << 8);  //prand 3 byte


	/* wait for aes ready */
	while(!(reg_aes_ctrl & BIT(2)));

	/*Read AES Result, need only the highest 3 bytes*/
	u32 aes_temp;
	u8 aes_result[4];
	aes_temp = reg_aes_data;
	aes_temp = reg_aes_data;
	aes_temp = reg_aes_data;
	#if (1) ///to remove warning.
		u8* tmp_aes = aes_result;
		* ((u32 *)tmp_aes) = reg_aes_data;  //hash 3 byte
	#else
		* ((u32 *)aes_result) = reg_aes_data;  //hash 3 byte
	#endif


	irq_restore(r);


	if( aes_result[3]==addr[0] && aes_result[2]==addr[1] && aes_result[1]==addr[2]){
		return 1;
	}

	return 0;
}
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
/*
Sample data:
u8 test_irk[16]  = {0x71, 0x4a ,0x57 ,0x3d,  0xf6 ,0x88, 0x69 ,0x0c,  0x57, 0x98, 0x50, 0x51, 0x82 ,0xf5 ,0x2a, 0xa0};
u8 test_mac[6]  = {0x3b, 0x3f, 0xfb, 0xeb, 0x1e, 0x78};

u8 result = smp_quickResolvPrivateAddr(test_irk, test_mac);

 */

//the RAM address cannot be greater than 64K, because the maximum address space for reg_AES_mode is only 0xFFFF
extern unsigned int aes_data_buff[8];

_attribute_ram_code_
bool smp_quickResolvPrivateAddr(u8 *key, u8 *addr) {


	u32 r = irq_disable();

	u32 *irk_key = (u32*)key;

	int i;

	for (i=0; i<4; i++) {
		reg_aes_key(i) = irk_key[i];
	}


	aes_data_buff[0] = ((addr[3] << 0) | (addr[4] << 8) | (addr[5] << 16) );  //prand 3 byte
	aes_data_buff[1] = 0;
	aes_data_buff[2] = 0;
	aes_data_buff[3] = 0;

    reg_aes_ptr = (u32)aes_data_buff;

    reg_aes_mode = FLD_AES_START|AES_ENCRYPT_MODE;   //encrypt

    while(reg_aes_mode & FLD_AES_START);


	irq_restore(r);


	if(	(aes_data_buff[4] & 0xffffff) == (addr[0] | addr[1]<<8 | addr[2]<<16) ){
		return 1;
	}

	return 0;
}
#endif



/*********************************************************************
 * @fn      ll_whiteList_reset
 *
 * @brief   API to reset the white list table.
 *
 * @param   None
 *
 * @return  LL Status
 */
ble_sts_t 	ll_whiteList_reset(void)
{
	ll_whiteList_tbl.wl_addr_tbl_index = 0;
	return BLE_SUCCESS;
}



/*********************************************************************
 * @fn      ll_whiteList_add
 *
 * @brief   API to add new entry to white list
 *
 * @param   None
 *
 * @return  LL Status
 */
ble_sts_t ll_whiteList_add(u8 type, u8 *addr)
{
	if(ll_searchAddrInWhiteListTbl(type, addr)) {

		return BLE_SUCCESS;
	}


	if(ll_whiteList_tbl.wl_addr_tbl_index < MAX_WHITE_LIST_SIZE) {
		ll_whiteList_tbl.wl_addr_tbl[ll_whiteList_tbl.wl_addr_tbl_index].type = type;
		smemcpy(ll_whiteList_tbl.wl_addr_tbl[ll_whiteList_tbl.wl_addr_tbl_index].address, addr, BLE_ADDR_LEN);
		ll_whiteList_tbl.wl_addr_tbl_index++;

		return BLE_SUCCESS;
	}
	else {
		return HCI_ERR_MEM_CAP_EXCEEDED;

	}
}

ble_sts_t ll_whiteList_add2(u8* p)
{
	if(ll_whiteList_tbl.wl_addr_tbl_index < MAX_WHITE_LIST_SIZE)
	{
		smemcpy(&ll_whiteList_tbl.wl_addr_tbl[ll_whiteList_tbl.wl_addr_tbl_index].type, p, 8);
		ll_whiteList_tbl.wl_addr_tbl_index++;

		return BLE_SUCCESS;
	}
	return HCI_ERR_MEM_CAP_EXCEEDED;
}

/*********************************************************************
 * @fn      ll_searchAddrInWhiteListTbl
 *
 * @brief   API to check if address is existed in white list table
 *
 * @param   None
 *
 * @return  BLE_SUCCESS(0, Exist in table)
 *
 */
u8 * ll_searchAddrInWhiteListTbl(u8 type, u8 *addr)
{

	for(int i=0; i<ll_whiteList_tbl.wl_addr_tbl_index; i++){
		if( type==ll_whiteList_tbl.wl_addr_tbl[i].type && MAC_MATCH8(ll_whiteList_tbl.wl_addr_tbl[i].address, addr) ){
			return (u8 *)(&(ll_whiteList_tbl.wl_addr_tbl[i]));
		}
	}


	return NULL;
}


//u8 ll_whiteList_rsvd_field(u8 type, u8 *addr)
//{
//	for(int i=0;i<ll_whiteList_tbl.wl_addr_tbl_index;i++)
//	{
//		if( MAC_MATCH8(ll_whiteList_tbl.wl_addr_tbl[i].address, addr) && type==ll_whiteList_tbl.wl_addr_tbl[i].type ){
//			return (u8 *)ll_whiteList_tbl.wl_addr_tbl[i].reserved;
//		}
//	}
//	return 0;
//}


ble_sts_t  ll_whiteList_delete(u8 type, u8 *addr)
{
	u8 *pEntry = NULL;
	pEntry = ll_searchAddrInWhiteListTbl(type, addr);

	/*If has not found the addr in irk_table, or addr_table, return  ? */
	if(pEntry == NULL) {

		return BLE_SUCCESS;
	}


	/*If it is not the last addr stored in the table, need to move the last addr to this index*/
	wl_addr_t *addr_temp = (wl_addr_t *)pEntry;

	if(!(addr_temp == &(ll_whiteList_tbl.wl_addr_tbl[ll_whiteList_tbl.wl_addr_tbl_index - 1]))) {
		smemcpy((u8 *)addr_temp, (u8 *)(&(ll_whiteList_tbl.wl_addr_tbl[ll_whiteList_tbl.wl_addr_tbl_index - 1])), sizeof(wl_addr_t));
	}

	ll_whiteList_tbl.wl_addr_tbl_index--;


	return BLE_SUCCESS;

}





/*********************************************************************
 * @fn      ll_whiteList_getSize
 *
 * @brief   API to get total number of white list entry size
 *
 * @param   returnSize - The returned entry size
 *
 * @return  LL Status
 */
ble_sts_t ll_whiteList_getSize(u8 *returnPublicAddrListSize) {
	//*returnPublicAddrListSize = MAX_WHITE_LIST_SIZE - ll_whiteList_tbl.wl_addr_tbl_index;

	// "the total number of white list entries that can stored in controller"
	*returnPublicAddrListSize = MAX_WHITE_LIST_SIZE;

	return BLE_SUCCESS;
}

