/*
 * Description :   Interface to BQ76952 BMS IC (by Texas Instruments) for Arduino platform.
 * Author      :   James Fotherby forked from pranjal-joshi/BQ76952Lib
 * Date        :   23/11/2024
 * License     :   MIT
 * This code is published as open source software. Feel free to share/modify.
 *
 * Audit fixes applied — see CHANGELOG below.
 *
 * CHANGELOG (audit fixes):
 *   FIX 1 [CRITICAL]  getTemperatureStatus() — OVERTEMP_FET now reads BIT_SB_OTF (bit 7)
 *                      instead of BIT_SB_OTC (bit 4). Was copy-paste bug; FET overtemp
 *                      faults were never detected.
 *   FIX 2 [CRITICAL]  setFET() — OFF/discharge case now matches enum value DCH (from
 *                      bq76952_fet) instead of DCHG (from bq76952_thermistor enum).
 *                      Was silently falling through to ALL_FETS_OFF.
 *   FIX 3 [CRITICAL]  Null-pointer guards added after every subCommandwithdata() call
 *                      in getAccumulatedCharge(), GetCellBalancingBitmask(),
 *                      GetCellBalancingTimes(). A single I2C glitch would crash the ESP32.
 *   FIX 4 [MODERATE]  Replaced fragile incremental computeChecksum() with inline uint16_t
 *                      sum pattern (same approach already used in subCommandWriteData).
 *                      Old version had an edge case where intermediate sum == 0x00 would
 *                      corrupt the running checksum.
 *   FIX 5 [MODERATE]  readDataMemory() now validates the received checksum against the
 *                      computed checksum and returns nullptr on mismatch. Previously it
 *                      printed both values but never compared them.
 *   FIX 6 [LOW]       getCellVoltage() bounds-checks cellNumber (1–18). Values 17/18 map
 *                      to VSTACK/VPACK intentionally; anything else returns 0.
 *   FIX 7 [LOW]       setConnectedCells() — out-of-range values now clamp to 16 AND
 *                      actually write. Previously the assignment was dead code.
 */

#include "BQ76952.h"
#include <Wire.h>

// Defines:
#define DBG_BAUD 115200
#define BQ_I2C_ADDR 0x08

#define CELL_NO_TO_ADDR(cellNo) (DIR_CMD_VCELL_1 + ((cellNo - 1) * 2))
#define LOW_BYTE(addr) (byte)(addr & 0x00FF)
#define HIGH_BYTE(addr) (byte)((addr >> 8) & 0x00FF)

bool BQ_DEBUG = false;

BQ76952::BQ76952()
{
}

void BQ76952::begin(void)
{
  Wire.begin();
  if (BQ_DEBUG)
  {
    Serial.begin(DBG_BAUD);
    debugPrintln(F("[+] Initializing BQ76952..."));
  }
}

void BQ76952::begin(int SDA_Pin, int SCK_Pin)
{
  Wire.begin(SDA_Pin, SCK_Pin, 400000);
  if (BQ_DEBUG)
  {
    Serial.begin(DBG_BAUD);
    debugPrintln(F("[+] Initializing BQ76952..."));
  }
}

void BQ76952::setDebug(byte debug)
{
  BQ_DEBUG = debug;
}

void BQ76952::reset(void)
{
  CommandOnlysubCommand(COSCMD_RESET);
}

void BQ76952::unseal(void)
{
  CommandOnlysubCommand(0x0414);
  delay(10);
  CommandOnlysubCommand(0x3672);
  delay(10);
  debugPrintln(F("[+] UNSEAL sequence sent (0x0414, 0x3672)"));
}

void BQ76952::seal(void)
{
  CommandOnlysubCommand(0x0030); // SEC_SEAL subcommand
  delay(10);
  debugPrintln(F("[+] SEAL sequence sent (0x0030)"));
}

void BQ76952::enterConfigUpdate(void)
{
  CommandOnlysubCommand(COSCMD_SET_CFGUPDATE);
  delayMicroseconds(2000);
}

