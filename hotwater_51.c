
#include "hotwater_51.h"
#include "ntc3950_adc.h"
#include "sin_timer.h"

void delay1ms() {
	unsigned char i, j;
	i = 22;
	j = 128;
	do {
		while (--j);
	} while (--i);
}

#define ADC_START 8
#define ADC_END 16
#define ADC_BASE (128/*|64|32*/)
code uint16_t BandGapVolt _at_ 0x0ff9;

//float Adc_Unit;
void AdcUpdate(uint8_t);
uint16_t GetAcVoltage();

#define CleanTDS  0
#define WaterLeak 1
#define WaterTDS  2
#define OutTemp   3
#define InTemp    4
#define ACVolt    5
#define VRef      6
uint16_t AdcVal[7];

//#define WATER_LEAK_LIMIT 0
#define PID_DEBUG

// WaterFlow 1 - 6 测试得出的参考值
code const uint8_t PUMP_VAL[] = {0, 42, 59, 80, 116, 175, 255};

int16_t OutTempSet;
uint8_t PumpSet;

uint8_t WaterFlow, WaterFlow1, WaterFlowTimer, NoWaterTimer;
uint16_t VoltTimer, VoltHz;
uint8_t PwmTimer, PwmPump, PwmHeat;
uint8_t PrevDigitalInput;
bit HotWater, EnableHotWater;

char ErrMsg;

void checkError(void);
void setWater(uint8_t on);
void tickWater(void);

#include "pid_fuzzy_new.c"

bit busy;
void UART_SendByte(uint8_t dat) {
	while(busy);
	ACC = dat;
	TB8 = P;
	busy = 1;
	SBUF = ACC;
}

uint8_t uart_state, uart_id, uart_val;
void UART_Handler() interrupt 4 {
	if (RI) {
		RI = 0;
		switch (uart_state) {
			case 0:
				if (SBUF == 0xAC) {
					uart_state = 2;
				} else if (SBUF == 0x56) {
					uart_state = 1;
				}
			break;
			case 1:
			case 2: 
				uart_id = SBUF;
				uart_state += 2;
			break;
			case 3:
				uart_val = SBUF;
				uart_state = 5;
			break;
		}
	}
	if (TI) {
		TI = 0;
		busy = 0;
	}
}

void AdcUpdate(uint8_t id) {
	EA = 0;
	ADC_CONTR = ADC_BASE | ADC_START | (id+2);
	while(!(ADC_CONTR & ADC_END));
	ADC_CONTR = ADC_BASE;
	AdcVal[id] = (((uint16_t)ADC_RES << 8) | ADC_RESL);
	EA = 1;
}

/*uint16_t GetAcVoltage() {
	return ((float)BandGapVolt / AdcVal[VRef]) * (float) AdcVal[ACVolt];
}*/

//过零中断
void ACZero_Handler() interrupt 0 {
	// 是的，依然只有一半，电路设计傻逼，但是不是我设计的啊（摊手）
	if (!I_AC_ON) return;

	VoltHz = VoltTimer;
	VoltTimer = 0;

	O_HEAT_PWR = 1;
	O_HEAT_ON = HotWater;

	AdcUpdate(ACVolt);
}

void PCA_Handler() interrupt 7 {
	if (CCF0) {
		CCF0 = 0;
		WaterFlow1++;
	}
}

