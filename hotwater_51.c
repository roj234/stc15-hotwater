// HotWater_51 v1.1 by Roj234 2024-2025

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
uint16_t AdcVal[];

#ifdef WATER_TANK_EXIST
int16_t Temperature[4];
#else
int16_t Temperature[2];
#endif

//����״̬
uint8_t PrevDigitalInput;
//PWM����
uint8_t PwmTimer, PwmPump, PwmHeat;

uint8_t ErrMsg;
void addError(uint8_t error) {
	uint8_t hasError = ErrMsg&0xC0;
	if (!hasError && !((ErrMsg - error)&0x3F))
		ErrMsg = error;
}

//���Եó���ˮ����ʼ�ο�ֵ
code const uint8_t PUMP_VAL[] = {0, 42, 59, 80, 116, 175, 255};
//ˮ���趨
uint8_t PumpSet;
void setWater(uint8_t waterFlow);
//ˮ�����
uint8_t WaterFlow, WFPCA, WaterFlowTimer, NoWaterTimer;
//������ˮ
uint16_t WaterVol, WaterVolLimit;

//����¶��趨
int16_t OutTempSet;
//�ӳ��������ȣ�ֱ��ˮ���ź�
bit HotWater, EnableHotWater;
//��ѹ��Ƶ��
uint16_t VoltTimer, VoltHz;

// ע������PID
#include "tick_heater.c"
#define tickHeater() PwmHeat = Heater_Update(OutTempSet - (int16_t)AdcVal[OutTemp]);

// v1.1
#ifdef WATER_TANK_EXIST
	//����ˮ��Packet_Water_Boiled ֱ�ӽ���״̬3
	//����ˮ��/

	//�����¶��趨
	int16_t TankTempSet;
	//ˮ��״̬
	// 0 �ر� (�ϵ�)
	// 1 ���� (������������)
	// 2 ���� (dT=0�󣬸��������ر�)
	// 3 ��ȴ (3���Ӻ����������رգ�ͬʱ����ɢ�ȷ���)
	// 4 ���� (��1��Ŀ��ؿ���)
	uint8_t TankState;
	#define Tank_Can_Drain(state) (state > 2)
	//������״̬(�����л�)
	bit MajorTankHeater, MinorTankHeater;
	//����
	bit WindCool;

	#include "tick_water_tank.c"
	#define tickTank Tank_Update
#endif

void UART_SendByte(uint8_t dat) {
	EA = 0;
	ACC = dat;
	TB8 = P;
	SBUF = dat;
	while(!TI);
	TI = 0;
	EA = 1;
}