void BQ76952::exitConfigUpdate(void)
{
  CommandOnlysubCommand(COSCMD_EXIT_CFGUPDATE);
  delayMicroseconds(2000);
}

// Send Direct command
// Direct commands have a single byte address followed by reading or writing 1 or 2 subsequent bytes
// However there are very few direct commands that involve writing data so we have only implemented reading
unsigned int BQ76952::directCommandRead(byte command)
{
  Wire.beginTransmission(BQ_I2C_ADDR); // Begin with the device I2C address
  Wire.write(command);                 // Write the command
  Wire.endTransmission();
  delayMicroseconds(1000);

  Wire.requestFrom(BQ_I2C_ADDR, 2); // Read 2 bytes (the particular command may only return 1 byte though)
  delayMicroseconds(1000);

  unsigned long startMillis = millis();
  while (!Wire.available())
  { // Wait for all bytes
    if (millis() - startMillis > 10)
    { // Timeout after 10ms
      debugPrint(F("[!] Timeout waiting for I2C response.\n"));
      return (0);
    }
  }
  byte lsb = Wire.read();
  byte msb = Wire.read();

  debugPrint(F("[+] Direct Cmd SENT -> "));
  debugPrintlnCmd((uint16_t)command);
  debugPrint(F("[+] Direct Cmd RESP <- "));
  debugPrintlnCmd((uint16_t)(msb << 8 | lsb));

  return (unsigned int)(msb << 8 | lsb);
}

// Command-Only subcommand
// These subcommands on their own force simple actions eg. device reset, enter config mode, turn fets off etc
void BQ76952::CommandOnlysubCommand(unsigned int command)
{
  Wire.beginTransmission(BQ_I2C_ADDR);       // Begin with the device I2C address
  Wire.write(CMD_DIR_SUBCMD_LOW);            // This sets the device's address pointer (ie 3E)
  Wire.write((byte)command & 0x00FF);        // This writes data into the address we just pointed to
  Wire.write((byte)(command >> 8) & 0x00FF); // This writes the next data byte (pointer address autoincrements)
  Wire.endTransmission();

  delayMicroseconds(1000);

  debugPrint(F("[+] Sub Cmd SENT to 0x3E -> "));
  debugPrintlnCmd((uint16_t)command);
}

// Subcommand with data
// These subcommands return data (some return nearly 32 bytes)
byte *BQ76952::subCommandwithdata(unsigned int command, int bytes_to_read)
{
  if (bytes_to_read > (int)sizeof(_DataBuffer))
  {
    debugPrint(F("[!] Error: bytes_to_read exceeds buffer size.\n"));
    return nullptr; // Prevent overflow
  }

  CommandOnlysubCommand(command); // Enact the subcommand

  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(CMD_DIR_RESP_START); // Set address to start of data buffer
  Wire.endTransmission();
  delayMicroseconds(1000); // Allow time for the device to respond

  Wire.requestFrom(BQ_I2C_ADDR, bytes_to_read);
  delayMicroseconds(1000);

  unsigned long startMillis = millis();
  while (Wire.available() < bytes_to_read)
  { // Wait for all bytes
    if (millis() - startMillis > 10)
    { // Timeout after 10ms
      debugPrint(F("[!] Timeout waiting for I2C response.\n"));
      return nullptr;
    }
  }

  debugPrint(F("[+] Sub Cmd RESP at 0x40 -> "));
  for (int i = 0; i < bytes_to_read; i++)
  {
    _DataBuffer[i] = Wire.read();

    // Format the current byte as a hexadecimal string (e.g., "0x1A")
    char hexString[5]; // 4 characters for "0xXX" + null terminator
    snprintf(hexString, sizeof(hexString), "0x%02X", _DataBuffer[i]);
    debugPrint(hexString); // Print the formatted hex string

    // Add a comma and space after each byte, except the last one
    if (i < bytes_to_read - 1)
    {
      debugPrint(F(", "));
    }
    else
    {
      debugPrintln(F(" "));
    }
  }

  return _DataBuffer; // Return pointer to buffer
}

