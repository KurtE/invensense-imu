/*
* Brian R Taylor
* brian.taylor@bolderflight.com
* 
* Copyright (c) 2024 Bolder Flight Systems Inc
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the “Software”), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include "mpu9150.h"  // NOLINT
#if defined(ARDUINO)
#include <Arduino.h>
#include "Wire.h"
#else
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "core/core.h"
#endif

namespace bfs {

void Mpu9150::Config(TwoWire *i2c, const I2cAddr addr) {
  imu_.Config(i2c, static_cast<uint8_t>(addr));
}
bool Mpu9150::Begin() {
  return Begin(MAG_PASSTHROUGH);
}
bool Mpu9150::Begin(const MagMode mode) {
  mag_mode_ = mode;
  imu_.Begin();
  /* Reset the IMU */
  WriteRegister(PWR_MGMNT_1_, H_RESET_);
  /* Wait for the IMU to come back up */
  delay(100);
  /* Select clock source to gyro */
  if (!WriteRegister(PWR_MGMNT_1_, CLKSEL_PLL_)) {
    return false;
  }
  /* Check the WHO AM I byte */
  if (!ReadRegisters(WHOAMI_, sizeof(who_am_i_), &who_am_i_)) {
    return false;
  }
  if (who_am_i_ != WHOAMI_MPU9150_) {
    return false;
  }
  if (mag_mode_ == MAG_PASSTHROUGH) {
    /* Enable I2C pass through */
    if (!WriteRegister(INT_PIN_CFG_, I2C_BYPASS_EN_)) {
      return false;
    }
  }
  /* Select clock source to gyro */
  if (!WriteRegister(PWR_MGMNT_1_, CLKSEL_PLL_)) {
    return false;
  }
  /* Set the accel range to 16G by default */
  if (!ConfigAccelRange(ACCEL_RANGE_16G)) {
    return false;
  }
  /* Set the gyro range to 2000DPS by default*/
  if (!ConfigGyroRange(GYRO_RANGE_2000DPS)) {
    return false;
  }
  /* Set the DLPF to 184HZ by default */
  if (!ConfigDlpfBandwidth(DLPF_BANDWIDTH_184HZ)) {
    return false;
  }
  /* Set the SRD to 0 by default */
  if (!ConfigSrd(0)) {
    return false;
  }
  return true;
}
bool Mpu9150::EnableDrdyInt() {
  if (mag_mode_ == MAG_PASSTHROUGH) {
    if (!WriteRegister(INT_PIN_CFG_, INT_PULSE_50US_ | I2C_BYPASS_EN_)) {
      return false;
    }
  } else {
    if (!WriteRegister(INT_PIN_CFG_, INT_PULSE_50US_)) {
      return false;
    }
  }
  if (!WriteRegister(INT_ENABLE_, INT_RAW_RDY_EN_)) {
    return false;
  }
  return true;
}
bool Mpu9150::DisableDrdyInt() {
  if (!WriteRegister(INT_ENABLE_, INT_DISABLE_)) {
    return false;
  }
  return true;
}
bool Mpu9150::EnableFifo() {
  if (!WriteRegister(USER_CTRL_, USER_CTRL_FIFO_EN_)) {
    return false;
  }
  if (!WriteRegister(FIFO_EN_, FIFO_EN_GYRO_ | FIFO_EN_ACCEL_)) {
    return false;
  }
  return true;
}
bool Mpu9150::DisableFifo() {
  if (!WriteRegister(USER_CTRL_, USER_CTRL_FIFO_DISABLE_)) {
    return false;
  }
  if (!WriteRegister(FIFO_EN_, FIF_EN_DISABLE_ALL_)) {
    return false;
  }
  return true;
}
bool Mpu9150::ConfigAccelRange(const AccelRange range) {
  /* Check input is valid and set requested range and scale */
  switch (range) {
    case ACCEL_RANGE_2G: {
      requested_accel_range_ = range;
      requested_accel_scale_ = 2.0f / 32767.5f;
      break;
    }
    case ACCEL_RANGE_4G: {
      requested_accel_range_ = range;
      requested_accel_scale_ = 4.0f / 32767.5f;
      break;
    }
    case ACCEL_RANGE_8G: {
      requested_accel_range_ = range;
      requested_accel_scale_ = 8.0f / 32767.5f;
      break;
    }
    case ACCEL_RANGE_16G: {
      requested_accel_range_ = range;
      requested_accel_scale_ = 16.0f / 32767.5f;
      break;
    }
    default: {
      return false;
    }
  }
  /* Try setting the requested range */
  if (!WriteRegister(ACCEL_CONFIG_, requested_accel_range_)) {
    return false;
  }
  /* Update stored range and scale */
  accel_range_ = requested_accel_range_;
  accel_scale_ = requested_accel_scale_;
  return true;
}
bool Mpu9150::ConfigGyroRange(const GyroRange range) {
  /* Check input is valid and set requested range and scale */
  switch (range) {
    case GYRO_RANGE_250DPS: {
      requested_gyro_range_ = range;
      requested_gyro_scale_ = 250.0f / 32767.5f;
      break;
    }
    case GYRO_RANGE_500DPS: {
      requested_gyro_range_ = range;
      requested_gyro_scale_ = 500.0f / 32767.5f;
      break;
    }
    case GYRO_RANGE_1000DPS: {
      requested_gyro_range_ = range;
      requested_gyro_scale_ = 1000.0f / 32767.5f;
      break;
    }
    case GYRO_RANGE_2000DPS: {
      requested_gyro_range_ = range;
      requested_gyro_scale_ = 2000.0f / 32767.5f;
      break;
    }
    default: {
      return false;
    }
  }
  /* Try setting the requested range */
  if (!WriteRegister(GYRO_CONFIG_, requested_gyro_range_)) {
    return false;
  }
  /* Update stored range and scale */
  gyro_range_ = requested_gyro_range_;
  gyro_scale_ = requested_gyro_scale_;
  return true;
}
bool Mpu9150::ConfigSrd(const uint8_t srd) {
  /* Set the IMU sample rate */
  if (!WriteRegister(SMPLRT_DIV_, srd)) {
    return false;
  }
  srd_ = srd;
  return true;
}
bool Mpu9150::ConfigDlpfBandwidth(const DlpfBandwidth dlpf) {
  /* Check input is valid and set requested dlpf */
  switch (dlpf) {
    case DLPF_BANDWIDTH_184HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_92HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_41HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_20HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_10HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_5HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    default: {
      return false;
    }
  }
  /* Try setting the dlpf */
  if (!WriteRegister(ACCEL_CONFIG2_, requested_dlpf_)) {
    return false;
  }
  if (!WriteRegister(CONFIG_, requested_dlpf_)) {
    return false;
  }
  /* Update stored dlpf */
  dlpf_bandwidth_ = requested_dlpf_;
  return true;
}


