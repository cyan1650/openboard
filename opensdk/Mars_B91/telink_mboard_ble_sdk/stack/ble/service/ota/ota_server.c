/********************************************************************************************************
 * @file	ota_server.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
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
#include "stack/ble/ble.h"

#include "ota.h"
#include "ota_stack.h"
#include "ota_server.h"




_attribute_ble_data_retention_	ota_server_t 		blotaSvr;

_attribute_ble_data_retention_	ota_startCb_t		otaStartCb = NULL;
_attribute_ble_data_retention_	ota_versionCb_t 	otaVersionCb = NULL;
_attribute_ble_data_retention_	ota_resIndicateCb_t otaResIndicateCb = NULL;


_attribute_ble_data_retention_	ota_write_fw_callback_t	ota_write_fw_cb = NULL;


/*
 * old data tested on Kite by SiHui:
 * for 16 Bytes data input, calculate in half byte
 * crc32_half_tbl on SRAM,  cost 80uS timing and 64 Bytes SRAM
 * crc32_half_tbl on FLASH, cost 136uS
 * so choose SRAM
 */
//static const unsigned long crc32_half_tbl[16] = {
_attribute_ble_data_retention_	static unsigned long crc32_half_tbl[16] = {
	0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
	0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
	0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};


_attribute_no_inline_
void blt_ota_reset(void)
{
	blotaSvr.ota_step = OTA_STEP_IDLE;
	blotaSvr.ota_busy = 0;
	blotaSvr.pdu_len = 16;
	blotaSvr.version_accept = 0;
	blotaSvr.last_adr_index = ota_firmware_max_size/16; //default: max_size div 16
	blotaSvr.flow_mask = 0;
	//blotaSvr.resume_mode = 0;
	blotaSvr.data_pending_type = 0;
	blotaSvr.cur_adr_index = -1;
	blotaSvr.flash_addr_mark = -1;
	blotaSvr.fw_check_match = 0;
	blotaSvr.process_timeout_100S_cnt = 0;
	blotaSvr.ota_start_tick = 0;
	blotaSvr.data_packet_tick = 0;
	blotaSvr.ota_connHandle = 0;
	blotaSvr.ota_write_attHandle = 0;
	blotaSvr.otaResult = OTA_SUCCESS;

	/* reset scheduler indication parameters, so if OTA result is coming, scheduler indication packet is abandoned */
	blotaSvr.schdl_pduNum_mark = 0;
	blotaSvr.schdl_pduNum_rpt = 0;
}



/**
 * @brief      this function is used for user to initialize OTA server module.
 * @param	   none
 * @return     none
 */
_attribute_no_inline_
void blc_ota_initOtaServer_module(void)
{

	blotaSvr.otaInit = 1;

	host_ota_main_loop_cb = blt_ota_server_main_loop;
	host_ota_terminate_cb = blt_ota_server_terminate;


	#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
		if(flash_type & FLASH_SONOS_ARCH){
			extern void ota_sonos_flash_init(void);
			ota_sonos_flash_init();
		}
	#endif


	//global status, only reset in user initialization stage
	blotaSvr.fw_check_en = 1; //default firmware check is must
	blotaSvr.fw_crc_default = 0xFFFFFFFF;
	blotaSvr.process_timeout_100S_num = 0;
	blotaSvr.process_timeout_us = 30 * 1000000;   //default 30 S
	blotaSvr.packet_timeout_us = 5 * 1000000;     //default 5 S


	//OTA flow status, should reset for each new OTA flow
	blt_ota_reset();

	if(!blotaSvr.newFwArea_clear){
		bls_ota_clearNewFwDataArea(); //must
	}

	my_dump_str_data(DBG_OTA_FLOW, "OTA server init", 0, 0);
}



/* hidden API, if user do not use firmware check, call  this API to disable it. */
void blc_ota_setOtaFirmwareCheckEnable(int en)
{
	blotaSvr.fw_check_en = en;
}

/* hidden API, if user do not want share same firmware check algorithm with all other customers,
 * he can call this API change crc32 init_value, pay attention that TestBench need also corresponding process. */
void blc_ota_setFirmwareCheckCrcInitValue(u32 crc_init_value){
	blotaSvr.fw_crc_default = crc_init_value; //attention: not set fw_crc_init !!!
}