// Write a subcommand with payload (no response data).
// Returns true on success.
// subcmd: 16-bit subcommand code
// data: pointer to payload (may be nullptr if len==0)
// len:  number of payload bytes (0..32 typically)
bool BQ76952::subCommandWriteData(uint16_t subcmd, const uint8_t *data, uint8_t len)
{
  // Build [LSB, MSB, payload...]
  const uint8_t maxBlock = sizeof(_DataBuffer);
  if ((uint16_t)len + 2 > maxBlock)
  {
    debugPrintln(F("[!] subCommandWriteData: payload too large"));
    return false;
  }

  _DataBuffer[0] = (uint8_t)(subcmd & 0xFF);
  _DataBuffer[1] = (uint8_t)((subcmd >> 8) & 0xFF);
  for (uint8_t i = 0; i < len; i++)
    _DataBuffer[2 + i] = data ? data[i] : 0;

  // Compute checksum = ~sum(block) & 0xFF ; length = blockLen + 2
  uint16_t sum = 0;
  for (uint8_t i = 0; i < (uint8_t)(len + 2); i++)
    sum += _DataBuffer[i];
  uint8_t checksum = (uint8_t)(~sum);        // same as (0xFF - (sum & 0xFF))
  uint8_t writelen = (uint8_t)(len + 2); // Correct per TI: subcommand bytes + payload bytes

  // Write block to 0x3E
  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(CMD_DIR_SUBCMD_LOW); // 0x3E
  for (uint8_t i = 0; i < (uint8_t)(len + 2); i++)
    Wire.write(_DataBuffer[i]);
  if (Wire.endTransmission() != 0)
  {
    debugPrintln(F("[!] I2C error writing subcmd block"));
    return false;
  }

  delayMicroseconds(1000); // brief settle; most cmds ~0.5–0.6 ms

  // Write checksum & length to 0x60
  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(0x60);
  Wire.write(checksum);
  Wire.write(writelen);
  if (Wire.endTransmission() != 0)
  {
    debugPrintln(F("[!] I2C error writing checksum/length"));
    return false;
  }

  // Optional: poll 0x3E/0x3F until device echoes the subcommand (completion)
  unsigned long t0 = millis();
  for (;;)
  {
    Wire.beginTransmission(BQ_I2C_ADDR);
    Wire.write(CMD_DIR_SUBCMD_LOW); // 0x3E
    if (Wire.endTransmission(false) != 0)
      break; // repeated start
    if (Wire.requestFrom(BQ_I2C_ADDR, 2) == 2)
    {
      uint8_t lo = Wire.read();
      uint8_t hi = Wire.read();
      if (lo == (uint8_t)(subcmd & 0xFF) && hi == (uint8_t)(subcmd >> 8))
        break;
    }
    if (millis() - t0 > 10)
    { // ~10 ms guard
      debugPrintln(F("[!] subcmd completion timeout"));
      return false;
    }
  }

  debugPrint(F("[+] Sub Cmd WRITE 0x"));
  debugPrintlnCmd(subcmd);
  return true;
}

