/********************************************************************************************************
 * @file	smp_storage.c
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




#if (SIMPLE_MULTI_MAC_EN)
	_attribute_data_retention_ u8 device_mac_index=1;
	_attribute_data_retention_ u8 simple_muti_mac_en = 0;
	_attribute_data_retention_ u8 flag_smp_param_save_base = 0x8A;
	_attribute_data_retention_ u8 flag_smp_param_mask = 0x0E;  // 0000 1111
	_attribute_data_retention_ u8 flag_smp_param_valid = 0x0A;  // 0000 1010
#endif

#if(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
 _attribute_data_retention_	int SMP_PARAM_NV_ADDR_START			=		0x74000;
#elif(MCU_CORE_TYPE == MCU_CORE_9518)
 _attribute_data_retention_	int SMP_PARAM_NV_ADDR_START			=		0xFC000;
#endif

_attribute_data_retention_	static u32 smp_param_current_start_addr = 0;    //current use sector
_attribute_data_retention_	static u32 smp_param_next_start_addr = 0;  		//next use sector


_attribute_data_retention_	int		bond_device_flash_cfg_idx;  //new device info stored flash idx

#if (SIMPLE_MULTI_MAC_EN)
	void blc_smp_set_simple_multi_mac_en(u8	en)
	{
		if(en == 1)
		{
			simple_muti_mac_en = en;
			flag_smp_param_save_base = (0x88+(device_mac_index&0x07));
			flag_smp_param_mask = 0xC8;  // 0000 1111
			flag_smp_param_valid = 0x88;  // 0000 1010
		}
		else if(en == 0)
		{
			simple_muti_mac_en = en;
			flag_smp_param_save_base = 0x8A;
			flag_smp_param_mask = 0x0E;  // 0000 1111
			flag_smp_param_valid = 0x0A;  // 0000 1010
		}
	}
#endif

#define DBG_ERR_MARK(dat)
//#define DBG_ERR_MARK(dat)		do{	irq_disable();write_reg32( 0x8000, 0x12345600 | (dat) ); while(1); }while(0)



_attribute_data_retention_	_attribute_aligned_(4) bond_device_t  tbl_bondDevice;


/* attention: for MLD application, use "blc_smp_multi_device_param_getCurrentBondingDeviceNumber" */
u8		blc_smp_param_getCurrentBondingDeviceNumber(void)
{
	return  tbl_bondDevice.cur_bondNum;
}




void bls_smp_configParingSecurityInfoStorageAddr (int addr)
{
	SMP_PARAM_NV_ADDR_START = addr;
}











int tmemcmp_ff (u8 *p, int n)
{
	for (int i=0; i<n; i++)
	{
		if (p[i] != 0xff)
		{
			return 1;
		}
	}
	return 0;
}



bool smp_erase_flash_sector(u32 addr)
{
	u32 sector_head, sector_mid, sector_tail;
	int i;
	for(i=0; i<3; i++)
	{

		flash_erase_sector(addr);

		flash_read_page(addr, 		 4, (u8 *)&sector_head);
		flash_read_page(addr + 2048, 4, (u8 *)&sector_mid);
		flash_read_page(addr + 4092, 4, (u8 *)&sector_tail);

		if(sector_head == U32_MAX && sector_mid == U32_MAX && sector_tail == U32_MAX){
			break;
		}
	}

	if(i>=3){
		return 0;
	}
	else{
		return 1;
	}

}


//NOTE: return 0 represent SUCCESS !!!!
flash_op_sts_t  smp_write_flash_page(u32 addr, u32 len, u8 *buf)
{

	u8 check_buf[SMP_PARAM_NV_UNIT];

	int i;
	for(i=0; i<3; i++)
	{
#if (ZBIT_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
		/* for ZBIT flash, write SMP parameters when BRX is not working, in case writing timing too log
		   cause RX data loss.
		 */
		if(flash_type == FLASH_ETOX_ZB){
			while( blc_ll_isBrxBusy() );
		}
#endif

		flash_write_page(addr, len, buf);

		flash_read_page(addr, len, check_buf);

		if(!memcmp(buf, check_buf, len)){
			break;
		}
	}

	if(i>=3){
		return FLASH_OP_FAIL;   //Fail
	}
	else{
		return FLASH_OP_SUCCESS;   //Success
	}
}




