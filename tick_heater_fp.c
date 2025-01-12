
//err低于该值使用模糊调节算法 ±5度
#define FUZZY_ERROR_MAX 50
#define BIG_CLIP        150

// v1.2 先增加
#define STABLE_INCREMENT 30
// 50ms/t -> 7000ms
#define STABLE_TIMEOUT 140

code const int8_t FUZZY_I[7] = {-75, -35, -9, 1, 10, 50, 100};
code const int8_t FUZZY_E[9] = {-80, -30, -15, -5, 0, 5, 15, 30, 80};

#define TF(x) (100 * (x))
#define FF(x) ((x) / 100)

struct HeaterState {
	uint8_t timer, cold_timer, stable_timer;
	uint8_t state, theta;
	int16_t ec, out;
} fz;

#define Heater_Init() \
	fz.timer = 0; \
	fz.cold_timer = 0; \
	fz.stable_timer = 0; \
	fz.state = 0; \
	fz.ec = 0; \
	if (AdcVal[OutTemp] > OutTempSet) { \
		fz.theta = 16; \
		fz.out = TF(128); \
	} else { \
		fz.theta = 128; \
		fz.out = 0; \
	}

uint8_t Heater_Update(int16_t error) {
	int16_t absError;
	int16_t total, delta;
	uint8_t i;
	int16_t a, b, c;
	uint8_t val, max;

	if (fz.stable_timer != STABLE_TIMEOUT) error += STABLE_INCREMENT;

	absError = error < 0 ? -error : error;

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

	if (++fz.timer >= 5) {
		fz.timer = 0;

		total = 0;
		delta = 0;
		for (i = 0; i < 7; i++) {
			a = FUZZY_E[i+0];
			c = FUZZY_E[i+2];
			if (error < a || error > c) continue;

			b = FUZZY_E[i+1];
			a = error <= b ? 100 * (error-a) / (b-a) : 100 * (c-error) / (c-b);

			total += a;
			delta += a * FUZZY_I[i];
		}

		fz.out += total ? delta / total : error;

		//对输出值限幅
		if(fz.out < 0) fz.out = 0;
		else if(fz.out > TF(255)) fz.out = TF(255);
	}

	if(absError > FUZZY_ERROR_MAX) {
		if(error > FUZZY_ERROR_MAX) {
			if (fz.state != 1) {
				fz.state = 1;
				if (fz.out < TF(192)) fz.out += TF(fz.theta);
			}

			val = (error > BIG_CLIP || fz.out > TF(255-48)) ? 255 : FF(fz.out) + 48;
		} else {
			if (fz.state != 2) {
				fz.state = 2;
				if (fz.out > TF(96)) fz.out -= TF(fz.theta);
			}

			val = (error < -BIG_CLIP || fz.out < TF(48)) ? 0 : FF(fz.out) - 48;
		}
		
		if (fz.state == 4) fz.state = 5;
	} else {
		if (fz.state == 5) fz.state = 0;
		else if (fz.state & 3) {
			fz.state = 4;
			if (fz.theta) fz.theta >>= 1;
		}

		if (fz.stable_timer != STABLE_TIMEOUT) { ++fz.stable_timer; }

		val = FF(fz.out);
	}

	if (val == 255) {
		if (!(++fz.cold_timer)) {
			if (PumpSet > 2) {
				PumpSet --;
				fz.out = TF(192);
				fz.theta = 32;
			}
		}
	} else {
		if (fz.cold_timer > 0) fz.cold_timer--;
	}

	return val < max ? val : max;
}