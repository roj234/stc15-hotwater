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

#if WATER_TANK_EXIST
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
	if (!(ErrMsg&0xC0)) ErrMsg = error;
}

//���Եó���ˮ����ʼ�ο�ֵ
code const uint8_t PUMP_VAL[] = {0, 42, 48, 72, 108, 160, 255, 255};
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

//��ͬˮ���ٶ��µĹ������ֵ
code const uint8_t HOT_MAX_PWR[] = {0, 144, 254};

#if USE_FIXED_POINT_ALG
#include "tick_heater_fp.c"
#else
#include "tick_heater.c"
#endif
#define tickHeater() PwmHeat = Heater_Update(OutTempSet - Temperature[1]);

// v2.0
code const uint8_t PacketSizeById[] = {0, 5, 0, 0, 1, 3};
bit packetReceived, sendStatusReport;
#include "serial.c"

// v1.1
#if WATER_TANK_EXIST
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
#if WATER_TANK_EXIST
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
#if WATER_TANK_EXIST
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

				if (++NoWaterTimer == 40) {
					addError(0x01);
#if WATER_TANK_EXIST
					MajorTankHeater = 0;
					MinorTankHeater = 0;
					TankState = 0;
#endif
				}
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

			// ��Ĥ�ø��������½��������������Ҳ��һ������������¶ȣ��ر��Ǿ�բ�ܵ�ɢ��
			if (PwmPump && (Temperature[0] >= (OutTempSet ? 500 : 800))) addError(0x2);

			if (HotWater) tickHeater();

#if WATER_TANK_EXIST
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
			//sendStatusReport = true;
		}
	}
}

void main() {
	uint8_t tmp, tmp2;
	uint16_t tmp16;

	//Ӳ������
	//WDT_CONTR = 0x24; // �������Ź� 32��Ƶ ���ʱ���ԼΪ0.5s

	//���ȹر����
// v1.1
#if WATER_TANK_EXIST
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

	// ��ʱ��@22.1184Mhz
	// T0_1T = 1; // 0x80 �ر�T0��12��Ƶ
	// T1_1T = 0; // 0x40 ����T1��12��Ƶ
	AUXR = 0x95;  // 0x15 �����������

	//T0 25600Hz / 1T
	TL0 = 0xA0;
	TH0 = 0xFC;

	//T1 125Hz / 12T
	//TL1 = 0x66;
	//TH1 = 0xC6;
	//TMOD |= 0x10; // �ر��Զ�����

	//T2 ���� 115200bps
	SCON = 0xD0;
	T2L = 0xD0;
	T2H = 0xFF;

	// TF0 = 0;
	// TF1 = 0; // �����־λ
	// TR0 = 1; // ����T0
	// TR1 = 0; // ������T1
	// IT0 = 0; // ������ INT0 ������+�½���
	TCON = 0x10;

	Serial_Send();
	Serial_Write(0x01);
	Serial_Flush();

	// �жϿ���: ET0 | ET1 | EX0 | ES | EA
	IE = 0x9b;

	while(1) {
		WDT_CONTR |= 0x10; // ι��

		if (packetReceived) {
			switch(serialBuf[1]) {
				case 0: // Status Request
					sendStatusReport = true;
				goto NoAck;
				case 1: // Drain Water
					// 0 to 95
					OutTempSet = (serialBuf[2] << 8) | serialBuf[3];
					if (OutTempSet > 950 || OutTempSet < 0) {
						OutTempSet = 0;
						uartError = true;
						break;
					}

					WaterVol = 0;
					WaterVolLimit = (serialBuf[5] << 8) | serialBuf[6];
					setWater(serialBuf[4]);
				break;
				case 2: // Clear Error
					if (ErrMsg & 0x40) {
						// ������ (watchdog)
						while (1);
					} else {
						ErrMsg = 0;
					}
				break;
				case 3: // Setting Request
					Serial_Send();
					Serial_Write(0x04);
					Serial_Write(OutTempSet>>8);
					Serial_Write(OutTempSet);
					Serial_Write(WaterVol>>8);
					Serial_Write(WaterVol);
#if WATER_TANK_EXIST
					Serial_Write(TankTempSet>>8);
					Serial_Write(TankTempSet);
					Serial_Write(WindCool);
#endif
					Serial_Flush();
				goto NoAck2;
#if WATER_TANK_EXIST
				case 4: // Tank Init
					Tank_Init(serialBuf[2]);
				break;
				case 5: // Tank Setting
					WindCool = serialBuf[2];
					TankTempSet = (serialBuf[3] << 8) | serialBuf[4];
					if (TankTempSet > 800 || TankTempSet < 0) {
						TankTempSet = 0;
						uartError = true;
					}
				break;
#endif
			}

			tmp = serialBuf[serialPtr];
			tmp2 = serialBuf[1];
			Serial_Send();
			Serial_Write(0x01);
			Serial_Write(tmp2);
			Serial_Write(tmp);
			Serial_Flush();
			NoAck:
			serialPtr = 0;

			NoAck2:
			packetReceived = false;
			ES = true;
		}

		if (sendStatusReport && !serialPtr) {
			ES = false;
			if (!serialPtr) {
				sendStatusReport = false;

				Serial_Send();
				Serial_Write(uartError ? 0x3 : 0x2);
				Serial_Write(ErrMsg);

				Serial_Write(PrevDigitalInput);

				tmp16 = 0;
				if (O_UV_LIGHT) tmp16 |= 1;
				if (OEXT_3) tmp16 |= 2;
				if (OEXT_4) tmp16 |= 4;
				if (OEXT_5) tmp16 |= 8;

				if (PwmPump) tmp16 |= 16;
				if (O_HEAT_ON) tmp16 |= 32;
				Serial_Write(tmp16);

				tmp16 = Temperature[0];
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);

				tmp16 = Temperature[1];
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);

				Serial_Write(WaterVol>>8);
				Serial_Write(WaterVol);

				tmp16 = AdcVal[ACVolt];
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);

				Serial_Write(VoltHz>>8);
				Serial_Write(VoltHz);

#if WATER_TANK_EXIST
				Serial_Write(0x25);
				Serial_Write(TankState);
				tmp16 = Temperature[2];
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);
				tmp16 = Temperature[3];
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);
#endif

#if DEBUG
				Serial_Write(0x14);
#if USE_FIXED_POINT_ALG
				tmp16 = fz.out;
#else
				tmp16 = fz.out * 100;
#endif
				Serial_Write(tmp16>>8);
				Serial_Write(tmp16);

				Serial_Write(PwmPump);
				Serial_Write(PwmHeat);
#endif

				Serial_Flush();
			}

			uartError = false;
			ES = true;
		}

		tmp = 0;
		if (!I_WATER_LOW ) tmp |= 1;
		if (!I_WATER_MED ) tmp |= 2;
		if (!I_WATER_HIGH) tmp |= 4;
		if (!I_WATER_LEVEL) tmp |= 8;
		if (!I_DIRTY_WATER_LEVEL) tmp |= 16;
		if (WaterVolLimit && WaterVol >= WaterVolLimit) {
			tmp |= 32;
			setWater(0);
		}

		if (PrevDigitalInput != tmp) {
			PrevDigitalInput = tmp;
			sendStatusReport = true;
		}

		if (ErrMsg) {
			EnableHotWater = 0;
			HotWater = 0;
			setWater(((ErrMsg&0x7F) == 3) ? 1 : 0);

			if (!(ErrMsg&0x80)) {
				ErrMsg |= 0x80;
				sendStatusReport = true;
			}
		}

		// �ӳٵ���ˮ���ź�������������
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
