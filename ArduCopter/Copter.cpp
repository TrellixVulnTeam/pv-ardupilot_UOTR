/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  ArduCopter (also known as APM, APM:Copter or just Copter)
 *  Wiki:           copter.ardupilot.org
 *  Creator:        Jason Short
 *  Lead Developer: Randy Mackay
 *  Lead Tester:    Marco Robustini
 *  Based on code and ideas from the Arducopter team: Leonard Hall, Andrew Tridgell, Robert Lefebvre, Pat Hickey, Michael Oborne, Jani Hirvinen,
                                                      Olivier Adler, Kevin Hester, Arthur Benemann, Jonathan Challinger, John Arne Birkeland,
                                                      Jean-Louis Naudin, Mike Smith, and more
 *  Thanks to:	Chris Anderson, Jordi Munoz, Jason Short, Doug Weibel, Jose Julio
 *
 *  Special Thanks to contributors (in alphabetical order by first name):
 *
 *  Adam M Rivera       :Auto Compass Declination
 *  Amilcar Lucas       :Camera mount library
 *  Andrew Tridgell     :General development, Mavlink Support
 *  Angel Fernandez     :Alpha testing
 *  AndreasAntonopoulous:GeoFence
 *  Arthur Benemann     :DroidPlanner GCS
 *  Benjamin Pelletier  :Libraries
 *  Bill King           :Single Copter
 *  Christof Schmid     :Alpha testing
 *  Craig Elder         :Release Management, Support
 *  Dani Saez           :V Octo Support
 *  Doug Weibel	        :DCM, Libraries, Control law advice
 *  Emile Castelnuovo   :VRBrain port, bug fixes
 *  Gregory Fletcher    :Camera mount orientation math
 *  Guntars             :Arming safety suggestion
 *  HappyKillmore       :Mavlink GCS
 *  Hein Hollander      :Octo Support, Heli Testing
 *  Igor van Airde      :Control Law optimization
 *  Jack Dunkle         :Alpha testing
 *  James Goppert       :Mavlink Support
 *  Jani Hiriven        :Testing feedback
 *  Jean-Louis Naudin   :Auto Landing
 *  John Arne Birkeland	:PPM Encoder
 *  Jose Julio          :Stabilization Control laws, MPU6k driver
 *  Julien Dubois       :PosHold flight mode
 *  Julian Oes          :Pixhawk
 *  Jonathan Challinger :Inertial Navigation, CompassMot, Spin-When-Armed
 *  Kevin Hester        :Andropilot GCS
 *  Max Levine          :Tri Support, Graphics
 *  Leonard Hall        :Flight Dynamics, Throttle, Loiter and Navigation Controllers
 *  Marco Robustini     :Lead tester
 *  Michael Oborne      :Mission Planner GCS
 *  Mike Smith          :Pixhawk driver, coding support
 *  Olivier Adler       :PPM Encoder, piezo buzzer
 *  Pat Hickey          :Hardware Abstraction Layer (HAL)
 *  Robert Lefebvre     :Heli Support, Copter LEDs
 *  Roberto Navoni      :Library testing, Porting to VRBrain
 *  Sandro Benigno      :Camera support, MinimOSD
 *  Sandro Tognana      :PosHold flight mode
 *  Sebastian Quilter   :SmartRTL
 *  ..and many more.
 *
 *  Code commit statistics can be found here: https://github.com/ArduPilot/ardupilot/graphs/contributors
 *  Wiki: http://copter.ardupilot.org/
 *
 */


   /////////////////////////////////////////////////////////////////
   //Modified by Leading Edge Aerial Technologies, LLC  (Feb 2021)//
   /////////////////////////////////////////////////////////////////

#include "Copter.h"

#define FORCE_VERSION_H_INCLUDE
#include "version.h"
#undef FORCE_VERSION_H_INCLUDE

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

#define SCHED_TASK(func, rate_hz, max_time_micros) SCHED_TASK_CLASS(Copter, &copter, func, rate_hz, max_time_micros)


#include <PV_Resuming/PVResumePointCreator.h>
using namespace PrecisionVision;


/*
  scheduler table for fast CPUs - all regular tasks apart from the fast_loop()
  should be listed here, along with how often they should be called (in hz)
  and the maximum time they are expected to take (in microseconds)
 */
