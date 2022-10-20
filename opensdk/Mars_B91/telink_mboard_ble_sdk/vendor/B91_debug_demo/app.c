/********************************************************************************************************
 * @file	app.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
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
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"





#if (TEST_MODE == LED_TEST)

u32 system_timer_tick = 0;

void user_init()
{
	gpio_set_gpio_en(GPIO_LED_BLUE|GPIO_LED_GREEN|GPIO_LED_WHITE|GPIO_LED_RED);
	gpio_set_output_en(GPIO_LED_BLUE|GPIO_LED_GREEN|GPIO_LED_WHITE|GPIO_LED_RED, 1);
	system_timer_tick = clock_time();
	gpio_write(GPIO_LED_BLUE,1);
}

/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
void main_loop (void)
{
	static u8 led_mode = 0;
	if((led_mode == 0) && clock_time_exceed(system_timer_tick,500*1000)){
		gpio_toggle(GPIO_LED_BLUE);
		led_mode = 1;
	}
	else if((led_mode == 1) && clock_time_exceed(system_timer_tick,1000*1000)){
		gpio_toggle(GPIO_LED_GREEN);
		led_mode = 2;
	}
	else if((led_mode == 2) && clock_time_exceed(system_timer_tick,1500*1000)){
		gpio_toggle(GPIO_LED_WHITE);
		led_mode = 3;
	}
	else if((led_mode == 3) && clock_time_exceed(system_timer_tick,2000*1000)){
		gpio_toggle(GPIO_LED_RED);
		led_mode = 0;
		system_timer_tick = clock_time();
	}

}


#elif (TEST_MODE == PRINT_TEST)

void user_init()
{
	uart_io_init();
	gpio_set_gpio_en(GPIO_LED_BLUE);
	gpio_set_output_en(GPIO_LED_BLUE, 1);

}
u32 mainloop_tick = 0;
/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
void main_loop (void)
{
	mainloop_tick++;
	gpio_toggle(GPIO_LED_BLUE);
	delay_ms(1000);
}
#elif (TEST_MODE == IRQ_PRIORITY_TEST)
#define TX_PKT_PAYLOAD			15

#define TEST_CHN   				37		// 37/38/39 adv channel

#define BLE_ACCESS_CODE			0xd6be898e //0xd6be898e//0xA5CC336A//0xd6be898e//

#define TEST_MAC				0xee

#define TX_INTERVAL_TICK		(50*16000)   //10ms

#define TX_INTERVAL_US			(TX_INTERVAL_TICK/16)

typedef struct{
	u32 dma_len;            //won't be a fixed number as previous, should adjust with the mouse package number

	u8 type   :4;
	u8 rfu1   :1;
	u8 chan_sel:1;
	u8 txAddr :1;
	u8 rxAddr :1;

	u8  rf_len;				//LEN(6)_RFU(2)

	u8	advA[6];			//address
	u8	data[31];			//0-31 byte
}_attribute_packed_ rf_packet_dbg_adv_t;

rf_packet_dbg_adv_t	debug_pkt_adv = {
		sizeof (rf_packet_dbg_adv_t) - 4,		// dma_len
		LL_TYPE_ADV_NONCONN_IND,
		0,
		0,
		0,
		0,
		sizeof (rf_packet_dbg_adv_t) - 6,		// rf_len
		{TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC},	// advA
};
unsigned int timer_capture_value = 0;
void rf_irq_handler(void)
{

	if(rf_get_irq_status(FLD_RF_IRQ_TX)){
		DBG_CHN1_HIGH;
		rf_clr_irq_status(FLD_RF_IRQ_TX);
		sleep_us(1000);
		DBG_CHN1_LOW;
	}
	else
	{
		rf_clr_irq_status(0xffff);
	}

}

void stimer_irq_handler(void)
{

	if(reg_system_cal_irq & STIMER_IRQ_CLR){
		DBG_CHN0_HIGH;
		systimer_clr_irq_status();
		reg_system_tick_irq = clock_time()+16*1000;
		sleep_us(500);
		DBG_CHN0_LOW;
	}

}
void timer0_irq_handler(void)
{

	if(reg_tmr_sta & FLD_TMR_STA_TMR0)
	{
		DBG_CHN3_HIGH;
		reg_tmr_sta = FLD_TMR_STA_TMR0;
		timer_capture_value += 24*1000;
		reg_tmr_capt(0) = timer_capture_value;
		sleep_us(200);
	}
	DBG_CHN3_LOW;

}
void user_init()
{

	gpio_set_gpio_en(GPIO_LED_BLUE);
	gpio_set_output_en(GPIO_LED_BLUE, 1);
	gpio_write(GPIO_LED_BLUE,1);


	core_interrupt_enable();

	plic_interrupt_enable(IRQ15_ZB_RT);

	systimer_irq_enable();

	rf_set_irq_mask(FLD_RF_IRQ_TX);

	stimer_set_irq_mask(STIMER_IRQ_MASK);

	plic_set_priority(IRQ15_ZB_RT, 1);

	plic_set_priority(IRQ1_SYSTIMER,2);

	reg_system_tick_irq = clock_time()+16*1000;

//timer 0 init
	plic_interrupt_enable(IRQ4_TIMER0);

	plic_set_priority(IRQ4_TIMER0,3);

	timer_capture_value = 24*1000;//pclk

	timer_set_mode(TIMER0, TIMER_MODE_SYSCLK,0,timer_capture_value);//cpu clock tick.

	reg_tmr_ctrl0 |= FLD_TMR_WD_CNT_CLR;//no wrap tick

	timer_start(TIMER0);
}
volatile u32 A_dbg = 0;
u32 mainloop_tick = 0;
/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
void main_loop (void)
{
#if 1
	rf_set_ble_chn(TEST_CHN);
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
//	rf_set_ble_crc_adv ();
//	rf_set_ble_access_code_value(BLE_ACCESS_CODE);

	rf_set_tx_dma(2,32);

	u8 rf_data_len = sizeof (rf_packet_dbg_adv_t) - 4;
	u32 rf_tx_dma_len = rf_tx_packet_dma_len(rf_data_len);//4 bytes align
	debug_pkt_adv.dma_len = rf_tx_dma_len;
	rf_set_txmode();
	u32 tx_begin_tick;
	while(1){
		tx_begin_tick = clock_time();
		debug_pkt_adv.data[0] ++;

		rf_tx_pkt ((void *)&debug_pkt_adv);
		delay_us(2000);  //2mS is enough for packet sending
//		if(rf_get_irq_status(FLD_RF_IRQ_TX)){
//			rf_clr_irq_status(FLD_RF_IRQ_TX);
//			gpio_toggle(GPIO_LED_GREEN);
//			DBG_CHN0_TOGGLE;
//		}
		A_dbg = read_sram32(0xe4001104);
		while(!clock_time_exceed(tx_begin_tick, TX_INTERVAL_US) );   //10mS
	}
#endif
}
#elif (TEST_MODE == RF_TEST)

#define TX_PKT_PAYLOAD			15

#define TEST_CHN   				37		// 37/38/39 adv channel

#define BLE_ACCESS_CODE			0x12345678 //0xd6be898e//0xA5CC336A//0xd6be898e//


#define TEST_MAC				0x33

#define TX_INTERVAL_TICK		(50*16000)   //10ms
#define TX_INTERVAL_US			(TX_INTERVAL_TICK/16)


#define TEST_MODE_MANUAL_TX		1
#define TEST_MODE_STX			2
#define TEST_MODE_BTX_TX		3  //test TX only, do not care RX of BTX
#define TEST_MODE_BTX			4

#define TEST_MODE_MANUAL_RX	    10
#define TEST_MODE_SRX	    	11
#define TEST_MODE_BRX_RX	    12	//test RX only, do not care TX of BRX
#define TEST_MODE_BRX	    	13

#define TEST_MODE_SELECT		TEST_MODE_SRX

#define RF_RX_IRQ_EN			1
#define RF_TX_IRQ_EN			0








void ble_manual_tx_test(void);
void ble_stx_test(void);
void ble_btx_tx_test(void);

void ble_manual_rx_test(void);
void ble_brx_rx_test(void);




typedef struct{
	u32 dma_len;            //won't be a fixed number as previous, should adjust with the mouse package number

	u8 type   :4;
	u8 rfu1   :1;
	u8 chan_sel:1;
	u8 txAddr :1;
	u8 rxAddr :1;

	u8  rf_len;				//LEN(6)_RFU(2)

	u8	advA[6];			//address
	u8	data[31];			//0-31 byte
}_attribute_packed_ rf_packet_dbg_adv_t;

rf_packet_dbg_adv_t	debug_pkt_adv = {
		sizeof (rf_packet_dbg_adv_t) - 4,		// dma_len
		LL_TYPE_ADV_NONCONN_IND,
		0,
		0,
		0,
		0,										// type
		sizeof (rf_packet_dbg_adv_t) - 6,		// rf_len
		{TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC, TEST_MAC},	// advA
};

#define			BLE_LL_BUFF_SIZE		80

unsigned char  	blt_rxbuffer[BLE_LL_BUFF_SIZE*4] __attribute__((aligned(4)));

int				blt_rx_wptr = 0;

unsigned char  	blt_txbuffer[BLE_LL_BUFF_SIZE*(4+1)] __attribute__((aligned(4)));

int				blt_tx_wptr = 0;

int AA_rx_irq_cnt = 0;
int AA_rx_crc_ok_cnt = 0;
int AA_rx_match_cnt = 0;

int AA_rx_match_flag;
int AA_rx_miss_cnt;
int AA_rx_miss_rate;
int AA_rx_set_number = 0;

volatile int brx_mode_flag = 0;

u32 bltc_tick_1st_rx = 0;
u32 bltc_connExpectTime = 0;
int bltParam_conn_rx_num = 0;

volatile int   RX_rcvd_tick;
volatile int   RX_rcvd_flag = 0;

u32 blt_timeStamp;
u32 blt_tick_now;

extern u8	blt_tx_empty_packet[6];

#if(RF_RX_IRQ_EN)
void rf_irq_handler(void)
{
#if (TEST_MODE_SELECT == TEST_MODE_SRX)
	if(rf_get_irq_status(FLD_RF_IRQ_RX))
	{
		blt_tick_now = clock_time();
		DBG_CHN0_TOGGLE;
		u8 * raw_pkt = (u8 *) (blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE);
		blt_rx_wptr = ( blt_rx_wptr + 1) & 3;
		AA_rx_irq_cnt ++;

		write_reg8(0x1004f5,0x40);//dma rx rptr ++

		if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

		{
			DBG_CHN1_TOGGLE;
			AA_rx_crc_ok_cnt ++;

			gpio_toggle(GPIO_LED_WHITE);

		}

		bltParam_conn_rx_num ++;
		reg_dma_size(1)=0xffffffff;
		rf_clr_irq_status(FLD_RF_IRQ_RX);
		rf_start_srx(0);
		rf_set_rx_settle_time(85);
	}
#elif (TEST_MODE_SELECT == TEST_MODE_BRX_RX)
	if(rf_get_irq_status(FLD_RF_IRQ_RX))
	{
		static u8 dma_rx_wptr_last = 0;
		blt_tick_now = clock_time();
		u8 rx_wptr = read_reg8(0x1004f4);
		u8 rx_rptr = read_reg8(0x1004f5);
		DBG_CHN0_TOGGLE;
		u8 * raw_pkt = (u8 *) (blt_rxbuffer + (rx_rptr & 3) * BLE_LL_BUFF_SIZE);
		AA_rx_irq_cnt ++;

		if(brx_mode_flag ){
			if(dma_rx_wptr_last != rx_wptr){//new packet,sn/nesn correct.
				dma_rx_wptr_last = rx_wptr;
				write_reg8(0x1004f5,0x40);//dma rx rptr ++
			}
		}


		if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

		{
			DBG_CHN1_TOGGLE;
			AA_rx_crc_ok_cnt ++;

			gpio_toggle(GPIO_LED_WHITE);

			blt_timeStamp = read_reg32(0x140850);//timestamp

			if(!brx_mode_flag){
				bltParam_conn_rx_num = 0;
			}

			if(!bltc_tick_1st_rx && !bltParam_conn_rx_num && (u32)(blt_tick_now - blt_timeStamp) < 16*1000){
				bltc_tick_1st_rx = blt_timeStamp;
				if(!brx_mode_flag){
					brx_mode_flag = 1;
					bltc_connExpectTime = blt_timeStamp + TX_INTERVAL_TICK;
				}
			}
		}

		bltParam_conn_rx_num ++;
		reg_dma_size(1)=0xffffffff;
		rf_clr_irq_status(FLD_RF_IRQ_RX);
	}
#elif (TEST_MODE_SELECT == TEST_MODE_BTX_TX)
	if(rf_get_irq_status(FLD_RF_IRQ_RX))
	{

		static u8 dma_rx_wptr_last = 0;
		u8 rx_wptr = read_reg8(0x1004f4);
		u8 rx_rptr = read_reg8(0x1004f5);
		DBG_CHN0_TOGGLE;
		u8 * raw_pkt = (u8 *) (blt_rxbuffer + (rx_rptr & 3) * BLE_LL_BUFF_SIZE);
		AA_rx_irq_cnt ++;

		if(dma_rx_wptr_last != rx_wptr){//new packet,sn/nesn correct.
			dma_rx_wptr_last = rx_wptr;
			write_reg8(0x1004f5,0x40);//dma rx rptr ++
		}


		if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

		{
			DBG_CHN1_TOGGLE;
			AA_rx_crc_ok_cnt ++;
			gpio_toggle(GPIO_LED_WHITE);

		}

		reg_dma_size(1)=0xffffffff;
		rf_clr_irq_status(FLD_RF_IRQ_RX);
	}
#elif (TEST_MODE_SELECT == TEST_MODE_MANUAL_RX)
	if(rf_get_irq_status(FLD_RF_IRQ_RX))
	{
		blt_tick_now = clock_time();
		DBG_CHN0_TOGGLE;
		u8 * raw_pkt = (u8 *) blt_rxbuffer ;
		AA_rx_irq_cnt ++;


		if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

		{
			DBG_CHN1_TOGGLE;
			AA_rx_crc_ok_cnt ++;

			gpio_toggle(GPIO_LED_WHITE);

		}

		bltParam_conn_rx_num ++;
		reg_dma_size(1)=0xffffffff;
		rf_clr_irq_status(FLD_RF_IRQ_RX);
	}
#else
	if(0){

	}
#endif
	else if(rf_get_irq_status(FLD_RF_IRQ_TX)){
		rf_clr_irq_status(FLD_RF_IRQ_TX);
	}
	else
	{
		rf_clr_irq_status(0xffff);
	}
	plic_interrupt_complete(IRQ15_ZB_RT);



}
#endif

void ble_manual_tx_test(void){

	rf_drv_ble_init();
	rf_set_ble_chn(TEST_CHN);
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
//	rf_set_ble_crc_adv ();
//	rf_set_ble_access_code_value(BLE_ACCESS_CODE);

	rf_set_tx_dma(2,32);

	#if (0 && RF_TX_IRQ_EN)
		core_interrupt_enable();
		plic_interrupt_enable(IRQ15_ZB_RT);
		rf_set_irq_mask(FLD_RF_IRQ_TX);
	#endif

	u8 rf_data_len = sizeof (rf_packet_dbg_adv_t) - 4;
	u32 rf_tx_dma_len = rf_tx_packet_dma_len(rf_data_len);//4 bytes align
	debug_pkt_adv.dma_len = rf_tx_dma_len;
	rf_set_txmode();
	u32 tx_begin_tick;
	while(1){
		tx_begin_tick = clock_time();
		debug_pkt_adv.data[0] ++;


		rf_tx_pkt ((void *)&debug_pkt_adv);
		delay_us(2000);  //2mS is enough for packet sending
		if(rf_get_irq_status(FLD_RF_IRQ_TX)){

			rf_clr_irq_status(FLD_RF_IRQ_TX);
			gpio_toggle(GPIO_LED_GREEN);
			DBG_CHN0_TOGGLE;
		}
		while(!clock_time_exceed(tx_begin_tick, TX_INTERVAL_US) );   //10mS
	}
}

volatile int AA_size;
void ble_stx_test(void){

		rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);


	AA_size = sizeof (rf_packet_dbg_adv_t);

	rf_drv_ble_init();

	rf_access_code_comm(BLE_ACCESS_CODE);

//	blt_ll_set_ble_access_code_adv ();

	rf_set_ble_crc_adv ();


	rf_set_tx_dma(2,80);

	#if (0 && RF_TX_IRQ_EN)
		core_interrupt_enable();
		plic_interrupt_enable(IRQ15_ZB_RT);
		rf_set_irq_mask(FLD_RF_IRQ_TX);
	#endif


	debug_pkt_adv.dma_len = rf_tx_packet_dma_len( debug_pkt_adv.rf_len + 2);

	u32 tx_begin_tick;
	static u32 stx_ok_cnt;
	while(1){
		DBG_CHN0_TOGGLE;

		tx_begin_tick = clock_time();

		debug_pkt_adv.data[0] ++;

//		debug_pkt_adv.header.type = ((debug_pkt_adv.data[0]) & BIT(0))? 0x0e:0x02;//sn nesn

		STOP_RF_STATE_MACHINE;

		rf_set_ble_chn(TEST_CHN);



		rf_start_stx((void *)&debug_pkt_adv, clock_time() + 100);

		delay_us(2000);  //2mS is enough for packet sending
		if(rf_get_irq_status(FLD_RF_IRQ_TX)){
			stx_ok_cnt++;

			rf_clr_irq_status(FLD_RF_IRQ_TX);
			gpio_toggle(GPIO_LED_RED);
			DBG_CHN0_TOGGLE;
		}

		while(!clock_time_exceed(tx_begin_tick, TX_INTERVAL_US) );   //10mS
	}

}
/**
*	@brief	  	This function serves to start Tx of ble_mode.
*	@param[in]	addr   Tx packet address in RAM. Should be 4-byte aligned.
*	@param[in]	tick  Tick value of system timer. It determines when to
*						  	  start ble mode and send packet.
*	@return	 	none
*/
void rf_start_btx (void* addr, unsigned int tick)
{
//	write_reg32 (0x80140a28, 0x0fffffff); 	//first rx timeout is not valid in BTX mode (confirmed by sihui & junwen 20190805)
	write_reg32(0x80140a18, tick);
    write_reg8(0x80140a16, read_reg8(0x80140a16) | 0x04);	// Enable cmd_schedule mode
	write_reg8 (0x80140a00, 0x81);						// ble tx
	reg_dma_src_addr(DMA0)=((unsigned int)((unsigned char*)(addr))-0x80000 + 0xc0200000);
}

