//
// Created by Marco Dittmann on 16.07.23.
//

#include <iostream>
#include <sstream>

/* ROS includes */
#include <ros/ros.h>
#include <std_msgs/Float32.h>
#include <mbot_msgs/Motor.h>
#include <mbot_msgs/EMG.h>

float emg01 = 0;
float emg02 = 0;
float ultrasonic_distance = 0.0;
ros::Time last_emg_update;

float constrain(float x, float min, float max) {
    return x < min ? min : x > max ? max : x; 
}

float map(float x, float in_min, float in_max, float out_min, float out_max) {
    if(in_max == in_min) return (out_min + out_max) / 2.0;
    else return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void ultrasonic_callback(std_msgs::Float32 msg)
{
    ultrasonic_distance = msg.data;
}


void emg_callback(mbot_msgs::EMG msg)
{
    emg01 = msg.ch1;
    emg02 = msg.ch2;
    last_emg_update = ros::Time::now();
}


int main(int argc, char** argv)
{

    //  ### INITIALISATION ###
    ros::init(argc, argv, "mbot_controller_node");
    ros::NodeHandle nh;

    // ROS publishers
    ros::Publisher motor_pub = nh.advertise<mbot_msgs::Motor>("/mbot/motor", 10);

    // ROS subscribers
    ros::Subscriber ultrasonic_sub = nh.subscribe("/mbot/ultrasonic", 10, ultrasonic_callback);
    ros::Subscriber emg_sub = nh.subscribe("/mbot/emg", 10, emg_callback);

    // ROS messages
    mbot_msgs::Motor motor;

    // avoid collision with the help of ultrasound
    float ultrasonic_safety_distance;
    nh.param<float>("ultrasonic_safety_distance", ultrasonic_safety_distance, 0.10);

    // EMG
    double emg_timeout;
    float emg01_min, emg01_max;
    float emg02_min, emg02_max;

    // get emg related parameters
    nh.param<double>("emg_timeout", emg_timeout, 0.2);
    nh.param<float>("v_emg01_min", emg01_min, 0.0);
    nh.param<float>("v_emg01_max", emg01_max, 3.3);
    nh.param<float>("v_emg02_min", emg02_min, 0.0);
    nh.param<float>("v_emg02_max", emg02_max, 3.3);


    // Read rate parameter
    double rate;
    nh.param<double>("rate", rate, 20.0); // Default rate: 10 Hz
    ros::Rate loop_rate(rate);

    while (ros::ok())
    {

        if((ros::Time::now() - last_emg_update).toSec() < emg_timeout)
        {
            // generate motor command
            constrain(emg01, emg01_min, emg01_max);
            constrain(emg02, emg02_min, emg02_max);
            float t = map(emg01, emg01_min, emg01_max, -1.0, 1.0);
            float r = map(emg02, emg02_min, emg02_max, -1.0, 1.0);

            // Calculate left and right motor speeds based on translational and rotational speed
            int ml = (t - r) * 255;
            int mr = (t + r) * 255;
            int cmd_l = std::min(std::max(ml, -255), 255);
            int cmd_r = std::min(std::max(mr, -255), 255);

            if(ultrasonic_distance < ultrasonic_safety_distance)
            {
                if(cmd_l > 0 || cmd_r > 0)
                {
                    cmd_l = 0;
                    cmd_r = 0;
                }
            }

            motor.left = cmd_l;
            motor.right = cmd_r;
        }
        else
        {
            // No recent EMG sensor data available -> stop the robot
            motor.left = 0;
            motor.right = 0;
        }


        motor_pub.publish(motor);
        ros::spinOnce();
        loop_rate.sleep();
    }

    // Cleanup

    return 0;
}