void Mpu9150::getScales(float *accScale, float *gyroScale, float *magScale){
  *accScale = accel_scale_;
  *gyroScale = gyro_scale_;
  magScale[0] = 0;
  magScale[1] = 0;
  magScale[2] = 0;
/*  
  Serial.print("Accelerometer Scale: "); Serial.println(accel_scale_, 5);
  Serial.print("Gyro Scale: "); Serial.println(gyro_scale_, 5);
  Serial.println("Magnetometer Scales:");
  Serial.print("\tMagx: "); Serial.println(mag_scale_[0],5);
  Serial.print("\tMagx: "); Serial.println(mag_scale_[1],5);
  Serial.print("\tMagx: "); Serial.println(mag_scale_[2],5);
  */
}


bool Mpu9150::Read() {
  /* Reset the new data flags */
  new_imu_data_ = false;
  /* Read the data registers */
  if (!ReadRegisters(INT_STATUS_, sizeof(data_buf_), data_buf_)) {
    return false;
  }
  /* Check if data is ready */
  new_imu_data_ = (data_buf_[0] & RAW_DATA_RDY_INT_);
  if (!new_imu_data_) {
    return false;
  }
  /* Unpack the buffer */
  accel_cnts_[0] = static_cast<int16_t>(data_buf_[1])  << 8 | data_buf_[2];
  accel_cnts_[1] = static_cast<int16_t>(data_buf_[3])  << 8 | data_buf_[4];
  accel_cnts_[2] = static_cast<int16_t>(data_buf_[5])  << 8 | data_buf_[6];
  temp_cnts_ =     static_cast<int16_t>(data_buf_[7])  << 8 | data_buf_[8];
  gyro_cnts_[0] =  static_cast<int16_t>(data_buf_[9])  << 8 | data_buf_[10];
  gyro_cnts_[1] =  static_cast<int16_t>(data_buf_[11]) << 8 | data_buf_[12];
  gyro_cnts_[2] =  static_cast<int16_t>(data_buf_[13]) << 8 | data_buf_[14];
  /* Convert to float values and rotate the accel / gyro axis */
  accel_[0] = static_cast<float>(accel_cnts_[1]) * accel_scale_ * G_MPS2_;
  accel_[1] = static_cast<float>(accel_cnts_[0]) * accel_scale_ * G_MPS2_;
  accel_[2] = static_cast<float>(accel_cnts_[2]) * accel_scale_ * -1.0f *
              G_MPS2_;
  temp_ = (static_cast<float>(temp_cnts_)) / TEMP_SCALE_ + 35.0f;
  gyro_[0] = static_cast<float>(gyro_cnts_[1]) * gyro_scale_ * DEG2RAD_;
  gyro_[1] = static_cast<float>(gyro_cnts_[0]) * gyro_scale_ * DEG2RAD_;
  gyro_[2] = static_cast<float>(gyro_cnts_[2]) * gyro_scale_ * -1.0f * DEG2RAD_;
  return true;
}

