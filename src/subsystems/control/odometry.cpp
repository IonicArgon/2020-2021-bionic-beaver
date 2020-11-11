//* 2020-2021-bionic-beaver
//* 
//* Author(s):      Dev Patel   <hello.devpatel@gmail.com>
//*                 Marco Tan   <marco.tan.200405@gmail.com>
//*             Neil Sachdeva   <Wiserlightning090@gmail.com>
//*
//* Desc:       Odometry class definitions

#include "subsystems/control/odometry.hpp"


//* Constructor and destructors *//

/// Odometry constructor. No default parameters, all parameters must be specified. 
/// \param starting_coords Struct with all starting coordinates.
/// \param goal_coords Coords of the center of each goal.
/// \param live_comp_coords Coords of the starting center of each ball during live comp.
/// \param skills_comp_coords Coords of the starting center of each ball during skills.
/// \param sensors_obj Reference to sensors object.
/// \param chassis_obj Reference to chassis object.
/// \param starting_side Which side the robot is starting on on the field.
c_Odometry::c_Odometry
(
    const c_Robot_Starting_Positions&       starting_coords,
    const c_All_Goal_Coords&                goal_coords,
    const c_Live_Comp_Setup_Startup_Coords& live_comp_coords,
    const c_Skills_Setup_Startup_Coords&    skills_comp_coords,
    h_Sensors*                  sensors_obj,
    h_Skid_Steer_Chassis*       chassis_obj,
    c_Robot_Starting_Pos_Side   starting_side
)   : m_starting_side{starting_side}, m_sensors_obj{sensors_obj}, m_chassis_obj{chassis_obj},
      m_goal_coords{goal_coords}, m_live_comp_coords{live_comp_coords}, m_skills_comp_coords{skills_comp_coords}
{
    switch (m_starting_side)
    {
    case c_Robot_Starting_Pos_Side::RED:
        m_starting_x = starting_coords.m_live_start_red.x;
        m_starting_y = starting_coords.m_live_start_red.y;
        m_starting_angle = starting_coords.m_live_start_red.head;
        break;
    case c_Robot_Starting_Pos_Side::BLUE:
        m_starting_x = starting_coords.m_live_start_blue.x;
        m_starting_y = starting_coords.m_live_start_blue.y;
        m_starting_angle = starting_coords.m_live_start_blue.head;
        break;
    case c_Robot_Starting_Pos_Side::SKILLS:
        m_starting_x = starting_coords.m_skills.x;
        m_starting_y = starting_coords.m_skills.y;
        m_starting_angle = starting_coords.m_skills.head;
        break;
    }

}


//* General methods *//

/// Start the odometry task.
void c_Odometry::start_odom(void)
{
    m_update_task = new pros::Task(std::bind(&c_Odometry::m_update_func, this));
}

/// Stop the odometry task.
void c_Odometry::stop_odom(void)
{
    if (m_update_task != nullptr)
    {
        m_update_task->remove();
        delete m_update_task;
        m_update_task = nullptr;
    }
}

/// Calibrate odometry.
void c_Odometry::calibrate(void)
{
    m_offset_x = 0;
    m_offset_y = 0;
    m_offset_angle = 0;

    m_global_x = m_starting_x + m_offset_x;
    m_global_y = m_starting_y + m_offset_y;
    m_global_angle = m_starting_angle + m_offset_angle;
}


//* Getter methods *//

/// \return Current X coord in inches.
double c_Odometry::get_x(void) {return m_global_x;}

/// \return Current Y coord in inches.
double c_Odometry::get_y(void) {return m_global_y;}

/// \return Current angle in degrees.
double c_Odometry::get_angle(void) {return m_global_angle;}

//* Private methods *//

/// Filter values
double c_Odometry::m_filter_values(double current_val, double last_val)
{
    double filter = current_val - last_val;
    if (std::fabs(filter) < 0.01)
        {filter = 0.0;}
    return filter;
}

