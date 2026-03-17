#include <Arduino.h>
#include <Wire.h>
#include "tb_config.h"

// ==================== BQ76952 REGISTER ADDRESSES ====================
#define REG_VCELL_1     0x14
#define REG_VCELL_16    0x32
#define REG_VSTACK      0x34
#define REG_VPACK       0x36
#define REG_CC2_CUR     0x3A
#define REG_SUBCMD_LOW  0x3E
#define REG_RESP_START  0x40
#define REG_RESP_CHKSUM 0x60
#define REG_INT_TEMP    0x68
#define REG_TS1         0x70
#define REG_TS2         0x72
#define REG_TS3         0x74
#define REG_FET_STAT    0x7F

// Subcommands
#define SUBCMD_RESET          0x0012
#define SUBCMD_RESET_PASSQ    0x0082
#define SUBCMD_CB_ACTIVE      0x0083
#define SUBCMD_CFGUPDATE      0x0090
#define SUBCMD_EXIT_CFGUPDATE 0x0092
#define SUBCMD_DSG_OFF        0x0093
#define SUBCMD_CHG_OFF        0x0094
#define SUBCMD_ALL_FETS_OFF   0x0095
#define SUBCMD_ALL_FETS_ON    0x0096
#define SUBCMD_DASTATUS_6     0x0076

// Custom subcommands for test board
#define SUBCMD_LED_ON         0x00FE
#define SUBCMD_LED_OFF        0x00FF
#define LED_PIN               2

// ==================== SIMULATED BMS STATE ====================
unsigned int cellVoltages[16];
unsigned int vStack = 0;
unsigned int vPack = 0;
int16_t current = -150;
float chipTemp = 30.0;
float ts1Temp = 26.0;
float ts2Temp = 27.0;
float ts3Temp = 25.5;
float accumulatedCharge = 2500.0;
uint32_t chargeTime = 0;
bool chgFetOn = true;
bool dsgFetOn = true;
bool inConfigMode = false;
bool ledState = false;

float cellDrift[16];

// ==================== I2C SLAVE STATE ====================
volatile uint8_t currentRegister = 0;
volatile uint16_t lastSubcommand = 0;

uint8_t transferBuffer[36];
uint8_t transferBufferLen = 0;
uint8_t transferChecksum = 0;

// ==================== HELPERS ====================
uint16_t tempToRaw(float degC)
{
  return (uint16_t)((degC + 273.15) * 10.0);
}

void initSimState()
{
  randomSeed(analogRead(0));
  for (int i = 0; i < 16; i++)
  {
    if (i < TB_CONNECTED_CELLS)
    {
      cellVoltages[i] = random(3500, 3900);
      cellDrift[i] = (float)random(-20, 21) / 10.0;
    }
    else
    {
      cellVoltages[i] = 0;
      cellDrift[i] = 0;
    }
  }
}

void updateSimulation()
{
  for (int i = 0; i < TB_CONNECTED_CELLS; i++)
  {
    int newV = (int)cellVoltages[i] + (int)cellDrift[i] + random(-3, 4);
    cellVoltages[i] = constrain(newV, 2500, 4250);
  }

  vStack = 0;
  for (int i = 0; i < TB_CONNECTED_CELLS; i++)
  {
    vStack += cellVoltages[i];
  }
  vPack = vStack - random(30, 150);

  chipTemp = constrain(chipTemp + (float)random(-5, 6) / 10.0, 15.0f, 60.0f);
  ts1Temp = constrain(ts1Temp + (float)random(-5, 6) / 10.0, 15.0f, 55.0f);
  ts2Temp = constrain(ts2Temp + (float)random(-5, 6) / 10.0, 15.0f, 55.0f);
  ts3Temp = constrain(ts3Temp + (float)random(-5, 6) / 10.0, 15.0f, 55.0f);

  if (chgFetOn && !dsgFetOn)
  {
    accumulatedCharge += 0.05;
    current = abs(current);
  }
  else if (dsgFetOn && !chgFetOn)
  {
    accumulatedCharge -= 0.03;
    if (accumulatedCharge < 0) accumulatedCharge = 0;
    current = -abs(current);
  }
  chargeTime += 1;
}