bool Mpu9150::Read(float * values) {
  /* Reset the new data flags */
  new_imu_data_ = false;
  /* Read the data registers */
  if (!ReadRegisters(INT_STATUS_, sizeof(data_buf_), data_buf_)) {
    return false;
  }
  /* Check if data is ready */
  new_imu_data_ = (data_buf_[0] & RAW_DATA_RDY_INT_);
  if (!new_imu_data_) {
    return false;
  }
  /* Unpack the buffer */
  accel_cnts_[0] = static_cast<int16_t>(data_buf_[1])  << 8 | data_buf_[2];
  accel_cnts_[1] = static_cast<int16_t>(data_buf_[3])  << 8 | data_buf_[4];
  accel_cnts_[2] = static_cast<int16_t>(data_buf_[5])  << 8 | data_buf_[6];
  temp_cnts_ =     static_cast<int16_t>(data_buf_[7])  << 8 | data_buf_[8];
  gyro_cnts_[0] =  static_cast<int16_t>(data_buf_[9])  << 8 | data_buf_[10];
  gyro_cnts_[1] =  static_cast<int16_t>(data_buf_[11]) << 8 | data_buf_[12];
  gyro_cnts_[2] =  static_cast<int16_t>(data_buf_[13]) << 8 | data_buf_[14];
  /* Convert to float values and rotate the accel / gyro axis */
  accel_[0] = static_cast<float>(accel_cnts_[1]) * accel_scale_ * G_MPS2_;
  accel_[1] = static_cast<float>(accel_cnts_[0]) * accel_scale_ * G_MPS2_;
  accel_[2] = static_cast<float>(accel_cnts_[2]) * accel_scale_ * -1.0f *
              G_MPS2_;
  temp_ = (static_cast<float>(temp_cnts_) - 21.0f) / TEMP_SCALE_ + 21.0f;
  gyro_[0] = static_cast<float>(gyro_cnts_[1]) * gyro_scale_ * DEG2RAD_;
  gyro_[1] = static_cast<float>(gyro_cnts_[0]) * gyro_scale_ * DEG2RAD_;
  gyro_[2] = static_cast<float>(gyro_cnts_[2]) * gyro_scale_ * -1.0f * DEG2RAD_;
  
  /* Convert to float values and rotate the accel / gyro axis */
  values[0] = static_cast<float>(accel_cnts_[1]) * accel_scale_ ;
  values[1] = static_cast<float>(accel_cnts_[0]) * accel_scale_ ;
  values[2] = static_cast<float>(accel_cnts_[2]) * accel_scale_ * -1.0f;
  values[9] = (static_cast<float>(temp_cnts_) - 21.0f) / TEMP_SCALE_ + 21.0f;
  values[3] = static_cast<float>(gyro_cnts_[1]) * gyro_scale_ ;
  values[4] = static_cast<float>(gyro_cnts_[0]) * gyro_scale_ ;
  values[5] = static_cast<float>(gyro_cnts_[2]) * gyro_scale_ * -1.0f ;
  values[7] = temp_;

  return true;
}