const AP_Scheduler::Task Copter::scheduler_tasks[] = {
    SCHED_TASK(rc_loop,              100,    130),
    SCHED_TASK(throttle_loop,         50,     75),
    SCHED_TASK(update_GPS,            50,    200),
#if OPTFLOW == ENABLED
    SCHED_TASK_CLASS(OpticalFlow,          &copter.optflow,             update,         200, 160),
#endif
    SCHED_TASK(update_batt_compass,   10,    120),
    SCHED_TASK_CLASS(RC_Channels,          (RC_Channels*)&copter.g2.rc_channels,      read_aux_all,    10,     50),
    SCHED_TASK(arm_motors_check,      10,     50),
#if TOY_MODE_ENABLED == ENABLED
    SCHED_TASK_CLASS(ToyMode,              &copter.g2.toy_mode,         update,          10,  50),
#endif
    SCHED_TASK(auto_disarm_check,     10,     50),
    SCHED_TASK(auto_trim,             10,     75),
#if RANGEFINDER_ENABLED == ENABLED
    SCHED_TASK(read_rangefinder,      20,    100),
#endif
#if PROXIMITY_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_Proximity,         &copter.g2.proximity,        update,         200,  50),
#endif
#if BEACON_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_Beacon,            &copter.g2.beacon,           update,         400,  50),
#endif
#if VISUAL_ODOMETRY_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_VisualOdom,       &copter.g2.visual_odom,        update,         400,  50),
#endif
    SCHED_TASK(update_altitude,       10,    100),
    SCHED_TASK(update_tank_sensor,   10,    100),
    SCHED_TASK(run_nav_updates,       50,    100),
    SCHED_TASK(update_throttle_hover,100,     90),
#if MODE_SMARTRTL_ENABLED == ENABLED
    SCHED_TASK_CLASS(ModeSmartRTL, &copter.mode_smartrtl,       save_position,    3, 100),
#endif
#if SPRAYER_ENABLED == ENABLED
    SCHED_TASK_CLASS(AC_Sprayer,           &copter.sprayer,             update,           3,  90),
#endif
    SCHED_TASK(three_hz_loop,          3,     75),
    SCHED_TASK_CLASS(AP_ServoRelayEvents,  &copter.ServoRelayEvents,      update_events, 50,     75),
    SCHED_TASK_CLASS(AP_Baro,              &copter.barometer,           accumulate,      50,  90),
#if AC_FENCE == ENABLED
    SCHED_TASK_CLASS(AC_Fence,             &copter.fence,               update,          10, 100),
#endif
#if PRECISION_LANDING == ENABLED
    SCHED_TASK(update_precland,      400,     50),
#endif
#if FRAME_CONFIG == HELI_FRAME
    SCHED_TASK(check_dynamic_flight,  50,     75),
#endif
#if LOGGING_ENABLED == ENABLED
    SCHED_TASK(fourhundred_hz_logging,400,    50),
#endif
    SCHED_TASK_CLASS(AP_Notify,            &copter.notify,              update,          50,  90),
    SCHED_TASK(one_hz_loop,            1,    100),
    SCHED_TASK(ekf_check,             10,     75),
    SCHED_TASK(check_vibration,       10,     50),
    SCHED_TASK(gpsglitch_check,       10,     50),
    SCHED_TASK(landinggear_update,    10,     75),
    SCHED_TASK(standby_update,        100,    75),
    SCHED_TASK(lost_vehicle_check,    10,     50),
    SCHED_TASK_CLASS(GCS,                  (GCS*)&copter._gcs,          update_receive, 400, 180),
    SCHED_TASK_CLASS(GCS,                  (GCS*)&copter._gcs,          update_send,    400, 550),
#if MOUNT == ENABLED
    SCHED_TASK_CLASS(AP_Mount,             &copter.camera_mount,        update,          50,  75),
#endif
#if CAMERA == ENABLED
    SCHED_TASK_CLASS(AP_Camera,            &copter.camera,              update_trigger,  50,  75),
#endif
#if LOGGING_ENABLED == ENABLED
    SCHED_TASK(ten_hz_logging_loop,   10,    350),
    SCHED_TASK(twentyfive_hz_logging, 25,    110),
    SCHED_TASK_CLASS(AP_Logger,      &copter.logger,           periodic_tasks, 400, 300),
#endif
    SCHED_TASK_CLASS(AP_InertialSensor,    &copter.ins,                 periodic,       400,  50),
    SCHED_TASK_CLASS(AP_Scheduler,         &copter.scheduler,           update_logging, 0.1,  75),
#if RPM_ENABLED == ENABLED
    SCHED_TASK(rpm_update,            40,    200),
#endif
    SCHED_TASK(compass_cal_update,   100,    100),
    SCHED_TASK(accel_cal_update,      10,    100),
    SCHED_TASK_CLASS(AP_TempCalibration,   &copter.g2.temp_calibration, update,          10, 100),
#if ADSB_ENABLED == ENABLED
    SCHED_TASK(avoidance_adsb_update, 10,    100),
#endif
#if ADVANCED_FAILSAFE == ENABLED
    SCHED_TASK(afs_fs_check,          10,    100),
#endif
#if AC_TERRAIN == ENABLED
    SCHED_TASK(terrain_update,        10,    100),
#endif
#if GRIPPER_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_Gripper,           &copter.g2.gripper,          update,          10,  75),
#endif
#if WINCH_ENABLED == ENABLED
    SCHED_TASK(winch_update,          10,     50),
#endif
#ifdef USERHOOK_FASTLOOP
    SCHED_TASK(userhook_FastLoop,    100,     75),
#endif
#ifdef USERHOOK_50HZLOOP
    SCHED_TASK(userhook_50Hz,         50,     75),
#endif
#ifdef USERHOOK_MEDIUMLOOP
    SCHED_TASK(userhook_MediumLoop,   10,     75),
#endif
#ifdef USERHOOK_SLOWLOOP
    SCHED_TASK(userhook_SlowLoop,     3.3,    75),
#endif
#ifdef USERHOOK_SUPERSLOWLOOP
    SCHED_TASK(userhook_SuperSlowLoop, 1,   75),
#endif
#if BUTTON_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_Button,            &copter.g2.button,           update,           5, 100),
#endif
#if STATS_ENABLED == ENABLED
    SCHED_TASK_CLASS(AP_Stats,             &copter.g2.stats,            update,           1, 100),
#endif
#if OSD_ENABLED == ENABLED
    SCHED_TASK(publish_osd_info, 1, 10),
#endif
};

