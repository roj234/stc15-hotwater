
//err低于该值使用模糊调节算法 ±5度
#define FUZZY_ERROR_MAX 50
#define BIG_CLIP        150

code const float FUZZY_I[7] = {-0.75, -0.35, -0.09, 0.01, 0.1, 0.5, 1};
code const int8_t FUZZY_E[9] = {-80, -30, -15, -5, 0, 5, 15, 30, 80};
//不同水流速度下的功率最大值
code const uint8_t HOT_MAX_PWR[] = {0, 144, 254};

struct HeaterState {
	uint8_t timer, cold_timer;
	uint8_t state, theta;
	int8_t ec;
	float out;
	//int16_t prevIn;
} fz;

#define Heater_Init() \
	fz.timer = 0; \
	fz.cold_timer = 0; \
	fz.state = 0; \
	fz.ec = 0; \
	/*fz.prevIn = AdcVal[InTemp]; */ \
	if (AdcVal[OutTemp] > OutTempSet) { \
		fz.theta = 16; \
		fz.out = 128; \
	} else { \
		fz.theta = 128; \
		fz.out = 0; \
	}

uint8_t Heater_Update(int16_t error) {
	int16_t absError = error < 0 ? -error : error;
	float total, delta;
	uint8_t i;
	float a, b, c;
	uint8_t val, max;

	delta = 5 * (error - fz.ec);
	fz.ec = error;

	if (WaterFlow < sizeof(HOT_MAX_PWR) && (!WaterFlow || Temperature[1/*OutTemp*/] > 600)) {
		max = HOT_MAX_PWR[WaterFlow];
	} else {
		max = 255;
	}

	if (max + delta > 255) max = 255;
	else if (max - delta < 0) max = 0;
	else max += delta;

	/*if (AdcVal[InTemp] > fz.prevIn + 50) {
		
	} else if (AdcVal[InTemp] < fz.prevIn - 50) {
		
	}*/

	if (++fz.timer >= 5) {
		fz.timer = 0;

		total = 0;
		delta = 0;
		for (i = 0; i < 7; i++) {
			a = FUZZY_E[i+0];
			c = FUZZY_E[i+2];
			if (error < a || error > c) continue;

			b = FUZZY_E[i+1];
			a = error <= b ? (error-a) / (b-a) : (c-error) / (c-b);

			total += a;
			delta += a * FUZZY_I[i];
		}

		if (total != 0) {
			fz.out += delta / total;
		} else {
			fz.out += 0.01 * error;
		}

		//对输出值限幅
		if(fz.out < 0) fz.out = 0;
		else if(fz.out > 255) fz.out = 255;
	}

	if(absError > FUZZY_ERROR_MAX) {
		if(error > FUZZY_ERROR_MAX) {
			if (fz.state != 1) {
				fz.state = 1;
				if (fz.out < 192) fz.out += fz.theta;
			}

			val = (error > BIG_CLIP || fz.out > (255-48)) ? 255 : fz.out + 48;
		} else {
			if (fz.state != 2) {
				fz.state = 2;
				if (fz.out > 96) fz.out -= fz.theta;
			}

			val = (error < -BIG_CLIP || fz.out < (48)) ? 0 : fz.out - 48;
		}
		
		if (fz.state == 4) fz.state = 5;
	} else {
		if (fz.state == 5) fz.state = 0;
		else if (fz.state & 3) {
			fz.state = 4;
			if (fz.theta) fz.theta >>= 1;
		}

		val = fz.out;
	}

	if (val == 255) {
		if (!(++fz.cold_timer)) {
			//fz.cold_timer = 0;

			if (PumpSet > 2) {
				PumpSet --;
				fz.out = 192;
				fz.theta = 32;
			}
		}
	} else {
		if (fz.cold_timer > 0) fz.cold_timer--;
	}

	return val < max ? val : max;
}