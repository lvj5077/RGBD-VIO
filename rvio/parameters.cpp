#include "parameters.h"
#include <iostream>
#include <iomanip>

double INIT_DEPTH;
double MIN_PARALLAX;
double ACC_N, ACC_W;
double GYR_N, GYR_W;

std::vector<Eigen::Matrix3d> RIC;
std::vector<Eigen::Vector3d> TIC;

Eigen::Vector3d G{0.0, 0.0, 9.8};
double nG;

double MAX_DPT_RANGE;
double DPT_VALID_RANGE = 2.4;
double BIAS_ACC_THRESHOLD;
double BIAS_GYR_THRESHOLD;
double SOLVER_TIME;
int NUM_ITERATIONS;
int MIN_USED_NUM = 2; // 2
int ESTIMATE_EXTRINSIC;
std::string EX_CALIB_RESULT_PATH;
std::string VINS_RESULT_PATH;
std::string VINS_CORRECT_RESULT_PATH; 
std::string OUTPUT_FILE_NAME = "rgbd_vio.csv";
std::string OUTPUT_COR_FILE_NAME = "corrected_pose.csv"; 
int LOOP_CLOSURE = 0;
int MIN_LOOP_NUM;
std::string CAM_NAMES;
// std::vector<std::string> CAM_NAMES;
std::string PATTERN_FILE;
std::string VOC_FILE;
std::string IMAGE_TOPIC;
std::string DPT_IMG_TOPIC;
std::string IMU_TOPIC;
int IMAGE_ROW, IMAGE_COL;
std::string VINS_FOLDER_PATH;
int MAX_KEYFRAME_NUM;
double PIX_SIGMA, FX, FY, CX, CY;

// TODO: parameterize these from config, if necessary 
double DFX = 212.7505; 
double DFY = 212.7505;
double DCX = 126.4288; 
double DCY = 95.6402; 

int MAX_CNT;
int MIN_DIST;
double F_THRESHOLD;
int SHOW_TRACK;
int FLOW_BACK;
bool PUB_THIS_FRAME;

double FREQ;

template <typename T>
T readParam(ros::NodeHandle &n, std::string name)
{
    T ans;
    if (n.getParam(name, ans))
    {
        ROS_INFO_STREAM("Loaded " << name << ": " << ans);
    }
    else
    {
        ROS_ERROR_STREAM("Failed to load " << name);
        n.shutdown();
    }
    return ans;
}

