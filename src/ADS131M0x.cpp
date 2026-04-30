#include "ADS131M0x.h"

#include <Arduino.h>
#include <SPI.h>

#ifdef IS_M02
#define DO_PRAGMA(x) _Pragma(#x)
#define INFO(x) DO_PRAGMA(message("\nREMARK: " #x))
//INFO Version for ADS131M02
#endif

/**
 * @brief Construct a new ADS131M0x::ADS131M0x object
 * 
 */
ADS131M0x::ADS131M0x(const uint8_t cs_pin, const uint8_t drdy_pin, SPIClass *port, const uint32_t spi_clock_speed)
  : CS_PIN{ cs_pin },
    DRDY_PIN{ drdy_pin },
    spiClockSpeed{ spi_clock_speed },
    spiPort{ port }
{
}

/**
 * @brief basic initialisation,  
 * call '.SetClockSpeed' before to set custom SPI-Clock (default=1MHz), 
 * call '.reset' to make extra hardware-reset (optional)
 *
 */
void ADS131M0x::begin()
{
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // CS HIGH --> not selected
  pinMode(DRDY_PIN, INPUT);    // DRDY Input
}

/**
 * @brief Set the SPI clock speed that will be used for communication with the 
 * ADS131M0x device.
 *
 * @param cSpeed The SPI clock speed value in Hz
 */
void ADS131M0x::setClockSpeed(const uint32_t cSpeed)
{
  spiClockSpeed = cSpeed;
}

/**
 * @brief Hardware reset (reset low active)
 *
 * @param reset_pin Pin number of the reset pin
 */
void ADS131M0x::reset(uint8_t reset_pin)
{
  pinMode(reset_pin, OUTPUT);
  digitalWrite(reset_pin, HIGH);
  delay(100);
  digitalWrite(reset_pin, LOW);
  delay(100);
  digitalWrite(reset_pin, HIGH);
  delay(1);
}

/**
 * @brief read status cmd-register
 * 
 * @return uint16_t 
 */
uint16_t ADS131M0x::isResetOK(void)
{
  return (readRegister(CMD_RESET));
}

/**
 * @brief software test of ADC data is ready
 * 
 * @param channel 
 * @return int8_t 
 */
int8_t ADS131M0x::isDataReadySoft(byte channel)
{
  if (channel == 0)
  {
    return (readRegister(REG_STATUS) & REGMASK_STATUS_DRDY0);
  }
  else if (channel == 1)
  {
    return (readRegister(REG_STATUS) & REGMASK_STATUS_DRDY1);
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    return (readRegister(REG_STATUS) & REGMASK_STATUS_DRDY2);
  }
  else if (channel == 3)
  {
    return (readRegister(REG_STATUS) & REGMASK_STATUS_DRDY3);
  }
#endif
  return -1;
}


/**
 * @brief read reset status (see datasheet)
 * 
 * @return true 
 * @return false 
 */
bool ADS131M0x::isResetStatus(void)
{
  return (readRegister(REG_STATUS) & REGMASK_STATUS_RESET);
}

/**
 * @brief read locked status (see datasheet)
 *
 * @return true
 * @return false
 */
bool ADS131M0x::isLockSPI(void)
{
  return (readRegister(REG_STATUS) & REGMASK_STATUS_LOCK);
}