// Read Bytes from Data memory of BQ76952
// Provide an address. Returns 32-byte buffer or nullptr on failure.
byte *BQ76952::readDataMemory(unsigned int addr)
{
  // FIX 4: Use simple uint16_t sum instead of fragile incremental computeChecksum()
  uint16_t chksumAccum = 0;
  chksumAccum += LOW_BYTE(addr);
  chksumAccum += HIGH_BYTE(addr);

  CommandOnlysubCommand(addr); // This causes a read from a register into the transfer buffer
  delayMicroseconds(2000);

  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(CMD_DIR_RESP_START); // Set address to start of data buffer
  Wire.endTransmission();
  delayMicroseconds(1000); // Allow time for the device to respond

  Wire.requestFrom(BQ_I2C_ADDR, 32);
  delayMicroseconds(1000);

  unsigned long startMillis = millis();
  while (Wire.available() < 32)
  { // Wait for all bytes
    if (millis() - startMillis > 10)
    { // Timeout after 10ms
      debugPrint(F("[!] Timeout waiting for I2C response.\n"));
      return nullptr;
    }
  }

  debugPrint(F("[+] Data RECEIVED -> "));
  for (int i = 0; i < 32; i++)
  {
    _DataBuffer[i] = Wire.read();

    chksumAccum += _DataBuffer[i];

    // Format the current byte as a hexadecimal string (e.g., "0x1A")
    char hexString[5]; // 4 characters for "0xXX" + null terminator
    snprintf(hexString, sizeof(hexString), "0x%02X", _DataBuffer[i]);
    debugPrint(hexString); // Print the formatted hex string

    // Add a comma and space after each byte, except the last one
    if (i < 31)
    {
      debugPrint(F(", "));
    }
    else
    {
      debugPrintln(F(" "));
    }
  }

  byte chksum = (byte)(~chksumAccum); // Final complement

  // We've just received the 32 byte transfer buffer. Now receive the checksum and check it
  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(CMD_DIR_RESP_CHKSUM); // Set address to checksum register
  Wire.endTransmission();
  delayMicroseconds(1000); // Allow time for the device to respond

  Wire.requestFrom(BQ_I2C_ADDR, 1);
  delayMicroseconds(1000);

  startMillis = millis();
  while (Wire.available() < 1)
  { // Wait for all bytes
    if (millis() - startMillis > 10)
    { // Timeout after 10ms
      debugPrint(F("[!] Timeout waiting for I2C response.\n"));
      return nullptr;
    }
  }

  byte Received_chksum = Wire.read();

  debugPrint(F("[+] Calculated Checksum: "));
  debugPrintlnCmd(chksum);

  debugPrint(F("[+] Received Checksum: "));
  debugPrintlnCmd(Received_chksum);

  // FIX 5: Actually validate the checksum instead of just printing both values
  if (chksum != Received_chksum)
  {
    debugPrintln(F("[!] Checksum MISMATCH — data may be corrupted"));
    return nullptr;
  }

  return _DataBuffer; // Return pointer to buffer
}

void BQ76952::writeDataMemory(unsigned int addr, byte *data_buffer, byte noOfBytes)
{
  // FIX 4: Use simple uint16_t sum instead of fragile incremental computeChecksum()
  uint16_t chksumAccum = 0;
  chksumAccum += LOW_BYTE(addr);
  chksumAccum += HIGH_BYTE(addr);

  enterConfigUpdate();
  delayMicroseconds(1000);

  debugPrint(F("[+] address SENT to 0x3E -> "));
  debugPrintlnCmd((uint16_t)addr);

  Wire.beginTransmission(BQ_I2C_ADDR); // Begin with the device I2C address
  Wire.write(CMD_DIR_SUBCMD_LOW);      // This sets the device's address pointer (ie 3E)
  Wire.write(LOW_BYTE(addr));          // This writes data into the address we just pointed to
  Wire.write(HIGH_BYTE(addr));         // This writes the next data byte (pointer address autoincrements)

  debugPrint(F("[+] Data SENT -> "));
  for (int i = 0; i < noOfBytes; i++)
  {
    Wire.write(data_buffer[i]); // Write from our passed buffer calculating checksum as we go

    chksumAccum += data_buffer[i];

    // Format the current byte as a hexadecimal string (e.g., "0x1A")
    char hexString[5]; // 4 characters for "0xXX" + null terminator
    snprintf(hexString, sizeof(hexString), "0x%02X", data_buffer[i]);
    debugPrint(hexString); // Print the formatted hex string

    // Add a comma and space after each byte, except the last one
    if (i < noOfBytes - 1)
    {
      debugPrint(F(", "));
    }
    else
    {
      debugPrintln(F(" "));
    }
  }

  for (int i = noOfBytes; i < 32; i++)
  {
    Wire.write(0); // Pad with zeros
  }

  byte chksum = (byte)(~chksumAccum); // Final complement

  Wire.write(chksum);        // write checksum
  Wire.write(noOfBytes + 4); // write length: 2 (addr) + noOfBytes (data) + 2 (per TI TRM)
  Wire.endTransmission();

  exitConfigUpdate();
}

