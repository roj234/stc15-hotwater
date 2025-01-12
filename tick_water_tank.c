
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

	// ÿ�����һ��
	if (++wts.timer < 20) return;
	wts.timer = 0;

	tmp = Temperature[3];
	switch (TankState) {
		// 1 ����
		case 1:
			adc = AdcVal[TankTemp];
			if (adc > wts.maxTemp || adc < wts.maxTemp - 1) {
				wts.maxTemp = adc;
				wts.second = 0;
			}

			// ˮ�ķе㲻һ����100�ȣ����Լ��һ��ʱ�����¶�û������
			if (++wts.second < WATER_TANK_NODECR) break;

			MajorTankHeater = 0;
#if USE_NOCL
			//���ձ��� 70������ˮ�²������ж�û��ˮ
			if (tmp < 700) {
				addError(0x05);
				MinorTankHeater = 0;
				TankState = 0;
				return;
			}

			TankState = 2;
			wts.second = 0;
		break;
		// 2 ����
		case 2:
			// ����������㷨ʵ�⣬�����ӹ��ˣ����ҹر���������Ҳ��
			if (++wts.second < 120) break;

			MinorTankHeater = 0;
#else
			//���ձ���
			MinorTankHeater = 0;
			if (tmp < 700) {
				addError(0x05);
				TankState = 0;
				return;
			}
#endif
			TankState = 3;
		// 3 ��ȴ
		case 3:
			O_FAN = WindCool;
			if (tmp > max(TankTempSet, 500)+WATER_TANK_ONOFF) break;
			// �رշ��䣬������û��
			O_FAN = 0;
			TankState = 4;
		// 4 ����
		case 4:
			tmp -= TankTempSet;
			if (tmp > 0) MinorTankHeater = 0;
			else if (tmp < -WATER_TANK_ONOFF) MinorTankHeater = 1;
	}
}