u32 blc_smp_param_getCurStartAddr ()
{
	return smp_param_current_start_addr;
}




#define DBG_FLASH_CLEAN   0

/* attention: for MLD application, use "bls_smp_multi_device_param_Cleanflash" */
void	bls_smp_param_Cleanflash (void)
{
#if (DBG_FLASH_CLEAN)
	if (bond_device_flash_cfg_idx < 256)
#else
	if (bond_device_flash_cfg_idx < SMP_PARAM_INIT_CLEAR_MAGIN_ADDR)
#endif
	{
		return;
	}


	flash_op_sts_t flash_op_result = FLASH_OP_SUCCESS;

	u8 	temp_buf[SMP_PARAM_NV_UNIT] = {0};
//	smp_param_save_t* smp_param_save = (smp_param_save_t*) temp_buf;
	u32 dest_addr = smp_param_next_start_addr;
	for(int i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		flash_read_page (tbl_bondDevice.bond_flash_idx[i], sizeof(smp_param_save_t), temp_buf);

		#if 0  //no need do data check, we can guarantee tbl_bondDevice is correctly initialized form flash
		       //no need write flag from pending data to last used data, because we check all the data written, then
		       //    write mark "0x3C"
 				if(smp_param_save->flag == FLAG_SMP_PARAM_SAVE_OLD)
				{
					smp_param_save->flag = FLAG_SMP_PARAM_SAVE_PENDING;
					smp_write_flash_page (dest_addr, sizeof(smp_param_save_t), temp_buf);

					u8 save_ok = FLAG_SMP_PARAM_SAVE_OLD;
					smp_write_flash_page (dest_addr, 1, (u8 *)&save_ok);
				}
				else
				{
					DBG_ERR_MARK(22);
				}
		#else
 				flash_op_result = smp_write_flash_page (dest_addr, sizeof(smp_param_save_t), temp_buf);
 				if(flash_op_result == FLASH_OP_FAIL){
 					break;
 				}
		#endif

		dest_addr += SMP_PARAM_NV_UNIT;
	}




	if(flash_op_result == FLASH_OP_SUCCESS){
		u8 smp_flg = FLAG_SMP_SECTOR_USE;
		smp_write_flash_page(smp_param_next_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );

		smp_flg = FLAG_SMP_SECTOR_CLEAR;
		smp_write_flash_page(smp_param_current_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );

		smp_erase_flash_sector(smp_param_current_start_addr);

	}
	else{
		return;
	}




	//swap
	u32 temp = smp_param_next_start_addr;
	smp_param_next_start_addr = smp_param_current_start_addr;
	smp_param_current_start_addr = temp;


	bond_device_flash_cfg_idx =  -SMP_PARAM_NV_UNIT;
	for(int i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		tbl_bondDevice.bond_flash_idx[i] = smp_param_current_start_addr + i*SMP_PARAM_NV_UNIT;
		//tbl_bondDevice.bond _flag[i] will not change, so no need set
		bond_device_flash_cfg_idx += SMP_PARAM_NV_UNIT;
	}

}