// Write a single byte to data memory
void BQ76952::writeByteToMemory(unsigned int addr, byte data)
{
  writeDataMemory(addr, &data, 1);
}

// Write a 2-byte unsigned int to data memory
void BQ76952::writeIntToMemory(unsigned int addr, unsigned int data)
{
  writeDataMemory(addr, reinterpret_cast<byte *>(&data), 2);
}

void BQ76952::writeFloatToMemory(unsigned int addr, float data)
{
  writeDataMemory(addr, reinterpret_cast<byte *>(&data), 4);
}

// Write to a direct command register
void BQ76952::directCommandWrite(byte command, byte data)
{
  Wire.beginTransmission(BQ_I2C_ADDR);
  Wire.write(command);
  Wire.write(data);
  Wire.endTransmission();
}

// -------------------------------------------------------------------
// --------------------------- API Functions -------------------------
// -------------------------------------------------------------------

// Set user-defined number of cells connected
void BQ76952::setConnectedCells(unsigned int Cells)
{
  // FIX 7: Clamp out-of-range AND actually write (was dead code before)
  if (Cells < 3 || Cells > 16)
    Cells = 16;

  uint16_t Cell_Flags = (1 << Cells) - 1;

  debugPrint(F("[+] Vcell Mode => "));
  debugPrintlnCmd(Cell_Flags);
  writeDataMemory(0x9304, reinterpret_cast<byte *>(&Cell_Flags), 2);
}

// Read single cell voltage
// cellNumber 1-16 = individual cells, 17 = VSTACK, 18 = VPACK
unsigned int BQ76952::getCellVoltage(byte cellNumber)
{
  // FIX 6: Bounds check — values beyond 18 read wrong registers
  if (cellNumber < 1 || cellNumber > 18)
  {
    debugPrintln(F("[!] getCellVoltage: cellNumber must be 1-18"));
    return 0;
  }
  return directCommandRead(CELL_NO_TO_ADDR(cellNumber));
}

// Measure CC2 current
int BQ76952::getCurrent(void)
{
  return (int16_t)directCommandRead(DIR_CMD_CC2_CUR);
}

// Returns the accumulated charge and updates the time since this was reset
float BQ76952::getAccumulatedCharge(void)
{
  byte *buffer = subCommandwithdata(DASTATUS_6, 12);

  // FIX 3: Guard against null pointer from I2C failure
  if (!buffer)
  {
    debugPrintln(F("[!] getAccumulatedCharge: I2C read failed"));
    return 0.0f;
  }

  // Extract the integer portion (I4 - 32-bit signed integer)
  int32_t accumChargeInteger =
      ((int32_t)buffer[3] << 24) |
      ((int32_t)buffer[2] << 16) |
      ((int32_t)buffer[1] << 8) |
      buffer[0];

  // Extract the fractional portion (U4 - 32-bit unsigned integer)
  uint32_t accumChargeFraction =
      ((uint32_t)buffer[7] << 24) |
      ((uint32_t)buffer[6] << 16) |
      ((uint32_t)buffer[5] << 8) |
      buffer[4];

  // Extract the integer time in Seconds
  this->AccumulatedChargeTime =
      ((uint32_t)buffer[11] << 24) |
      ((uint32_t)buffer[10] << 16) |
      ((uint32_t)buffer[9] << 8) |
      buffer[8];

  // Convert the fractional portion to userAh by dividing by 2^32
  float fractionalCharge = (float)accumChargeFraction / (float)(1ULL << 32);

  // Combine the integer and fractional portions
  float totalAccumulatedCharge = accumChargeInteger + fractionalCharge;

  return totalAccumulatedCharge;
}

