/********************************************************************************************************
 * @file	phy.c
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



#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)


/**************************************************************************************************************************
 * 	   PHYs			  timing(uS)
 *   1M PHY   :    (rf_len + 10) * 8,      // 10 = 1(BLE preamble) + 9(accesscode 4 + crc 3 + header 2)
 *   2M PHY   :	   (rf_len + 11) * 4	   // 11 = 2(BLE preamble) + 9(accesscode 4 + crc 3 + header 2)
 *
 *  Coded PHY :    376 + (N*8+27)*S
 *   			 = 376 + ((rf_len+2)*8+27)*S
 *  			 = 376 + (rf_len*8+43)*S  		// 376uS = 80uS(preamble) + 256uS(Access Code) + 16uS(CI) + 24uS(TERM1)
 *  			 = rf_len*S*8 + 43*S + 376
 *  	S2	  :  = rf_len*16 + 462
 *  	S8	  :	 = rf_len*64 + 720
 *
 *  	 Empty packet time					rf_len = 27 packet time			rf_len = 255 packet time
 *    1M 	PHY   	:    80 uS						296 uS							2120 uS
 *    2M 	PHY   	:    44 uS						152 uS							1064 uS
 *    COded PHY S2  :   462 uS						894 uS							4542 uS
 *    COded PHY S8  :   720 uS					   2448 uS						   17040 uS
 *************************************************************************************************************************/
#if 0
_attribute_data_retention_  u32 full_pkt_us[4] = {
		2120,   // 1M PHY		 : (255 + 10) * 8
		1064,	// 2M PHY		 : (255 + 11) * 4
		4542,	// Coded PHY(S2) :  376 + (255*8+43)*2
		17040,	// Coded PHY(S8) :  376 + (255*8+43)*8
};
packet_us = full_pkt_us[(p_ext_adv->sec_phy-1) + (p_ext_adv->coding_ind>>3)];
//extern u32 full_pkt_us[4];
#endif


_attribute_data_retention_	_attribute_aligned_(4) ll_phy_t		bltPHYs = {
	/* Does not depend on whether the function is registered: blc_ll_init2MPhyCodedPhy_feature */
	.cur_llPhy = BLE_PHY_1M,  //default 1M
	.cur_CI    = LE_CODED_S2, //default S2, TODO: S8 ?
	.oneByte_us = 8,		  //default 1M
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	.prmb_ac_us	= 40 + 20,	  //default 1M
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	.prmb_ac_us	= 40,	 	  //default 1M
#endif
};

_attribute_data_retention_	ll_get_conn_phy_ptr_callback_t  ll_get_conn_phy_ptr_cb = NULL;

_attribute_data_retention_	ll_phy_switch_callback_t		ll_phy_switch_cb = NULL;

_attribute_data_retention_	u8	tx_settle_adv[4] =   {	0, LL_ADV_TX_SETTLE, 	LL_ADV_TX_STL_2M, 	 LL_ADV_TX_STL_CODED };
_attribute_data_retention_	u8	tx_settle_slave[4] = {	0, LL_SLAVE_TX_SETTLE,  LL_SLAVE_TX_STL_2M,  LL_SLAVE_TX_STL_CODED };
_attribute_data_retention_	u8	tx_settle_master[4] ={	0, LL_MASTER_TX_SETTLE, LL_MASTER_TX_STL_2M, LL_MASTER_TX_STL_CODED };
_attribute_data_retention_	u8	tx_settle_slave_high_freq[4] = {	0, LL_SLAVE_TX_SETTLE_HIGH_FREQ,  LL_SLAVE_TX_STL_2M,  LL_SLAVE_TX_STL_CODED };
/*******************************************************************************************************************************
 *  tx sequence          CMD trigger -> tx settle -> preamble ->access code -> PDU -> CRC
 *
 * in each ConnIntervel, slave will calculate the TxTimestamp by master, TxTimestamp(master) = RxTimestamp - accessCode - preamble
 *
 *	PHYs			time(accessCode + preamble)
 *	1M		PHY		5*8 = 40us
 *	2M		PHY		(2*8 + 4*8)/2 = 24us
 *  coded   PHY     80us + 256us = 336us
 *
 *******************************************************************************************************************************/