/**
 * @brief set DRDY format (see datasheet)
 * 
 * @param drdyFormat 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setDrdyFormat(uint8_t drdyFormat)
{
  if (drdyFormat > 1)
  {
    return false;
  }
  else
  {
    writeRegisterMasked(REG_MODE, drdyFormat, REGMASK_MODE_DRDY_FMT);
    return true;
  }
}

/**
 * @brief set DRDY state (see datasheet)
 * 
 * @param drdyState 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setDrdyStateWhenUnavailable(uint8_t drdyState)
{
  if (drdyState > 1)
  {
    return false;
  }
  else
  {
    writeRegisterMasked(REG_MODE, drdyState < 1, REGMASK_MODE_DRDY_HiZ);
    return true;
  }
}

/**
 * @brief set power mode (see datasheet)
 * 
 * @param powerMode 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setPowerMode(uint8_t powerMode)
{
  if (powerMode > 3)
  {
    return false;
  }
  else
  {
    writeRegisterMasked(REG_CLOCK, powerMode, REGMASK_CLOCK_PWR);
    return true;
  }
}

/**
 * @brief set OSR digital filter (see datasheet)
 * 
 * @param osr 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setOsr(uint16_t osr)
{
  if (osr > 7)
  {
    return false;
  }
  else
  {
    writeRegisterMasked(REG_CLOCK, osr << 2, REGMASK_CLOCK_OSR);
    return true;
  }
}

uint8_t ADS131M0x::getChannelCount()
{
  return static_cast<uint8_t>(
    (readRegister(REG_ID) & REGMASK_ID_CHANCNT) >> 8);
}

/**
 * @brief input channel enable 
 * 
 * @param channel 
 * @param enable 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setChannelEnable(uint8_t channel, uint16_t enable)
{
  if (channel > 3)
  {
    return false;
  }
  if (channel == 0)
  {
    writeRegisterMasked(REG_CLOCK, enable << 8, REGMASK_CLOCK_CH0_EN);
    return true;
  }
  else if (channel == 1)
  {
    writeRegisterMasked(REG_CLOCK, enable << 9, REGMASK_CLOCK_CH1_EN);
    return true;
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    writeRegisterMasked(REG_CLOCK, enable << 10, REGMASK_CLOCK_CH2_EN);
    return true;
  }
  else if (channel == 3)
  {
    writeRegisterMasked(REG_CLOCK, enable << 11, REGMASK_CLOCK_CH3_EN);
    return true;
  }
#endif
  return false;
}

/**
 * @brief set gain per channel (see datasheet)
 * 
 * @param channel 
 * @param pga 
 * @return true 
 * @return false 
 */
bool ADS131M0x::setChannelPGA(uint8_t channel, uint16_t pga)
{
  if (channel == 0)
  {
    writeRegisterMasked(REG_GAIN, pga, REGMASK_GAIN_PGAGAIN0);
    return true;
  }
  else if (channel == 1)
  {
    writeRegisterMasked(REG_GAIN, pga << 4, REGMASK_GAIN_PGAGAIN1);
    return true;
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    writeRegisterMasked(REG_GAIN, pga << 8, REGMASK_GAIN_PGAGAIN2);
    return true;
  }
  else if (channel == 3)
  {
    writeRegisterMasked(REG_GAIN, pga << 12, REGMASK_GAIN_PGAGAIN3);
    return true;
  }
#endif
  return false;
}

/// @brief Set global Chop (see datasheet)
/// @param global_chop
void ADS131M0x::setGlobalChop(uint16_t global_chop)
{
  writeRegisterMasked(REG_CFG, global_chop << 8, REGMASK_CFG_GC_EN);
}


/// @brief Set global Chop Delay
/// @param delay todo:  ms or us ??
void ADS131M0x::setGlobalChopDelay(uint16_t delay)
{
  writeRegisterMasked(REG_CFG, delay << 9, REGMASK_CFG_GC_DLY);
}

bool ADS131M0x::setInputChannelSelection(uint8_t channel, uint8_t input)
{
  if (channel == 0)
  {
    writeRegisterMasked(REG_CH0_CFG, input, REGMASK_CHX_CFG_MUX);
    return true;
  }
  else if (channel == 1)
  {
    writeRegisterMasked(REG_CH1_CFG, input, REGMASK_CHX_CFG_MUX);
    return true;
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    writeRegisterMasked(REG_CH2_CFG, input, REGMASK_CHX_CFG_MUX);
    return true;
  }
  else if (channel == 3)
  {
    writeRegisterMasked(REG_CH3_CFG, input, REGMASK_CHX_CFG_MUX);
    return true;
  }
#endif
  return false;
}