/* this function must be called before "sys_init" or "cpu_wakeup_init".*/
ble_sts_t blc_ota_setFirmwareSizeAndBootAddress(int firmware_size_k, multi_boot_addr_e boot_addr)
{
	int param_valid = 0;

	if( (firmware_size_k & 3) == 0)
	{
		if( boot_addr == MULTI_BOOT_ADDR_0x20000 && firmware_size_k <= 124){
			param_valid = 1;
		}
		else if(boot_addr == MULTI_BOOT_ADDR_0x40000 && firmware_size_k <= 252){
			param_valid = 1;
		}
		/*not use MULTI_BOOT_ADDR_0x80000 here, in case some MCU do not support 0x80000 boot */
		else if(boot_addr == 0x80000 && firmware_size_k <= 508){
			param_valid = 1;
		}
	}


	if(param_valid){
		ota_firmware_max_size = firmware_size_k<<10; //*1024
		ota_program_bootAddr  = boot_addr;
		return BLE_SUCCESS;
	}
	else{
		return SERVICE_ERR_INVALID_PARAMETER;
	}
}






void blc_ota_setFirmwareVersionNumber(u16 version_number)
{
	blotaSvr.local_version_num = version_number;
}



void blc_ota_registerOtaStartCmdCb(ota_startCb_t cb)
{
	otaStartCb = cb;
}

void blc_ota_registerOtaFirmwareVersionReqCb(ota_versionCb_t cb)
{
	otaVersionCb = cb;
}

void blc_ota_registerOtaResultIndicationCb(ota_resIndicateCb_t cb)
{
	otaResIndicateCb = cb;
}

_attribute_no_inline_
ble_sts_t blc_ota_setOtaProcessTimeout(int timeout_second)
{
	if(timeout_second > 4 && timeout_second < 1001){
		blotaSvr.process_timeout_100S_num = timeout_second/100;
		blotaSvr.process_timeout_us  = (timeout_second%100)*1000000;

		return BLE_SUCCESS;
	}
	else{
		return SERVICE_ERR_INVALID_PARAMETER;
	}
}

_attribute_no_inline_
ble_sts_t blc_ota_setOtaDataPacketTimeout(int timeout_second)
{
	if(timeout_second > 0 && timeout_second < 21){
		blotaSvr.packet_timeout_us = timeout_second * 1000000;
		return BLE_SUCCESS;
	}
	else{
		return SERVICE_ERR_INVALID_PARAMETER;
	}
}


/**
 * @brief      This function is used to set resolution of OTA schedule indication by PDU number
 * @param[in]  pdu_num -
 * @return     Status - 0x00: command succeeded; 0x01-0xFF: command failed
 */
_attribute_no_inline_
ble_sts_t blc_ota_setOtaScheduleIndication_by_pduNum(int pdu_num)
{
	/* can only select one from PDU number & FW size */
	if(blotaSvr.schedule_fw_size){
		return SERVICE_ERR_INVALID_PARAMETER;
	}
	else{
		blotaSvr.schedule_pdu_num = pdu_num;
		return BLE_SUCCESS;
	}
}



//Hidden API, release later
/**
 * @brief      This function is used to set resolution of OTA schedule indication by firmware size
 * @param[in]  fw_size -
 * @return     Status - 0x00: command succeeded; 0x01-0xFF: command failed
 */
_attribute_no_inline_
ble_sts_t blc_ota_setOtaScheduleIndication_by_FiimwareSize(int fw_size)
{
	/* can only select one from PDU number & FW size */
	if(blotaSvr.schedule_pdu_num){
		return SERVICE_ERR_INVALID_PARAMETER;
	}
	else{
		blotaSvr.schedule_fw_size = fw_size;
		return BLE_SUCCESS;
	}
}



void	  blc_ota_setAttHandleOffset(s8 attHandle_offset)
{
	blotaSvr.handle_offset = attHandle_offset;
}




void blt_ota_writeBootMark(void)
{
	u32 flag = BOOT_MARK_VALUE;
	flash_write_page(ota_program_offset + BOOT_MARK_ADDR, 1, (u8 *)&flag);		//Set FW flag
	flag = 0;
	flash_write_page((ota_program_offset ? 0 : ota_program_bootAddr) + BOOT_MARK_ADDR, 1, (u8 *)&flag);	//Invalid flag
}




