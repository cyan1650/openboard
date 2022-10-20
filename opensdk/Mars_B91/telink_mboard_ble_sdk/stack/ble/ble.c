/********************************************************************************************************
 * @file	ble.c
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
#include "ble_common.h"
#include "ble.h"
#include "tl_common.h"
#include "drivers.h"


_attribute_session_(".ram_code")
__attribute__((noinline)) void  smemset(void * dest, int val, int len)
{
	register unsigned char *ptr = (unsigned char*) dest;
	while (len-- > 0)
		*ptr++ = (unsigned char)val;
}


_attribute_session_(".ram_code")
__attribute__((noinline)) void smemcpy(void *pd, void *ps, int len)
{
	u8 *pi = (u8*) ps;
	u8 *po = (u8*) pd;
	while (len--)
	{
		*po++ = *pi++;
	}

}


_attribute_session_(".ram_code")
__attribute__((noinline)) int smemcmp(void * m1, void * m2, int len)
{
	unsigned char *st1 = (unsigned char *) m1;
	unsigned char *st2 = (unsigned char *) m2;

	while(len--){
		if(*st1 != *st2){
			return 1; //return (*st1 - *st2)
		}
		st1++;
		st2++;
	}
	return 0;
}

unsigned char blc_get_sdk_version(unsigned char *pbuf,unsigned char number)
{
	if(number < 5 || number > 16)	//number invalid
	{
		return 1;	//fail
	}

	//Totally
	u8	version[] = {CERTIFICATION_MARK, SOFT_STRUCTURE, MAJOR_VERSION, MINOR_VERSION, PATCH,
					 RSVD, RSVD, RSVD, RSVD, RSVD, RSVD, RSVD, RSVD, RSVD, RSVD, RSVD};
	memcpy(pbuf,version,number);

	return 0;	//success

}
