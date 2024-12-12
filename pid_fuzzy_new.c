
//err低于该值使用模糊调节算法 ±5度
#define FUZZY_ERROR_MAX 50

code const float FUZZY_I[7] = {-0.75, -0.35, -0.09, 0.01, 0.1, 0.5, 1};
code const int8_t FUZZY_E[9] = {-80, -30, -15, -5, 0, 5, 15, 30, 80};
//不同水流速度下的功率最大值
code const uint8_t HOT_MAX_PWR[] = {0, 144, 254};

struct PID {
	uint8_t timer, cold_timer;
	uint8_t state, theta;
	int8_t ec;
	float out;
} _pid;

#define PID_Init() \
	_pid.timer = 0; \
	_pid.cold_timer = 0; \
	_pid.state = 0; \
	_pid.ec = 0; \
	if (AdcVal[OutTemp] > OutTempSet) { \
		_pid.theta = 16; \
		_pid.out = 128; \
	} else { \
		_pid.theta = 128; \
		_pid.out = 0; \
	}

uint8_t PID_Update(int16_t error) {
	int16_t absError = error < 0 ? -error : error;
	float total, delta;
	uint8_t i;
	float a, b, c;
	uint8_t val, max;

	delta = 5 * (error - _pid.ec);
	_pid.ec = error;

	if (WaterFlow < sizeof(HOT_MAX_PWR) && (!WaterFlow || AdcVal[OutTemp] > 600)) {
		max = HOT_MAX_PWR[WaterFlow];
	} else {
		max = 255;
	}

	if (max + delta > 255) max = 255;
	else if (max - delta < 0) max = 0;
	else max += delta;


	if (++_pid.timer >= 5) {
		_pid.timer = 0;

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
			_pid.out += delta / total;
		} else {
			_pid.out += 0.01 * error;
		}

		//对输出值限幅
		if(_pid.out < 0) _pid.out = 0;
		else if(_pid.out > 255) _pid.out = 255;
	}

	if(absError > FUZZY_ERROR_MAX) {
		if(error > FUZZY_ERROR_MAX) {
			if (_pid.state != 1) {
				_pid.state = 1;
				if (_pid.out < 192) _pid.out += _pid.theta;
			}

			val = _pid.out > (255-48) ? 255 : _pid.out + 48;
		} else {
			if (_pid.state != 2) {
				_pid.state = 2;
				if (_pid.out > 96) _pid.out -= _pid.theta;
			}

			val = _pid.out < 48 ? 0 : _pid.out - 48;
		}
		
		if (_pid.state == 4) _pid.state = 5;
	} else {
		if (_pid.state == 5) _pid.state = 0;
		else if (_pid.state & 3) {
			_pid.state = 4;
			if (_pid.theta) _pid.theta >>= 1;
		}

		val = _pid.out;
	}

	if (val == 255) {
		if (!(++_pid.cold_timer)) {
			_pid.cold_timer = 0;

			if (PumpSet > 2) {
				PumpSet --;
				_pid.out = 192;
				_pid.theta = 32;
			}
		}
	} else {
		if (_pid.cold_timer > 0) _pid.cold_timer--;
	}

	return val < max ? val : max;
}