// ==================== SUBCOMMAND RESPONSES ====================
void prepareDaStatus6()
{
  int32_t intPart = (int32_t)accumulatedCharge;
  float fracPart = accumulatedCharge - (float)intPart;
  uint32_t fracRaw = (uint32_t)(fracPart * (float)(1ULL << 32));

  transferBuffer[0] = (uint8_t)(intPart & 0xFF);
  transferBuffer[1] = (uint8_t)((intPart >> 8) & 0xFF);
  transferBuffer[2] = (uint8_t)((intPart >> 16) & 0xFF);
  transferBuffer[3] = (uint8_t)((intPart >> 24) & 0xFF);
  transferBuffer[4] = (uint8_t)(fracRaw & 0xFF);
  transferBuffer[5] = (uint8_t)((fracRaw >> 8) & 0xFF);
  transferBuffer[6] = (uint8_t)((fracRaw >> 16) & 0xFF);
  transferBuffer[7] = (uint8_t)((fracRaw >> 24) & 0xFF);
  transferBuffer[8] = (uint8_t)(chargeTime & 0xFF);
  transferBuffer[9] = (uint8_t)((chargeTime >> 8) & 0xFF);
  transferBuffer[10] = (uint8_t)((chargeTime >> 16) & 0xFF);
  transferBuffer[11] = (uint8_t)((chargeTime >> 24) & 0xFF);
  transferBufferLen = 12;
}

void prepareCbActiveCells()
{
  transferBuffer[0] = 0x00;
  transferBuffer[1] = 0x00;
  transferBufferLen = 2;
}

void computeTransferChecksum()
{
  uint8_t sum = (uint8_t)(lastSubcommand & 0xFF) + (uint8_t)((lastSubcommand >> 8) & 0xFF);
  for (int i = 0; i < transferBufferLen; i++)
  {
    sum += transferBuffer[i];
  }
  transferChecksum = ~sum;
}

// ==================== HANDLE SUBCOMMAND ====================
void handleSubcommand(uint16_t subcmd)
{
  lastSubcommand = subcmd;
  transferBufferLen = 0;

  switch (subcmd)
  {
  case SUBCMD_RESET:
    Serial.println("[TB-BQ] RESET");
    initSimState();
    break;
  case SUBCMD_RESET_PASSQ:
    Serial.println("[TB-BQ] Reset charge counter");
    accumulatedCharge = 0;
    chargeTime = 0;
    break;
  case SUBCMD_ALL_FETS_ON:
    Serial.println("[TB-BQ] All FETs ON");
    chgFetOn = true;
    dsgFetOn = true;
    break;
  case SUBCMD_ALL_FETS_OFF:
    Serial.println("[TB-BQ] All FETs OFF");
    chgFetOn = false;
    dsgFetOn = false;
    break;
  case SUBCMD_CHG_OFF:
    Serial.println("[TB-BQ] CHG FET OFF");
    chgFetOn = false;
    break;
  case SUBCMD_DSG_OFF:
    Serial.println("[TB-BQ] DSG FET OFF");
    dsgFetOn = false;
    break;
  case SUBCMD_CFGUPDATE:
    inConfigMode = true;
    break;
  case SUBCMD_EXIT_CFGUPDATE:
    inConfigMode = false;
    break;
  case SUBCMD_DASTATUS_6:
    prepareDaStatus6();
    computeTransferChecksum();
    break;
  case SUBCMD_CB_ACTIVE:
    prepareCbActiveCells();
    computeTransferChecksum();
    break;
  case SUBCMD_LED_ON:
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[TB-BQ] LED ON");
    break;
  case SUBCMD_LED_OFF:
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println("[TB-BQ] LED OFF");
    break;
  default:
    Serial.printf("[TB-BQ] Unknown subcmd: 0x%04X\n", subcmd);
    memset(transferBuffer, 0, 32);
    transferBufferLen = 32;
    computeTransferChecksum();
    break;
  }
}