_attribute_no_inline_
int ota_save_data(u32 flash_addr, int len, u8 * data)
{
	if(flash_addr <= BOOT_MARK_ADDR && (flash_addr + len) > BOOT_MARK_ADDR){
		u8 offset = BOOT_MARK_ADDR - flash_addr;
		u8 vendor_fw_mark[4] = {0x4B, 0x4E, 0x4C, 0x54};

		if(memcmp(data + offset, vendor_fw_mark, 4)){  //do not equal
			return OTA_FIRMWARE_MARK_ERR;
		}

		data[offset] = 0xFF;
	}

	u32 real_flash_addr = ota_program_offset + flash_addr;

	//attention: Kite/Vulture now do not support across page write
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	flash_write_page(real_flash_addr, len, data);

	u8 flash_check[240];  //biggest value 240
	flash_read_page(real_flash_addr, len, flash_check);

	if(memcmp(flash_check, data, len)){  //do not equal
		return OTA_WRITE_FLASH_ERR;
	}
#else
	for(int i=0; i<len; i+=16){
		flash_write_page(real_flash_addr + i, 16, data + i);  //16B 16M:162 uS;  32M: 133uS

		u8 flash_check[16];  //biggest value 240
		flash_read_page(real_flash_addr + i, 16, flash_check);

		if(memcmp(flash_check, data + i, 16)){  //do not equal
			return OTA_WRITE_FLASH_ERR;
		}
	}
#endif


	return OTA_SUCCESS;
}








