/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Lithesh
 * All rights reserved.
 */

#include <iostream>
#include <iomanip>
#include "sp2_hw/hardware_interface/AsyncSocketCan.hpp"
#include "rclcpp/rclcpp.hpp"

void read_can(can_frame rx_frame)
{
    std::cout << "CAN ID: " << std::hex << int(rx_frame.can_id) << ", Data: ";
    for (int j = 0; j < rx_frame.can_dlc; ++j)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(rx_frame.data[j]) << " ";
    std::cout << std::endl;
}

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("node_01");
    RCLCPP_INFO(node->get_logger(), "node_01 节点已经启动.");
    can::SocketCan sock;
    sock.open("can0", read_can);

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}