// ==================== DIRECT COMMAND RESPONSE ====================
uint16_t getDirectCommandValue(uint8_t reg)
{
  if (reg >= REG_VCELL_1 && reg <= REG_VCELL_16 && ((reg - REG_VCELL_1) % 2 == 0))
  {
    return (uint16_t)cellVoltages[(reg - REG_VCELL_1) / 2];
  }

  switch (reg)
  {
  case REG_VSTACK:    return (uint16_t)vStack;
  case REG_VPACK:     return (uint16_t)vPack;
  case REG_CC2_CUR:   return (uint16_t)current;
  case REG_INT_TEMP:  return tempToRaw(chipTemp);
  case REG_TS1:       return tempToRaw(ts1Temp);
  case REG_TS2:       return tempToRaw(ts2Temp);
  case REG_TS3:       return tempToRaw(ts3Temp);
  case REG_FET_STAT:
  {
    uint8_t f = 0;
    if (chgFetOn) f |= 0x01;
    if (dsgFetOn) f |= 0x04;
    return (uint16_t)f;
  }
  case REG_SUBCMD_LOW: return lastSubcommand;
  default:            return 0;
  }
}

// ==================== I2C CALLBACKS ====================
void onReceiveHandler(int numBytes)
{
  if (numBytes < 1) return;
  uint8_t firstByte = Wire.read();
  numBytes--;

  if (firstByte == REG_SUBCMD_LOW && numBytes >= 2)
  {
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    numBytes -= 2;
    while (numBytes-- > 0) Wire.read();
    handleSubcommand((uint16_t)hi << 8 | lo);
  }
  else if (firstByte == 0x60)
  {
    while (numBytes-- > 0) Wire.read();
  }
  else
  {
    currentRegister = firstByte;
    while (numBytes-- > 0) Wire.read();
  }
}

void onRequestHandler()
{
  if (currentRegister == REG_RESP_START)
  {
    if (transferBufferLen > 0)
      Wire.write(transferBuffer, transferBufferLen);
    else
    {
      uint8_t zeros[32] = {0};
      Wire.write(zeros, 32);
    }
  }
  else if (currentRegister == REG_RESP_CHKSUM)
  {
    Wire.write(transferChecksum);
  }
  else
  {
    uint16_t val = getDirectCommandValue(currentRegister);
    uint8_t r[2] = {(uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF)};
    Wire.write(r, 2);
  }
}

// ==================== SETUP ====================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TB-BQ] === BQ76952 Test Board (I2C Slave) ===");
  Serial.printf("[TB-BQ] I2C Addr: 0x%02X  SDA:%d SCL:%d\n", BQ_I2C_ADDR_TB, TB_I2C_SDA, TB_I2C_SCL);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  initSimState();

  Wire.begin((uint8_t)BQ_I2C_ADDR_TB, TB_I2C_SDA, TB_I2C_SCL, 400000);
  Wire.onReceive(onReceiveHandler);
  Wire.onRequest(onRequestHandler);

  Serial.printf("[TB-BQ] Simulating %d cells — ready\n", TB_CONNECTED_CELLS);
}

// ==================== LOOP ====================
unsigned long lastUpdate = 0;

void loop()
{
  if (millis() - lastUpdate >= TB_UPDATE_INTERVAL_MS)
  {
    lastUpdate = millis();
    updateSimulation();
    Serial.printf("[TB-BQ] C1=%u C2=%u C3=%u Stack=%u Cur=%d CHG=%s DSG=%s LED=%s\n",
                  cellVoltages[0], cellVoltages[1], cellVoltages[2], vStack, current,
                  chgFetOn ? "ON" : "OFF", dsgFetOn ? "ON" : "OFF", ledState ? "ON" : "OFF");
  }
  delay(10);
}