#if (BLE_MULTIPLE_CONNECTION_ENABLE || MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_no_inline_ int otaWrite(u16 connHandle, void * p)
#else
_attribute_no_inline_ int otaWrite(void * p)
#endif
{
	if(!blotaSvr.otaInit){
		return 1;
	}

	blotaSvr.ota_busy = 1;  //any OTA command coming triggers OTA busy, only clear it in OTA_STEP_FINISH

	/* previous OTA result process not finished, can not process another OTA flow */ 	//TODO


	/* OTA error has happened, do not process anymore */
	if(blotaSvr.otaResult || blotaSvr.ota_step == OTA_STEP_FINISH){
		return 1;
	}

	
	#if (BLE_MULTIPLE_CONNECTION_ENABLE)
		if(blotaSvr.ota_connHandle && connHandle != blotaSvr.ota_connHandle){
			return 1;
		}
		else if(!blotaSvr.ota_connHandle){
			blotaSvr.ota_connHandle = connHandle;
		}
	#else
		blotaSvr.ota_connHandle = BIT(6); //BLS_CONN_HANDLE;
	#endif


	rf_packet_att_data_t *pAttDat = (rf_packet_att_data_t*)p;
	blotaSvr.ota_cmd_adr =  pAttDat->dat[0] | (pAttDat->dat[1]<<8);


	if(!blotaSvr.ota_write_attHandle){
		blotaSvr.ota_write_attHandle = pAttDat->handle;  //record att_handle
		blotaSvr.ota_notify_attHandle = blotaSvr.ota_write_attHandle + blotaSvr.handle_offset;
	}

	int err_flag = OTA_SUCCESS;
	/* 1. OTA command */
	if(blotaSvr.ota_cmd_adr >= CMD_OTA_VERSION)
	{
		/* 1.1 old version command, no not change it, maybe some customers used this */
		if(blotaSvr.ota_cmd_adr == CMD_OTA_VERSION)
		{
			if(otaVersionCb){
				otaVersionCb();
			}
		}
		/* 1.2 version_req_ext.  if version compare enable, record compare result*/
		else if(blotaSvr.ota_cmd_adr == CMD_OTA_FW_VERSION_REQ)
		{
			/* If version compare fail, but peer device do not send OTA start command, OTA flow do not give any OTA result, no need disconnect */
			if(blotaSvr.flow_mask){ //previous OTA flow not finish
				err_flag = OTA_FLOW_ERR;
			}
			else{
				ota_versionReq_t *pVerReq = (ota_versionReq_t *)pAttDat->dat;

				blotaSvr.version_accept = 1;
				if(pVerReq->version_compare && pVerReq->version_num <= blotaSvr.local_version_num){
					blotaSvr.version_accept = 0;
				}

				if(blt_ota_pushVersionRsp() != BLE_SUCCESS){
					blotaSvr.data_pending_type = DATA_PENDING_VERSION_RSP;
				}
			}
		}
		/* 1.3 old OTA start command, trigger short PDU(16Byte) only */
		else if(blotaSvr.ota_cmd_adr == CMD_OTA_START || blotaSvr.ota_cmd_adr == CMD_OTA_START_EXT)
		{

			my_dump_str_u32s(DBG_OTA_FLOW, "OTA start", connHandle, pAttDat->handle, 0, 0);

			/* previous OTA flow not finish */
			if(blotaSvr.flow_mask){
				err_flag = OTA_FLOW_ERR;
			}
			else if(blotaSvr.ota_cmd_adr == CMD_OTA_START_EXT){
				ota_startExt_t *pStartExt = (ota_startExt_t *)pAttDat->dat;
				if( pStartExt->version_compare && !blotaSvr.version_accept ){
					err_flag = OTA_VERSION_COMPARE_ERR;
				}
				else if( (pStartExt->pdu_length & 0x0F) != 0 || pStartExt->pdu_length == 0){//not 16*n, or 0
					err_flag = OTA_PDU_LEN_ERR;
				}
				#if (MCU_CORE_TYPE == MCU_CORE_825x)
				    /* kite hardware CRC24 bug, using software CRC24 cost some time which will be longer when DLE is bigger
				     * OTA flash write 16B disable IRQ. these things combine together, will cause RX IRQ drop, then MIC fail
				     * use max 80 bytes after evaluation and test
				     * TODO: not very essential, it can be optimized to support large MTU when DLE is not in effect */
					else if(pStartExt->pdu_length > 80){
						err_flag = OTA_MCU_NOT_SUPPORTED;
					}
			    #endif
				else{
					blotaSvr.pdu_len = pStartExt->pdu_length;
				}

				my_dump_str_data(DBG_OTA_FLOW, "start ext", &blotaSvr.pdu_len, 1);
			}

			if(!err_flag){
				/* OTA status clear and re_set */
				blotaSvr.flow_mask |= OTA_FLOW_START;
				blotaSvr.ota_start_tick = clock_time() | 1;  //mark time
				blotaSvr.process_timeout_100S_cnt = blotaSvr.process_timeout_100S_num;
				blotaSvr.cur_adr_index = -1;
				blotaSvr.fw_check_match = 0;

				if(blotaSvr.schedule_pdu_num){
					blotaSvr.schdl_pduNum_mark = blotaSvr.schedule_pdu_num; //set first mark value
				}

				#if ZBIT_FLASH_WRITE_TIME_LONG_WORKAROUND_EN
					if(flash_type == FLASH_ETOX_ZB)
					{
						#if (MCU_CORE_TYPE == MCU_CORE_827x)
							analog_write(0x09, ((analog_read(0x09) & 0x8f) | (FLASH_VOLTAGE_1V95 << 4)));    		//ldo mode flash ldo trim 1.95V
							analog_write(0x0c, ((analog_read(0x0c) & 0xf8) | FLASH_VOLTAGE_1V9));					//dcdc mode flash ldo trim 1.90V
						#elif(MCU_CORE_TYPE == MCU_CORE_825x)
							analog_write(0x0c, (analog_read(0x0c) | FLASH_VOLTAGE_1V95));
						#endif
					}
				#endif

				#if (SONOS_FLASH_WRITE_TIME_LONG_WORKAROUND_EN)
					blotaSvr.hold_data_len = 0;
					blotaSvr.cur_flash_addr = 0;
				#endif

				/* OTA start callback triggers only no err_flag*/
				if(otaStartCb){
					otaStartCb();
				}
			}

		}
		/* 1.4 OTA END*/
		else if(blotaSvr.ota_cmd_adr == CMD_OTA_END)
		{
			/* 1.4.1 no OTA start & OTA valid data before OTA end */
			if( (blotaSvr.flow_mask & (OTA_FLOW_START | OTA_FLOW_VALID_DATA)) == 0 )
			{
				err_flag = OTA_FLOW_ERR;
			}
			else{
				ota_end_t *pEnd = (ota_end_t *)pAttDat->dat;

				/* if no index_max check, set OTA success directly, otherwise we check if any index_max match */
				if( (pEnd->adr_index_max ^ pEnd->adr_index_max_xor) == 0xFFFF){  //index_max valid, we can check
					if(pEnd->adr_index_max != blotaSvr.cur_adr_index){  //last one or more packets missed
						err_flag = OTA_DATA_UNCOMPLETE;
					}
				}

				#if (BLE_OTA_FW_CHECK_EN)
					if(blotaSvr.fw_check_en && !blotaSvr.fw_check_match){
						err_flag = OTA_FW_CHECK_ERR;
					}
				#endif
			}

			blt_ota_setResult(OTA_STEP_FEEDBACK, err_flag); //set result no matter OTA success or fail

			return 0; //must return here, or "blt_ota_setResult" will execute again at the end of this function
		}
        /* invalid OTA command */
		else
		{
			err_flag = OTA_PACKET_INVALID;
		}
	}
	/* 2. OTA valid data */
	else if(blotaSvr.ota_cmd_adr <= blotaSvr.last_adr_index)
	{
		//blotaSvr.flow_mask |= OTA_FLOW_DATA_COME;
		int ota_actual_pdu_len;
		if(blotaSvr.ota_cmd_adr < blotaSvr.last_adr_index){
			ota_actual_pdu_len = blotaSvr.pdu_len;
		}
		else{
			ota_actual_pdu_len = blotaSvr.last_actual_pdu_len;
		}

		/* 2.1 no OTA start */
		if( !(blotaSvr.flow_mask & OTA_FLOW_START) )
		{
			err_flag = OTA_FLOW_ERR;
		}
		/* 2.2 no FW size, FW size on 0x00018, so choose adr_index = 2 to check is OK */
		else if(blotaSvr.ota_cmd_adr == 2 && !(blotaSvr.flow_mask & OTA_FLOW_GET_SIZE))
		{
			err_flag = OTA_FW_SIZE_ERR;
		}
		/* 2.3 OTA PDU not match with define in OTA_SATRT_EXT or last PDU not correct */
		else if(pAttDat->l2cap != (ota_actual_pdu_len + 4 + 3)) //4: adr_index(2) + CRC(2); 3: opcode(1) + attHandle(2)
		{
			err_flag = OTA_PDU_LEN_ERR;
		}
		/* 2.4 adr_index err, repeated OTA PDU or lost some OTA PDU */
		else if(blotaSvr.ota_cmd_adr != blotaSvr.cur_adr_index + 1)
		{
			err_flag = OTA_DATA_PACKET_SEQ_ERR;
		}
		else
		{
			//DBG_C HN9_HIGH;
			/* 16M clock, function in RamCode, OTA PDU max 240 Byte, 1400 uS
			 * TODO: use lookup table method to save time, just like crc32 */
			u16 crc16_cal = crc16(pAttDat->dat, ota_actual_pdu_len + 2);
			//DBG_C HN9_LOW;
			u16 crc16_rcv = (pAttDat->dat[ota_actual_pdu_len + 3]<<8) | pAttDat->dat[ota_actual_pdu_len + 2];

			#if 0  //debug
				if(blotaSvr.ota_cmd_adr == blotaSvr.last_adr_index){
					my_dump_str_u32s(DBG_OTA_FLOW, "CRC16 last", crc16_cal, crc16_rcv, 0, 0);
				}
			#endif

			/* 2.5 CRC16 error */
			if(crc16_cal != crc16_rcv)
			{
				err_flag = OTA_DATA_CRC_ERR;
			}
			else
			{
					/***************************************** OTA Data Process  *************************************************/
					int flash_addr = blotaSvr.ota_cmd_adr*blotaSvr.pdu_len;
					u8	*pFwDat = pAttDat->dat + 2;

					if(flash_addr <= FW_SIZE_ADDR && (flash_addr + blotaSvr.pdu_len) > FW_SIZE_ADDR){  //firmware_size packet
						u8 offset = FW_SIZE_ADDR - flash_addr;
						u32 fw_size = pFwDat[offset] | pFwDat[offset+1] <<8 | pFwDat[offset+2]<<16 | pFwDat[offset+3]<<24;

						if(fw_size < FW_MIN_SIZE || fw_size > ota_firmware_max_size){
							err_flag = OTA_FW_SIZE_ERR;
						}
						else if(blotaSvr.fw_check_en && (fw_size & 0x0f) != 4 ){  //firmware check: size must be 16*n + 4
							err_flag = OTA_FW_SIZE_ERR;
						}
						else{
							//blotaSvr.firmware_size = fw_size
							blotaSvr.last_adr_index = (fw_size - 1)/blotaSvr.pdu_len;  //-1 is impotant
							blotaSvr.last_valid_pdu_len = fw_size % blotaSvr.pdu_len;
							int align16_makeup_len = 16 - (blotaSvr.last_valid_pdu_len & 15); //only make up to make sure 16B aligned(compatible with old protocol)
							blotaSvr.last_actual_pdu_len = blotaSvr.last_valid_pdu_len + align16_makeup_len;
							blotaSvr.last_pdu_crc_offset = (blotaSvr.last_valid_pdu_len & 0xF0);
							blotaSvr.flow_mask |= OTA_FLOW_GET_SIZE;

							my_dump_str_u32s(DBG_OTA_FLOW, "FW size", fw_size, blotaSvr.last_valid_pdu_len, blotaSvr.last_actual_pdu_len, blotaSvr.last_pdu_crc_offset);
						}
					}



					if(err_flag == OTA_SUCCESS)
					{

						#if (BLE_OTA_FW_CHECK_EN)
							if(blotaSvr.fw_check_en)
							{
								if(blotaSvr.ota_cmd_adr == 0){  //first PDU
									blotaSvr.fw_crc_init = blotaSvr.fw_crc_default;  //CRC_init recover
								}

								u32 fw_check_value = 0;
								int ota_fw_crc_len = blotaSvr.pdu_len;
								if(blotaSvr.ota_cmd_adr == blotaSvr.last_adr_index){ //last adr_index
									ota_fw_crc_len = blotaSvr.last_pdu_crc_offset;  //maybe 0x00/0x10/0x20 ..
									fw_check_value = pFwDat[blotaSvr.last_pdu_crc_offset] | pFwDat[blotaSvr.last_pdu_crc_offset+1] <<8 | pFwDat[blotaSvr.last_pdu_crc_offset+2]<<16 | pFwDat[blotaSvr.last_pdu_crc_offset+3]<<24;

									my_dump_str_u32s(DBG_OTA_FLOW, "FW last", ota_actual_pdu_len, ota_fw_crc_len, fw_check_value, 0);
								}

								if(ota_fw_crc_len){
									#if 0  //do not need now
										blotaSvr.fw_crc_init = crc32_cal(blotaSvr.fw_crc_init, ota_dat, crc32_tbl, ota_fw_crc_len);
									#else
										u8 ota_dat[240*2];  //maximum 240 Bytes
										for(int i=0; i<ota_fw_crc_len; i++){
											ota_dat[i*2]   = pFwDat[i] & 0x0f;
											ota_dat[i*2+1] = pFwDat[i]>>4;
										}
										//my_dump_str_data(DBG_OTA_FLOW, "FW CRC", &blotaSvr.fw_crc_init, 4);
										//DBG_C HN10_HIGH;
										/* 16M clock, OTA PDU maximum length 240 Byte, 390 uS*/
										blotaSvr.fw_crc_init = crc32_half_cal(blotaSvr.fw_crc_init, ota_dat, (unsigned long* )crc32_half_tbl, ota_fw_crc_len*2);
										//DBG_C HN10_LOW;
									#endif
								}


								if(blotaSvr.ota_cmd_adr == blotaSvr.last_adr_index){
									if(fw_check_value == blotaSvr.fw_crc_init){  //CRC32 match
										blotaSvr.fw_check_match = 1;
									}
									else{
										err_flag = OTA_FW_CHECK_ERR;
									}

									my_dump_str_u32s(DBG_OTA_FLOW, "FW check", blotaSvr.ota_cmd_adr, fw_check_value, blotaSvr.fw_crc_init, blotaSvr.fw_check_match);
								}
							}
						#endif


						if(err_flag == OTA_SUCCESS){
						  //blotaSvr.flash_addr_mark = flash_addr;
							blotaSvr.flash_addr_mark = flash_addr + ota_actual_pdu_len;  //important for "+ pdu_len"
							if(ota_write_fw_cb){
								err_flag = ota_write_fw_cb (flash_addr, ota_actual_pdu_len, pAttDat->dat + 2);  //ota_sonos_flash_write_fw
							}
							else{
								err_flag = ota_save_data (flash_addr, ota_actual_pdu_len, pAttDat->dat + 2);
							}
						}
					}
					/*********************************************************************************************************************/
			}



			if(err_flag == OTA_SUCCESS){ //update current adr_index only when no error happen
				blotaSvr.cur_adr_index = blotaSvr.ota_cmd_adr;
				blotaSvr.flow_mask |= OTA_FLOW_VALID_DATA;
				blotaSvr.data_packet_tick = clock_time() | 1;

				/* OTA schedule indication */
				if(blotaSvr.schedule_pdu_num){
					u16 cur_pdu_num = blotaSvr.ota_cmd_adr + 1;
					if(blotaSvr.schdl_pduNum_mark == cur_pdu_num){
						/* report PDU number with handleValueNotify, if old indication packet is not send OK, replace it
						 * with new indication, so for master, it may not see continuous indication due to RF block */
						blotaSvr.schdl_pduNum_rpt = cur_pdu_num;
						blotaSvr.schdl_pduNum_mark += blotaSvr.schedule_pdu_num; //update new mark
					}
				}
			}
		}
	}
	/* 3. invalid packet */
	else
	{
		err_flag = OTA_PACKET_INVALID;
	}



	if(err_flag){
		blt_ota_setResult(OTA_STEP_FEEDBACK, err_flag);
	}


	return 0;  //attention: can not return 1
}