uint8_t uart_state, uart_id, uart_val;
void UART_Handler() interrupt 4 {
	RI = 0;
	switch (uart_state) {
		case 0:
			if (SBUF == 0xAC) {
				uart_state = 2;
			} else if (SBUF == 0x56) {
				uart_state = 1;
			}
		break;
		case 1, 2: 
			uart_id = SBUF;
			uart_state += 2;
		break;
		case 3:
			uart_val = SBUF;
			uart_state = 5;
		break;
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

bit FlagAdcSumReset;
void AdcUpdateSum(uint8_t id) {
	ADC_CONTR = ADC_BASE | ADC_START | (id+2);
	while(!(ADC_CONTR & ADC_END));
	if (FlagAdcSumReset) AdcVal[id] = 0;
	AdcVal[id] += (((uint16_t)ADC_RES << 8) | ADC_RESL);
}

/*uint16_t GetAcVoltage() {
	return ((float)BandGapVolt / AdcVal[VRef]) * (float) AdcVal[ACVolt];
}*/

//�����ж�
void ACZero_Handler() interrupt 0 {
	// �ǵģ���Ȼֻ��һ�룬��·���ɵ�ƣ����ǲ�������Ƶİ���̯�֣�
	if (!I_AC_ON) return;

	VoltHz = VoltTimer;
	VoltTimer = 0;

	O_HEAT_PWR = 1;
	O_HEAT_ON = HotWater;
#ifdef WATER_TANK_EXIST
	O_MAJOR_HEATER = MajorTankHeater;
	O_MINOR_HEATER = MinorTankHeater;
#endif

	AdcUpdate(ACVolt);
}

void PCA_Handler() interrupt 7 {
	if (CCF0) {
		CCF0 = 0;
		WFPCA++;
	}
}

//25.6kHz (PWM 256Level @ 100Hz)
void Timer0_Handler() interrupt 1 {
	O_PUMP = PwmPump && PwmTimer <= PwmPump;

	if (HotWater && PwmHeat) {
		// 0 => Enable, 1 => Disable
		O_HEAT_PWR =
			SIN_TIMER[PwmHeat]
			&& VoltTimer != SIN_TIMER[PwmHeat]
			&& VoltTimer != (VoltHz>>1)+SIN_TIMER[PwmHeat];
	}

	//��������� (4096���� = 160ms)
	if ((++VoltTimer) >> 12) {
		VoltHz = 0;
		AdcVal[ACVolt] = 0;
		addError(O_HEAT_ON ? 0x41 : 0x40);
	}

	//256��Ƶ -> 100Hz
	if (!(++PwmTimer)) {
#if WATER_LEAK_LIMIT
		AdcUpdate(WaterLeak);
		if (AdcVal[WaterLeak] > WATER_LEAK_LIMIT) addError(0x4);
#endif

		//5��Ƶ -> 20Hz
		#define WATERFLOW_DIV 5

		//���������ȡƽ��ֵ
		EA = 0;
		AdcUpdateSum(InTemp);
		AdcUpdateSum(OutTemp);
#ifdef WATER_TANK_EXIST
		AdcUpdateSum(RoomTemp);
		AdcUpdateSum(TankTemp);
#endif
		ADC_CONTR = ADC_BASE;
		FlagAdcSumReset = 0;
		EA = 1;

		if (++WaterFlowTimer == WATERFLOW_DIV) {
			WaterFlow = WFPCA;
			WFPCA = 0;
			WaterFlowTimer = 0;

			if (WaterFlow < PumpSet) {
				if (PwmPump < 255) PwmPump++;

				if (++NoWaterTimer == 40) addError(0x01);
			} else {
				if (WaterFlow > PumpSet && PwmPump > 40) PwmPump--;

				if (NoWaterTimer > 0) NoWaterTimer--;
			}

			WaterVol += WaterFlow;

			AdcVal[InTemp] /= WATERFLOW_DIV;
			AdcVal[OutTemp] /= WATERFLOW_DIV;

			if (AdcVal[InTemp] < ADC_IN_LOOKUP_MIN) addError(0x42);
			if (AdcVal[InTemp] >= ADC_IN_LOOKUP_MAX) addError(0x43);
			if (AdcVal[OutTemp] < ADC_OUT_LOOKUP_MIN) addError(0x44);
			if (AdcVal[OutTemp] >= ADC_OUT_HOT) {
				addError(AdcVal[OutTemp] >= ADC_OUT_LOOKUP_MAX ? 0x45 : 0x3);
			}

			Temperature[0] = AdcInToTemp(AdcVal[InTemp]+INTEMP_CAL);
			Temperature[1] = AdcOutToTemp(AdcVal[OutTemp]+OUTTEMP_CAL);

			if (OutTempSet && AdcVal[InTemp] >= 500) {
				addError(0x2);
				OutTempSet = 0;
			}

			if (HotWater) tickHeater();

#ifdef WATER_TANK_EXIST
			AdcVal[RoomTemp] /= WATERFLOW_DIV;
			AdcVal[TankTemp] /= WATERFLOW_DIV;

			if (AdcVal[RoomTemp] < ADC_IN_LOOKUP_MIN) addError(0x46);
			if (AdcVal[RoomTemp] >= ADC_IN_LOOKUP_MAX) addError(0x47);
			if (AdcVal[TankTemp] < ADC_OUT_LOOKUP_MIN) addError(0x48);
			if (AdcVal[TankTemp] >= ADC_OUT_LOOKUP_MAX) addError(0x49);

			Temperature[2] = AdcInToTemp(AdcVal[RoomTemp]+ROOMTEMP_CAL);
			Temperature[3] = AdcOutToTemp(AdcVal[TankTemp]+TANKTEMP_CAL);

			if (TankState) Tank_Update();
#endif

			FlagAdcSumReset = 1;
		}
	}
}

void main() {
	uint8_t tmp;
	uint16_t tmp16;

	//���ȹر����
// v1.1
#ifdef WATER_TANK_EXIST
	//ԭˮTDS; ��ǰ��; ����
	//��ˮTDS2 ��ǰ��; ˮ���¶�
	//TDS2_B�ṩADC100%�Ĳο���ѹ
	P2 = 16;
#else
	P2 = 0;
#endif
	P3 = 3;

	//����ʼ��IO
	// PxM0; ����MOS
	// PxM1: ��ֹ����
	//P0 ����          3 1
	P0M1 = 11;// 0b    1011
	//P0 ����           2 0
	P0M0 = 4;//  0b    0100

	//P1 ����      765432
	//P1M1 = 252;//0b11111100
	//P1 ����
	//P1M0 = 0;

	//P2 ����       65
	P2M1 = 224;//0b11100000
	//P2 ����      7  4 210
	P2M0 = 23;// 0b00010111

	//P3 ����       65  2
	P3M1 = 36;// 0b 0100100
	//P3 ����      76 43
	P3M0 = 216;//0b11011000

	//��ʼ��������
	//CCP1_S1 (CCP0_3)
	AUXR1 |= 32;
	//�����ز��� �ж� CCP0_3
	CCAPM0 = 64 | 32 | 1;
	//ʹ��
	CR = 1;

	//��ʼ��ADC
	// POWER SPEED1 SPEED0
	ADC_CONTR = ADC_BASE;
	CLK_DIV |= 32;
	
	delay1ms();

	ADC_CONTR = ADC_BASE | ADC_START;
	while(!(ADC_CONTR & ADC_END));
	ADC_CONTR = ADC_BASE;

	AdcVal[VRef] = (((uint16_t)ADC_RES << 8) | ADC_RESL);
	P1ASF = 252;

	//��ʼ����ʱ��
	AUXR = 0x15 | 128; // �ر�12��Ƶ���Լ������������

	//T0 25600Hz
	TL0 = 0xA0;
	TH0 = 0xFC;
	TF0 = 0; // �����־λ
	ET0 = 1; // �����ж�
	TR0 = 1; // ����T0

	//��ʼ������ T2 115200bps at 22.1184Mhz
	SCON = 0xD0;
	T2L = 0xD0;
	T2H = 0xFF;
	ES = 1;

	//��ʼ��������
	//INT0 ������+�½���
	IT0 = 0;
	EX0 = 1;

	//�����ж�
	EA = 1;

	while(1) {
		if (uart_state == 4) {
			// GET
			switch (uart_id) {
				case 2: //Get analog input
					UART_SendByte(0x12);
					tmp16 = Temperature[0];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);

					tmp16 = Temperature[1];
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
				case 4:
					UART_SendByte(0x14);
					UART_SendByte(WaterVol>>8);
					UART_SendByte(WaterVol);
				break;
#ifdef WATER_TANK_EXIST
				case 5:
					UART_SendByte(0x15);

					UART_SendByte(TankState);

					UART_SendByte(TankTempSet>>8);
					UART_SendByte(TankTempSet);

					tmp16 = Temperature[2];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);

					tmp16 = Temperature[3];
					UART_SendByte(tmp16>>8);
					UART_SendByte(tmp16);
				break;
#endif
#ifdef PID_DEBUG
				case 6:
					UART_SendByte(0x13);
					tmp16 = fz.out * 100;
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
				case 1: OutTempSet = --uart_val > 118 ? 0 : 360+(uart_val*5); break;
				case 2:
					uart_val -= 0xA0;
#ifdef WATER_TANK_EXIST
					if (!Tank_Can_Drain(TankState)) addError(0x2);
					else
#endif
					setWater(uart_val > 6 ? 0 : uart_val);
				break;
				case 0: ErrMsg = 0; break;

				case 3: WaterVol = 0; WaterVolLimit = uart_val; break;
				case 4: WaterVolLimit |= (uint16_t)uart_val << 8; break;

#ifdef WATER_TANK_EXIST
				case 5: Tank_Init(uart_val == 0xBB); break;
				case 6: TankTempSet = --uart_val > 88 ? 0 : 360+(uart_val*5); break;
				case 7: WindCool = uart_val; break;
#endif
			}
			uart_state = 0;
		}

		tmp = 0;
		if (!I_WATER_LOW ) tmp |= 1;
		if (!I_WATER_MED ) tmp |= 2;
		if (!I_WATER_HIGH) tmp |= 4;

		if (!I_WATER_LEVEL) tmp |= 8;
		if (!I_DIRTY_WATER_LEVEL) tmp |= 16;

		if (WaterVolLimit && WaterVol >= WaterVolLimit) {
			tmp |= 32;
			WaterVol = 0;
			setWater(0);
		}

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
			Heater_Init();
			HotWater = 1;
		}
	}
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