constexpr int8_t Copter::_failsafe_priorities[7];

void Copter::setup()
{
    // Load the default values of variables listed in var_info[]s
    AP_Param::setup_sketch_defaults();

    // setup storage layout for copter
    StorageManager::set_layout_copter();

    init_ardupilot();

  

    // initialise the main loop scheduler
    scheduler.init(&scheduler_tasks[0], ARRAY_SIZE(scheduler_tasks), MASK_LOG_PM);
}

void Copter::loop()
{
    scheduler.loop();
    G_Dt = scheduler.get_last_loop_time_s();
}


// Main loop - 400hz
void Copter::fast_loop()
{
    // update INS immediately to get current gyro data populated
    ins.update();

    // run low level rate controllers that only require IMU data
    attitude_control->rate_controller_run();

    // send outputs to the motors library immediately
    motors_output();

    // run EKF state estimator (expensive)
    // --------------------
    read_AHRS();

#if FRAME_CONFIG == HELI_FRAME
    update_heli_control_dynamics();
    #if MODE_AUTOROTATE_ENABLED == ENABLED
        heli_update_autorotation();
    #endif
#endif //HELI_FRAME

    // Inertial Nav
    // --------------------
    read_inertia();

    // check if ekf has reset target heading or position
    check_ekf_reset();

    // run the attitude controllers
    update_flight_mode();

    // update home from EKF if necessary
    update_home_from_EKF();

    // check if we've landed or crashed
    update_land_and_crash_detectors();

#if MOUNT == ENABLED
    // camera mount's fast update
    camera_mount.update_fast();
#endif

    // log sensor health
    if (should_log(MASK_LOG_ANY)) {
        Log_Sensor_Health();
    }
}

// rc_loops - reads user input from transmitter/receiver
// called at 100hz
void Copter::rc_loop()
{
    // Read radio and 3-position switch on radio
    // -----------------------------------------
    read_radio();
    rc().read_mode_switch();
}