_attribute_no_inline_
void blt_ota_setResult(int next_step, int result)
{
	blotaSvr.otaResult = result;
	blotaSvr.ota_step = next_step;
	blotaSvr.ota_start_tick = 0; //disable OTA timeout trigger
	blotaSvr.process_timeout_100S_cnt = 0;
	blotaSvr.data_packet_tick = 0;

	if(next_step == OTA_STEP_FEEDBACK){
		blotaSvr.feedback_begin_tick = clock_time();
		blotaSvr.data_pending_type = DATA_PENDING_OTA_RESULT;  //if version_rsp pending, overwrite it
	}
	else if(next_step == OTA_STEP_FINISH){ //just feedback step
		blotaSvr.feedback_begin_tick = 0;
		blotaSvr.data_pending_type = 0;
	}

	my_dump_str_u32s(DBG_OTA_FLOW, "OTA result", result, next_step, 0, 0);
}




_attribute_no_inline_
void blt_ota_procTimeout(void)
{
	if(blotaSvr.process_timeout_100S_cnt && clock_time_exceed(blotaSvr.ota_start_tick , 100*1000*1000)){
		blotaSvr.process_timeout_100S_cnt--;
		blotaSvr.ota_start_tick = clock_time() | 1;
	}
	else if(!blotaSvr.process_timeout_100S_cnt){
		if(blotaSvr.ota_start_tick && clock_time_exceed(blotaSvr.ota_start_tick , blotaSvr.process_timeout_us)){  //OTA timeout
			blt_ota_setResult(OTA_STEP_FEEDBACK, OTA_TIMEOUT);
		}
	}

	/* data packet interval timeout */
	if(blotaSvr.data_packet_tick && clock_time_exceed(blotaSvr.data_packet_tick, blotaSvr.packet_timeout_us)){
		blt_ota_setResult(OTA_STEP_FEEDBACK, OTA_DATA_PACKET_TIMEOUT);
	}

	/* OTA schedule indication */
	//TODO
}


