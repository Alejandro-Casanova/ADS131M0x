#include "ADS131M0x.h"
#include "CRC.h"

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

  // Initialize SPI in case it is not done externally
  // WARNING: SPI implementation should handle double initialization gracefully
  spiPort->begin();
}

bool ADS131M0x::disableAllChannels()
{
  static constexpr uint16_t ALL_CHANNELS_MASK = 
    REGMASK_CLOCK_CH0_EN | REGMASK_CLOCK_CH1_EN
#ifndef IS_M02
    | REGMASK_CLOCK_CH2_EN | REGMASK_CLOCK_CH3_EN
#endif
    ;
  return writeRegisterMasked(REG_CLOCK, UINT16_C(0x00), ALL_CHANNELS_MASK);
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
 * @brief Reset device from register, read CAP 8.5.1.10 Commands from official 
 * documentation
 * 
 * @retval true 
 * The device responded with the RSP_RESET_OK message
 * @retval false 
 * The device did not respond with the RSP_RESET_OK message
 */
bool ADS131M0x::resetSoft(void)
{
  // TODO: review implementation. Full frame must be sent so reset is latched
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
 * @brief read reset status (see datasheet)
 * 
 * @return true 
 * @return false 
 */
bool ADS131M0x::isResetStatus(void)
{
  uint16_t statusRegister;
  if (!readRegister(REG_STATUS, statusRegister))
  {
    return false;  // Return false if the register read fails
  }

  return (statusRegister & REGMASK_STATUS_RESET);
}

/**
 * @brief software test of ADC data is ready
 * 
 * @param channel 
 * @return bool 
 */
bool ADS131M0x::isDataReadySoft(byte channel)
{
  uint16_t statusRegister;
  if (!readRegister(REG_STATUS, statusRegister))
  {
    return false;  // Return false if the register read fails
  }
  
  // Check the appropriate DRDY bit based on the channel number
  if (channel == 0)
  {
    return (statusRegister & REGMASK_STATUS_DRDY0);
  }
  else if (channel == 1)
  {
    return (statusRegister & REGMASK_STATUS_DRDY1);
  }
#ifndef IS_M02
  else if (channel == 2)
  {
    return (statusRegister & REGMASK_STATUS_DRDY2);
  }
  else if (channel == 3)
  {
    return (statusRegister & REGMASK_STATUS_DRDY3);
  }
#endif

  return false;  // Return false for invalid channel numbers
}

/**
 * @brief read locked status (see datasheet)
 *
 * @return true
 * @return false
 */
bool ADS131M0x::isLockSPI(void)
{
  uint16_t statusRegister;
  if (!readRegister(REG_STATUS, statusRegister))
  {
    return false;  // Return false if the register read fails
  }

  return (statusRegister & REGMASK_STATUS_LOCK);
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

  return writeRegisterMasked(REG_CLOCK, osr << 2, REGMASK_CLOCK_OSR);
}

bool ADS131M0x::getChannelCount(uint8_t &numChannelsOut)
{
  uint16_t id_reg;
  if (!readRegister(REG_ID, id_reg))
  {
    return false;  // Return false if the register read fails
  }

  numChannelsOut = static_cast<uint8_t>(
    (id_reg & REGMASK_ID_CHANCNT) >> 8);

  return true;
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
  switch (channel)
  {
    case 0:
      return writeRegisterMasked(REG_CLOCK, enable << 8, REGMASK_CLOCK_CH0_EN);
    case 1:
      return writeRegisterMasked(REG_CLOCK, enable << 9, REGMASK_CLOCK_CH1_EN);
#ifndef IS_M02
    case 2:
      return writeRegisterMasked(REG_CLOCK, enable << 10, REGMASK_CLOCK_CH2_EN);
    case 3:
      return writeRegisterMasked(REG_CLOCK, enable << 11, REGMASK_CLOCK_CH3_EN);
#endif
    default:
      return false;
  }
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
  switch (channel)
  {
    case 0:
      return writeRegisterMasked(REG_GAIN, pga, REGMASK_GAIN_PGAGAIN0);
    case 1:
      return writeRegisterMasked(REG_GAIN, pga << 4, REGMASK_GAIN_PGAGAIN1);
#ifndef IS_M02
    case 2:
      return writeRegisterMasked(REG_GAIN, pga << 8, REGMASK_GAIN_PGAGAIN2);
    case 3:
      return writeRegisterMasked(REG_GAIN, pga << 12, REGMASK_GAIN_PGAGAIN3);
#endif
    default:
      return false;
  }
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
  if (LOW == digitalRead(DRDY_PIN))
  {
    return true;
  }
  return false;
}

/**
 * @brief Read ADC port (all Ports)
 * 
 * @param adcOut Reference to adcOutput structure to store the read values
 * 
 * @return true if the read operation was successful, false otherwise
 */
bool ADS131M0x::readADC(adcOutput &adcOut)
{
  FrameType buffer;

  if (!processFullFrame(buffer))
  {
    // Return false if frame processing fails (probably due to CRC failure)
    return false;
  }

  adcOut = adcOutput{}; // Initialize adcOut with default values

  // Status is the first 16 bits of the first 24-bit word
  adcOut.status = this->bytesToUint16(buffer[0], buffer[1]);

  // Channel 0 Data is the second word
  adcOut.ch0 = this->bytesToInt32(buffer[3], buffer[4], buffer[5]);

  // Channel 1 Data is the third word
  adcOut.ch1 = this->bytesToInt32(buffer[6], buffer[7], buffer[8]);

#ifndef IS_M02
  // Channel 2 Data is the fourth word
  adcOut.ch2 = this->bytesToInt32(buffer[9], buffer[10], buffer[11]);

  // Channel 3 Data is the fifth word
  adcOut.ch3 = this->bytesToInt32(buffer[12], buffer[13], buffer[14]);
#endif

  return true;
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
 * @param address Register address to write to
 * @param value Value to write to the register
 * @return true if the write operation was successful, false otherwise 
 */
bool ADS131M0x::writeRegister(const uint8_t address, const uint16_t value)
{
  static constexpr uint8_t NUM_REGISTERS = UINT8_C(1);  // Single register write
  const uint16_t cmd = (CMD_WRITE_REG) | (address << 7) | (NUM_REGISTERS - 1);

  // Two frames are needed. Write command is sent in the first frame, and the
  // response is received in the second frame.

  // Short frame (2 words) will be used for the write command.
  uint8_t cmdBuff[2 * BYTES_IN_WORD] = { 0 };

  // Append the command to the beginning of the buffer
  cmdBuff[0] = this->extractMSB(cmd);
  cmdBuff[1] = this->extractLSB(cmd);

  // Add the value to write to the buffer
  cmdBuff[3] = this->extractMSB(value);
  cmdBuff[4] = this->extractLSB(value);

  this->startSPI();
  spiPort->transfer(cmdBuff, sizeof(cmdBuff));
  this->endSPI();

  // Status is the first 16 bits of the first 24-bit word
  const uint16_t status = this->bytesToUint16(cmdBuff[0], cmdBuff[1]);
  (void)status;  // TODO: check status...?

  // Process full frame for the response, in order to include CRC validation.
  FrameType rspBuff;
  if (!processFullFrame(rspBuff))
  {
    return false; // Failure (probably CRC failure)
  }

  const uint16_t response = this->bytesToUint16(rspBuff[0], rspBuff[1]);

  // Check for response ok code
  if (RSP_WREG_OK != (response & CMDMASK_RWREG_RESPONSE))
  {
    return false;
  }

  // Response code is ok, now check if the address in the response matches the
  // requested address and if the number of bytes matches the expected value.
  const uint8_t addressRcv =
    static_cast<uint8_t>((response & CMDMASK_RWREG_ADDRESS) >> 7);
  const uint8_t bytesRcv =
    static_cast<uint8_t>(response & CMDMASK_RWREG_NBYTES) + 1;

  if (addressRcv != address || NUM_REGISTERS != bytesRcv)
  {
    return false;
  }

  return true; // Success!
}

/**
 * @brief Write a value to the register, applying the mask to touch only the necessary bits.
 * It does not carry out the shift of bits (shift), it is necessary to pass the shifted value to the correct position
 *
 * @param address
 * @param value
 * @param mask
 * 
 * @return Number of registers successfully written. Should be 1.
 */
bool ADS131M0x::writeRegisterMasked(
  const uint8_t address, const uint16_t value, const uint16_t mask)
{
  // Read the current content of the register
  uint16_t registerContents = 0;
  if (!readRegister(address, registerContents))
  {
    return false; // Failure to read the register
  }

  // Change the mask bit by bit (it remains 1 in the bits that must not be 
  // touched and 0 in the bits to be modified). An AND is performed with the 
  // current content of the record. "0" remain in the part to be modified
  registerContents &= ~mask;

  // OR is made with the value to load in the registry. value must be in the 
  // correct position (shift)
  registerContents |= (value & mask);

  return writeRegister(address, registerContents);
}

/**
 * @brief Read the value of a single register, specified by the address 
 *  parameter.
 *
 * @param address The address of the register to read
 * @param valueOut Reference to a variable where the read value will be stored
 * @return true if the read operation was successful, false otherwise
 */
bool ADS131M0x::readRegister(const uint8_t address, uint16_t &valueOut)
{
  static constexpr uint8_t NUM_REGISTERS = UINT8_C(1);  // Single register read
  const uint16_t cmd = CMD_READ_REG | (address << 7) | (NUM_REGISTERS - 1);

  // Two frames are needed. Write command is sent in the first frame, and the
  // response is received in the second frame.

  // Short frame (just 1 word) will be used for the read command.
  uint8_t cmdBuff[BYTES_IN_WORD] = { 0 };

  // Append the command to the beginning of the buffer
  cmdBuff[0] = this->extractMSB(cmd);
  cmdBuff[1] = this->extractLSB(cmd);

  this->startSPI();
  spiPort->transfer(cmdBuff, sizeof(cmdBuff));
  this->endSPI();

  // Status is the first 16 bits of the first 24-bit word
  const uint16_t status = this->bytesToUint16(cmdBuff[0], cmdBuff[1]);
  (void)status;  // TODO: check status...?

  // Process full frame for the response, in order to include CRC validation.
  FrameType rspBuff;
  if (!processFullFrame(rspBuff))
  {
    return false; // Failure (probably CRC failure)
  }

  valueOut = this->bytesToUint16(rspBuff[0], rspBuff[1]);

  return true; // Success!
}

bool ADS131M0x::processFullFrame(FrameType &frameOut)
{
  // Set frame to all zeros before sending (NULL command)
  frameOut.fill(static_cast<FrameType::value_type>(0));

  this->startSPI();
  spiPort->transfer(frameOut.data(), frameOut.size());
  this->endSPI();

  // Response is now in frameOut. Next, validate the CRC.

  const uint16_t crcRcv = this->bytesToUint16(
    frameOut[CRC_FRAME_OFFSET], frameOut[CRC_FRAME_OFFSET + 1]);

  static_assert(
    std::tuple_size<FrameType>::value > CRC_FRAME_OFFSET, 
    "CRC frame offset is out of bounds of the response buffer");
  const uint16_t crcComputed = ComputeCrc(frameOut.data(), CRC_FRAME_OFFSET);

  if (crcRcv != crcComputed)
  {
    return false;
  }

  return true; // CRC is valid, frame is processed successfully
}

int32_t ADS131M0x::bytesToInt32(
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