// throttle_loop - should be run at 50 hz
// ---------------------------
void Copter::throttle_loop()
{
    // update throttle_low_comp value (controls priority of throttle vs attitude control)
    update_throttle_mix();

    // check auto_armed status
    update_auto_armed();

#if FRAME_CONFIG == HELI_FRAME
    // update rotor speed
    heli_update_rotor_speed_targets();

    // update trad heli swash plate movement
    heli_update_landing_swash();
#endif

    // compensate for ground effect (if enabled)
    update_ground_effect_detector();

    update_dynamic_notch();
}

// update_batt_compass - read battery and compass
// should be called at 10hz
void Copter::update_batt_compass(void)
{
    // read battery before compass because it may be used for motor interference compensation
    battery.read();

    if(AP::compass().enabled()) {
        // update compass with throttle value - used for compassmot
        compass.set_throttle(motors->get_throttle());
        compass.set_voltage(battery.voltage());
        compass.read();
    }
}

// Full rate logging of attitude, rate and pid loops
// should be run at 400hz
void Copter::fourhundred_hz_logging()
{
    if (should_log(MASK_LOG_ATTITUDE_FAST) && !copter.flightmode->logs_attitude()) {
        Log_Write_Attitude();
    }
}

// ten_hz_logging_loop
// should be run at 10hz
void Copter::ten_hz_logging_loop()
{
    // log attitude data if we're not already logging at the higher rate
    if (should_log(MASK_LOG_ATTITUDE_MED) && !should_log(MASK_LOG_ATTITUDE_FAST) && !copter.flightmode->logs_attitude()) {
        Log_Write_Attitude();
    }
    // log EKF attitude data
    if (should_log(MASK_LOG_ATTITUDE_MED) || should_log(MASK_LOG_ATTITUDE_FAST)) {
        Log_Write_EKF_POS();
    }
    if (should_log(MASK_LOG_MOTBATT)) {
        Log_Write_MotBatt();
    }
    if (should_log(MASK_LOG_RCIN)) {
        logger.Write_RCIN();
        if (rssi.enabled()) {
            logger.Write_RSSI();
        }
    }
    if (should_log(MASK_LOG_RCOUT)) {
        logger.Write_RCOUT();
    }
    if (should_log(MASK_LOG_NTUN) && (flightmode->requires_GPS() || landing_with_GPS())) {
        pos_control->write_log();
    }
    if (should_log(MASK_LOG_IMU) || should_log(MASK_LOG_IMU_FAST) || should_log(MASK_LOG_IMU_RAW)) {
        logger.Write_Vibration();
    }
    if (should_log(MASK_LOG_CTUN)) {
        attitude_control->control_monitor_log();
#if PROXIMITY_ENABLED == ENABLED
        logger.Write_Proximity(g2.proximity);  // Write proximity sensor distances
#endif
#if BEACON_ENABLED == ENABLED
        logger.Write_Beacon(g2.beacon);
#endif
    }
#if FRAME_CONFIG == HELI_FRAME
    Log_Write_Heli();
#endif
}

// twentyfive_hz_logging - should be run at 25hz
void Copter::twentyfive_hz_logging()
{
#if HIL_MODE != HIL_MODE_DISABLED
    // HIL for a copter needs very fast update of the servo values
    gcs().send_message(MSG_SERVO_OUTPUT_RAW);
#endif

#if HIL_MODE == HIL_MODE_DISABLED
    if (should_log(MASK_LOG_ATTITUDE_FAST)) {
        Log_Write_EKF_POS();
    }

    if (should_log(MASK_LOG_IMU)) {
        logger.Write_IMU();
    }
#endif

#if PRECISION_LANDING == ENABLED
    // log output
    Log_Write_Precland();
#endif

#if MODE_AUTOROTATE_ENABLED == ENABLED
    if (should_log(MASK_LOG_ATTITUDE_MED) || should_log(MASK_LOG_ATTITUDE_FAST)) {
        //update autorotation log
        g2.arot.Log_Write_Autorotation();
    }
#endif
}

// three_hz_loop - 3.3hz loop
void Copter::three_hz_loop()
{
    // check if we've lost contact with the ground station
    failsafe_gcs_check();

    // check if we've lost terrain data
    failsafe_terrain_check();

#if AC_FENCE == ENABLED
    // check if we have breached a fence
    fence_check();
#endif // AC_FENCE_ENABLED


    // update ch6 in flight tuning
    tuning();
}