/// @brief set offset calibration per channel
/// @param channel
/// @param offset
/// @return
bool ADS131M0x::setChannelOffsetCalibration(uint8_t channel, int32_t offset)
{

  uint16_t MSB = offset >> 8;
  uint8_t LSB = offset;


  if (channel == 0)
  {
    writeRegisterMasked(REG_CH0_OCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH0_OCAL_LSB, LSB << 8, REGMASK_CHX_OCAL0_LSB);
    return true;
  }
  else if (channel == 1)
  {
    writeRegisterMasked(REG_CH1_OCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH1_OCAL_LSB, LSB << 8, REGMASK_CHX_OCAL0_LSB);
    return true;
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    writeRegisterMasked(REG_CH2_OCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH2_OCAL_LSB, LSB << 8, REGMASK_CHX_OCAL0_LSB);
    return true;
  }
  else if (channel == 3)
  {
    writeRegisterMasked(REG_CH3_OCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH3_OCAL_LSB, LSB << 8, REGMASK_CHX_OCAL0_LSB);
    return true;
  }
#endif
  return false;
}

/// @brief set gain calibration per channel
/// @param channel
/// @param gain
/// @return
bool ADS131M0x::setChannelGainCalibration(uint8_t channel, uint32_t gain)
{

  uint16_t MSB = gain >> 8;
  uint8_t LSB = gain;

  if (channel == 0)
  {
    writeRegisterMasked(REG_CH0_GCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH0_GCAL_LSB, LSB << 8, REGMASK_CHX_GCAL0_LSB);
    return true;
  }
  else if (channel == 1)
  {
    writeRegisterMasked(REG_CH1_GCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH1_GCAL_LSB, LSB << 8, REGMASK_CHX_GCAL0_LSB);
    return true;
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    writeRegisterMasked(REG_CH2_GCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH2_GCAL_LSB, LSB << 8, REGMASK_CHX_GCAL0_LSB);
    return true;
  }
  else if (channel == 3)
  {
    writeRegisterMasked(REG_CH3_GCAL_MSB, MSB, 0xFFFF);
    writeRegisterMasked(REG_CH3_GCAL_LSB, LSB << 8, REGMASK_CHX_GCAL0_LSB);
    return true;
  }
#endif
  return false;
}

/// @brief hardware-pin test if data is ready
/// @return
bool ADS131M0x::isDataReady()
{
  if (digitalRead(DRDY_PIN) == HIGH)
  {
    return false;
  }
  return true;
}

/**
 * @brief Reset device from register, read CAP 8.5.1.10 Commands from official 
 * documentation
 * 
 * @retval true 
 * The device responded with the RSP_RESET_OK message
 * @retval false 
 * The device did not respond with the RSP_RESET_OK message
 */
bool ADS131M0x::resetDevice(void)
{
  uint8_t x = 0;
  uint8_t x2 = 0;
  uint16_t ris = 0;

  this->startSPI();

  x = spiPort->transfer(0x00);
  x2 = spiPort->transfer(0x11);
  spiPort->transfer(0x00);

  ris = ((x << 8) | x2);

  this->endSPI();

  if (RSP_RESET_OK == ris)
  {
    return true;
  }
  return false;
}

/**
 * @brief Read ADC port (all Ports)
 * 
 */
adcOutput ADS131M0x::readADC(void)
{
#ifndef IS_M02
  // 1 word (status) + 4 (channels) + 1 word (crc) = 18 bytes (3 bytes per word)
  static constexpr uint8_t BYTES_IN_FRAME = 18;
#else
  // 1 word (status) + 2 (channels) + 1 word (crc) = 12 bytes (3 bytes per word)
  static constexpr uint8_t BYTES_IN_FRAME = 12;
#endif

  // Each transaction is 4 24-bit words
  uint8_t buffer[BYTES_IN_FRAME] = { 0 };

  this->startSPI();

  spiPort->transfer(buffer, sizeof(buffer));

  this->endSPI();

  adcOutput res;

  // Status is the first 16 bits of the first 24-bit word
  res.status = ((buffer[0] << 8) | buffer[1]);

  // Channel 0 Data is the second word
  res.ch0 = this->convertBytesToInt32(buffer[3], buffer[4], buffer[5]);

  // Channel 1 Data is the third word
  res.ch1 = this->convertBytesToInt32(buffer[6], buffer[7], buffer[8]);

#ifdef IS_M02
  uint32_t crc = ((buffer[9] << 16) | (buffer[10] << 8) | buffer[11]);
  (void)crc; // TODO: validate CRC
#else
  // Channel 2 Data is the fourth word
  res.ch2 = this->convertBytesToInt32(buffer[9], buffer[10], buffer[11]);

  // Channel 3 Data is the fifth word
  res.ch3 = this->convertBytesToInt32(buffer[12], buffer[13], buffer[14]);

  uint32_t crc = ((buffer[15] << 16) | (buffer[16] << 8) | buffer[17]);
  (void)crc; // TODO: validate CRC
#endif

  return res;
}

