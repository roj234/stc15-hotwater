#include <stc15xx.h>

// HotWater_51 v1.1 by Roj234 2024-2025

#ifndef _HOTWATER_51_H_
#define _HOTWATER_51_H_

#define true 1
#define false 0

// �궨�� ��ʼ

// �Ƿ����ñ��¿�ˮ����ϵͳ
#define WATER_TANK_EXIST true
// ©ˮ���ADCֵ��������ʱ��©ˮ������ֹͣ��ˮ (�缫ͨ�����ڼ�ˮ����)
#define WATER_LEAK_LIMIT false
// �Ƿ���ö�������tick_heater_fp
#define USE_FIXED_POINT_ALG true
// ����
#define DEBUG true

// NTCУ׼
// ע�⣬�������ҵİ��ӵĲ�������ʹ��ʱ��ظĳ�0Ȼ��У׼
#define INTEMP_CAL (-20)
#define OUTTEMP_CAL (0)
#define ROOMTEMP_CAL (0)
#define TANKTEMP_CAL (1)

// �궨�� ����

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long int uint32_t;
typedef char int8_t;
typedef int int16_t;
typedef long int int32_t;

//����������
sbit I_WATER_LOW = P2^6;
sbit I_WATER_MED = P0^1;
sbit I_WATER_HIGH = P0^3;
sbit I_WATER_LEVEL = P5^4;
sbit I_DIRTY_WATER_LEVEL = P5^5;
sbit I_AC_ON = P3^2;

//ģ��������;
sbit I_WATER_LEAK = P1^3;
sbit I_WATER_TEMP_IN = P1^5;
sbit I_WATER_TEMP_OUT = P1^6;
sbit I_AC_VOLTAGE = P1^7;
// CCP0_3 ����������
sbit I_WATER_FLOW = P2^5;

//ԭˮTDS;
sbit TDS1_IN = P1^4;
sbit TDS1_A = P2^7;
sbit TDS1_B = P0^2;

//��ˮTDS;
sbit TDS2_IN = P1^2;
sbit TDS2_A = P0^0;
sbit TDS2_B = P2^4;

//�ڲ�����ӿ�;
sbit O_UV_LIGHT = P2^2;
sbit O_PUMP = P2^1;
sbit O_HEAT_ON = P3^4;
sbit O_HEAT_PWR = P3^6;

//�ⲿ����ӿ� ˿ӡ ������(�����) ��ˮ����Ԥ����(С����);
sbit OEXT_3 = P3^3;
sbit OEXT_4 = P2^0;
sbit OEXT_5 = P3^7;

#define CleanTDS  0
#define WaterLeak 1
#define WaterTDS  2
#define OutTemp   3
#define InTemp    4
#define ACVolt    5
#define VRef      6
uint16_t AdcVal[7];

// v1.1
#if WATER_TANK_EXIST
	// �������Ǽ̵�������̬����Ȧ�������Բ���Ҫ��OEXT_3
	#define O_MAJOR_HEATER OEXT_5
	#define O_MINOR_HEATER OEXT_4
	// ɢ�ȷ��� ���ø�PWM��
	#define O_FAN O_UV_LIGHT
	// TDS1
	#define RoomTemp  2
	// TDS2
	#define TankTemp  0

	#undef CleanTDS
	#undef WaterTDS
#endif

#endif