// Return the time in seconds since the accumulated charge was reset
uint32_t BQ76952::getAccumulatedChargeTime(void)
{
  getAccumulatedCharge();
  return this->AccumulatedChargeTime;
}

// Resets accumulated charge and timer
void BQ76952::ResetAccumulatedCharge(void)
{
  CommandOnlysubCommand(COSCMD_RESET_PASSQ);
}

// Returns a bit mask of cells being balanced
uint16_t BQ76952::GetCellBalancingBitmask(void)
{
  byte *buffer = subCommandwithdata(CB_ACTIVE_CELLS, 2);

  // FIX 3: Guard against null pointer from I2C failure
  if (!buffer)
  {
    debugPrintln(F("[!] GetCellBalancingBitmask: I2C read failed"));
    return 0;
  }

  return ((uint16_t)buffer[0]) | ((uint16_t)buffer[1] << 8);
}

// Sets the bit mask of cells to be balanced (Host-Controlled)
void BQ76952::setBalancingMask(uint16_t mask)
{
  uint8_t data[2];
  data[0] = (uint8_t)(mask & 0xFF);
  data[1] = (uint8_t)((mask >> 8) & 0xFF);
  subCommandWriteData(0x0083, data, 2);
}

// Returns the Manufacturing Status word (includes SEC and CONFIGUPDATE bits)
uint16_t BQ76952::getManufacturingStatus(void)
{
  uint8_t *buffer = subCommandwithdata(COSCMD_MANUF_STATUS, 2);
  if (!buffer) return 0;
  return ((uint16_t)buffer[0]) | ((uint16_t)buffer[1] << 8);
}

// Sets the device power mode
// Per BQ76952 TRM Section 7.3/7.4:
//   SLEEP is entered autonomously when SLEEP_EN is set and current < threshold.
//   DEEPSLEEP requires sending 0x000F twice within 4 seconds.
//   SHUTDOWN requires sending 0x0010 twice within 4 seconds (if sealed).
//   Wake from SLEEP: send SLEEP_DISABLE (0x009A).
//   Wake from DEEPSLEEP: send EXIT_DEEPSLEEP (0x000E) or HW RST_SHUT pin.
void BQ76952::setPowerMode(uint8_t mode)
{
  switch (mode)
  {
  case 1: // Sleep — enable SLEEP mode so device enters sleep when current is low
    CommandOnlysubCommand(COSCMD_SLEEP_ENABLE);
    debugPrintln(F("[+] SLEEP_ENABLE sent (0x0099)"));
    break;
  case 2: // Deep Sleep — must send twice within 4 seconds per TRM
    CommandOnlysubCommand(COSCMD_DEEP_SLEEP);
    delay(100);
    CommandOnlysubCommand(COSCMD_DEEP_SLEEP);
    debugPrintln(F("[+] DEEPSLEEP sent twice (0x000F)"));
    break;
  case 3: // Shutdown — must send twice within 4 seconds per TRM
    CommandOnlysubCommand(COSCMD_SHUTDOWN);
    delay(100);
    CommandOnlysubCommand(COSCMD_SHUTDOWN);
    debugPrintln(F("[+] SHUTDOWN sent twice (0x0010)"));
    break;
  default: // Normal / Wake
    CommandOnlysubCommand(COSCMD_SLEEP_DISABLE);
    debugPrintln(F("[+] SLEEP_DISABLE sent (0x009A) — wake from SLEEP"));
    break;
  }
}