void ADS131M0x::startSPI() const
{
  spiPort->beginTransaction(SPISettings(spiClockSpeed, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_PIN, LOW);
#ifndef NO_CS_DELAY
  delayMicroseconds(1);
#endif
}

void ADS131M0x::endSPI() const
{
  digitalWrite(CS_PIN, HIGH);
#ifndef NO_CS_DELAY
  delayMicroseconds(1);
#endif
  spiPort->endTransaction();
}

/**
 * @brief Write to ADC131M01 or ADC131M04 register
 * 
 * @param address 
 * @param value 
 * @return uint8_t 
 */
uint8_t ADS131M0x::writeRegister(const uint8_t address, const uint16_t value)
{
  uint16_t res;
  uint8_t addressRcv;
  uint8_t bytesRcv;
  uint16_t cmd = 0;

  this->startSPI();

  cmd = (CMD_WRITE_REG) | (address << 7) | 0;

  //res = spiPort->transfer16(cmd);
  spiPort->transfer16(cmd);
  spiPort->transfer(0x00);

  spiPort->transfer16(value);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

#ifndef IS_M02
  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#endif

  res = spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#ifndef IS_M02
  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#endif

  this->endSPI();

  addressRcv = (res & CMDMASK_RWREG_ADDRESS) >> 7;
  bytesRcv = (res & CMDMASK_RWREG_NBYTES);

  if (addressRcv == address)
  {
    return bytesRcv + 1;
  }

  return 0;
}

/**
 * @brief Write a value to the register, applying the mask to touch only the necessary bits.
 * It does not carry out the shift of bits (shift), it is necessary to pass the shifted value to the correct position
 *
 * @param address
 * @param value
 * @param mask
 */
void ADS131M0x::writeRegisterMasked(const uint8_t address, const uint16_t value, const uint16_t mask)
{
  //Read the current content of the register
  uint16_t register_contents = readRegister(address);
  // Change the mask bit by bit (it remains 1 in the bits that must not be touched and 0 in the bits to be modified)
  // An AND is performed with the current content of the record. "0" remain in the part to be modified
  register_contents &= ~mask;
  // OR is made with the value to load in the registry. value must be in the correct position (shift)
  register_contents |= (value & mask);
  writeRegister(address, register_contents);
}

/**
 * @brief 
 *
 * @param address
 * @return uint16_t
 */
uint16_t ADS131M0x::readRegister(const uint8_t address)
{
  uint16_t cmd;
  uint16_t data;

  cmd = CMD_READ_REG | (address << 7 | 0);

  this->startSPI();

  spiPort->transfer16(cmd);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#ifndef IS_M02
  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#endif

  this->endSPI();
  this->startSPI();

  data = spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#ifndef IS_M02
  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);

  spiPort->transfer16(0x0000);
  spiPort->transfer(0x00);
#endif

  this->endSPI();

  return data;
}

int32_t ADS131M0x::convertBytesToInt32(
  const uint8_t msb, const uint8_t mid, const uint8_t lsb)
{
  uint32_t value = 
    (static_cast<uint32_t>(msb) << 16) |
    (static_cast<uint32_t>(mid) << 8) |
    static_cast<uint32_t>(lsb);

  // Check if the value is negative (if the most significant bit is set)
  if (value & 0x00800000)
  {
    // Perform sign extension for negative values
    value |= 0xFF000000;
  }

  return static_cast<int32_t>(value);
}
