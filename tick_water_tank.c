
// ��1��
#define WATER_TANK_ONOFF 10
// ���������¶Ȳ��������ж�Ϊˮ��
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

	// ÿ�����һ��
	if (++wts.timer < 20) return;
	wts.timer = 0;

	switch (TankState) {
		// 1 ���� (������������)
		case 1:
			if (tmp > wts.maxTemp) {
				wts.maxTemp = tmp;
				wts.second = 0;
			}

			// ˮ�ķе㲻һ����100�ȣ����Լ��һ��ʱ�����¶�û������
			if (++wts.second < WATER_TANK_NODECR) break;

			MinorTankHeater = 0;
			//���ձ��� 70������ˮ�²������ж�û��ˮ
			if (tmp < 700) {
				addError(0x07);
				MajorTankHeater = 0;
				TankState = 0;
				return;
			}

			TankState = 2;
			wts.second = 0;
		break;
		// 2 ���� (dT=0�󣬸��������ر�)
		case 2:
			// ����3���ӳ���
			if (++wts.second < 180) break;

			MajorTankHeater = 0;
			TankState = 3;
		// 3 ��ȴ (3���Ӻ󣬸��������ر�)
		case 3:
			O_FAN = WindCool;
			if (tmp-TankTempSet > WATER_TANK_ONOFF) break;
			// �رշ��䣬������û��
			O_FAN = 0;
			TankState = 4;
		// 4 ���� (��1��Ŀ��ؿ���)
		case 4:
			tmp -= TankTempSet;
			if (tmp > WATER_TANK_ONOFF) MinorTankHeater = 0;
			else if (tmp < -WATER_TANK_ONOFF) MinorTankHeater = 1;
	}
}