void bls_smp_param_initFromFlash(void)
{
	// 1. init current start addr
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	#if (FLASH_SMP_PARAM_READ_BY_API)
		u8 sector_mark0;
		u8 sector_mark1;
		flash_read_page(SMP_PARAM_NV_ADDR_START 	+ FLASH_SECTOR_OFFSET, 1, &sector_mark0);
		flash_read_page(SMP_PARAM_NV_SEC_ADDR_START + FLASH_SECTOR_OFFSET, 1, &sector_mark1);
	#else
		/* must using "0x20000000 | address" when reading flash data by address pointer */
		u8 sector_mark0 = *(u8 *)(FLASH_R_BASE_ADDR | (SMP_PARAM_NV_ADDR_START + FLASH_SECTOR_OFFSET));
		u8 sector_mark1 = *(u8 *)(FLASH_R_BASE_ADDR | (SMP_PARAM_NV_SEC_ADDR_START + FLASH_SECTOR_OFFSET));
	#endif
#elif(MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	u8 sector_mark0 = *(u8 *)(SMP_PARAM_NV_ADDR_START + FLASH_SECTOR_OFFSET);
	u8 sector_mark1 = *(u8 *)(SMP_PARAM_NV_SEC_ADDR_START + FLASH_SECTOR_OFFSET);
#endif

	u8 invalid_sector_mark;

	if(sector_mark0 == U8_MAX && sector_mark1 == U8_MAX){  //first power on, choose sector1, write mark
		smp_param_current_start_addr = SMP_PARAM_NV_ADDR_START;  //use sector 0
		smp_param_next_start_addr = SMP_PARAM_NV_SEC_ADDR_START;




		u8 smp_flg = FLAG_SMP_SECTOR_USE;
		smp_write_flash_page(smp_param_current_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );
	}
	else{
		if( sector_mark0 == FLAG_SMP_SECTOR_USE){
			smp_param_current_start_addr = SMP_PARAM_NV_ADDR_START;  //use sector 0
			smp_param_next_start_addr = SMP_PARAM_NV_SEC_ADDR_START;

			invalid_sector_mark = sector_mark1;
		}
		else{
			smp_param_current_start_addr = SMP_PARAM_NV_SEC_ADDR_START;   //use sector 1
			smp_param_next_start_addr = SMP_PARAM_NV_ADDR_START;

			invalid_sector_mark = sector_mark0;
		}


		//erase next used sector
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		#if (FLASH_SMP_PARAM_READ_BY_API)
			u32 sector_head;
			flash_read_page(smp_param_next_start_addr, 4, (u8*)&sector_head);
		#else
			u32 sector_head = *(u32 *)(FLASH_R_BASE_ADDR | smp_param_next_start_addr); //must using "0x20000000 | address" when reading flash data by address pointer
		#endif
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		u32 sector_head = *(u32 *)(smp_param_next_start_addr);
	#endif
		if(invalid_sector_mark != U8_MAX || sector_head != U32_MAX){
			smp_erase_flash_sector(smp_param_next_start_addr);
		}

	}




	//2. load bonding device info from flash to sram(tbl_bondDevice)
	u8 mark;
	u32 current_flash_adr;
	for (bond_device_flash_cfg_idx = 0; bond_device_flash_cfg_idx < FLASH_SECTOR_OFFSET; bond_device_flash_cfg_idx += SMP_PARAM_NV_UNIT)
	{
		current_flash_adr = smp_param_current_start_addr + bond_device_flash_cfg_idx;
	#if (MCU_CORE_TYPE == MCU_CORE_9518)
		#if (FLASH_SMP_PARAM_READ_BY_API)
			flash_read_page(current_flash_adr, 1, &mark);
		#else
			mark = *(u8 *)(FLASH_R_BASE_ADDR | current_flash_adr); //must using "0x20000000 | address" when reading flash data by address pointer
		#endif
	#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
		mark = *(u8 *)current_flash_adr;
	#endif

#if (SIMPLE_MULTI_MAC_EN)
		if( (mark & flag_smp_param_mask) == flag_smp_param_valid)  //data storage OK
#else
		if( (mark & FLAG_SMP_PARAM_MASK) == FLAG_SMP_PARAM_VALID)  //data storage OK
#endif
		{
			#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
				if(mlDevMng.mldev_en)
				{
					u8	slave_dev_index;
					flash_read_page(current_flash_adr + OFFSETOF(smp_param_save_t, cflg_union), 1, &slave_dev_index);
					slave_dev_index &= DEVICE_IDX_MASK;
					if(tbl_bondDevice.mldCur_bondNum[slave_dev_index] < blc_smpMng.bonding_maxNum){
						tbl_bondDevice.mldBond_flash_idx[slave_dev_index][tbl_bondDevice.mldCur_bondNum[slave_dev_index]] = current_flash_adr;
						tbl_bondDevice.mldBond_flag[slave_dev_index][tbl_bondDevice.mldCur_bondNum[slave_dev_index]] = mark;
						tbl_bondDevice.mldCur_bondNum[slave_dev_index] ++;

						//attention: do not set "cur bondNum" here, cause do not know which device is in connection
					}
					else{ //slave mac in flash more than max, we think it's code bug
						DBG_ERR_MARK(0x11);
					}
				}
				else
			#endif
				{
					if(tbl_bondDevice.cur_bondNum < blc_smpMng.bonding_maxNum){
						tbl_bondDevice.bond_flash_idx[tbl_bondDevice.cur_bondNum] = current_flash_adr;
						tbl_bondDevice.bond_flag[tbl_bondDevice.cur_bondNum] = mark;
						tbl_bondDevice.cur_bondNum ++;
					}
					else{ //slave mac in flash more than max, we think it's code bug
						DBG_ERR_MARK(0x11);
					}
				}

		}
		else if(mark == U8_MAX){
			break;
		}
	}

	bond_device_flash_cfg_idx -= SMP_PARAM_NV_UNIT;





	//3. too many device in sector,   clean
	#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
		if(mlDevMng.mldev_en)
		{
			bls_smp_multi_device_param_Cleanflash();
		}
		else
	#endif
		{
			bls_smp_param_Cleanflash ();
		}
}