#if (MCU_CORE_TYPE == MCU_CORE_825x)
_attribute_ram_code_ void rf_ble_switch_phy(le_phy_type_t phy, le_coding_ind_t coding_ind)
{
	bltPHYs.cur_llPhy = phy;

	if(phy == BLE_PHY_1M)//BLE1M 01
	{


		write_reg8(0x1220, 0x16);
		write_reg8(0x1221, 0x0a);
		write_reg8(0x1222, 0x20);
		write_reg8(0x1223, 0x23);

		write_reg8(0x124a,0x0e);
		write_reg8(0x124b,0x09);
		// driver code: core_124e/124f set to default value 0x0f09 in 1M/Coded PHY, not set in 2M PHY. So all PHYs not set.

		write_reg8(0x1254,0x89); // driver code: core_1254...1257 not set in 1M/Coded PHY, so here restore to default value
		write_reg8(0x1255,0x06);
		write_reg8(0x1256,0x8c);
		write_reg8(0x1257,0x07);

		write_reg8(0x1276, 0x45); //poweron_dft: 0x57
		write_reg8(0x1277, 0x7b); //poweron_dft: 0x73
		write_reg8(0x1279, 0x08); //poweron_dft: 0x00

		write_reg8(0x1273,0x01);  		//default: 01
		write_reg8(0x1236,0xb7);		//default: b7
		write_reg8(0x1237,0x8e);		//default: 8e
		write_reg8(0x1238,0xc4);		//default: c4
		write_reg8(0x1239,0x71);		//default: 71

		//write_reg8(0x401, 0x01);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x46);
		write_reg8(0x404, 0xf5);
		write_reg8(0x405, 0x04);
		write_reg8(0x420, 0x1e);
		//write_reg8(0x430, 0x3e);		//core_401 all 0x3e set in "rf_ble_1m_param_init", so no set here
		write_reg32(0x460, 0x5f4f4434);  //grx_3~0
		write_reg16(0x464, 0x766b);      //grx_5~4
		
		bltPHYs.oneByte_us = 8;
		bltPHYs.prmb_ac_us = 40;  //timing: preamble + access_code
	}
	else if(phy == BLE_PHY_2M) //BLE2M 10
	{


		write_reg8(0x1220, 0x04);
		write_reg8(0x1221, 0x2a);
		write_reg8(0x1222, 0x43);
		write_reg8(0x1223, 0x06);

		write_reg8(0x1276, 0x50); //poweron_dft: 0x57
		write_reg8(0x1277, 0x73); //poweron_dft: 0x73
		write_reg8(0x1279, 0x00); //poweron_dft: 0x00

		write_reg8(0x124a,0x89);  // driver code: core_124a/124b not set in 2M PHY, so here restore to default value
		write_reg8(0x124b,0x06);
		// driver code: core_124e/124f set to default value 0x0f09 in 1M/Coded PHY, not set in 2M PHY. So all PHYs not set.

		write_reg8(0x1254,0x0e);
		write_reg8(0x1255,0x09);
		write_reg8(0x1256,0x0c);
		write_reg8(0x1257,0x08);
		// driver code: core_1258/1259 set to default value 0x0f09 in 2M PHY, not set in 1M/Coded PHY. So all PHYs not set.

		write_reg8(0x1273,0x01);  		//default: 01
		write_reg8(0x1236,0xb7);		//default: b7
		write_reg8(0x1237,0x8e);		//default: 8e
		write_reg8(0x1238,0xc4);		//default: c4
		write_reg8(0x1239,0x71);		//default: 71

		//write_reg8(0x401, 0x01);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x4c);   //0x4a -> 0x4c: 12 - 2 = 10,  10*4=40uS, be same as 1M PHY 6 preamble timing
		write_reg8(0x404, 0xe5);
		write_reg8(0x405, 0x04);
		write_reg8(0x420, 0x1e);
		//write_reg8(0x430, 0x3e);		//core_401 all 0x3e set in "rf_ble_1m_param_init", so no set here
		write_reg32(0x460, 0x61514636);  //grx_3~0
		write_reg16(0x464, 0x786d);      //grx_5~4
		
		bltPHYs.oneByte_us = 4;
		bltPHYs.prmb_ac_us = 24;
	}
	else //LE coded PHY
	{


		bltPHYs.cur_CI = coding_ind;
		bltPHYs.prmb_ac_us = 336;
		if(coding_ind == LE_CODED_S2){
			write_reg8(0x1236,0xee);
			write_reg8(0x405, 0xa4);	//continuous writing core_405 will be ERROR
			bltPHYs.oneByte_us = 16;
		}
		else{  // S8
			write_reg8(0x1236,0xf6);
			write_reg8(0x405, 0xb4);
			bltPHYs.oneByte_us = 64;
		}

		write_reg8(0x1220, 0x17);
		write_reg8(0x1221, 0x0a);
		write_reg8(0x1222, 0x20);
		write_reg8(0x1223, 0x23);

		write_reg8(0x1276, 0x50); //poweron_dft: 0x57
		write_reg8(0x1277, 0x73); //poweron_dft: 0x73
		write_reg8(0x1279, 0x00); //poweron_dft: 0x00

		write_reg8(0x124a,0x0e);
		write_reg8(0x124b,0x09);
		// driver code: core_124e/124f set to default value 0x0f09 in 1M/Coded PHY, not set in 2M PHY. So all PHYs not set.

		write_reg8(0x1254,0x89); // driver code: core_1254...1257 not set in 1M/Coded PHY, so here restore to default value
		write_reg8(0x1255,0x06);
		write_reg8(0x1256,0x8c);
		write_reg8(0x1257,0x07);

		write_reg8(0x1273,0x21);  		//default: 01
		//write_reg8(0x1236,0xee);		//default: b7       //different value for S2/S8
		write_reg8(0x1237,0x8c);		//default: 8e
		write_reg8(0x1238,0xc8);		//default: c4
		write_reg8(0x1239,0x7d);		//default: 71

		//write_reg8(0x401, 0x01);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x4a);
		write_reg8(0x404, 0xf5);
		//write_reg8(0x405, 0xa4);		//different value for S2/S8
		write_reg8(0x420, 0xf0);
		//write_reg8(0x430, 0x3e);		//core_401 all 0x3e set in "rf_ble_1m_param_init", so no set here

		write_reg32(0x460, 0x5f4f4434);  //grx_3~0
		write_reg16(0x464, 0x766b);      //grx_5~4

		//TODO:When the sys clock is 24/32/48M , it must be delayed, but it is strange not to know why.Add by tyf 190813
		sleep_us(5);
	}
}