//25.6kHz (PWM 256Level @ 100Hz)
void Timer0_Handler() interrupt 1 {
	O_PUMP = PwmPump && PwmTimer <= PwmPump;

	if (HotWater && PwmHeat) {
		// 0 => Enable, 1 => Disable
		O_HEAT_PWR =
			// TODO Might not be here
			SIN_TIMER[PwmHeat]
			&& VoltTimer != SIN_TIMER[PwmHeat]
			&& VoltTimer != (VoltHz>>1)+SIN_TIMER[PwmHeat];
	}

	//计数器溢出 (4096周期 = 160ms)
	if ((++VoltTimer) >> 12) {
		VoltHz = 0;
		AdcUpdate(ACVolt);
		if (!ErrMsg) ErrMsg = 5;
	}

	//256分频 -> 100Hz
	if (!(++PwmTimer)) {
#ifdef WATER_LEAK_LIMIT
		AdcUpdate(WaterLeak);
#endif
		AdcUpdate(InTemp);
		AdcUpdate(OutTemp);
		checkError();

		AdcVal[InTemp] = AdcInToTemp(AdcVal[InTemp]);
		AdcVal[OutTemp] = AdcOutToTemp(AdcVal[OutTemp]);

		//5分频 -> 20Hz
		if (++WaterFlowTimer == 5) {
			WaterFlow = WaterFlow1;
			WaterFlow1 = 0;
			WaterFlowTimer = 0;

			if (WaterFlow < PumpSet) {
				if (PwmPump < 255) PwmPump++;

				if (++NoWaterTimer == 40) ErrMsg = 6;
			} else {
				if (WaterFlow > PumpSet && PwmPump > 40) PwmPump--;

				if (NoWaterTimer > 0) NoWaterTimer--;
			}


			if (HotWater) tickWater();
		}
	}
}

void main() {
	uint8_t tmp;
	uint16_t tmp16;
	
	//首先关闭输出
	P2 = 0;
	P3 = 3;

	//随后初始化IO
	// PxM0; 开启MOS
	// PxM1: 禁止上拉
	//P0 高阻          3 1
	P0M1 = 10;// 0b    1010
	//P0 推挽           2 0
	P0M0 = 5;//  0b    0101

	//P1 高阻      765432
	//P1M1 = 252;//0b11111100
	//P1 推挽
	//P1M0 = 0;

	//P2 高阻       65
	P2M1 = 96;// 0b 1100000
	//P2 推挽      7  4 210
	P2M0 = 151;//0b10010111

	//P3 高阻       65  2
	P3M1 = 36;// 0b 0100100
	//P3 推挽      76 43
	P3M0 = 216;//0b11011000

	//初始化流量计
	//CCP1_S1 (CCP0_3)
	AUXR1 |= 32;
	//上升沿捕获 中断 CCP0_3
	CCAPM0 = 64 | 32 | 1;
	//使能
	CR = 1;

	//初始化ADC
	// POWER SPEED1 SPEED0
	ADC_CONTR = ADC_BASE;
	CLK_DIV |= 32;
	
	delay1ms();

	ADC_CONTR = ADC_BASE | ADC_START;
	while(!(ADC_CONTR & ADC_END));
	ADC_CONTR = ADC_BASE;

	AdcVal[VRef] = (((uint16_t)ADC_RES << 8) | ADC_RESL);
	P1ASF = 252;

	//初始化计时器
	AUXR = 0x15 | 128; // 关闭12分频，以及串口相关设置

	//T0 25600Hz
	TL0 = 0xA0;
	TH0 = 0xFC;
	TF0 = 0; // 清除标志位
	ET0 = 1; // 允许中断
	TR0 = 1; // 开启T0

	//初始化串口 T2 115200bps at 22.1184Mhz
	SCON = 0xD0;
	T2L = 0xD0;
	T2H = 0xFF;
	ES = 1;

	//初始化过零检测
	//INT0 上升沿+下降沿
	IT0 = 0;
	EX0 = 1;

	//允许中断
	EA = 1;

	while(1) {
		if (uart_state == 4) {
			// GET
			switch (uart_id) {
				case 2: //Get analog input
					UART_SendByte(0x12);
					tmp16 = AdcVal[InTemp];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);

					tmp16 = AdcVal[OutTemp];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);

					tmp16 = OutTempSet;
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);

					UART_SendByte(WaterFlow>>8);
					UART_SendByte(WaterFlow);

					tmp16 = AdcVal[ACVolt];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);
				
					UART_SendByte(VoltHz>>8);
					UART_SendByte(VoltHz);
				break;
				case 0: //Get digital input
					PrevDigitalInput = 0x80;
				break;
				case 1: //Get digital output
					tmp16 = 0;
					if (O_UV_LIGHT) tmp16 |= 1;
					if (OEXT_3) tmp16 |= 2;
					if (OEXT_4) tmp16 |= 4;
					if (OEXT_5) tmp16 |= 8;

					if (O_PUMP) tmp16 |= 16;
					if (O_HEAT_ON) tmp16 |= 32;
					UART_SendByte(0x11);
					UART_SendByte(tmp16);
				break;
				case 3:
					UART_SendByte(0x80 | ErrMsg);
				break;