// Read cumulative time spent balancing for each cell into a passed buffer
void BQ76952::GetCellBalancingTimes(uint32_t *Cell_Balance_Times)
{
  byte *buffer = subCommandwithdata(CBSTATUS2, 32);

  // FIX 3: Guard against null pointer from I2C failure
  if (!buffer)
  {
    debugPrintln(F("[!] GetCellBalancingTimes: CBSTATUS2 I2C read failed"));
    memset(Cell_Balance_Times, 0, 16 * sizeof(uint32_t));
    return;
  }

  for (byte i = 0; i < 8; i++)
  {
    uint8_t offset = 4 * i;

    Cell_Balance_Times[i] =
        ((uint32_t)buffer[3 + offset] << 24) |
        ((uint32_t)buffer[2 + offset] << 16) |
        ((uint32_t)buffer[1 + offset] << 8) |
        buffer[0 + offset];
  }

  buffer = subCommandwithdata(CBSTATUS3, 32);

  // FIX 3: Guard against null pointer from I2C failure
  if (!buffer)
  {
    debugPrintln(F("[!] GetCellBalancingTimes: CBSTATUS3 I2C read failed"));
    memset(&Cell_Balance_Times[8], 0, 8 * sizeof(uint32_t));
    return;
  }

  for (byte i = 0; i < 8; i++)
  {
    uint8_t offset = 4 * i;

    Cell_Balance_Times[i + 8] =
        ((uint32_t)buffer[3 + offset] << 24) |
        ((uint32_t)buffer[2 + offset] << 16) |
        ((uint32_t)buffer[1 + offset] << 8) |
        buffer[0 + offset];
  }
}

void BQ76952::setFET(bq76952_fet fet, bq76952_fet_state state)
{
  uint16_t subcmd;
  uint8_t data[2];
  bool needsData = false;

  switch (state)
  {
  case OFF:
    needsData = true;
    data[0] = 0x02; // Force OFF
    data[1] = 0x00;
    switch (fet)
    {
    case DCH: // Discharge FET
      subcmd = FET_CTRL_DSG;
      break;
    case CHG: // Charge FET
      subcmd = FET_CTRL_CHG;
      break;
    default:
      // For ALL, use command-only version (no data needed)
      needsData = false;
      subcmd = COSCMD_ALL_FETS_OFF;
      break;
    }
    break;

  case ON:
    needsData = true;
    data[0] = 0x01; // Force ON
    data[1] = 0x00;
    switch (fet)
    {
    case DCH: // Discharge FET
      subcmd = FET_CTRL_DSG;
      break;
    case CHG: // Charge FET
      subcmd = FET_CTRL_CHG;
      break;
    default:
      // For ALL, use command-only version (no data needed)
      needsData = false;
      subcmd = COSCMD_ALL_FETS_ON;
      break;
    }
    break;
  }

  if (needsData)
  {
    subCommandWriteData(subcmd, data, 2);
  }
  else
  {
    CommandOnlysubCommand(subcmd);
  }
}

// is Charging FET ON?
bool BQ76952::isCharging(void)
{
  byte regData = (byte)directCommandRead(DIR_CMD_FET_STAT);
  if (regData & 0x01)
  {
    debugPrintln(F("[+] Charging FET -> ON"));
    return true;
  }
  debugPrintln(F("[+] Charging FET -> OFF"));
  return false;
}

// is Discharging FET ON?
bool BQ76952::isDischarging(void)
{
  byte regData = (byte)directCommandRead(DIR_CMD_FET_STAT);
  if (regData & 0x04)
  {
    debugPrintln(F("[+] Discharging FET -> ON"));
    return true;
  }
  debugPrintln(F("[+] Discharging FET -> OFF"));
  return false;
}

// are cells being balanced?
uint16_t BQ76952::isBalancing(void)
{
  return GetCellBalancingBitmask();
}

// Measure chip temperature in °C
float BQ76952::getInternalTemp(void)
{
  float raw = directCommandRead(DIR_CMD_INT_TEMP) / 10.0;
  return (raw - 273.15);
}