#elif(MCU_CORE_TYPE == MCU_CORE_827x)
_attribute_ram_code_ void rf_ble_switch_phy(le_phy_type_t phy, le_coding_ind_t coding_ind)
{
	bltPHYs.cur_llPhy = phy;

	if(phy == BLE_PHY_1M)//BLE1M 01
	{
		write_reg32(0x1220, 0x23200a16);

		write_reg8(0x1273,0x01);  		//default: 01
		write_reg16(0x1236,0x8eb7);	//default: 71c48eb7
		write_reg16(0x1238,0x71c4);

//		write_reg8(0x401, 0x08);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x46);
		write_reg8(0x404, 0xf5);
		write_reg8(0x405, 0x04);
		write_reg8(0x420, 0x1e);

		write_reg8(0x430, 0x36);		//tx timestamp capture disable
		write_reg32(0x460, 0x5f4f4434);  //grx_3~0
		write_reg16(0x464, 0x766b);      //grx_5~4
		
		bltPHYs.oneByte_us = 8;
		bltPHYs.prmb_ac_us = 40;  //timing: preamble + access_code
	}
	else if(phy == BLE_PHY_2M) //BLE2M 10
	{

		write_reg32(0x1220, 0x06432a04);
		write_reg8(0x1273,0x01);  		//default: 01
		write_reg16(0x1236,0x8eb7);	//default: 71c48eb7
		write_reg16(0x1238,0x71c4);
		//write_reg8(0x401, 0x08);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x4c);   		//0x4a -> 0x4c: 12 - 2 = 10,  10*4=40uS, be same as 1M PHY 6 preamble timing
		write_reg8(0x404, 0xe5);
		write_reg8(0x405, 0x04);
		write_reg8(0x420, 0x1e);
		write_reg8(0x430, 0x34);		//tx timestamp capture disable
		write_reg32(0x460, 0x61514636);  //grx_3~0
		write_reg16(0x464, 0x786d);      //grx_5~4
		
		bltPHYs.oneByte_us = 4;
		bltPHYs.prmb_ac_us = 24;

	}
	else //LE coded PHY
	{

		bltPHYs.cur_CI = coding_ind;
		bltPHYs.prmb_ac_us = 336;
		if(coding_ind == LE_CODED_S2){
			write_reg8(0x1236,0xee);
			write_reg8(0x405, 0xa4);	//continuous writing core_405 will be ERROR
			bltPHYs.oneByte_us = 16;
		}
		else{  // S8
			write_reg8(0x1236,0xf6);
			write_reg8(0x405, 0xb4);
			bltPHYs.oneByte_us = 64;
		}

		write_reg32(0x1220, 0x23200a17);

		write_reg8(0x1273,0x21);  		//default: 01
		//write_reg8(0x1236,0xee);		//default: b7       //different value for S2/S8
		write_reg8(0x1237,0x8c);		//default: 8e
		write_reg16(0x1238,0x7dc8);		//default: 0x71c4

		//write_reg8(0x401, 0x08);		//core_401 all 0x01 set in "rf_ble_1m_param_init", so no set here
		write_reg8(0x402, 0x4a);
		write_reg8(0x404, 0xf5);
		//write_reg8(0x405, 0xa4);		//different value for S2/S8
		write_reg8(0x420, 0xf0);
		write_reg8(0x430, 0x34);		//tx timestamp capture disable

		write_reg32(0x460, 0x5f4f4434);  //grx_3~0
		write_reg16(0x464, 0x766b);      //grx_5~4

		//TODO:When the sys clock is 24/32/48M , it must be delayed, but it is strange not to know why.Add by tyf 190813
		sleep_us(5);
	}
}
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_ram_code_ void rf_ble_switch_phy(le_phy_type_t phy, le_coding_ind_t coding_ind)
{
	bltPHYs.cur_llPhy = phy;
	if(phy == BLE_PHY_1M)//BLE1M 01
	{
		write_reg8(0x140e3d,0x61);
		write_reg32(0x140e20,0x23200a16);
		write_reg8(0x140c20,0x84);
		write_reg8(0x140c22,0x00);
		write_reg8(0x140c4d,0x01);
		write_reg8(0x140c4e,0x1e);
		write_reg16(0x140c36,0x0eb7);
		write_reg16(0x140c38,0x71c4);
		write_reg8(0x140c73,0x01);
		#if RF_RX_SHORT_MODE_EN
			write_reg8(0x140c79,0x38);			//default:0x00;RX_DIS_PDET_BLANK
		#else
			write_reg8(0x140c79,0x08);
		#endif
		write_reg16(0x140cc2,0x4b39);
		write_reg32(0x140cc4,0x796e6256);
		write_reg32(0x140800,0x4446081f);
		write_reg16(0x140804,0x04f5);

		bltPHYs.oneByte_us = 8;
		bltPHYs.prmb_ac_us = 40 + 20;  //timing: preamble + access_code + AD convert delay
	}
	else if(phy == BLE_PHY_2M) //BLE2M 10
	{

		write_reg8(0x140e3d,0x41);
		write_reg32(0x140e20,0x26432a06);
		write_reg8(0x140c20,0x84);
		write_reg8(0x140c22,0x01);
		write_reg8(0x140c4d,0x01);
		write_reg8(0x140c4e,0x1e);
		write_reg16(0x140c36,0x0eb7);
		write_reg16(0x140c38,0x71c4);
		write_reg8(0x140c73,0x01);
		#if RF_RX_SHORT_MODE_EN
			write_reg8(0x140c79,0x30);			//default:0x00;RX_DIS_PDET_BLANK
		#else
			write_reg8(0x140c79,0x00);
		#endif
		write_reg16(0x140cc2,0x4c3b);
		write_reg32(0x140cc4,0x7a706458);
		write_reg32(0x140800,0x4446081f);
		write_reg16(0x140804,0x04e5);

		bltPHYs.oneByte_us = 4;
		bltPHYs.prmb_ac_us = 24 + 10;  //timing: preamble + access_code + AD convert delay
	}
	else //LE coded PHY
	{

		bltPHYs.cur_CI = coding_ind;
		bltPHYs.prmb_ac_us = 336 + 14;  //timing: preamble + access_code + AD convert delay
		if(coding_ind == LE_CODED_S2){
			write_reg16(0x140c36,0x0cee);
			write_reg16(0x140804,0xa4f5);

			bltPHYs.oneByte_us = 16;
		}
		else{  // S8
			write_reg16(0x140c36,0x0cf6);
			write_reg16(0x140804,0xb4f5);

			bltPHYs.oneByte_us = 64;
		}
		write_reg8(0x140e3d,0x61);
		write_reg32(0x140e20,0x23200a16);
		write_reg8(0x140c20,0x85);
		write_reg8(0x140c22,0x00);
		write_reg8(0x140c4d,0x01);
		write_reg8(0x140c4e,0xf0);
		write_reg16(0x140c38,0x7dc8);
		write_reg8(0x140c73,0x21);
		#if RF_RX_SHORT_MODE_EN
			write_reg8(0x140c79,0x30);
		#else
			write_reg8(0x140c79,0x00);
		#endif
		write_reg16(0x140cc2,0x4836);
		write_reg32(0x140cc4,0x796e6254);
		write_reg32(0x140800,0x444a081f);


		//TODO:When the sys clock is 24/32/48M , it must be delayed, but it is strange not to know why.Add by tyf 190813
		sleep_us(5);
	}
}
#endif