/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
u32	bls_smp_searchBondingDevice_in_Flash_by_Address(u8 addr_type, u8* addr )
{

	smp_param_save_t smp_param_save;
	int mac_match = 0;

	int i;
	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	for(i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		u32 current_addr;
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en)
			{
				current_addr = tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i];
			}
			else
		#endif
			{
				current_addr = tbl_bondDevice.bond_flash_idx[i];
			}

	#if (SIMPLE_MULTI_MAC_EN)
		if( ((simple_muti_mac_en)&&((smp_param_save.flag&0x07) == device_mac_index)) || (!simple_muti_mac_en) )
	#endif
		{
			if(IS_RESOLVABLE_PRIVATE_ADDR(addr_type, addr) ){
				u8 peerIrk[16];
				flash_read_page (current_addr + OFFSETOF(smp_param_save_t, peer_irk), 16, peerIrk);  //read addr 8 byte only

				if(smp_quickResolvPrivateAddr(peerIrk, addr)){
					mac_match = 1;
				}
			}
			else{
				flash_read_page (current_addr, 8, (u8*)&(smp_param_save));  //read addr 8 byte only
				if( smp_param_save.peer_addr_type ==  addr_type && (memcmp(addr, smp_param_save.peer_addr, 6 ) == 0) ){
					mac_match = 1;
				}
			}
		}

		if(mac_match){
			return current_addr;
		}
	}


	return 0;
}











/*
 * Used for load smp parameter base on addr type
 * */
/* attention: for MLD application, use "bls_smp_multi_device_param_loadByIndex" */
u32 bls_smp_param_loadByIndex(u8 index, smp_param_save_t* smp_param_load)
{
	smp_param_save_t smp_param_save;


	if(index < tbl_bondDevice.cur_bondNum)
	{
		u32 current_addr = tbl_bondDevice.bond_flash_idx[index];

		flash_read_page (current_addr, sizeof(smp_param_save_t), (u8*)&(smp_param_save));
		memcpy ((u8*)smp_param_load, (u8*)&(smp_param_save), sizeof(smp_param_save_t));

		return current_addr;
	}


	return 0;
}


/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
u8	bls_smp_param_getBondFlag_by_flashAddress(u32 flash_addr)
{
	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	for(int i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en)
			{
				if(flash_addr == tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i]){
					return tbl_bondDevice.mldBond_flag[smp_param_own.devc_idx][i];
				}
			}
			else
		#endif
			{
				if(flash_addr == tbl_bondDevice.bond_flash_idx[i]){
					return tbl_bondDevice.bond_flag[i];
				}
			}
	}

	return 0xFF;
}


/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
u8	bls_smp_param_getIndexByFLashAddr(u32 flash_addr)
{

	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	for(int i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en)
			{
				if(flash_addr == tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i]){
					return i;
				}
			}
			else
		#endif
			{
				if(flash_addr == tbl_bondDevice.bond_flash_idx[i]){
					return i;
				}
			}
	}

	return ADDR_NOT_BONDED;
}