void readParameters(ros::NodeHandle &n)
{
    static bool once = false;
    if(once ) return;
    std::string config_file("/home/davidz/work/ros/kinetic/src/demo_rgbd_new/config/downsample.yaml");
    // config_file = readParam<std::string>(n, "config_file");
    n.param("config_file", config_file, config_file);
    cv::FileStorage fsSettings(config_file, cv::FileStorage::READ);
    if(!fsSettings.isOpened())
    {
        std::cerr << "ERROR: Wrong path to settings, config_file = "<<config_file << std::endl;
    }else{
        ROS_DEBUG("parameters.cpp: succeed to load config_file: %s", config_file.c_str());
    }

    // VINS_FOLDER_PATH = readParam<std::string>(n, "vins_folder");
    fsSettings["image_topic"] >> IMAGE_TOPIC;
    fsSettings["imu_topic"] >> IMU_TOPIC;
    fsSettings["dpt_img_topic"] >> DPT_IMG_TOPIC;

    MAX_CNT = fsSettings["max_cnt"];
    MIN_DIST = fsSettings["min_dist"];
    F_THRESHOLD = fsSettings["F_threshold"];
    SHOW_TRACK = fsSettings["show_track"];
    FLOW_BACK = fsSettings["flow_back"];
    FREQ = fsSettings["freq"];

    IMAGE_COL = fsSettings["image_width"];
    IMAGE_ROW = fsSettings["image_height"];

    SOLVER_TIME = fsSettings["max_solver_time"];
    NUM_ITERATIONS = fsSettings["max_num_iterations"];
    MIN_PARALLAX = fsSettings["keyframe_parallax"];
    // FOCAL_LENGTH = fsSettings["virtual_focal_length"];
    MIN_PARALLAX = MIN_PARALLAX / FOCAL_LENGTH;

    fsSettings["output_path"] >> VINS_RESULT_PATH;
    // VINS_RESULT_PATH = VINS_FOLDER_PATH + VINS_RESULT_PATH;
    // create folder if not exists
    Utility::FileSystemHelper::createDirectoryIfNotExists(VINS_RESULT_PATH.c_str());
    VINS_CORRECT_RESULT_PATH = VINS_RESULT_PATH + "/" + OUTPUT_COR_FILE_NAME; 
    VINS_RESULT_PATH = VINS_RESULT_PATH + "/" + OUTPUT_FILE_NAME;
    ROS_DEBUG("VINS_RESULT_PATH: %s", VINS_RESULT_PATH.c_str());
    std::ofstream foutC(VINS_RESULT_PATH, std::ios::out);
    foutC.close();

    std::ofstream foutC2(VINS_CORRECT_RESULT_PATH, std::ios::out);
    foutC2.close();

    ACC_N = fsSettings["acc_n"];
    ACC_W = fsSettings["acc_w"];
    GYR_N = fsSettings["gyr_n"];
    GYR_W = fsSettings["gyr_w"];
    G.z() = fsSettings["g_norm"];

    PIX_SIGMA = fsSettings["F_threshold"];
    ROS_DEBUG("SOLVER_TIME: %lf PIX_SIGMA = %lf ACC_N = %lf ACC_W = %lf GYR_N = %lf GYR_W = %lf G.z = %lf", SOLVER_TIME, PIX_SIGMA, ACC_N, ACC_W, GYR_N, GYR_W, G.z());

    MAX_DPT_RANGE = fsSettings["max_depth_range"];
    DPT_VALID_RANGE = fsSettings["valid_depth_range"]; // within this range, depth are very accurate
    nG = fsSettings["norm_g"];
    ROS_DEBUG("MAX_DEPTH_RANGE: %lf DPT_VALID_RANGE: %lf normal g: %lf", MAX_DPT_RANGE, DPT_VALID_RANGE, nG);
    cv::FileNode node = fsSettings["projection_parameters"];
    FX = static_cast<double>(node["fx"]);
    FY = static_cast<double>(node["fy"]);
    CX = static_cast<double>(node["cx"]);
    CY = static_cast<double>(node["cy"]);
    ROS_DEBUG("FX = %f FY = %f CX = %f CY = %f", FX, FY, CX, CY);

    ESTIMATE_EXTRINSIC = fsSettings["estimate_extrinsic"];
    if (ESTIMATE_EXTRINSIC == 2){
        ROS_WARN("have no prior about extrinsic param, calibrate extrinsic param");
        RIC.push_back(Eigen::Matrix3d::Identity());
        TIC.push_back(Eigen::Vector3d::Zero());
        fsSettings["ex_calib_result_path"] >> EX_CALIB_RESULT_PATH;
        EX_CALIB_RESULT_PATH = VINS_FOLDER_PATH + EX_CALIB_RESULT_PATH;

    }
    else
    {
        if ( ESTIMATE_EXTRINSIC == 1){
            ROS_WARN(" Optimize extrinsic param around initial guess!");
            fsSettings["ex_calib_result_path"] >> EX_CALIB_RESULT_PATH;
            EX_CALIB_RESULT_PATH = VINS_FOLDER_PATH + EX_CALIB_RESULT_PATH;
        }
        if (ESTIMATE_EXTRINSIC == 0)
            ROS_WARN(" fix extrinsic param ");
/*
        cv::Mat cv_R, cv_T;
        fsSettings["body_T_cam0"] >> cv_T;
        Eigen::Matrix4d T;
        cv::cv2eigen(cv_T, T);

        RIC.push_back(T.block<3,3>(0,0));
        ROS_INFO_STREAM("before normalize Extrinsic_R : " << std::endl << RIC[0]);
        Eigen::Quaterniond Q(RIC[0]);
        RIC[0] = Q.normalized();
        // ROS_INFO_STREAM(" Q : " << std::endl << Q.w()<<" "<<Q.x()<<" "<<Q.y()<<" "<<Q.z());
        TIC.push_back(T.block<3,1>(0,3));
        ROS_INFO_STREAM("Extrinsic_R : " << std::endl << RIC[0]);
        ROS_INFO_STREAM("Extrinsic_T : " << std::endl << TIC[0].transpose());

        if(NUM_OF_CAM == 2){
            fsSettings["body_T_cam1"] >> cv_T;
            cv::cv2eigen(cv_T, T);
            RIC.push_back(T.block<3,3>(0,0));
            Eigen::Quaterniond Q(RIC[1]);
            RIC[1] = Q.normalized();
            TIC.push_back(T.block<3,1>(0,3));
            ROS_INFO_STREAM("cam2 Extrinsic_R : " << std::endl << RIC[1]);
            ROS_INFO_STREAM("cam2 Extrinsic_T : " << std::endl << TIC[1].transpose());
        }
*/

        cv::Mat cv_R, cv_T;
        fsSettings["extrinsicRotation"] >> cv_R;
        fsSettings["extrinsicTranslation"] >> cv_T;
        Eigen::Matrix3d eigen_R;
        Eigen::Vector3d eigen_T;
        cv::cv2eigen(cv_R, eigen_R);
        cv::cv2eigen(cv_T, eigen_T);
        Eigen::Quaterniond Q(eigen_R);
        eigen_R = Q.normalized();
        RIC.push_back(eigen_R);
        TIC.push_back(eigen_T);
        ROS_INFO_STREAM("Extrinsic_R : " << std::endl << RIC[0]);
        ROS_INFO_STREAM("Extrinsic_T : " << std::endl << TIC[0].transpose());

    }

    LOOP_CLOSURE = fsSettings["loop_closure"];
    if (LOOP_CLOSURE == 1)
    {
        fsSettings["voc_file"] >> VOC_FILE;;
        fsSettings["pattern_file"] >> PATTERN_FILE;
        VOC_FILE = VINS_FOLDER_PATH + VOC_FILE;
        PATTERN_FILE = VINS_FOLDER_PATH + PATTERN_FILE;
        MIN_LOOP_NUM = fsSettings["min_loop_num"];
       // CAM_NAMES = config_file;
    }

    CAM_NAMES = config_file;

    INIT_DEPTH =  15.0; // 7.0; // 15.0; // 5.0;
    fsSettings.release();
    once = true;
    return ;
}