void ble_btx_tx_test(void){

	rf_drv_ble_init();
	rf_set_ble_chn(TEST_CHN);
	write_reg8(0x140830,0x36);//disable tx timestamp
	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
	reg_rf_rx_timeout = 600;
//	rf_set_ble_crc_adv ();
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_rx_dma((u8*)(blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE),3,0x05);
	rf_set_tx_dma(2,80);
	u8 rf_data_len = sizeof (rf_packet_dbg_adv_t) - 4;
	u32 rf_tx_dma_len = rf_tx_packet_dma_len(rf_data_len);//4 bytes align
	debug_pkt_adv.dma_len = rf_tx_dma_len;

#if (RF_RX_IRQ_EN)
	core_interrupt_enable();
	plic_interrupt_enable(IRQ15_ZB_RT);
	rf_set_irq_mask(FLD_RF_IRQ_RX);
#endif
	memcpy((void*)(blt_txbuffer + (blt_tx_wptr)*BLE_LL_BUFF_SIZE),(void *)&debug_pkt_adv,sizeof(rf_packet_dbg_adv_t));

	u32 tx_begin_tick;
	static u32 stx_ok_cnt;
	while(1){
		u8 wptr = read_reg8(0x100500);
		tx_begin_tick = clock_time();
		STOP_RF_STATE_MACHINE;//STOP SM
		debug_pkt_adv.data[0] ++;
		rf_set_ble_chn (TEST_CHN);  //2402

		memcpy((void*)(blt_txbuffer + (blt_tx_wptr + 1)*BLE_LL_BUFF_SIZE),(void *)&debug_pkt_adv,sizeof(rf_packet_dbg_adv_t));
		blt_tx_wptr = (blt_tx_wptr + 1) & 3;
		write_reg8(0x100500,wptr+1);//dma tx wptr ++

		rf_start_btx ((void *)blt_txbuffer, clock_time() + 100);
		delay_us(2000);  //2mS is enough for packet sending
		STOP_RF_STATE_MACHINE;//STOP SM


		stx_ok_cnt++;

		gpio_toggle(GPIO_LED_RED);
		DBG_CHN0_TOGGLE;


		while(!clock_time_exceed(tx_begin_tick, TX_INTERVAL_US) );   //10mS
	}


}
void ble_manual_rx_test(void){
	rf_drv_ble_init();
	rf_set_ble_chn(TEST_CHN);
	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
//	rf_set_ble_crc_adv ();
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_rx_dma((u8*)(blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE),3,0x05);
#if(RF_RX_IRQ_EN)
	core_interrupt_enable();
	plic_interrupt_enable(IRQ15_ZB_RT);
	rf_set_irq_mask(FLD_RF_IRQ_RX);
#endif

#if(RF_RX_IRQ_EN)
	rf_set_rxmode();
	while(1){
		gpio_toggle(GPIO_LED_RED);
		delay_ms(500);
	}

#else

	rf_set_rxmode();
	while(1)
	{
		if(rf_get_irq_status(FLD_RF_IRQ_RX))
		{
			u8 * raw_pkt = (u8 *) (blt_rxbuffer);

			AA_rx_irq_cnt ++;

			if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

			{
				AA_rx_crc_ok_cnt ++;
				gpio_toggle(GPIO_LED_WHITE);

			}

			reg_dma_size(1)=0xffffffff;
			rf_clr_irq_status(FLD_RF_IRQ_RX);
		}

	}
#endif

}