// one_hz_loop - runs at 1Hz
void Copter::one_hz_loop()
{
    if (should_log(MASK_LOG_ANY)) {
        Log_Write_Data(DATA_AP_STATE, ap.value);
    }

    arming.update();

    if (!motors->armed()) {
        // make it possible to change ahrs orientation at runtime during initial config
        ahrs.update_orientation();

        update_using_interlock();

        // check the user hasn't updated the frame class or type
        motors->set_frame_class_and_type((AP_Motors::motor_frame_class)g2.frame_class.get(), (AP_Motors::motor_frame_type)g.frame_type.get());

#if FRAME_CONFIG != HELI_FRAME
        // set all throttle channel settings
        motors->set_throttle_range(channel_throttle->get_radio_min(), channel_throttle->get_radio_max());
#endif
    }

    // update assigned functions and enable auxiliary servos
    SRV_Channels::enable_aux_servos();

    // log terrain data
    terrain_logging();

#if ADSB_ENABLED == ENABLED
    adsb.set_is_flying(!ap.land_complete);
#endif

    AP_Notify::flags.flying = !ap.land_complete;
}

// called at 50hz
void Copter::update_GPS(void)
{
    static uint32_t last_gps_reading[GPS_MAX_INSTANCES];   // time of last gps message
    bool gps_updated = false;

    gps.update();

    // log after every gps message
    for (uint8_t i=0; i<gps.num_sensors(); i++) {
        if (gps.last_message_time_ms(i) != last_gps_reading[i]) {
            last_gps_reading[i] = gps.last_message_time_ms(i);
            gps_updated = true;
            break;
        }
    }

    if (gps_updated) {
#if CAMERA == ENABLED
        camera.update();
#endif
    }
}

void Copter::init_simple_bearing()
{
    // capture current cos_yaw and sin_yaw values
    simple_cos_yaw = ahrs.cos_yaw();
    simple_sin_yaw = ahrs.sin_yaw();

    // initialise super simple heading (i.e. heading towards home) to be 180 deg from simple mode heading
    super_simple_last_bearing = wrap_360_cd(ahrs.yaw_sensor+18000);
    super_simple_cos_yaw = simple_cos_yaw;
    super_simple_sin_yaw = simple_sin_yaw;

    // log the simple bearing
    if (should_log(MASK_LOG_ANY)) {
        Log_Write_Data(DATA_INIT_SIMPLE_BEARING, ahrs.yaw_sensor);
    }
}

// update_simple_mode - rotates pilot input if we are in simple mode
void Copter::update_simple_mode(void)
{
    float rollx, pitchx;

    // exit immediately if no new radio frame or not in simple mode
    if (ap.simple_mode == 0 || !ap.new_radio_frame) {
        return;
    }

    // mark radio frame as consumed
    ap.new_radio_frame = false;

    if (ap.simple_mode == 1) {
        // rotate roll, pitch input by -initial simple heading (i.e. north facing)
        rollx = channel_roll->get_control_in()*simple_cos_yaw - channel_pitch->get_control_in()*simple_sin_yaw;
        pitchx = channel_roll->get_control_in()*simple_sin_yaw + channel_pitch->get_control_in()*simple_cos_yaw;
    }else{
        // rotate roll, pitch input by -super simple heading (reverse of heading to home)
        rollx = channel_roll->get_control_in()*super_simple_cos_yaw - channel_pitch->get_control_in()*super_simple_sin_yaw;
        pitchx = channel_roll->get_control_in()*super_simple_sin_yaw + channel_pitch->get_control_in()*super_simple_cos_yaw;
    }

    // rotate roll, pitch input from north facing to vehicle's perspective
    channel_roll->set_control_in(rollx*ahrs.cos_yaw() + pitchx*ahrs.sin_yaw());
    channel_pitch->set_control_in(-rollx*ahrs.sin_yaw() + pitchx*ahrs.cos_yaw());
}