void		blc_ll_init2MPhyCodedPhy_feature(void)
{
	LL_FEATURE_MASK_0 |= (LL_FEATURE_ENABLE_LE_2M_PHY<<8 | 	LL_FEATURE_ENABLE_LE_CODED_PHY<<11);

	ll_phy_switch_cb = rf_ble_switch_phy;

#if 0  // if multi-connection, should change here

#else
	ll_conn_phy_update_cb = blt_ll_updateConnPhy;
	ll_conn_phy_swicth_cb = blt_ll_switchConnPhy;
#endif
}





ble_sts_t	blc_ll_readPhy(u16 connHandle, hci_le_readPhyCmd_retParam_t  *para)
{

	ll_conn_phy_t* pConnPhy = ll_get_conn_phy_ptr_cb(connHandle);
	if(pConnPhy == NULL){
		return HCI_ERR_UNKNOWN_CONN_ID;
	}


	para->status = BLE_SUCCESS;
	para->handle[0] = U16_LO(connHandle);	//connection handle 12bits meaningful
	para->handle[1] = U16_HI(connHandle) & 0x0E;
	para->tx_phy = pConnPhy->conn_cur_phy;
	para->rx_phy = pConnPhy->conn_cur_phy;

	return BLE_SUCCESS;
}

// if Coded PHY is used, this API set default S2/S8 mode for Connection
ble_sts_t	blc_ll_setDefaultConnCodingIndication(le_ci_prefer_t prefer_CI)
{

	if(prefer_CI == CODED_PHY_PREFER_S2){
		bltPHYs.dft_CI = LE_CODED_S2;
	}
	else if(prefer_CI == CODED_PHY_PREFER_S8){
		bltPHYs.dft_CI = LE_CODED_S8;
	}



	return BLE_SUCCESS;
}