/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
int	bls_smp_param_deleteByIndex(u8 index)
{

#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
	if(mlDevMng.mldev_en)
	{
		u32 current_addr = tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][index];
		if(index < tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx] && current_addr != 0)
		{
			u8 delete_mark = FLAG_SMP_PARAM_SAVE_ERASE;
			smp_write_flash_page (current_addr, 1, &delete_mark);

			for(int i=index; i<tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx] - 1; i++){ 	//move data
				tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i] = tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i+1];
			}

			tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx] --;

			/* for MLA, very important: "cur bondNum" update to mldCur bondNum[device index] new value */
			tbl_bondDevice.cur_bondNum = tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx];

			return current_addr;
		}
	}
	else
#endif
	{
		u32 current_addr = tbl_bondDevice.bond_flash_idx[index];
		if(index < tbl_bondDevice.cur_bondNum && current_addr != 0)
		{
			u8 delete_mark = FLAG_SMP_PARAM_SAVE_ERASE;
			smp_write_flash_page (current_addr, 1, &delete_mark);

			for(int i=index; i<tbl_bondDevice.cur_bondNum - 1; i++){ 	//move data
				tbl_bondDevice.bond_flash_idx[i] = tbl_bondDevice.bond_flash_idx[i+1];
			}

			tbl_bondDevice.cur_bondNum --;

			return current_addr;
		}
	}


	return 0;

}






/*
 * Used for load smp parameter base ediv and random[8]
 * */
/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
u32 bls_smp_param_loadByEdivRand(u16 ediv, u8* random, smp_param_save_t* smp_param_load)
{
	u8 rand[10];
	rand[0] = ~ediv;
	rand[1] = ~(ediv >> 8);
	int i;
	for(i=0; i<8; i++){
		rand[i+2] = ~random[i];
	}


	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	for(i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{

		u32 current_addr;
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en)
			{
				current_addr = tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i];
			}
			else
		#endif
			{
				current_addr = tbl_bondDevice.bond_flash_idx[i];
			}


		flash_read_page (current_addr, SMP_PARAM_NV_UNIT, (u8 *)smp_param_load);


		if( !memcmp(rand, smp_param_load->own_ltk, 10 ) )
		{
			return current_addr;
		}
	}

	return 0;
}







