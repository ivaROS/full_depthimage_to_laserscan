/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * Author: Chad Rockey
 */

#include <full_depthimage_to_laserscan/DepthImageToLaserScan.h>

using namespace full_depthimage_to_laserscan;
  
DepthImageToLaserScan::DepthImageToLaserScan(){
}

DepthImageToLaserScan::~DepthImageToLaserScan(){
}

double DepthImageToLaserScan::magnitude_of_ray(const cv::Point3d& ray) const{
  return sqrt(pow(ray.x, 2.0) + pow(ray.y, 2.0) + pow(ray.z, 2.0));
}

double DepthImageToLaserScan::angle_between_rays(const cv::Point3d& ray1, const cv::Point3d& ray2) const{
  double dot_product = ray1.x*ray2.x + ray1.y*ray2.y + ray1.z*ray2.z;
  double magnitude1 = magnitude_of_ray(ray1);
  double magnitude2 = magnitude_of_ray(ray2);;
  return acos(dot_product / (magnitude1 * magnitude2));
}

bool DepthImageToLaserScan::use_point_old(const float new_value, const float old_value, const float range_min, const float range_max) const{  
  // Check for NaNs and Infs, a real number within our limits is more desirable than these.
  bool new_finite = std::isfinite(new_value);
  bool old_finite = std::isfinite(old_value);
  
  // Infs are preferable over NaNs (more information)
  if(!new_finite && !old_finite){ // Both are not NaN or Inf.
    if(!std::isnan(new_value)){ // new is not NaN, so use it's +-Inf value.
      return true;
    }
    return false; // Do not replace old_value
  }
  
  // If not in range, don't bother
  bool range_check = range_min <= new_value && new_value <= range_max;
  if(!range_check){
    return false;
  }
  
  if(!old_finite){ // New value is in range and finite, use it.
    return true;
  }
  
  // Finally, if they are both numerical and new_value is closer than old_value, use new_value.
  bool shorter_check = new_value < old_value;
  return shorter_check;
}

bool DepthImageToLaserScan::use_point_new(const float new_value, const float old_value, const float range_min, const float range_max) const{  
  return true;
  
}

void DepthImageToLaserScan::updateCache()
{
  const sensor_msgs::CameraInfoConstPtr info_msg;
  
  //Can only update cache if we know what the previous conditions were
  if(cache_.limits)
  {
    updateCache(cache_.limits, info_msg);
  }
}

void DepthImageToLaserScan::updateCache(const sensor_msgs::ImageConstPtr& depth_msg, const sensor_msgs::CameraInfoConstPtr& info_msg)
{
  bool camera_params_changed=false;
  bool data_type_changed=false;
  bool buffer_size_changed=false;
  bool safe_limits_changed=false;
  bool range_min_changed=false;
  
  //First, determine if camera parameters have changed:
  if(info_msg && cam_model_.fromCameraInfo(info_msg))
  {
    cam_model_.init();
    camera_params_changed=true;
  }
  
  if(depth_msg && cache_.limits && depth_msg->encoding != cache_.limits->encoding)
  {
    data_type_changed=true;
  }
  
  if(scan_height_ != cache_.scan_height)
  {
    buffer_size_changed=true;
  }
  
  if(floor_dist_ != cache_.floor_dist || overhead_dist_!=cache_.overhead_dist)
  {
    safe_limits_changed=true;
  }
  
  if(range_min_ != cache_.range_min)
  {
    range_min_changed=true;
  }
  
  if(camera_params_changed || safe_limits_changed || data_type_changed)
  {
    ROS_INFO_STREAM("Updating safe limits");
    update_limits(depth_msg);
  }
  
  if(camera_params_changed)
  {
    ROS_INFO_STREAM("Updating mapping");
    update_mapping(depth_msg);
  }
  
  if(camera_params_changed || range_min_changed || data_type_changed)
  { 
    ROS_INFO_STREAM("Updating min range");
    update_min_range(depth_msg);
  }
  
  if(camera_params_changed || buffer_size_changed || data_type_changed)
  {
    ROS_INFO_STREAM("Updating buffer");
    update_buffer(depth_msg);
  }
  


}

sensor_msgs::LaserScanPtr DepthImageToLaserScan::convert_msg(const sensor_msgs::ImageConstPtr& depth_msg,
      const sensor_msgs::CameraInfoConstPtr& info_msg, int approach, sensor_msgs::ImageConstPtr& image)
{
  //Update cached variables based on current image
  updateCache(depth_msg, info_msg);
  image=cache_.limits;
  
  // Fill in laserscan message
  sensor_msgs::LaserScanPtr scan_msg = boost::make_shared<sensor_msgs::LaserScan>();
  scan_msg->header = depth_msg->header;
  if(output_frame_id_.length() > 0){
    scan_msg->header.frame_id = output_frame_id_;
  }
  scan_msg->angle_min = cache_.angle_min;
  scan_msg->angle_max = cache_.angle_max;
  scan_msg->angle_increment = (scan_msg->angle_max - scan_msg->angle_min) / (depth_msg->width - 1);
  scan_msg->time_increment = 0.0;
  scan_msg->scan_time = scan_time_;
  scan_msg->range_min = range_min_;
  scan_msg->range_max = range_max_;
  
  // Check scan_height vs image_height
  if(scan_height_/2 > cam_model_.cy() || scan_height_/2 > depth_msg->height - cam_model_.cy()){
    std::stringstream ss;
    ss << "scan_height ( " << scan_height_ << " pixels) is too large for the image height.";
    throw std::runtime_error(ss.str());
  }

  // Calculate and fill the ranges
  uint32_t ranges_size = depth_msg->width;
  scan_msg->ranges.assign(ranges_size, std::numeric_limits<float>::quiet_NaN());
  
  /*
  if(approach==0)
  {
    //ROS_INFO_STREAM("APPROACH 0");
    if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_16UC1)
    {
      convert_old<uint16_t>(depth_msg, cam_model_, scan_msg, scan_height_);
    }
    else if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_32FC1)
    {
      convert_old<float>(depth_msg, cam_model_, scan_msg, scan_height_);
    }
    else
    {
      std::stringstream ss;
      ss << "Depth image has unsupported encoding: " << depth_msg->encoding;
      throw std::runtime_error(ss.str());
    }
  }
  else if(approach==1)
    */
  {
    if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_16UC1)
    {
      convert_new<uint16_t>(depth_msg, cam_model_, scan_msg, scan_height_, cache_);
    }
    else if (depth_msg->encoding == sensor_msgs::image_encodings::TYPE_32FC1)
    {
      convert_new<float>(depth_msg, cam_model_, scan_msg, scan_height_, cache_);
    }
    else
    {
      std::stringstream ss;
      ss << "Depth image has unsupported encoding: " << depth_msg->encoding;
      throw std::runtime_error(ss.str());
    }
  }
  
  return scan_msg;
}

void DepthImageToLaserScan::set_scan_time(const float scan_time){
  scan_time_ = scan_time;
}

void DepthImageToLaserScan::set_range_limits(const float range_min, const float range_max){
  range_min_ = range_min;
  range_max_ = range_max;
}

void DepthImageToLaserScan::set_scan_height(const int scan_height){
  scan_height_ = scan_height;
}

void DepthImageToLaserScan::set_output_frame(const std::string output_frame_id){
  output_frame_id_ = output_frame_id;
}

void DepthImageToLaserScan::set_filtering_limits(const float floor_dist, const float overhead_dist)
{
  floor_dist_=floor_dist;
  overhead_dist_=overhead_dist;
}