_attribute_no_inline_
ble_sts_t blt_ota_pushVersionRsp(void)
{
	u8 temp_buffer[sizeof(ota_versionRsp_t)];
	ota_versionRsp_t *pVerRsp = (ota_versionRsp_t *)temp_buffer;
	pVerRsp->ota_cmd = CMD_OTA_FW_VERSION_RSP;
	pVerRsp->version_num = blotaSvr.local_version_num;
	pVerRsp->version_accept = blotaSvr.version_accept;


	u8 status = blc_gatt_pushHandleValueNotify (blotaSvr.ota_connHandle, blotaSvr.ota_notify_attHandle, temp_buffer, sizeof(ota_versionRsp_t));
	if(status == BLE_SUCCESS){
		blotaSvr.data_pending_type = 0;
	}

	return status;
}



/**
 *  @brief OTA service flow connection terminate callback, should be registered in GAP layer
 */
_attribute_no_inline_
int blt_ota_server_terminate(u16 connHandle)
{
	/* can auto handle zero value */
	if(connHandle == blotaSvr.ota_connHandle){
		if(blotaSvr.ota_step == OTA_STEP_FEEDBACK){
			blotaSvr.ota_step = OTA_STEP_FINISH;
			blotaSvr.data_pending_type = 0;
		}
		else if(blotaSvr.ota_step != OTA_STEP_IDLE){
			/* jump feedback step, cause can not send any notify data when connection terminate */
			blt_ota_setResult(OTA_STEP_FINISH, OTA_FAIL_DUE_TO_CONNECTION_TERMIANTE);
		}
	}

	return 0;
}