//////////////////////////////////////ll phy update////////////////////////////////////////////
ble_sts_t 	blc_ll_setDefaultPhy(le_phy_prefer_mask_t all_phys, le_phy_prefer_type_t tx_phys, le_phy_prefer_type_t rx_phys)	//set for the device
{

	if(all_phys & PHY_TX_NO_PREFER){
		bltPHYs.dft_tx_prefer_phys = 0;
	}
	else{
		bltPHYs.dft_tx_prefer_phys = (u8)tx_phys;
	}


	if(all_phys & PHY_RX_NO_PREFER){
		bltPHYs.dft_rx_prefer_phys = 0;
	}
	else{
		bltPHYs.dft_rx_prefer_phys = (u8)rx_phys;
	}


	//do not support Asymmetric PHYs, dft_prefer_phy = dft_tx_prefer_phys & dft_rx_prefer_phys
	bltPHYs.dft_prefer_phy = bltPHYs.dft_tx_prefer_phys & bltPHYs.dft_rx_prefer_phys; //
	if(bltPHYs.dft_prefer_phy){  //at least 1 PHY is selected

		//code below have 2 functions:
		// 1. set "le_phy_type_t" according to "le_phy_prefer_type_t"
		// 2. in case that at least 2 kind of PHYs are preferred. In this situation, we select 1M->2M->Coded by order
		if(bltPHYs.dft_prefer_phy & PHY_PREFER_1M){
			bltPHYs.dft_prefer_phy = BLE_PHY_1M;
		}
		else if(bltPHYs.dft_prefer_phy & PHY_PREFER_2M){
			bltPHYs.dft_prefer_phy = BLE_PHY_2M;
		}
		else{
			bltPHYs.dft_prefer_phy = BLE_PHY_CODED;
		}
	}

	return BLE_SUCCESS;
}

