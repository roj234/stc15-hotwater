
// ±1度
#define WATER_TANK_ONOFF 10
// 多少秒内温度不再升高判断为水开
#define WATER_TANK_NODECR 45

struct WaterTankState {
	uint8_t timer, second;
	int16_t maxTemp, tmpSum;
} wts;

#define Tank_Init(boil) {\
	wts.maxTemp = 0; \
	wts.timer = 0; \
	if (boil) { \
	MajorTankHeater = 1; \
	MinorTankHeater = 1; \
	TankState = 1; } \
	else { \
	TankState = 3; } }

void Tank_Update() {
	int16_t tmp = Temperature[3];

	// 每秒更新一次
	if (++wts.timer < 20) return;
	wts.timer = 0;

	switch (TankState) {
		// 1 加热 (主副加热器打开)
		case 1:
			if (tmp > wts.maxTemp) {
				wts.maxTemp = tmp;
				wts.second = 0;
			}

			// 水的沸点不一定是100度，所以检测一段时间内温度没有升高
			if (++wts.second < WATER_TANK_NODECR) break;

			MinorTankHeater = 0;
			//干烧保护 70度以下水温不上升判断没加水
			if (tmp < 700) {
				addError(0x07);
				MajorTankHeater = 0;
				TankState = 0;
				return;
			}

			TankState = 2;
			wts.second = 0;
		break;
		// 2 除氯 (dT=0后，副加热器关闭)
		case 2:
			// 沸腾3分钟除氯
			if (++wts.second < 180) break;

			MajorTankHeater = 0;
			TankState = 3;
		// 3 冷却 (3分钟后，副加热器关闭)
		case 3:
			O_FAN = WindCool;
			if (tmp-TankTempSet > WATER_TANK_ONOFF) break;
			// 关闭风冷，管他开没开
			O_FAN = 0;
			TankState = 4;
		// 4 保温 (±1℃的开关控制)
		case 4:
			tmp -= TankTempSet;
			if (tmp > WATER_TANK_ONOFF) MinorTankHeater = 0;
			else if (tmp < -WATER_TANK_ONOFF) MinorTankHeater = 1;
	}
}