/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
int bls_smp_param_saveBondingInfo(smp_param_save_t* save_param)
{

	if(bond_device_flash_cfg_idx >= FLASH_SECTOR_OFFSET){
		return 0;
	}

	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	if(tbl_bondDevice.cur_bondNum >= blc_smpMng.bonding_maxNum ){
		//local device index is "smp param own.devc idx" for MLD application
		bls_smp_param_deleteByIndex(0);  //delete index 0 (oldest) of table,
	}



	bond_device_flash_cfg_idx += SMP_PARAM_NV_UNIT;
	u32 current_addr = smp_param_current_start_addr + bond_device_flash_cfg_idx;



#if 0
	smp_write_flash_page (current_addr, sizeof(smp_param_save_t), (u8 *)save_param);
#else



	int flash_write_check = 0;  //Success

	u8 flag_pending = FLAG_SMP_PARAM_SAVE_PENDING;
	flash_write_check += smp_write_flash_page (current_addr, 1, &flag_pending);

	// save  peer_addr_type ~ cflg_union
	flash_write_check += smp_write_flash_page (current_addr + OFFSETOF(smp_param_save_t, peer_addr_type), 8, (u8 *)&save_param->peer_addr_type);


	if(save_param->peer_id_adrType != BLE_ADDR_INVALID){
		flash_write_check += smp_write_flash_page (current_addr + OFFSETOF(smp_param_save_t, peer_id_adrType), 7, (u8 *)&save_param->peer_id_adrType);
	}


	flash_write_check += smp_write_flash_page (current_addr + OFFSETOF(smp_param_save_t, own_ltk), 16, save_param->own_ltk);



	u8 empty_16_ff[16] = {0xFF, 0xFF, 0xFF, 0xFF,  0xFF, 0xFF, 0xFF, 0xFF,  0xFF, 0xFF, 0xFF, 0xFF,  0xFF, 0xFF, 0xFF, 0xFF};
	if(memcmp(save_param->peer_irk, empty_16_ff, 16)){
		flash_write_check += smp_write_flash_page (current_addr + OFFSETOF(smp_param_save_t, peer_irk), 16, save_param->peer_irk);
	}


	if(memcmp(save_param->local_irk, empty_16_ff, 16)){
		flash_write_check += smp_write_flash_page (current_addr + OFFSETOF(smp_param_save_t, local_irk), 16, save_param->local_irk);
	}
#endif




	if(flash_write_check == FLASH_OP_SUCCESS){  //flash write OK
		u8 bondFlg = save_param->flag;
		flash_write_check = smp_write_flash_page (current_addr, 1, (u8 *)&bondFlg);

		if(flash_write_check == FLASH_OP_SUCCESS){

			#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
				if(mlDevMng.mldev_en)
				{
					tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx]] = current_addr;
					tbl_bondDevice.mldBond_flag[smp_param_own.devc_idx][tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx]] = bondFlg;
					tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx] ++;

					/* for MLA, very important: "cur bondNum" update to mldCur bondNum[device index] new value */
					tbl_bondDevice.cur_bondNum = tbl_bondDevice.mldCur_bondNum[smp_param_own.devc_idx];
				}
				else
			#endif
				{
					tbl_bondDevice.bond_flash_idx[tbl_bondDevice.cur_bondNum] = current_addr;
					tbl_bondDevice.bond_flag[tbl_bondDevice.cur_bondNum] = bondFlg;
					tbl_bondDevice.cur_bondNum ++;
				}

			return current_addr;
		}
	}


	return 0;  //Fail

}


#if (LL_FEATURE_ENABLE_LL_PRIVACY)

int	blc_smp_setPeerAddrResSupportFlg(u32 flash_addr, u8 support)
{
	//smp bonding: dft bond flg's bit7 is '1': not support peer addr resolution
	if(support){
		u8 bondingFlg = 0;
		flash_read_page(flash_addr, 1, (u8 *)&bondingFlg);

		u8 suppArMsk = ~((support ? 1 : 0) << 7); //7F  or FF
		bondingFlg &= suppArMsk; //bit7: 1: not support peer addr resolution; 0: support  peer addr resolution
		u8 res = smp_write_flash_page(flash_addr, 1, (u8 *)&bondingFlg);

		if(res == FLASH_OP_SUCCESS){
			return flash_addr;
		}
	}

	return 0;
}


#endif

/*
 * Used for load smp parameter base on addr type
 * */
/* this function only used by stack, only used in connection state, so use "smp param own.devc idx" */
u32 bls_smp_param_loadByAddr(u8 addr_type, u8* addr, smp_param_save_t* smp_param_load)
{

	smp_param_save_t smp_param_save;
	int mac_match = 0;


	/* attention: for MLD application, "cur bondNum" has already match to mldCur bondNum[device index] when connection complete */
	for(int i=0; i< tbl_bondDevice.cur_bondNum; i++)
	{
		u32 current_addr;
		#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
			if(mlDevMng.mldev_en)
			{
				current_addr = tbl_bondDevice.mldBond_flash_idx[smp_param_own.devc_idx][i];
			}
			else
		#endif
			{
				current_addr = tbl_bondDevice.bond_flash_idx[i];
			}

		flash_read_page (current_addr, sizeof(smp_param_save_t), (u8*)&(smp_param_save));

#if SIMPLE_MULTI_MAC_EN
		if( (simple_muti_mac_en && ((smp_param_save.flag&0x07) == device_mac_index)) || (!simple_muti_mac_en) )
#endif
		{
			if( IS_RESOLVABLE_PRIVATE_ADDR(addr_type, addr) ){

				if(smp_quickResolvPrivateAddr(smp_param_save.peer_irk, addr)){
					mac_match = 1;
				}
			}
			else{

				if( smp_param_save.peer_addr_type ==  addr_type && (memcmp(addr, smp_param_save.peer_addr, 6 ) == 0) ){
					mac_match = 1;
				}
			}
		}

		if(mac_match){
			memcpy ((u8*)smp_param_load, (u8*)&(smp_param_save), sizeof(smp_param_save_t));
			return current_addr;
		}
	}


	return 0;
}

