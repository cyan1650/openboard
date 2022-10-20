/********************************************************************************************************
 * @file	ecdsa.c
 *
 * @brief	This is the source file for B91
 *
 * @author	Driver Group
 * @date	2019
 *
 * @par		Copyright (c) 2019, Telink Semiconductor (Shanghai) Co., Ltd.
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
/********* pke version:1.0 *********/
#include "lib/include/pke/ecdsa.h"


extern void pke_set_exe_cfg(unsigned int cfg);

#if DIRVER_CONFILCT
/**
 * @brief		Generate ECDSA Signature in U32 little-endian big integer style.
 * @param[in]	curve	- ecc curve struct pointer, please make sure it is valid.
 * @param[in]	e		- derived from hash value.
 * @param[in]	k		- internal random integer k.
 * @param[in]	dA		- private key.
 * @param[out]	r		- signature r.
 * @param[out]	s		- signature s.
 * @return		ECDSA_SUCCESS(success), other(error).
 */
unsigned char ecdsa_sign_uint32(eccp_curve_t *curve, unsigned int *e, unsigned int *k, unsigned int *dA, unsigned int *r, unsigned int *s)
{
	unsigned char ret;
	unsigned int nWordLen = GET_WORD_LEN(curve->eccp_n_bitLen);
	unsigned int pWordLen = GET_WORD_LEN(curve->eccp_p_bitLen);
	unsigned int tmp1[ECC_MAX_WORD_LEN];

	if(NULL == curve || NULL == e || NULL == k || NULL == dA || NULL == r || NULL == s)
	{
		return ECDSA_POINTOR_NULL;
	}

	if(curve->eccp_p_bitLen > ECC_MAX_BIT_LEN)
	{
		return ECDSA_INVALID_INPUT;
	}

	//make sure k in [1, n-1]
	if(ismemzero4(k, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}
	if(big_integer_compare(k, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		return ECDSA_INTEGER_TOO_BIG;
	}

	//get x1
	ret = pke_eccp_point_mul(curve, k, curve->eccp_Gx, curve->eccp_Gy, tmp1, NULL);  //y coordinate is not needed
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//r = x1 mod n
	ret = pke_mod(tmp1, pWordLen, curve->eccp_n, curve->eccp_n_h, curve->eccp_n_n1, nWordLen, r);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//make sure r is not zero
	if(ismemzero4(r, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}

	//tmp1 =  r*dA mod n
	if(curve->eccp_n_h && curve->eccp_n_n1)
	{
		pke_load_pre_calc_mont(curve->eccp_n_h, curve->eccp_n_n1, nWordLen);
		pke_set_exe_cfg(PKE_EXE_CFG_ALL_NON_MONT);
		ret = pke_modmul_internal(curve->eccp_n, r, dA, tmp1, nWordLen);
	}
	else
	{
		ret = pke_mod_mul(curve->eccp_n, r, dA, tmp1, nWordLen);
	}
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//tmp1 = e + r*dA mod n
	ret = pke_mod_add(curve->eccp_n, e, tmp1, tmp1, nWordLen);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//s = k^(-1) mod n
	ret = pke_mod_inv(curve->eccp_n, k, s, nWordLen, nWordLen);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//s = (k^(-1))*(e + r*dA) mod n
	ret = pke_modmul_internal(curve->eccp_n, s, tmp1, s, nWordLen);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//make sure s is not zero
	if(ismemzero4(s, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}

	return ECDSA_SUCCESS;
}

/**
 * @brief		Generate ECDSA Signature in octet string style.
 * @param[in]	curve		- ecc curve struct pointer, please make sure it is valid.
 * @param[in]	E			- hash value, big-endian.
 * @param[in]	EByteLen	- byte length of E.
 * @param[in]	priKey		- private key, big-endian.
 * @param[out]	signature	- signature r and s, big-endian.
 * @return		ECDSA_SUCCESS(success), other(error).
 */
unsigned char ecdsa_sign(eccp_curve_t *curve, unsigned char *E, unsigned int EByteLen, unsigned char *rand_k, unsigned char *priKey,
				   unsigned char *signature)
{
	unsigned int tmpLen;
	unsigned int nByteLen = GET_BYTE_LEN(curve->eccp_n_bitLen);
	unsigned int nWordLen = GET_WORD_LEN(curve->eccp_n_bitLen);
	unsigned int e[ECC_MAX_WORD_LEN], k[ECC_MAX_WORD_LEN], dA[ECC_MAX_WORD_LEN];
	unsigned int r[ECC_MAX_WORD_LEN], s[ECC_MAX_WORD_LEN];
	unsigned char ret;

	if(NULL == curve || NULL == E || NULL == priKey || NULL == signature)
	{
		return ECDSA_POINTOR_NULL;
	}

	if(0 == EByteLen)
	{
		return ECDSA_INVALID_INPUT;
	}

	if(curve->eccp_p_bitLen > ECC_MAX_BIT_LEN)
	{
		return ECDSA_INVALID_INPUT;
	}

    //get integer e from hash value E(according to SEC1-V2 2009)
	uint32_clear(e, nWordLen);
	if(curve->eccp_n_bitLen >= (EByteLen<<3)) //in this case, make E as e directly
	{
		reverse_byte_array((unsigned char *)E, (unsigned char *)e, EByteLen);
	}
	else                                     //in this case, make left eccp_n_bitLen bits of E as e
	{
		memcpy(e, E, nByteLen);
		reverse_byte_array((unsigned char *)E, (unsigned char *)e, nByteLen);
		tmpLen = (curve->eccp_n_bitLen)&7;
		if(tmpLen)
		{
			div2n_u32(e, nWordLen, 8-tmpLen);
		}
	}

	//get e = e mod n, i.e., make sure e in [0, n-1]
	if(big_integer_compare(e, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		sub_u32(e, curve->eccp_n, k, nWordLen);
		uint32_copy(e, k, nWordLen);
	}

	//make sure priKey in [1, n-1]
	memset(dA, 0, (nWordLen<<2)-nByteLen);
	reverse_byte_array((unsigned char *)priKey, (unsigned char *)dA, nByteLen);
	if(ismemzero4(dA, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}
	if(big_integer_compare(dA, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		return ECDSA_INTEGER_TOO_BIG;
	}

	//get k
	memset(k, 0, (nWordLen<<2)-nByteLen);
	if(rand_k)
	{
		reverse_byte_array(rand_k, (unsigned char *)k, nByteLen);
	}
	else
	{
ECDSA_SIGN_LOOP:

		ret = rand_get((unsigned char *)k, nByteLen);
		if(TRNG_SUCCESS != ret)
		{
			return ret;
		}

		//make sure k has the same bit length as n
		tmpLen = (curve->eccp_n_bitLen)&0x1F;
		if(tmpLen)
		{
			k[nWordLen-1] &= (1<<(tmpLen))-1;
		}
	}

	//sign
	ret = ecdsa_sign_uint32(curve, e, k, dA, r, s);
	if(ECDSA_ZERO_ALL == ret || ECDSA_INTEGER_TOO_BIG == ret)
	{
		if(rand_k)
		{
			return ret;
		}
		else
		{
			goto ECDSA_SIGN_LOOP;
		}
	}
	else if(ECDSA_SUCCESS != ret)
	{
		return ret;
	}

	reverse_byte_array((unsigned char *)r, signature, nByteLen);
	reverse_byte_array((unsigned char *)s, signature+nByteLen, nByteLen);

	return ECDSA_SUCCESS;
}
#endif
#if DIRVER_CONFILCT
/**
 * @brief		Verify ECDSA Signature in octet string style.
 * @param[in]	curve		- ecc curve struct pointer, please make sure it is valid.
 * @param[in]	E			- hash value, big-endian.
 * @param[in]	EByteLen	- byte length of E.
 * @param[in]	pubKey		- public key, big-endian.
 * @param[in]	signature	- signature r and s, big-endian.
 * @return		ECDSA_SUCCESS(success), other(error).
 */
unsigned char ecdsa_verify(eccp_curve_t *curve, unsigned char *E, unsigned int EByteLen, unsigned char *pubKey, unsigned char *signature)
{
	unsigned int tmpLen;
	unsigned int nByteLen = GET_BYTE_LEN(curve->eccp_n_bitLen);
	unsigned int nWordLen = GET_WORD_LEN(curve->eccp_n_bitLen);
	unsigned int pByteLen = GET_BYTE_LEN(curve->eccp_p_bitLen);
	unsigned int pWordLen = GET_WORD_LEN(curve->eccp_p_bitLen);
	unsigned int maxWordLen = GET_MAX_LEN(nWordLen,pWordLen);
	unsigned int e[ECC_MAX_WORD_LEN], r[ECC_MAX_WORD_LEN], s[ECC_MAX_WORD_LEN];
	unsigned int tmp[ECC_MAX_WORD_LEN], x[ECC_MAX_WORD_LEN];
	unsigned char ret;

	if(NULL == curve || NULL == E || NULL == pubKey || NULL == signature)
	{
		return ECDSA_POINTOR_NULL;
	}

	if(0 == EByteLen)
	{
		return ECDSA_INVALID_INPUT;
	}

	if(curve->eccp_p_bitLen > ECC_MAX_BIT_LEN)
	{
		return ECDSA_INVALID_INPUT;
	}

	//make sure r in [1, n-1]
	memset(r, 0, (nWordLen<<2)-nByteLen);
	reverse_byte_array(signature, (unsigned char *)r, nByteLen);
	if(ismemzero4(r, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}
	if(big_integer_compare(r, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		return ECDSA_INTEGER_TOO_BIG;
	}

	//make sure s in [1, n-1]
	memset(s, 0, (nWordLen<<2)-nByteLen);
	reverse_byte_array(signature+nByteLen, (unsigned char *)s, nByteLen);
	if(ismemzero4(s, nWordLen))
	{
		return ECDSA_ZERO_ALL;
	}
	if(big_integer_compare(s, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		return ECDSA_INTEGER_TOO_BIG;
	}

	//tmp = s^(-1) mod n
	ret = pke_mod_inv(curve->eccp_n, s, tmp, nWordLen, nWordLen);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

    //get integer e from hash value E(according to SEC1-V2 2009)
	uint32_clear(e, nWordLen);
	if(curve->eccp_n_bitLen >= (EByteLen<<3)) //in this case, make E as e directly
	{
		reverse_byte_array((unsigned char *)E, (unsigned char *)e, EByteLen);
	}
	else                                     //in this case, make left eccp_n_bitLen bits of E as e
	{
		memcpy(e, E, nByteLen);
		reverse_byte_array((unsigned char *)E, (unsigned char *)e, nByteLen);
		tmpLen = (curve->eccp_n_bitLen)&7;
		if(tmpLen)
		{
			div2n_u32(e, nWordLen, 8-tmpLen);
		}
	}

	//get e = e mod n, i.e., make sure e in [0, n-1]
	if(big_integer_compare(e, nWordLen, curve->eccp_n, nWordLen) >= 0)
	{
		sub_u32(e, curve->eccp_n, x, nWordLen);
		uint32_copy(e, x, nWordLen);
	}

	//x =  e*(s^(-1)) mod n
	if(curve->eccp_n_h && curve->eccp_n_n1)
	{
		pke_load_pre_calc_mont(curve->eccp_n_h, curve->eccp_n_n1, nWordLen);
		pke_set_exe_cfg(PKE_EXE_CFG_ALL_NON_MONT);
		ret = pke_modmul_internal(curve->eccp_n, e, tmp, x, nWordLen);
	}
	else
	{
		ret = pke_mod_mul(curve->eccp_n, e, tmp, x, nWordLen);
	}
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//tmp =  r*(s^(-1)) mod n
	ret = pke_modmul_internal(curve->eccp_n, r, tmp, tmp, nWordLen);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	//check public key
	memset(e, 0, (maxWordLen<<2)-pByteLen);
	memset(s, 0, (maxWordLen<<2)-pByteLen);
	reverse_byte_array(pubKey, (unsigned char *)e, pByteLen);
	reverse_byte_array(pubKey+pByteLen, (unsigned char *)s, pByteLen);
	ret = pke_eccp_point_verify(curve, e, s);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	ret = pke_eccp_point_mul(curve, tmp, e, s, e, s);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	if(!ismemzero4(x, nWordLen))
	{
		ret = pke_eccp_point_mul(curve, x, curve->eccp_Gx, curve->eccp_Gy, x, tmp);
		if(PKE_SUCCESS != ret)
		{
			return ret;
		}

		ret = pke_eccp_point_add(curve, e, s, x, tmp, e, s);
		if(PKE_SUCCESS != ret)
		{
			return ret;
		}
	}

	//x = x1 mod n
	ret = pke_mod(e, pWordLen, curve->eccp_n, curve->eccp_n_h, curve->eccp_n_n1, nWordLen, tmp);
	if(PKE_SUCCESS != ret)
	{
		return ret;
	}

	if(big_integer_compare(tmp, nWordLen, r, nWordLen))
	{
		return ECDSA_VERIFY_FAILED;
	}

	return ECDSA_SUCCESS;
}
#endif


