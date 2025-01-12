
#define max(a, b) ((a) > (b) ? (a) : (b))

struct WaterTankState {
	uint8_t timer, second;
	int16_t maxTemp, tmpSum;
} wts;

#define Tank_Init(boil) {\
	wts.maxTemp = 0; \
	wts.timer = 0; \
	if (MajorTankHeater = (boil)) { \
		MinorTankHeater = 1; \
		TankState = 1; } \
	else { \
		TankState = 4; } }

void Tank_Update() {
	int16_t tmp;
	uint16_t adc;

	// 每秒更新一次
	if (++wts.timer < 20) return;
	wts.timer = 0;

	tmp = Temperature[3];
	switch (TankState) {
		// 1 加热
		case 1:
			adc = AdcVal[TankTemp];
			if (adc > wts.maxTemp || adc < wts.maxTemp - 1) {
				wts.maxTemp = adc;
				wts.second = 0;
			}

			// 水的沸点不一定是100度，所以检测一段时间内温度没有升高
			if (++wts.second < WATER_TANK_NODECR) break;

			MajorTankHeater = 0;
#if USE_NOCL
			//干烧保护 70度以下水温不上升判断没加水
			if (tmp < 700) {
				addError(0x05);
				MinorTankHeater = 0;
				TankState = 0;
				return;
			}

			TankState = 2;
			wts.second = 0;
		break;
		// 2 除氯
		case 2:
			// 根据上面的算法实测，两分钟够了，而且关闭主加热器也够
			if (++wts.second < 120) break;

			MinorTankHeater = 0;
#else
			//干烧保护
			MinorTankHeater = 0;
			if (tmp < 700) {
				addError(0x05);
				TankState = 0;
				return;
			}
#endif
			TankState = 3;
		// 3 冷却
		case 3:
			O_FAN = WindCool;
			if (tmp > max(TankTempSet, 500)+WATER_TANK_ONOFF) break;
			// 关闭风冷，管他开没开
			O_FAN = 0;
			TankState = 4;
		// 4 保温
		case 4:
			tmp -= TankTempSet;
			if (tmp > 0) MinorTankHeater = 0;
			else if (tmp < -WATER_TANK_ONOFF) MinorTankHeater = 1;
	}
}