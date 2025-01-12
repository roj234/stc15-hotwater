
// 定义接收缓冲区和指针
uint8_t serialBuf[32];
uint8_t serialPtr;
bit uartError;

// 函数声明
void ProcessDataPacket(void);

uint8_t CRC8(uint8_t length) {
    uint8_t i = 0, j;
    uint8_t crc = 0;
    while (i < length) {
        crc ^= serialBuf[i++];

        j = 7;
        do {
          if (crc & 0x80) crc = (crc << 1) ^ 0x7;
          else crc <<= 1;
        } while (--j);
    }
    return crc;
}

#define Serial_Send() serialPtr = 1;

#define Serial_Write(b) serialBuf[serialPtr++] = (b);
void Serial_Flush() {
  uint8_t i = 0;

  serialBuf[0] = serialPtr - 2;
  serialBuf[serialPtr] = CRC8(serialPtr);
  while (i <= serialPtr) {
    ACC = serialBuf[i++];
    TB8 = P;
    SBUF = ACC;
    while(!TI);
    TI = 0;
  }
  serialPtr = 0;
}

void Timer1_Handler() interrupt 3 {
    uartError = true;
    sendStatusReport = true;
    serialPtr = 0;
    TR1 = 0;
}
void UART_Handler() interrupt 4 {
	uint8_t packetId;
  TR1 = 0;

  serialBuf[serialPtr] = SBUF;
  RI = 0;
  // 检查是否接收到完整的数据包
  // length
  // type
  // data
  // crc
  if (serialPtr == serialBuf[0] + 2 || serialPtr == sizeof(serialBuf)-1) {
    if (CRC8(serialPtr) == serialBuf[serialPtr]) {
      packetId = serialBuf[1];
      if (packetId < sizeof(PacketSizeById) && serialBuf[0] == PacketSizeById[packetId]) {
        packetReceived = true;
        ES = 0;
        return;
      }
    }

    serialPtr = 0;
    uartError = true;
  } else {
    serialPtr++;

    //T1 125Hz / 12T
    TL1 = 0x66;
    TH1 = 0xC6;
    TR1 = 1;
  }
}
