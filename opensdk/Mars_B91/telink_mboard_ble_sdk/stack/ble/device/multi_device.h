/********************************************************************************************************
 * @file	multi_device.h
 *
 * @brief	This is the header file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2022.02
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

#ifndef STACK_BLE_CONTROLLER_LL_LL_DEVICE_H_
#define STACK_BLE_CONTROLLER_LL_LL_DEVICE_H_


/**
 *  @brief  local device index
 *  if multiple device function not needed, please use DEFAULT_DEVICE_INDEX in any relative APIs
 */
typedef enum {
	LOCAL_DEVICE_INDEX_0	= 0x00,		DEFAULT_DEVICE_INDEX = 0x00,
	LOCAL_DEVICE_INDEX_1	= 0x01,
	LOCAL_DEVICE_INDEX_2	= 0x02,
	LOCAL_DEVICE_INDEX_3	= 0x03,
} loc_dev_idx_t;


/**
 *  @brief  multiple local device enable
 */
typedef enum {
	MULTI_DEV_DISABLE = 0x00,
	MULTI_DEV_ENABLE  = 0x01,
} muti_dev_en_t;


/**
 * @brief      for user to enable multiple local device feature. If not set, this feature is disabled by default.
 * 			   attention: this API must be called before SMP initialization if user need this feature!!!
 * @param[in]  enable - enable or disable.
 * @return     none
 */
void 		blc_ll_setMultipleLocalDeviceEnable (muti_dev_en_t enable);


/**
 * @brief      for user to bond local device index and identity address
 * @param[in]  local_dev_idx - local device index
 * @param[in]  id_adrType -  local device identity address type, 0 for public, 1 for random
 * @param[in]  id_addr - local device identity address, must be public address or static random address
 * @return     status, 0x00:  succeed
 * 					   other: failed
 */
ble_sts_t 	blc_ll_setLocalDeviceIndexAndIdentityAddress(int local_dev_idx,  u8 id_adrType, u8 *id_addr);




/**
 * @brief      for user to indicate which device to use now
 * @param[in]  local_dev_idx - local device index
 * @return     status, 0x00:  succeed
 * 					   other: failed
 */
ble_sts_t 	blc_ll_setCurrentLocalDevice_by_index(int local_dev_idx);




#endif /* STACK_BLE_CONTROLLER_LL_LL_DEVICE_H_ */