#ifdef PID_DEBUG
				case 4:
					UART_SendByte(0x13);
					tmp16 = _pid.out * 100;
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);
				
					UART_SendByte(PwmPump);
					UART_SendByte(PwmHeat);
				break;
#endif
			}
			uart_state = 0;
		} else if (uart_state == 5) {
			// SET
			switch (uart_id) {
				case 2: uart_val -= 0xA0; setWater(uart_val > 6 ? 0 : uart_val); break;
				case 1: OutTempSet = --uart_val > 108 ? 0 : 360+(uart_val*5); break;
				case 0: ErrMsg = 0; break;
				case 3: OEXT_3 = uart_val; break;
				case 4: OEXT_4 = uart_val; break;
				case 5: OEXT_5 = uart_val; break;
				case 6: O_UV_LIGHT = uart_val; break;
				//case 7: PumpSet = uart_val; break;
			}
			uart_state = 0;
		}

		tmp = 0;
		if (!I_WATER_LOW ) tmp |= 1;
		if (!I_WATER_MED ) tmp |= 2;
		if (!I_WATER_HIGH) tmp |= 4;

		if (!I_WATER_LEVEL) tmp |= 8;
		if (!I_DIRTY_WATER_LEVEL) tmp |= 16;

		if (PrevDigitalInput != tmp) {
			PrevDigitalInput = tmp;
			UART_SendByte(0x10);
			UART_SendByte(tmp);
		}

		if (ErrMsg) {
			EnableHotWater = 0;
			HotWater = 0;
			setWater(((ErrMsg&0x7F) == 3) ? 1 : 0);

			if (!(ErrMsg&0x80)) {
				ErrMsg |= 0x80;
				UART_SendByte(ErrMsg);
			}
		}

		if (EnableHotWater && WaterFlow) {
			EnableHotWater = 0;

			PwmHeat = 0;
			PID_Init();
			HotWater = 1;
		}
	}
}

void checkError() {
	if (AdcVal[InTemp] > ADC_IN_HOT) {
		ErrMsg = 1;
	} else if (AdcVal[InTemp] < ADC_IN_COLD) {
		ErrMsg = 2;
	}
	if (AdcVal[OutTemp] > ADC_OUT_HOT) {
		ErrMsg = 3;
	} else if (AdcVal[OutTemp] < ADC_OUT_COLD) {
		ErrMsg = 4;
	}
#ifdef WATER_LEAK_LIMIT
	if (AdcVal[WaterLeak] > WATER_LEAK_LIMIT) {
		ErrMsg = 7;
	}
#endif
}

void setWater(uint8_t on) {
	int16_t dt;
	if (PumpSet == on) return;

	if (on && OutTempSet) {
		dt = OutTempSet - AdcVal[InTemp];
		if (dt > 300 && on == 6) on = 5;
		if (dt > 340 && on == 5) on = 4;
		if (dt > 420 && on == 4) on = 3;
		if (dt > 560 && on == 3) on = 2;

		EnableHotWater = 1;
	} else {
		EnableHotWater = 0;
		HotWater = 0;
	}

	PumpSet = on;
	PwmPump = PUMP_VAL[on];
}

void tickWater() {
	PwmHeat = PID_Update(OutTempSet - (int16_t)AdcVal[OutTemp]);
}