_attribute_no_inline_
void blt_ota_procOtaResultFeedback(void)
{
	if(clock_time_exceed(blotaSvr.feedback_begin_tick, 4000000)){ //4S
		//time cost too long, do not consider sending feedback to peer device, enter OTA indicate and OTA result immediately
		blotaSvr.ota_step = OTA_STEP_FINISH;
		blotaSvr.data_pending_type = 0;
	}
	else{

		if(blotaSvr.data_pending_type == DATA_PENDING_OTA_RESULT){

			/* send OTA result on notify data
			 * if connection disconnect, can not notify any data to peer device */
			u8 temp_buffer[sizeof(ota_result_t)];
			ota_result_t *pResult = (ota_result_t *)temp_buffer;
			pResult->ota_cmd = CMD_OTA_RESULT;
			pResult->result = blotaSvr.otaResult;
			pResult->rsvd = 0;

			u8 status = blc_gatt_pushHandleValueNotify (blotaSvr.ota_connHandle, blotaSvr.ota_notify_attHandle, temp_buffer, sizeof(ota_result_t));
			if(status == BLE_SUCCESS){
				blotaSvr.data_pending_type = DATA_PENDING_TERMINATE_CMD;
			}
		}

		if(blotaSvr.data_pending_type == DATA_PENDING_TERMINATE_CMD){
		
			#if (BLE_MULTIPLE_CONNECTION_ENABLE)
				u8 status = blc_ll_disconnect(blotaSvr.ota_connHandle, HCI_ERR_REMOTE_USER_TERM_CONN);
			#else
				u8 status = bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			#endif
			
			if(status == BLE_SUCCESS || status == HCI_ERR_UNKNOWN_CONN_ID){
				blotaSvr.data_pending_type = 0;
			}
		}

	}

}