// update_super_simple_bearing - adjusts simple bearing based on location
// should be called after home_bearing has been updated
void Copter::update_super_simple_bearing(bool force_update)
{
    if (!force_update) {
        if (ap.simple_mode != 2) {
            return;
        }
        if (home_distance() < SUPER_SIMPLE_RADIUS) {
            return;
        }
    }

    const int32_t bearing = home_bearing();

    // check the bearing to home has changed by at least 5 degrees
    if (labs(super_simple_last_bearing - bearing) < 500) {
        return;
    }

    super_simple_last_bearing = bearing;
    const float angle_rad = radians((super_simple_last_bearing+18000)/100);
    super_simple_cos_yaw = cosf(angle_rad);
    super_simple_sin_yaw = sinf(angle_rad);
}

void Copter::read_AHRS(void)
{
    // Perform IMU calculations and get attitude info
    //-----------------------------------------------
#if HIL_MODE != HIL_MODE_DISABLED
    // update hil before ahrs update
    gcs().update();
#endif

    // we tell AHRS to skip INS update as we have already done it in fast_loop()
    ahrs.update(true);
}



//PrecisionVision: TankSensor is currently wired to an aux pin on the carrier board
//it sends high signal when the tank is below the level (warning) 
void Copter::update_tank_sensor()
{
    static bool inBootDelay = true;
    if(g.pvtank_pin <= 0){return;}

    hal.gpio->pinMode(((uint8_t)g.pvtank_pin), HAL_GPIO_INPUT);
    uint8_t pin_state = hal.gpio->read((uint8_t)g.pvtank_pin);   //high signal means tank is full, 0 signal means tank is low. 

          //if we are transitioning to the low state, we alert the user, and if we happen to be in a guided/auto flight mode,
          //we kick out into brake mode. 


        ////right now, the tank sensor is always on, may need to set this up with a parameter so that we can
        //change the configuration, particularly if we get a new board. 
        //if(_tank_sensor_status == TankSensorState::SENSOR_UNAVAILABLE){
        //    return; 
        //}

        //TODO: we need to insert the resume waypoint,
        //but I think we have to send a signal to the GCS that the waypoints have changed.
        //maybe a refactoring such that we send a signal and the resume point doesn't show up on the
        //gcs (finalized) until it gets the message.  


        //if we WERE full, and now we are not, send it into brake mode! 
        if(_tank_sensor_status == TankSensorState::TANK_FULL  && pin_state == 0){
            //BUT only do so if A.) we are flying, B.) in regular auto mode (not loiter, takeoff, landing, or rtl, etc.)
            //and C.) at least 8 seconds has passed since bootup, just to make sure everything powers up right

            if(inBootDelay && millis() > 20000){ 
                inBootDelay = false; 
            }
            
            if(!inBootDelay)
            {   
                if(copter.control_mode == Mode::Number::AUTO)
                {      
                    if(!flightmode->is_taking_off() && !flightmode->is_landing()){
                        if(copter.control_mode != Mode::Number::BRAKE){
                            BrakeAndInsertResumePointIfNeeded();      
                        }
                    }
                }
            }
            
        }
    
    
    _tank_sensor_status = pin_state > 0 ? TankSensorState::TANK_FULL : TankSensorState::TANK_EMPTY; 
}