/*
 * Used for load smp parameter base on resolved addr with the irk
 * */
u32 smp_param_loadByIRK (u8* addr, smp_param_save_t* smp_param_load)
{
#if 0
	smp_param_save_t smp_param_save;
	u32 start_addr = blc_smp_param_getCurStartAddr();

	for(int i_addr = start_addr ; i_addr < start_addr + SMP_PARAM_NV_MAX_LEN; i_addr += SMP_PARAM_NV_UNIT){

		flash_read_page (i_addr, sizeof(smp_param_save_t), (u8*)&(smp_param_save));

		if(tmemcmp_ff((u8*)&smp_param_save, sizeof(smp_param_save_t)) == 0){
			break;
		}
		if((smp_param_save.flag == FLAG_SMP_PARAM_SAVE) && smp_resolvPrivateAddr (smp_param_save.peer_irk, addr) == BLE_SUCCESS) {
			memcpy ((u8*)smp_param_load, (u8*)&(smp_param_save), sizeof(smp_param_save_t));
			return i_addr;
		}
	}
#endif
	return 0;
}

/*
 * Used for load smp parameter base on ediv and random
 * */
u32 smp_param_loadVsEdivRand (u16 ediv, u8* Random, smp_param_save_t* smp_param_load)
{
#if 0
	smp_param_save_t smp_param_save;
	u32 start_addr = blc_smp_param_getCurStartAddr();

	for(int i_addr = start_addr + SMP_PARAM_NV_MAX_LEN; i_addr > start_addr ; i_addr -= SMP_PARAM_NV_UNIT){

		flash_read_page (i_addr, sizeof(smp_param_save_t), (u8*)&(smp_param_save));
		if((smp_param_save.flag == FLAG_SMP_PARAM_SAVE) && (smp_param_save.peer_ediv ==  ediv) && (memcmp(Random, smp_param_save.peer_random, 8 ) == 0) ){
			memcpy ((u8*)smp_param_load, (u8*)&(smp_param_save), sizeof(smp_param_save_t));
			return i_addr;
		}
	}
#endif

	return 0;
}



u32 bls_smp_loadParamVsRand (u16 ediv, u8* random)
{

	smp_param_save_t smp_param_save;
	u32 flash_addr = 0;

	flash_addr = bls_smp_param_loadByEdivRand (ediv, random, &smp_param_save);

	if( flash_addr ){  //load success
		memcpy (smp_param_own.local_irk, smp_param_save.local_irk, 16);
		memcpy (smp_param_peer.peer_irk,  smp_param_save.peer_irk, 16);
		memcpy (smp_param_own.own_ltk,  smp_param_save.own_ltk, 16);
	}


	return flash_addr;

}




/*
 * Used for initiator load key.
 * */
int bls_smp_loadParamVsAddr (u16 addr_type, u8* addr)
{
	smp_param_save_t smp_param_save;
	u32 flash_addr = 0;

	flash_addr = bls_smp_param_loadByAddr (addr_type, addr, &smp_param_save);

	if( flash_addr ){  //load success
		memcpy (smp_param_own.local_irk, smp_param_save.local_irk, 16);
		memcpy (smp_param_peer.peer_irk,  smp_param_save.peer_irk, 16);
		memcpy (smp_param_own.own_ltk,  smp_param_save.own_ltk, 16);
	}
	return flash_addr;
}


u32 blc_smp_param_updateToNearestByIndex(u8 index)
{
	smp_param_save_t smp_param_temp;

	#if (MULTIPLE_LOCAL_DEVICE_ENABLE)
		if(mlDevMng.mldev_en)
		{
			bls_smp_multi_device_param_loadByIndex(smp_param_own.devc_idx, index, &smp_param_temp);
		}
		else
	#endif
		{
			bls_smp_param_loadByIndex(index, &smp_param_temp);
		}

	/* local device index is "smp param own.devc idx" for MLD application */
	bls_smp_param_deleteByIndex(index);


	return bls_smp_param_saveBondingInfo(&smp_param_temp);
}