/**
*	@brief	  	This function serves to start Tx of ble_mode.
*	@param[in]	addr   Tx packet address in RAM. Should be 4-byte aligned.
*	@param[in]	tick  Tick value of system timer. It determines when to
*						  	  start ble mode and send packet.
*	@return	 	none
*/
void rf_start_brx  (void* addr, unsigned int tick)
{
	write_reg32 (0x80140a28, 0x0fffffff);
	write_reg32(0x80140a18, tick);
    write_reg8(0x80140a16, read_reg8(0x80140a16) | 0x04);	// Enable cmd_schedule mode
	write_reg8 (0x80140a00, 0x82);						// ble rx
	reg_dma_src_addr(DMA0)=((unsigned int)((unsigned char*)(addr))-0x80000 + 0xc0200000);
}

void ble_brx_rx_test(void){
	rf_drv_ble_init();
	rf_set_ble_chn(TEST_CHN);
	write_reg8(0x140830,0x36);//disable tx timestamp
	REG_ADDR16(0x80140a04) = 80;
	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
//	rf_set_ble_crc_adv ();
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_rx_dma((u8*)(blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE),3,0x05);
	rf_set_tx_dma(2,80);
	u8 rf_data_len = 2;//empty pkt
	u32 rf_tx_dma_len = rf_tx_packet_dma_len(rf_data_len);//4 bytes align
	blt_tx_empty_packet[3] = (rf_tx_dma_len >> 24) & 0xff;
	blt_tx_empty_packet[2] = (rf_tx_dma_len >> 16) & 0xff;
	blt_tx_empty_packet[1] = (rf_tx_dma_len >> 8) & 0xff;
	blt_tx_empty_packet[0] = rf_tx_dma_len & 0xff;

#if(RF_RX_IRQ_EN)
	core_interrupt_enable();
	plic_interrupt_enable(IRQ15_ZB_RT);
	rf_set_irq_mask(FLD_RF_IRQ_RX);
#endif

	brx_mode_flag = 0;
//enter manual RX first, get peer TX device timeStamp

	rf_set_rxmode ();
	while(!brx_mode_flag){
		gpio_toggle(GPIO_LED_RED);
		delay_ms(500);
	}

#if 0
	rf_set_tx_rx_off();  //stop manual RX
#else
	write_reg8 (0x80140a16, 0x29);
	write_reg8 (0x80140828, 0x80);	// rx disable
	write_reg8 (0x80140a02, 0x45);	// reset tx/rx state machine
#endif
	memcpy((void*)(blt_txbuffer + blt_tx_wptr*BLE_LL_BUFF_SIZE),(void *)blt_tx_empty_packet,6);
//manual RX exit, timeStamp capture OK
	AA_rx_match_cnt = 0;
	AA_rx_set_number = 0;
	AA_rx_crc_ok_cnt = 0;
	while(1)
	{
		// 2mS before peer devoce TX
		u8 wptr = read_reg8(0x100500);
		while( (u32)(clock_time() - (bltc_connExpectTime - 2 * 16*1000)) > BIT(30) );
		bltc_tick_1st_rx = 0;
		bltParam_conn_rx_num = 0;
		AA_rx_match_flag = 0;
		rf_set_ble_chn(TEST_CHN);

		memcpy((void*)(blt_txbuffer + (blt_tx_wptr + 1)*BLE_LL_BUFF_SIZE),(void *)blt_tx_empty_packet,6);
		blt_tx_wptr = (blt_tx_wptr + 1) & 3;
		write_reg8(0x100500,wptr+1);//dma tx wptr ++
		rf_start_brx ((void *)blt_txbuffer, clock_time () + 100);   //BRX
		DBG_CHN2_HIGH;
		delay_us(4 * 1000);  //4mS RX duration
		DBG_CHN2_LOW;
		AA_rx_set_number ++;
		AA_rx_miss_cnt = AA_rx_set_number - AA_rx_crc_ok_cnt;
		AA_rx_miss_rate = (AA_rx_miss_cnt * 1000)/AA_rx_set_number;

		//stop CRX
		core_interrupt_disable();
		STOP_RF_STATE_MACHINE;   //stop FSM
		delay_us(100);
		rf_clr_irq_status(FLD_RF_IRQ_RX);
		core_interrupt_enable();
		//timing control
		if(bltc_tick_1st_rx){
			bltc_connExpectTime = bltc_tick_1st_rx + TX_INTERVAL_TICK;
		}
		else{
			bltc_connExpectTime += TX_INTERVAL_TICK;
		}
	}
}
void ble_srx_test(void){

	rf_drv_ble_init();

	rf_set_ble_chn(TEST_CHN);

	rf_set_power_level_index (RF_POWER_INDEX_P2p79dBm);
//	rf_set_ble_crc_adv ();
	rf_access_code_comm(BLE_ACCESS_CODE);
	rf_set_rx_dma((u8*)(blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE),3,0x05);
#if(RF_RX_IRQ_EN)
	core_interrupt_enable();
	plic_interrupt_enable(IRQ15_ZB_RT);
	rf_set_irq_mask(FLD_RF_IRQ_RX);
#endif

#if(RF_RX_IRQ_EN)
	rf_start_srx(0);
	rf_set_rx_settle_time(85);
	while(1){
		gpio_toggle(GPIO_LED_RED);
		delay_ms(500);
	}

#else

	rf_start_srx(0);
	rf_set_rx_settle_time(85);
	while(1)
	{
		if(rf_get_irq_status(FLD_RF_IRQ_RX))
		{
			u8 * raw_pkt = (u8 *) (blt_rxbuffer + blt_rx_wptr * BLE_LL_BUFF_SIZE);
			blt_rx_wptr = ( blt_rx_wptr + 1) & 3;

			AA_rx_irq_cnt ++;
			write_reg8(0x1004f5,0x40);

			if ( RF_BLE_RF_PAYLOAD_LENGTH_OK(raw_pkt) && RF_BLE_RF_PACKET_CRC_OK(raw_pkt) )

			{
				AA_rx_crc_ok_cnt ++;
				gpio_toggle(GPIO_LED_WHITE);

			}

			reg_dma_size(1)=0xffffffff;
			rf_clr_irq_status(FLD_RF_IRQ_RX);
			rf_start_srx(0);
			rf_set_rx_settle_time(85);
		}

	}
#endif
}

void user_init()
{
	uart_io_init();		//GPIO_PA0
	gpio_set_gpio_en(GPIO_LED_BLUE|GPIO_LED_GREEN|GPIO_LED_WHITE|GPIO_LED_RED);
	gpio_set_output_en(GPIO_LED_BLUE|GPIO_LED_GREEN|GPIO_LED_WHITE|GPIO_LED_RED, 1);
	gpio_write(GPIO_LED_BLUE,1);

}

/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
void main_loop (void)
{
#if(TEST_MODE_SELECT == TEST_MODE_MANUAL_TX)
	ble_manual_tx_test();
#elif(TEST_MODE_SELECT == TEST_MODE_MANUAL_RX)
	ble_manual_rx_test();
#elif(TEST_MODE_SELECT == TEST_MODE_STX)
	ble_stx_test();
#elif(TEST_MODE_SELECT == TEST_MODE_SRX)
	ble_srx_test();
#elif(TEST_MODE_SELECT == TEST_MODE_BTX_TX)
	ble_btx_tx_test();
#elif(TEST_MODE_SELECT == TEST_MODE_BRX_RX)
	ble_brx_rx_test();
#endif
}


#endif