void Copter::BrakeAndInsertResumePointIfNeeded()
{
    hal.console->printf("BrakeAndInsertResumePointIfNeeded\n"); 
    printf("BrakeAndInsertResumePointIfNeeded\n");
    bool wasSpraying = copter.sprayer.spraying();

    int curnavidx = -1;
    if(copter.control_mode == Mode::Number::AUTO){
        curnavidx = copter.mode_auto.mission.get_prev_nav_cmd_with_wp_index();
    }

    set_mode(Mode::Number::BRAKE, ModeReason::FAILSAFE);
    if(!copter.wp_nav->reached_prev_wpt())
    {
        //if we haven't reached a waypoint already, we don't do any inserts. 
        hal.console->printf("Not yet reached prev_wpt, skipping resume generaton\n");
        return;
    }else
    {
        hal.console->printf("prev_wpt is true, checking takeoff condition...\n");

        hal.console->printf("current_nav_index is %d\n",curnavidx);
        if(curnavidx <= 3)
        {
          hal.console->printf("still in takeoff waypoint command range, skipping resume");
          return;
        }

    }
    
    int current_wp_idx = copter.mode_auto.mission.get_current_nav_index();
    int oMissionSize = copter.mode_auto.mission.num_commands();
    if(oMissionSize < copter.mode_auto.mission.num_commands_max())
    {           
        //send a USER_3 command back to the GCS, **do not confuse with GCS sending USER_3 to us! 
        //when we send MAV_CMD_USER_3 to the GCS, we are informing that we have inserted
        //a resume point. Right now, we can only use existing messages in the mavlink dialect
        //otherwise we have to compile special commands in the mavlink router for PrecisionVision

        PVResumePointCreator resumeMaker(
        *AP_Mission::get_singleton(),
        AP::ahrs(),
        *AC_Sprayer::get_singleton()
        );
                            
        ERRCODE result = resumeMaker.CreateResumePointAtCurrentUavLocationAndState();
        copter.sprayer.run(false,false); //important that this comes after the creation so it detects spray state, smells like a refactor would be good though!

        if((int)result  > 0)
        {   
            hal.console->printf("Resumepoint created successful, sending cmd...");
            printf("resumepoint created successful, sending cmd...");

            int newMissionSize = copter.mode_auto.mission.num_commands();
            //grab the wpt cmd from the mission
                                
            AP_Mission::Mission_Command chkCmd; 
            copter.mode_auto.mission.read_cmd_from_storage(current_wp_idx, chkCmd);


            //AltitudeModeNone,           // Being used as distance value unrelated to ground (for example distance to structure\
            //AltitudeModeRelative,       // MAV_FRAME_GLOBAL_RELATIVE_ALT Global (WGS84) coordinate frame + altitude relative to the home position. First value / x: latitude, second value / y: longitude, third value / z: positive altitude with 0 being at the altitude of the home location.
            //AltitudeModeAbsolute,       // MAV_FRAME_GLOBAL: Global (WGS84) coordinate frame + MSL altitude. First value / x: latitude, second value / y: longitude, third value / z: positive altitude over mean sea level (MSL).
            //AltitudeModeAboveTerrain,   // Absolute altitude above terrain calculated from terrain data -- this is a QGC only feature where it is sending AMSL after calculating locally against terrain data.  I believe more recent QGC now implements TERRAIN_REQUEST so not sure if this mode is used now?
            //AltitudeModeTerrainFrame    // MAV_FRAME_GLOBAL_TERRAIN_ALT  //informs ardupilot to use terrain data onboard (either obtained by requesting terrain_data from the qgc, i believe TERRAIN_DATA_REQUEST, or by using onboard rangefinders
                                
            int mavlink_altitude_frame_being_used = MAV_FRAME_GLOBAL_RELATIVE_ALT;
            if (chkCmd.content.location.relative_alt) {
                mavlink_altitude_frame_being_used = MAV_FRAME_GLOBAL_RELATIVE_ALT;
            }else if(chkCmd.content.location.terrain_alt){
                mavlink_altitude_frame_being_used = MAV_FRAME_GLOBAL_TERRAIN_ALT;
            }
            else{
                mavlink_altitude_frame_being_used = MAV_FRAME_GLOBAL;
            }

            if(newMissionSize != oMissionSize)
            {
                gcs().send_resume_point_details_back_to_gcs(
                                        current_wp_idx,
                                        newMissionSize,
                                        chkCmd.content.location.lat, 
                                        chkCmd.content.location.lng,
                                        chkCmd.content.location.alt,
                                        mavlink_altitude_frame_being_used,
                                        wasSpraying ? 1 : 0
                );

            }
        }
    }
    else
    {
        hal.console->printf("Resume point creation failed!\n");
        printf("resume point creation failed!\n");
    }
}




TankSensorState Copter::get_tank_sensor_status()
{
    return _tank_sensor_status;
}