void	bls_smp_setIndexUpdateMethod(index_updateMethod_t method)
{
	tbl_bondDevice.index_update_method = method;
}





void	bls_smp_eraseAllParingInformation(void)
{

	smp_erase_flash_sector(SMP_PARAM_NV_ADDR_START);
	smp_erase_flash_sector(SMP_PARAM_NV_SEC_ADDR_START);

	smp_param_current_start_addr = SMP_PARAM_NV_ADDR_START;  //use sector 0
	smp_param_next_start_addr = SMP_PARAM_NV_SEC_ADDR_START;


	u8 smp_flg = FLAG_SMP_SECTOR_USE;
	smp_write_flash_page(smp_param_current_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );

	bond_device_flash_cfg_idx = -64;
	memset(&tbl_bondDevice, 0, sizeof(tbl_bondDevice));

}







#if (MULTIPLE_LOCAL_DEVICE_ENABLE)


void	bls_smp_multi_device_param_Cleanflash(void)
{
#if (DBG_FLASH_CLEAN)
	if (bond_device_flash_cfg_idx < 256)
#else
	if (bond_device_flash_cfg_idx < SMP_PARAM_INIT_CLEAR_MAGIN_ADDR)
#endif
	{
		return;
	}


	flash_op_sts_t flash_op_result = FLASH_OP_SUCCESS;

	u8 	temp_buf[SMP_PARAM_NV_UNIT] = {0};

	u32 dest_addr = smp_param_next_start_addr;



	for(int i=0; i<LOCAL_DEVICE_NUM_MAX; i++){
		int slave_cur_bond_num = tbl_bondDevice.mldCur_bondNum[i];
		for(int j=0; j<slave_cur_bond_num; j++){
			flash_read_page(tbl_bondDevice.mldBond_flash_idx[i][j], sizeof(smp_param_save_t), temp_buf);

			flash_op_result = smp_write_flash_page (dest_addr, sizeof(smp_param_save_t), temp_buf);
			if(flash_op_result == FLASH_OP_FAIL){
				break;
			}

			tbl_bondDevice.mldBond_flash_idx[i][j] = dest_addr;

			dest_addr += SMP_PARAM_NV_UNIT;
		}
	}



	if(flash_op_result == FLASH_OP_SUCCESS){
		u8 smp_flg = FLAG_SMP_SECTOR_USE;
		smp_write_flash_page(smp_param_next_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );

		smp_flg = FLAG_SMP_SECTOR_CLEAR;
		smp_write_flash_page(smp_param_current_start_addr + FLASH_SECTOR_OFFSET, 1, (u8 *)&smp_flg );

		smp_erase_flash_sector(smp_param_current_start_addr);

	}
	else{
		return;
	}




	//swap
	u32 temp = smp_param_next_start_addr;
	smp_param_next_start_addr = smp_param_current_start_addr;
	smp_param_current_start_addr = temp;

	bond_device_flash_cfg_idx = dest_addr - SMP_PARAM_NV_UNIT;
}


int		blc_smp_multi_device_param_getCurrentBondingDeviceNumber(int local_dev_idx)
{
	if(local_dev_idx < LOCAL_DEVICE_NUM_MAX){
		return  tbl_bondDevice.mldCur_bondNum[local_dev_idx];
	}

	return 0;
}



u32 	bls_smp_multi_device_param_loadByIndex(int local_dev_idx, int bond_dev_idx, smp_param_save_t* smp_param_load)
{

	if(local_dev_idx < LOCAL_DEVICE_NUM_MAX && bond_dev_idx < tbl_bondDevice.mldCur_bondNum[local_dev_idx])
	{
		u32 current_addr = tbl_bondDevice.mldBond_flash_idx[local_dev_idx][bond_dev_idx];

		flash_read_page (current_addr, sizeof(smp_param_save_t), (u8*)&(smp_param_load));

		return current_addr;
	}


	return 0;
}

#endif
