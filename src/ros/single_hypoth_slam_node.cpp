#include <iostream>
#include <memory>
#include <random>

#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/OccupancyGrid.h>

#include "topic_with_transform.h"
#include "laser_scan_observer.h"
#include "init_utils.h"

#include "launch_properties_provider.h"
#include "../utils/init_slam.h"
#include "../core/maps/plain_grid_map.h"

using ObservT = sensor_msgs::LaserScan;
// TODO: make map type configurable via properties
using SlamT = SingleStateHypothesisLaserScanGridWorld<UnboundedPlainGridMap>;
using SlamMapT = SlamT::MapType;

std::shared_ptr<PropertiesProvider> init_properties_provider() {
  // TODO: merge both launch and file properties in a single provider
  auto launch_pp = LaunchPropertiesProvider{};
  auto cfg_path = launch_pp.get_str("slam/config", "");
  if (cfg_path.empty()) {
    return std::make_shared<decltype(launch_pp)>(launch_pp);
  }
  auto file_pp = FilePropertiesProvider{};
  file_pp.append_file_content(cfg_path);
  return std::make_shared<decltype(file_pp)>(file_pp);
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "1H_SLAM");

  auto props = init_properties_provider();
  auto slam = init_1h_slam<SlamMapT>(*props);

  // connect the slam to a ros-topic based data provider
  ros::NodeHandle nh;
  double ros_map_publishing_rate, ros_tf_buffer_size;
  int ros_filter_queue, ros_subscr_queue;
  init_constants_for_ros(ros_tf_buffer_size, ros_map_publishing_rate,
                         ros_filter_queue, ros_subscr_queue);
  auto scan_provider = std::make_unique<TopicWithTransform<ObservT>>(
    nh, laser_scan_2D_ros_topic_name(*props), tf_odom_frame_id(*props),
    ros_tf_buffer_size, ros_filter_queue, ros_subscr_queue
  );
  auto occup_grid_pub_pin = create_occupancy_grid_publisher<SlamMapT>(
    slam.get(), nh, ros_map_publishing_rate);

  auto pose_pub_pin = create_pose_correction_tf_publisher<ObservT, SlamMapT>(
    slam.get(), scan_provider.get(), *props);
  auto rp_pub_pin = create_robot_pose_tf_publisher<SlamMapT>(slam.get());

  auto scan_obs = std::make_shared<LaserScanObserver>(
    slam, get_skip_exceeding_lsr(*props), get_use_trig_cache(*props));
  // NB: pose_pub_pin must be subscribed first
  scan_provider->subscribe(scan_obs);

  ros::spin();
}