/*
//temporary location 
void Copter::createResumePointAtCurrentLocation(int sprayStateForResumePt)
{

 
    // // set new waypoint to current location
        // Location temp_loc;
    // const AP_AHRS &ahrs = AP::ahrs();
    // if(!ahrs.get_position(temp_loc)){
    //     return MAV_RESULT_FAILED;
    // }
        
        //float altitudeEstimate = 0;
        //ahrs.get_hagl(altitudeEstimate); 
        //float inCm = altitudeEstimate * 100.0;
        //temp_loc.set_alt_cm(inCm, Location::AltFrame::ABOVE_TERRAIN);
        
        AP_Mission* mission = AP_Mission::get_singleton();  
        if(!mission){return;}

        int idx = mission->get_prev_nav_cmd_with_wp_index();  
        if(idx <= 0 || idx > mission->num_commands()){
            return;
        }
        
        // create new mission command
        AP_Mission::Mission_Command wp_cmd = {};
        AP_Mission::Mission_Command spry_cmd = {};
        AP_Mission::Mission_Command spd_cmd = {};
        AP_Mission::Mission_Command prevWptCmd = {}; 

        
        if(!mission->read_cmd_from_storage(idx, prevWptCmd)){
            return;
        }

        if(prevWptCmd.id == MAV_CMD_NAV_TAKEOFF){
            return; //we do nothing, I originally wanted to use "Failed" but 
            //it produces a message on QGC and for precisionvision purposes, we don't need to overly complicate. 
        }
        
        Location temp_loc;
        if(!ahrs.get_position(temp_loc)){
            return;
        }

        Location::AltFrame lastWptAltframe = prevWptCmd.content.location.get_alt_frame();
        int32_t altCm = 0;
        if(!prevWptCmd.content.location.get_alt_cm(lastWptAltframe,altCm)){
            return;
        }

        temp_loc.set_alt_cm(altCm, lastWptAltframe);
        wp_cmd.content.location = temp_loc;  
        // make the new command to a waypoint
        wp_cmd.id = MAV_CMD_NAV_WAYPOINT;
        //not sure what actually is going to make this trigger, filling it all out. 
        spry_cmd.id = MAV_CMD_USER_1;

       
        spry_cmd.p1 = sprayStateForResumePt;
        AP_Mission::User1_Command sprayInfo;
        sprayInfo.param1= sprayStateForResumePt;
        sprayInfo.param2= sprayStateForResumePt;
        spry_cmd.content.user1 = sprayInfo; 

        AP_Mission::Change_Speed_Command speedchange;
        spd_cmd.id = MAV_CMD_DO_CHANGE_SPEED;
        speedchange.speed_type = 1; //specify we want groundspeed
        speedchange.target_ms = -1;  //indicate no change for now  // packet.param4; //the packet from user should be meters per second
        speedchange.throttle_pct = -1;
        spd_cmd.content.speed = speedchange;

        int current_wp_idx = copter.mode_auto.mission.get_current_nav_index();
        AP_Mission::Mission_Command arr[3] = {wp_cmd, spry_cmd, spd_cmd}; // spd_cmd, spry_cmd }; 

        // save command
        if (copter.mode_auto.mission.insert_cmds(current_wp_idx, arr, 3)) {
            hal.console->printf("inserted waypoint");
        }
}
*/



// read baro and log control tuning
void Copter::update_altitude()
{
//    static int i=0;

    // read in baro altitude
    read_barometer();

   
//test

   

    if (should_log(MASK_LOG_CTUN)) {
        Log_Write_Control_Tuning();
    }
}

#if OSD_ENABLED == ENABLED
void Copter::publish_osd_info()
{
    AP_OSD::NavInfo nav_info;
    nav_info.wp_distance = flightmode->wp_distance() * 1.0e-2f;
    nav_info.wp_bearing = flightmode->wp_bearing();
    nav_info.wp_xtrack_error = flightmode->crosstrack_error() * 1.0e-2f;
    nav_info.wp_number = mode_auto.mission.get_current_nav_index();
    osd.set_nav_info(nav_info);
}
#endif

/*
  constructor for main Copter class
 */
Copter::Copter(void)
    : logger(g.log_bitmask),
    flight_modes(&g.flight_mode1),
    control_mode(Mode::Number::STABILIZE),
    simple_cos_yaw(1.0f),
    super_simple_cos_yaw(1.0),
    land_accel_ef_filter(LAND_DETECTOR_ACCEL_LPF_CUTOFF),
    rc_throttle_control_in_filter(1.0f),
    inertial_nav(ahrs),
    param_loader(var_info),
    flightmode(&mode_stabilize)
{
    // init sensor error logging flags
    sensor_health.baro = true;
    sensor_health.compass = true;
    
    _tank_sensor_status = TankSensorState::SENSOR_UNAVAILABLE; 

//    _pvResumeWaypointCreator = new PVResumePointCreator()

}

Copter copter;
AP_Vehicle& vehicle = copter;

AP_HAL_MAIN_CALLBACKS(&copter);