/// Updates odometry values.
void c_Odometry::m_update_func(void)
{
    // Current position values
    double  m_current_rotation {0.0};   // Robot current rotation in degrees
    double  m_filtered_rotation {0.0};  // Filtered robot rotations in degrees
    double  m_last_rotation {0.0};      // Previous robot rotations in degrees
    double  m_current_pitch {0.0};      // Robot current pitch in degrees
    double  m_filtered_pitch {0.0};     // Filtered robot pitch in degrees
    double  m_last_pitch {0.0};         // Previous robot rotation in degrees
    double  m_current_roll {0.0};       // Robot current roll in degrees
    double  m_filtered_roll {0.0};      // Filtered robot roll in degrees
    double  m_last_roll {0.0};          // Previous robot roll in degrees

    // local position values
    double m_len_right {0.0};       // Distance right tracking wheel travelled in inches
    double m_len_middle {0.0};      // Distance middle tracking wheel travelled in inches
    double m_delta_right {0.0};     // Change in right distance from last calculated distance
    double m_delta_middle {0.0};    // Change in middle distance from last calculated distance
    double m_last_right {0.0};      // Previous right distance value
    double m_last_middle {0.0};     // Previous middle distance value
    double m_delta_theta {0.0};     // Change in rotation from last recorded distance
    double m_alpha {0.0};           // Used for chord calc and offset later
    double m_radius_right {0.0};    // Right radius
    double m_radius_middle {0.0};   // Middle radius
    double m_chord_right {0.0};     // Right chord
    double m_chord_middle {0.0};    // Middle chord
    double m_polar_offset {0.0};    // Angle + robot angle
    
    while(true)
    {
        // Getting rotation, pitch, and roll
        m_current_rotation = m_sensors_obj->imu_get_rotation();
        m_filtered_rotation += m_filter_values(m_current_rotation, m_last_rotation);
        m_last_rotation = m_current_rotation;

        m_current_pitch = m_sensors_obj->imu_get_pitch();
        m_filtered_pitch += m_filter_values(m_current_pitch, m_last_pitch);
        m_last_pitch = m_current_pitch;

        m_current_roll = m_sensors_obj->imu_get_roll();
        m_filtered_roll += m_filter_values(m_current_roll, m_last_roll);
        m_last_roll = m_current_roll;

        // Find out the length each encoder moved.
        m_len_right = m_sensors_obj->tracking_wheels_get(h_Sensors_Tracking_Wheel_IDs::RIGHT) / 360.0 \
                      * (m_sensors_obj->tracking_wheels_get_diameter() * M_PI);
        m_len_middle = m_sensors_obj->tracking_wheels_get(h_Sensors_Tracking_Wheel_IDs::MIDDLE) / 360.0 \
                       * (m_sensors_obj->tracking_wheels_get_diameter() * M_PI);
    
        // Find the change since last update.
        m_delta_right = m_len_right - m_last_right;
        m_delta_middle = m_len_middle - m_last_middle;

        // Update previous values.
        m_last_right = m_len_right;
        m_last_middle = m_len_middle;

        // Determine change in angle.
        m_delta_theta = u_deg_to_rad(m_filtered_rotation) - m_global_angle;

        // Determine if robot turned or not and calculate accordingly.
        if (m_delta_theta)
        {
            m_alpha = m_delta_theta / 2.0;

            m_radius_right = m_delta_right / m_delta_theta + m_sensors_obj->tracking_wheels_get_side_radius();
            m_chord_right = (m_radius_right * sin(m_alpha)) * 2;

            m_radius_middle = m_delta_middle / m_delta_theta + m_sensors_obj->tracking_wheels_get_middle_radius();
            m_chord_middle = (m_radius_middle * sin(m_alpha)) * 2;
        }
        else
        {
            m_alpha = 0.0;
            m_chord_right = m_delta_right;
            m_chord_middle = m_delta_middle;
        }

        // Find the polar offset needed.
        m_polar_offset = m_global_angle + m_alpha;

        // Add the change to the global coordinates.
        m_global_x += m_chord_right * sin(m_polar_offset);
        m_global_x += m_chord_middle * cos(m_polar_offset);

        m_global_y += m_chord_right * cos(m_polar_offset);
        m_global_y += m_chord_middle * -sin(m_polar_offset);
        
        // Update the global angle.
        m_global_angle += m_delta_theta;

        // Delay.
        pros::lcd::print(0, "%f, %f, %f", m_current_rotation, m_current_pitch, m_current_roll);
        pros::lcd::print(1, "%f, %f, %f", m_filtered_rotation, m_filtered_pitch, m_filtered_roll);
        pros::lcd::print(2, "%f, %f, %f", m_last_rotation, m_last_pitch, m_last_roll);
        pros::lcd::print(3, "%f", m_filter_values(m_current_rotation, m_last_rotation));
        pros::delay(10);
    }
}
