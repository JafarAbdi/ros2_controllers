///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2014, SRI International
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of SRI International nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

/// \author Sachin Chitta, Adolfo Rodriguez Tsouroukdissian

#pragma once

// C++ standard
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>

// ROS
#include <rclcpp/rclcpp.hpp>

// ROS messages
#include <control_msgs/action/gripper_command.hpp>

// rclcpp_action
#include <rclcpp_action/create_server.hpp>

// ros_controls
#include <controller_interface/controller_interface.hpp>
#include <gripper_action_controller/visibility_control.h>
#include <hardware_interface/loaned_command_interface.hpp>
#include <hardware_interface/loaned_state_interface.hpp>
#include <realtime_tools/realtime_buffer.h>
#include <realtime_tools/realtime_server_goal_handle.h>

// Project
#include <gripper_action_controller/hardware_interface_adapter.h>

namespace gripper_action_controller {

/**
 * \brief Controller for executing a gripper command action for simple
 * single-dof grippers.
 *
 * \tparam HardwareInterface Controller hardware interface. Currently \p
 * hardware_interface::PositionJointInterface and \p
 * hardware_interface::EffortJointInterface are supported out-of-the-box.
 */
template <const char *HardwareInterface>
class GripperActionController
    : public controller_interface::ControllerInterface {
public:
  /**
   * \brief Store position and max effort in struct to allow easier realtime
   * buffer usage
   */
  struct Commands {
    double position_;   // Last commanded position
    double max_effort_; // Max allowed effort
  };

  GRIPPER_ACTION_CONTROLLER_PUBLIC GripperActionController();

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  controller_interface::return_type
  init(const std::string &controller_name) override;

  /**
   * @brief command_interface_configuration This controller requires the
   * position command interfaces for the controlled joints
   */
  GRIPPER_ACTION_CONTROLLER_PUBLIC
  controller_interface::InterfaceConfiguration
  command_interface_configuration() const override;

  /**
   * @brief command_interface_configuration This controller requires the
   * position and velocity state interfaces for the controlled joints
   */
  GRIPPER_ACTION_CONTROLLER_PUBLIC
  controller_interface::InterfaceConfiguration
  state_interface_configuration() const override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  controller_interface::return_type update() override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State &previous_state) override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State &previous_state) override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State &previous_state) override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State &previous_state) override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_error(const rclcpp_lifecycle::State &previous_state) override;

  GRIPPER_ACTION_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State &previous_state) override;

  realtime_tools::RealtimeBuffer<Commands> command_;
  // pre-allocated memory that is re-used to set the realtime buffer
  Commands command_struct_, command_struct_rt_;

private:
  using GripperCommandAction = control_msgs::action::GripperCommand;
  using ActionServer = rclcpp_action::Server<GripperCommandAction>;
  using ActionServerPtr = ActionServer::SharedPtr;
  using GoalHandle = rclcpp_action::ServerGoalHandle<GripperCommandAction>;
  using RealtimeGoalHandle = realtime_tools::RealtimeServerGoalHandle<
      control_msgs::action::GripperCommand>;
  using RealtimeGoalHandlePtr = std::shared_ptr<RealtimeGoalHandle>;

  using HwIfaceAdapter = HardwareInterfaceAdapter<HardwareInterface>;

  bool update_hold_position_;

  bool verbose_ = false; ///< Hard coded verbose flag to help in debugging
  std::string name_;     ///< Controller name.
  hardware_interface::LoanedCommandInterface *joint_position_command_interface_;
  hardware_interface::LoanedStateInterface *joint_position_state_interface_;
  hardware_interface::LoanedStateInterface *joint_velocity_state_interface_;

  std::string joint_name_; ///< Controlled joint names.

  HwIfaceAdapter
      hw_iface_adapter_; ///< Adapts desired goal state to HW interface.

  RealtimeGoalHandlePtr
      rt_active_goal_; ///< Currently active action goal, if any.
  control_msgs::action::GripperCommand::Result::SharedPtr pre_alloc_result_;

  rclcpp::Duration action_monitor_period_;

  // ROS API
  ActionServerPtr action_server_;

  rclcpp::TimerBase::SharedPtr goal_handle_timer_;

  rclcpp_action::GoalResponse
  goal_callback(const rclcpp_action::GoalUUID &uuid,
                std::shared_ptr<const GripperCommandAction::Goal> goal);

  rclcpp_action::CancelResponse
  cancel_callback(const std::shared_ptr<GoalHandle> goal_handle);

  void accepted_callback(std::shared_ptr<GoalHandle> goal_handle);

  void preempt_active_goal();

  void set_hold_position();

  rclcpp::Time last_movement_time_ =
      rclcpp::Time(0, 0, RCL_ROS_TIME); ///< Store stall time
  double computed_command_;             ///< Computed command

  double stall_timeout_,
      stall_velocity_threshold_; ///< Stall related parameters
  double default_max_effort_;    ///< Max allowed effort
  double goal_tolerance_;

  /**
   * \brief Check for success and publish appropriate result and feedback.
   **/
  void check_for_success(const rclcpp::Time &time, double error_position,
                         double current_position, double current_velocity);
};

} // namespace gripper_action_controller
#include <gripper_action_controller/gripper_action_controller_impl.h>