_attribute_no_inline_
void blt_ota_procOtaFinish(void)
{
	if(otaResIndicateCb){
		otaResIndicateCb(blotaSvr.otaResult);   //OTA result(Success/Fail) indicate callback
	}


	if(blotaSvr.otaResult){  //OTA Fail
		/* need reboot for those SDK which can not process erasing Flash while BLE still works,
		 * For Eagle PUYA flash,  no need reboot, just tell peer device OTA fail and disconnect */
		if(blotaSvr.flash_addr_mark >= 0)
		{
			#if (MCU_CORE_TYPE == MCU_CORE_9518)
				/* erase from end to head */
				for(int adr = blotaSvr.flash_addr_mark; adr >= 0; adr -= 4096) {
					flash_erase_sector(ota_program_offset + adr);
				}
			#else
				irq_disable();
				/* erase from end to head */
				for(int adr = blotaSvr.flash_addr_mark; adr >= 0; adr -= 4096) {
					flash_erase_sector(ota_program_offset + adr);
				}
				start_reboot();
			#endif
		}

		blt_ota_reset();  //must reset OTA flow status for next OTA
	}
	else{ //OTA Success, must reboot
		/* attention: can not erase any data before new firmware start to work !!! */
		blt_ota_writeBootMark();
		start_reboot();
	}
}


_attribute_no_inline_
ble_sts_t blt_ota_pushSchedulerIndication(void)
{
	u8 temp_buffer[sizeof(ota_sche_pdu_num_t)];
	ota_sche_pdu_num_t *pScheIndicate = (ota_sche_pdu_num_t *)temp_buffer;
	pScheIndicate->ota_cmd = CMD_OTA_SCHEDULE_PDU_NUM;
	pScheIndicate->success_pdu_cnt = blotaSvr.schdl_pduNum_rpt;

	u8 status = blc_gatt_pushHandleValueNotify (blotaSvr.ota_connHandle, blotaSvr.ota_notify_attHandle, temp_buffer, sizeof(ota_sche_pdu_num_t));
	if(status == BLE_SUCCESS){
		blotaSvr.schdl_pduNum_rpt = 0; //clear when pushTxFifo OK
	}

	return status;
}



_attribute_no_inline_
int blt_proc_ota_server(void)
{
	if(blotaSvr.data_pending_type == DATA_PENDING_VERSION_RSP){
		blt_ota_pushVersionRsp();
	}

	if(blotaSvr.ota_step == OTA_STEP_FEEDBACK){
		blt_ota_procOtaResultFeedback();
	}

	if(blotaSvr.ota_step == OTA_STEP_FINISH){
		blt_ota_procOtaFinish();
	}

	if(blotaSvr.ota_start_tick || blotaSvr.data_packet_tick){
		blt_ota_procTimeout();
	}


	/* scheduler indication notify process
	 * when OTA feedback or OTA result is coming, scheduler indication packet is abandoned */
	if(blotaSvr.schdl_pduNum_rpt && blotaSvr.ota_step != OTA_STEP_FEEDBACK && blotaSvr.ota_step != OTA_STEP_FINISH){
		blt_ota_pushSchedulerIndication();
	}

	return 0;
}




/**
 *  @brief OTA flow main_loop callback, should be registered in GAP layer
 */
_attribute_ram_code_  //called in gap_main_loop, need execute ASAP
int blt_ota_server_main_loop(void)
{
	if(blotaSvr.ota_busy){
		blt_proc_ota_server();
	}

	return 0;
}


bool blt_ota_isOtaBusy(void)
{
	return blotaSvr.ota_busy;
}

















void bls_ota_clearNewFwDataArea(void)
{
	 //in case customer not call in old version
	if(!blotaSvr.otaInit){
		blc_ota_initOtaServer_module();
	}



	blotaSvr.newFwArea_clear = 1;

	u32 tmp1 = 0;
	u32 tmp2 = 0;
	int cur_flash_setor;

	for(int i = 0; i < (ota_firmware_max_size>>12); ++i)  // "size>>12" equal to "size/4K"
	{
		cur_flash_setor = ota_program_offset + i*0x1000;
		flash_read_page(cur_flash_setor, 		4, (u8 *)&tmp1);
		flash_read_page(cur_flash_setor + 2048, 4, (u8 *)&tmp2);

		if(tmp1 != ONES_32 || tmp2 != ONES_32)
		{
			flash_erase_sector(cur_flash_setor);
		}
	}

}


void blt_ota_registerOtaWriteFwCallback (ota_write_fw_callback_t cb)
{
	ota_write_fw_cb = cb;
}