// Measure thermistor temperature in °C
float BQ76952::getThermistorTemp(bq76952_thermistor thermistor)
{
  byte cmd;
  switch (thermistor)
  {
  case TS1:
    cmd = 0x70;
    break;
  case TS2:
    cmd = 0x72;
    break;
  case TS3:
    cmd = 0x74;
    break;
  case HDQ:
    cmd = 0x76;
    break;
  case DCHG:
    cmd = 0x78;
    break;
  case DDSG:
    cmd = 0x7A;
    break;
  }
  float raw = directCommandRead(cmd) / 10.0;
  return (raw - 273.15);
}

// Check Primary Protection status (Reads Active ALERTS, not latched faults)
bq_protection_t BQ76952::getProtectionStatus(void)
{
  bq_protection_t prot;
  byte regData = (byte)directCommandRead(DIR_CMD_SAFETY_ALERT_A); // 0x02

  prot.bits.SC_DCHG = bitRead(regData, BIT_SA_SC_DCHG);
  prot.bits.OC2_DCHG = bitRead(regData, BIT_SA_OC2_DCHG);
  prot.bits.OC1_DCHG = bitRead(regData, BIT_SA_OC1_DCHG);
  prot.bits.OC_CHG = bitRead(regData, BIT_SA_OC_CHG);
  prot.bits.CELL_OV = bitRead(regData, BIT_SA_CELL_OV);
  prot.bits.CELL_UV = bitRead(regData, BIT_SA_CELL_UV);
  return prot;
}

// Check Temperature Protection status (Reads Active ALERTS, not latched faults)
bq_temperature_t BQ76952::getTemperatureStatus(void)
{
  bq_temperature_t prot;
  byte regData = (byte)directCommandRead(DIR_CMD_SAFETY_ALERT_B); // 0x04
  prot.bits.OVERTEMP_FET = bitRead(regData, BIT_SB_OTF);    // FIX 1: Was BIT_SB_OTC (bit 4), must be BIT_SB_OTF (bit 7)
  prot.bits.OVERTEMP_INTERNAL = bitRead(regData, BIT_SB_OTINT);
  prot.bits.OVERTEMP_DCHG = bitRead(regData, BIT_SB_OTD);
  prot.bits.OVERTEMP_CHG = bitRead(regData, BIT_SB_OTC);
  prot.bits.UNDERTEMP_INTERNAL = bitRead(regData, BIT_SB_UTINT);
  prot.bits.UNDERTEMP_DCHG = bitRead(regData, BIT_SB_UTD);
  prot.bits.UNDERTEMP_CHG = bitRead(regData, BIT_SB_UTC);
  return prot;
}

// Clear all latched Safety Faults
void BQ76952::clearFaults(void)
{
  // Writing 0xFF to the Status registers clears any latched fault bits
  // This helps recover the BQ76952 from latched states
  directCommandWrite(DIR_CMD_SAFETY_STATUS_A, 0xFF); // 0x03
  directCommandWrite(DIR_CMD_SAFETY_STATUS_B, 0xFF); // 0x05
  directCommandWrite(DIR_CMD_SAFETY_STATUS_C, 0xFF); // 0x07
  debugPrintln(F("[+] Cleared Latched Safety Faults (0x03, 0x05, 0x07)"));
}


///// UTILITY FUNCTIONS /////

void BQ76952::setDebug(bool d)
{
  BQ_DEBUG = d;
}

// Debug printing utilites
void BQ76952::debugPrint(const char *msg)
{
  if (BQ_DEBUG)
    Serial.print(msg);
}

void BQ76952::debugPrintln(const char *msg)
{
  if (BQ_DEBUG)
    Serial.println(msg);
}

void BQ76952::debugPrint(const __FlashStringHelper *msg)
{
  if (BQ_DEBUG)
    Serial.print(msg);
}

void BQ76952::debugPrintln(const __FlashStringHelper *msg)
{
  if (BQ_DEBUG)
    Serial.println(msg);
}

void BQ76952::debugPrintlnCmd(unsigned int cmd)
{
  if (BQ_DEBUG)
  {
    Serial.print(F("0x"));
    Serial.println(cmd, HEX);
  }
}