extern u8 protocol_collision;


#if (MCU_CORE_TYPE == MCU_CORE_9518)
_attribute_ram_code_
#endif
int blt_reset_conn_phy_param(ll_conn_phy_t* pconn_phy)
{
	pconn_phy->conn_cur_phy = BLE_PHY_1M;
	pconn_phy->conn_cur_CI = bltPHYs.dft_CI ? bltPHYs.dft_CI : LE_CODED_S8;
	pconn_phy->conn_next_CI = 0;
	pconn_phy->phy_req_pending = 0;
	pconn_phy->phy_req_trigger = 0;
	pconn_phy->phy_update_pending = 0;

	return 0;
}


ble_sts_t  	blc_ll_setPhy(	u16 connHandle,					le_phy_prefer_mask_t all_phys,
							le_phy_prefer_type_t tx_phys, 	le_phy_prefer_type_t rx_phys,
							le_ci_prefer_t phy_options)
{

	ll_conn_phy_t* pConnPhy = ll_get_conn_phy_ptr_cb(connHandle);
	if(pConnPhy == NULL){
		return HCI_ERR_UNKNOWN_CONN_ID;
	}


#if CERT_SCHEME
	//1.incline to change phy if a different phy bit is set along with current phy
	//2.anything asymmetric would return unsupported parameter
	volatile u8 tx_preferPhys = 0, rx_preferPhys = 0, comm_phy = 0, asym = 0;		//its wired here, only values set normal, or
	//abnormal optimization happens

	if(all_phys == 1 || all_phys == 2)
		asym = 1;
	else if(all_phys == 3)
	{
		rx_preferPhys = tx_preferPhys = 0;
	}
	else if(all_phys == 0)
	{
		rx_preferPhys = rx_phys;
		tx_preferPhys = tx_phys;
	}

	if(tx_preferPhys == 0 && rx_preferPhys == 0 && all_phys == 3)
	{
		comm_phy = bltPHYs.dft_prefer_phy;
	}
	else if(tx_preferPhys != rx_preferPhys)
	{
		asym = 1;
	}
	else if(tx_preferPhys == rx_preferPhys)
	{
		comm_phy = tx_preferPhys;
	}
	//todo: if certain feature is not supported, then return unsupported parameters

	int cur_PHY_match = comm_phy;
	if(comm_phy && !asym)
	{
		pConnPhy->conn_prefer_phys = comm_phy;
		pConnPhy->phy_req_pending = 1;
		pConnPhy->phy_req_trigger = 1;  //mark that Request triggered, it must generate PHY Update Event at last(even no PHY changes)
	}
	else
		return HCI_ERR_UNSUPPORTED_FEATURE_PARAM_VALUE;
#else
	u8 tx_preferPhys, rx_preferPhys, comm_phy;

	if(all_phys & PHY_TX_NO_PREFER){
		tx_preferPhys = 0;
	}
	else{
		tx_preferPhys = (u8)tx_phys;
	}

	if(all_phys & PHY_RX_NO_PREFER){
		rx_preferPhys = 0;
	}
	else{
		rx_preferPhys = (u8)rx_phys;
	}




	comm_phy = tx_preferPhys & rx_preferPhys;  //support symmetric PHYs only



	//no prefer PHYs || current using PHY is among preferred PHYs, PHY Update Complete Event is generated with status "BLE_SUCCESS"
	//NOTE:   PHY_type: 1 2 3,  prefer_PHYs: BIT(0) BIT(1) BIT(2),  so  prefer_PHYs = BIT(PHY_type - 1)
	int cur_PHY_match = comm_phy & BIT(pConnPhy->conn_cur_phy - 1);
	if(!comm_phy || cur_PHY_match ){
		blc_tlkEvent_pending |= EVENT_MASK_PHY_UPDATE;
	}
	else{

		pConnPhy->conn_prefer_phys = comm_phy;
		pConnPhy->phy_req_pending = 1;
		pConnPhy->phy_req_trigger = 1;  //mark that Request triggered, it must generate PHY Update Event at last(even no PHY changes)

	}
#endif


#if 1
	//for both current using PHY is among preferred PHYs(no need PHY Update)   and   PHY Update is needed,
	//if new Coded PHY using, need update coding_ind according to "phy_options"
	if( (comm_phy & PHY_PREFER_CODED) && phy_options){   // host preferred PHYs include Coded_PHY &&  host has preferred Coding Indication
		le_coding_ind_t new_CI = 0;
		if(phy_options & CODED_PHY_PREFER_S2){
			new_CI = LE_CODED_S2;
		}
		else if(phy_options & CODED_PHY_PREFER_S8){
			new_CI = LE_CODED_S8;
		}


		if(cur_PHY_match){  //current using PHY is among preferred PHYs
			pConnPhy->conn_cur_CI = new_CI;  //no PHY Update procedure, should update Coding Indication immediately
		}
		else{
			pConnPhy->conn_next_CI = new_CI;  //update Coding Indication when PHY Update procedure complete
		}
	}
#endif

	return BLE_SUCCESS;
}











#endif  // end of (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)