bool Mpu9150::Read_raw(int16_t * values) {
  /* Reset the new data flags */
  new_imu_data_ = false;
  /* Read the data registers */
  if (!ReadRegisters(INT_STATUS_, sizeof(data_buf_), data_buf_)) {
    return false;
  }
  /* Check if data is ready */
  new_imu_data_ = (data_buf_[0] & RAW_DATA_RDY_INT_);
  if (!new_imu_data_) {
    return false;
  }
  /* Unpack the buffer */
  //Accel
  values[0] = static_cast<int16_t>(data_buf_[1])  << 8 | data_buf_[2];
  values[1] = static_cast<int16_t>(data_buf_[3])  << 8 | data_buf_[4];
  values[2] = static_cast<int16_t>(data_buf_[5])  << 8 | data_buf_[6];
  //Temp
  values[6] = static_cast<int16_t>(data_buf_[7])  << 8 | data_buf_[8];
  //Gyro
  values[3] =  static_cast<int16_t>(data_buf_[9])  << 8 | data_buf_[10];
  values[4] =  static_cast<int16_t>(data_buf_[11]) << 8 | data_buf_[12];
  values[5] =  static_cast<int16_t>(data_buf_[13]) << 8 | data_buf_[14];
  //Serial.printf("%d, %d, %d, %d, %d, %d, %d, %d\n", data_buf_[0], values[0], values[1], values[2], values[3], values[4], values[5], values[6]);

  return true;
}

int16_t Mpu9150::ReadFifo(uint8_t * const data, const size_t len) {
  if (!data) {
    return -1;
  }
  /* Read the FIFO interrupt */
  if (!ReadRegisters(INT_STATUS_, 1, data_buf_)) {
    return -1;
  }
  /* Check for fifo overflow */
  fifo_overflowed_ = (data_buf_[0] & FIFO_OFLOW_INT_);
  /* FIFO Count */
  if (!ReadRegisters(FIFO_COUNT_H_, 2, data_buf_)) {
    return -1;
  }
  fifo_count_ = static_cast<int16_t>(data_buf_[0]) << 8 | data_buf_[1];
  if (fifo_count_ > 0) {
    if (len < fifo_count_) {
      bytes_to_read_ = len;
    } else {
      bytes_to_read_ = fifo_count_;
    }
    if (!ReadFifo(FIFO_R_W_, bytes_to_read_, data)) {
      return -1;
    }
    return bytes_to_read_;
  } else {
    return 0;
  }
}
int16_t Mpu9150::ProcessFifoData(uint8_t * const data, const size_t len,
                                 float * const gx, float * const gy, float * const gz,
                                 float * const ax, float * const ay, float * const az) {
  if ((!data) || (!gx) || (!gy) || (!gz) || (!ax) || (!ay) || (!az)) {
    return -1;
  }
  size_t j = 0;
  for (size_t i = 0; i < len; i = i + 12) {
    /* Unpack the buffer */
    accel_cnts_[0] = static_cast<int16_t>(data[i + 0])  << 8 | data[i + 1];
    accel_cnts_[1] = static_cast<int16_t>(data[i + 2])  << 8 | data[i + 3];
    accel_cnts_[2] = static_cast<int16_t>(data[i + 4])  << 8 | data[i + 5];
    gyro_cnts_[0] =  static_cast<int16_t>(data[i + 6])  << 8 | data[i + 7];
    gyro_cnts_[1] =  static_cast<int16_t>(data[i + 8]) << 8 | data[i + 9];
    gyro_cnts_[2] =  static_cast<int16_t>(data[i + 10]) << 8 | data[i + 11];
    /* Convert to float values and rotate the accel / gyro axis */
    ax[j] = static_cast<float>(accel_cnts_[1]) * accel_scale_ * G_MPS2_;
    ay[j] = static_cast<float>(accel_cnts_[0]) * accel_scale_ * G_MPS2_;
    az[j] = static_cast<float>(accel_cnts_[2]) * accel_scale_ * -1.0f * G_MPS2_;
    gx[j] = static_cast<float>(gyro_cnts_[1]) * gyro_scale_ * DEG2RAD_;
    gy[j] = static_cast<float>(gyro_cnts_[0]) * gyro_scale_ * DEG2RAD_;
    gz[j] = static_cast<float>(gyro_cnts_[2]) * gyro_scale_ * -1.0f * DEG2RAD_;
    j++;
  }
  return j;
}
bool Mpu9150::WriteRegister(const uint8_t reg, const uint8_t data) {
  return imu_.WriteRegister(reg, data, 0);
}
bool Mpu9150::ReadRegisters(const uint8_t reg, const uint8_t count,
                            uint8_t * const data) {
  return imu_.ReadRegisters(reg, count, 0, data);
}
bool Mpu9150::ReadFifo(const uint8_t reg, const uint8_t count,
                       uint8_t * const data) {
  return imu_.ReadFifo(reg, count, 0, data);
}

}  // namespace bfs
