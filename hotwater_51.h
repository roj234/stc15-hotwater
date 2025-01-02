#include <stc15xx.h>

// HotWater_51 v1.1 by Roj234 2024-2025

#ifndef _HOTWATER_51_H_
#define _HOTWATER_51_H_

#define true 1
#define false 0

// 宏定义 开始

// 是否启用保温开水壶子系统
#define WATER_TANK_EXIST true
// 漏水检测ADC值超过多少时报漏水错误并且停止放水 (电极通常放在集水盘上)
#define WATER_LEAK_LIMIT false
// 是否采用定点运算tick_heater_fp
#define USE_FIXED_POINT_ALG true
// 调试
#define DEBUG true

// NTC校准
// 注意，这里是我的板子的参数，你使用时务必改成0然后校准
#define INTEMP_CAL (-20)
#define OUTTEMP_CAL (0)
#define ROOMTEMP_CAL (0)
#define TANKTEMP_CAL (1)

// 宏定义 结束

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long int uint32_t;
typedef char int8_t;
typedef int int16_t;
typedef long int int32_t;

//开关量输入
sbit I_WATER_LOW = P2^6;
sbit I_WATER_MED = P0^1;
sbit I_WATER_HIGH = P0^3;
sbit I_WATER_LEVEL = P5^4;
sbit I_DIRTY_WATER_LEVEL = P5^5;
sbit I_AC_ON = P3^2;

//模拟量输入;
sbit I_WATER_LEAK = P1^3;
sbit I_WATER_TEMP_IN = P1^5;
sbit I_WATER_TEMP_OUT = P1^6;
sbit I_AC_VOLTAGE = P1^7;
// CCP0_3 霍尔传感器
sbit I_WATER_FLOW = P2^5;

//原水TDS;
sbit TDS1_IN = P1^4;
sbit TDS1_A = P2^7;
sbit TDS1_B = P0^2;

//净水TDS;
sbit TDS2_IN = P1^2;
sbit TDS2_A = P0^0;
sbit TDS2_B = P2^4;

//内部输出接口;
sbit O_UV_LIGHT = P2^2;
sbit O_PUMP = P2^1;
sbit O_HEAT_ON = P3^4;
sbit O_HEAT_PWR = P3^6;

//外部输出接口 丝印 自吸泵(大电流) 废水阀、预留阀(小电流);
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
	// 两个都是继电器（固态或线圈），所以不需要用OEXT_3
	#define O_MAJOR_HEATER OEXT_5
	#define O_MINOR_HEATER OEXT_4
	// 散热风扇 懒得搞PWM了
	#define O_FAN O_UV_LIGHT
	// TDS1
	#define RoomTemp  2
	// TDS2
	#define TankTemp  0

	#undef CleanTDS
	#undef WaterTDS
#endif

#endif