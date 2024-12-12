#include <stc15xx.h>

#ifndef _HOTWATER_51_H_
#define _HOTWATER_51_H_

#define true 1
#define false 0
#define null 0

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
sbit TDS1_U = P2^7;
sbit TDS1_L = P0^2;

//净水TDS;
sbit TDS2_IN = P1^2;
sbit TDS2_U = P0^0;
sbit TDS2_L = P2^4;

//内部输出接口;
sbit O_UV_LIGHT = P2^2;
sbit O_PUMP = P2^1;
sbit O_HEAT_ON = P3^4;
sbit O_HEAT_PWR = P3^6;

//外部输出接口 丝印 自吸泵(大电流) 废水阀、预留阀(小电流);
sbit OEXT_3 = P3^3;
sbit OEXT_4 = P2^0;
sbit OEXT_5 = P3^7;

#endif