// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ze/cameras/camera_rig.hpp>

#include <imp/bridge/opencv/cv_bridge.hpp>
#include <imp/core/image_raw.hpp>
#include <ze/cameras/camera_utils.hpp>
#include <ze/cameras/camera_yaml_serialization.hpp>
#include <ze/common/path_utils.hpp>

DEFINE_string(calib_filename, "", "Camera calibration file.");
DEFINE_string(mask_cam0, "", "Mask for camera 0");
DEFINE_string(mask_cam1, "", "Mask for camera 1");
DEFINE_string(mask_cam2, "", "Mask for camera 2");
DEFINE_string(mask_cam3, "", "Mask for camera 3");
DEFINE_bool(calib_use_single_camera, false, "");

namespace ze {

// -----------------------------------------------------------------------------
CameraRig::CameraRig(
    const TransformationVector& T_C_B,
    const CameraVector& cameras,
    const std::string& label,
    const real_t stereo_min_fov_overlap,
    const real_t stereo_min_baseline)
  : T_C_B_(T_C_B)
  , cameras_(cameras)
  , label_(label)
{
  CHECK_EQ(T_C_B_.size(), cameras_.size());
  for(size_t i = 0; i < size(); ++i)
  {
    CHECK_NOTNULL(cameras_[i].get());
  }

  if (size() > 1u)
  {
    setStereoPairs(identifyStereoPairsInRig(
                     *this, stereo_min_fov_overlap, stereo_min_baseline));
  }
}

// -----------------------------------------------------------------------------
CameraRig::Ptr CameraRig::getSubRig(
    const std::vector<uint32_t>& camera_indices,
    const std::string& label)
{
  CameraVector cameras;
  TransformationVector T;
  for (uint32_t i : camera_indices)
  {
    cameras.push_back(atShared(i));
    T.push_back(T_C_B(i));
  }
  return std::make_shared<CameraRig>(T, cameras, label);
}

// -----------------------------------------------------------------------------
CameraRig::Ptr cameraRigFromYaml(const std::string& yaml_file)
{
  CHECK(fileExists(yaml_file)) << "File does not exist: " << yaml_file;
  CameraRig::Ptr rig;
  try
  {
    YAML::Node doc = YAML::LoadFile(yaml_file.c_str());
    rig = doc.as<CameraRig::Ptr>();
  }
  catch (const std::exception& ex)
  {
    LOG(ERROR) << "Cannot load CameraRig from file:" << yaml_file << "\n"
               << ex.what();
    return nullptr;
  }

  return rig;
}

// -----------------------------------------------------------------------------
CameraRig::Ptr cameraRigFromGflags()
{
  CHECK(fileExists(FLAGS_calib_filename)) << "Camera file does not exist.";
  CameraRig::Ptr rig = cameraRigFromYaml(FLAGS_calib_filename);
  CHECK(rig);
  if (FLAGS_calib_use_single_camera)
  {
    rig = rig->getSubRig({0}, rig->label());
    CHECK(rig);
  }

  if(!FLAGS_mask_cam0.empty())
  {
    CHECK_GT(rig->size(), 0u);
    CHECK(fileExists(FLAGS_mask_cam0));
    ImageCv8uC1::Ptr mask;
    cvBridgeLoad<Pixel8uC1>(mask, FLAGS_mask_cam0, PixelOrder::gray);
    rig->atShared(0)->setMask(mask);
  }

  if(!FLAGS_mask_cam1.empty())
  {
    CHECK_GT(rig->size(), 1u);
    CHECK(fileExists(FLAGS_mask_cam1));
    ImageCv8uC1::Ptr mask;
    cvBridgeLoad<Pixel8uC1>(mask, FLAGS_mask_cam1, PixelOrder::gray);
    rig->atShared(1)->setMask(mask);
  }

  if(!FLAGS_mask_cam2.empty())
  {
    CHECK_GT(rig->size(), 2u);
    CHECK(fileExists(FLAGS_mask_cam2));
    ImageCv8uC1::Ptr mask;
    cvBridgeLoad<Pixel8uC1>(mask, FLAGS_mask_cam2, PixelOrder::gray);
    rig->atShared(2)->setMask(mask);
  }

  if(!FLAGS_mask_cam3.empty())
  {
    CHECK_GT(rig->size(), 3u);
    CHECK(fileExists(FLAGS_mask_cam3));
    ImageCv8uC1::Ptr mask;
    cvBridgeLoad<Pixel8uC1>(mask, FLAGS_mask_cam3, PixelOrder::gray);
    rig->atShared(3)->setMask(mask);
  }

  return rig;
}

// -----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const CameraRig& rig)
{
  out << "Camera Rig: \n"
      << "  Label = " << rig.label() << "\n"
      << "  Stereo pairs =" << rig.stereoPairs() << "\n";
  for (size_t i = 0; i < rig.size(); ++i)
  {
    out << "- Camera " << i << "\n"
        << rig.at(i) << "\n"
        << "    T_B_C = \n" << rig.T_C_B(i).inverse() << "\n";
  }
  return out;
}

// -----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const StereoIndexPairs& stereo_pairs)
{
  for (auto it : stereo_pairs)
  {
    out << " (" << static_cast<int>(it.first) << ", "
        << static_cast<int>(it.second) << ")";
  }
  return out;
}

// -----------------------------------------------------------------------------
StereoIndexPairs identifyStereoPairsInRig(
    const CameraRig& rig,
    const real_t& min_fov_overlap,
    const real_t& min_baseline)
{
  StereoIndexPairs pairs;
  for (uint32_t cam_A = 0u; cam_A < rig.size(); ++cam_A)
  {
    for (uint32_t cam_B = cam_A + 1u; cam_B < rig.size(); ++cam_B)
    {
      real_t overlap = overlappingFieldOfView(rig, cam_A, cam_B);
      real_t baseline = (rig.T_C_B(cam_B) * rig.T_C_B(cam_A).inverse()).getPosition().norm();


      if (overlap > min_fov_overlap && baseline > min_baseline)
      {
        VLOG(1) << "Camera " << cam_A << " and " << cam_B
                << ": Overlap = " << overlap << ", Baseline = " << baseline
                << " -> Stereo Rig.";
        pairs.push_back(std::make_pair(cam_A, cam_B));
      }
      else
      {
        VLOG(1) << "Camera " << cam_A << " and " << cam_B
                << ": Overlap = " << overlap << ", Baseline = " << baseline
                << " -> No stereo rig (baseline or overlap too small)";
      }
    }
  }
  return pairs;
}

} // namespace ze
