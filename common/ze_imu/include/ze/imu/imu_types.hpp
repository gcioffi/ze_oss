// Copyright (C) ETH Zurich, Wyss Zurich, Zurich Eye - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

#pragma once

#include <ze/common/types.hpp>

namespace ze {

//! Extract the Accelerometer component of an ImuAccGyr block.
inline Eigen::VectorBlock<const Vector6, 3> getAcc(const ImuAccGyr& imu)
{
    return imu.head<3>();
}

//! Extract the Gyroscope component of an ImuAccGyr block.
inline Eigen::VectorBlock<const Vector6, 3> getGyr(const ImuAccGyr& imu)
{
  return imu.tail<3>();
}

//! Extract the Accelerometer component of an ImuAccGyr block.
inline Eigen::VectorBlock<Vector6, 3> getAcc(ImuAccGyr& imu)
{
    return imu.head<3>();
}

//! Extract the Gyroscope component of an ImuAccGyr block.
inline Eigen::VectorBlock<Vector6, 3> getGyr(ImuAccGyr& imu)
{
  return imu.tail<3>();
}

} // namespace ze
