#include "../include/scans_merger.h"

using namespace obstacle_detector;

void ScansMerger::updateParams(const ros::TimerEvent& event) {
  static bool first_call = true;

  if (first_call) {
    if (!nh_local_.getParam("front_scan_topic", p_front_scan_topic_))
      p_front_scan_topic_ = "front_scan";

    if (!nh_local_.getParam("rear_scan_topic", p_rear_scan_topic_))
      p_rear_scan_topic_ = "rear_scan";

    if (!nh_local_.getParam("pcl_topic", p_pcl_topic_))
      p_pcl_topic_ = "pcl_from_scans";

    first_call = false;
  }

  if (!nh_local_.getParam("frame_id", p_frame_id_))
    p_frame_id_ = "base";

  if (!nh_local_.getParam("omit_overlapping_scans", p_omit_overlapping_scans_))
    p_omit_overlapping_scans_ = true;

  if (!nh_local_.getParam("scanners_separation", p_scanners_separation_))
    p_scanners_separation_ = 0.45;

  if (!nh_local_.getParam("max_unreceived_scans", p_max_unreceived_scans_))
    p_max_unreceived_scans_ = 1;
}

ScansMerger::ScansMerger() : nh_(), nh_local_("~") {
  updateParams(ros::TimerEvent());

  front_scan_sub_ = nh_.subscribe<sensor_msgs::LaserScan>(p_front_scan_topic_, 5, &ScansMerger::frontScanCallback, this);
  rear_scan_sub_ = nh_.subscribe<sensor_msgs::LaserScan>(p_rear_scan_topic_, 5, &ScansMerger::rearScanCallback, this);
  pcl_pub_ = nh_.advertise<sensor_msgs::PointCloud>(p_pcl_topic_, 5);
  params_tim_ = nh_.createTimer(ros::Duration(0.5), &ScansMerger::updateParams, this);

  first_scan_received_ = false;
  second_scan_received_ = false;
  unreceived_scans1_ = 0;
  unreceived_scans2_ = 0;
}

void ScansMerger::frontScanCallback(const sensor_msgs::LaserScan::ConstPtr& scan) {
  geometry_msgs::Point32 point;

  float phi = scan->angle_min;

  for (const float r : scan->ranges) {
    if (r > scan->range_min && r < scan->range_max) {
      point.x = r * cos(phi) + p_scanners_separation_ / 2.0; // Y_|X
      point.y = r * sin(phi);

      if (!(p_omit_overlapping_scans_ && point.x < 0.0))
        pcl_msg_.points.push_back(point);
    }
    phi += scan->angle_increment;
  }

  first_scan_received_ = true;

  if (second_scan_received_ || unreceived_scans2_ > p_max_unreceived_scans_) {
    publishPCL();

    unreceived_scans1_ = 0;
  }
  else unreceived_scans2_++;
}

void ScansMerger::rearScanCallback(const sensor_msgs::LaserScan::ConstPtr& scan) {
  geometry_msgs::Point32 point;

  float phi = scan->angle_min;

  for (const float r : scan->ranges) {
    if (r > scan->range_min && r < scan->range_max) {
      point.x = -r * cos(phi) - p_scanners_separation_ / 2.0; // Y_|X
      point.y = -r * sin(phi);

      if (!(p_omit_overlapping_scans_ && point.x > 0.0))
        pcl_msg_.points.push_back(point);
    }
    phi += scan->angle_increment;
  }

  second_scan_received_ = true;

  if (first_scan_received_ || unreceived_scans1_ > p_max_unreceived_scans_) {
    publishPCL();

    unreceived_scans2_ = 0;
  }
  else unreceived_scans1_++;
}

void ScansMerger::publishPCL() {
  pcl_msg_.header.frame_id = p_frame_id_;
  pcl_msg_.header.stamp = ros::Time::now();
  pcl_pub_.publish(pcl_msg_);
  pcl_msg_.points.clear();

  // There is no need to omit overlapping scans from one scan
  p_omit_overlapping_scans_ = (unreceived_scans1_ <= p_max_unreceived_scans_ && unreceived_scans2_ <= p_max_unreceived_scans_);

  first_scan_received_ = false;
  second_scan_received_ = false;
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "scans_merger");
  ScansMerger S2P;

  ROS_INFO("Starting Scans Merger.");
  ros::spin();

  return 0;
}