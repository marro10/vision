#include <roi_extractor/roi_extractor.h>
#include <sensor_msgs/Image.h>
#include <std_msgs/Float64.h>
#include <wall_detector/wall_extractor.h>
#include <common/parameter.h>
#include <common/types.h>
#include <pcl_ros/point_cloud.h>
#include <pre_filter/pre_filter.h>
#include <pcl_msgs/Vertices.h>
#include <object_detector/ROI.h>

const double PUBLISH_FREQUENCY = 10.0;

//------------------------------------------------------------------------------
// Member variables

ROIExtractor _roi_extractor;
WallExtractor _wall_extractor;
PreFilter _pre_filter;

common::SharedPointCloudRGB _pcloud;
common::PointCloudRGB::Ptr _filtered;
Eigen::Matrix4f _camera_matrix;

// Parameters of pre filter
Parameter<double> _frustum_near("/vision/filter/frustum/near", 0.3);
Parameter<double> _frustum_far("/vision/filter/frustum/far", 1.7);
Parameter<double> _frustum_horz_fov("/vision/filter/frustum/horz_fov", 60.0);
Parameter<double> _frustum_vert_fov("/vision/filter/frustum/vert_fov", 50.0);
Parameter<double> _leaf_size("/vision/filter/down_sampling/leaf_size", 0.003); //voxel downsampling
Parameter<bool> _fast_down_sampling("/vision/filter/down_sampling/enable_fast", false); //fast downsampling
Parameter<int> _down_sample_target_n("/vision/filter/down_sampling/fast_target_n", 5000); //fast downsampling

// Parameters of wall extractor
Parameter<double> _distance_threshold("/vision/walls/dist_thresh", 0.01);
Parameter<double> _halt_condition("/vision/walls/halt_condition", 0.2);

Parameter<int>    _outlier_meanK("/vision/walls/outliers/meanK", 50);
Parameter<double> _outlier_thresh("/vision/walls/outliers/thresh", 0.5);

// Parameters of object detector
Parameter<double> _wall_thickness("/vision/rois/dist_thresh", 0.008);
Parameter<int> _cluster_min("/vision/rois/cluster_min", 100);
Parameter<int> _cluster_max("/vision/rois/cluster_max", 5000);
Parameter<double> _cluster_tolerance("/vision/rois/cluster_tolerance", 0.015);
Parameter<double> _max_object_height("/vision/rois/max_object_height", 0.07);


void callback_point_cloud(const pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr& pcloud)
{
    _pcloud = pcloud;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "object_detector");

    ros::NodeHandle n;
    ros::Subscriber sub_pcloud = n.subscribe<pcl::PointCloud<pcl::PointXYZRGB> >("/camera/depth_registered/points", 3, callback_point_cloud);
    ros::Publisher pub_detection = n.advertise<std_msgs::Float64>("/vision/detector/obstacle/distance",1);
    ros::Publisher pub_rois = n.advertise<object_detector::ROI>("/vision/obstacles/rois",1);

    ros::Rate loop_rate(PUBLISH_FREQUENCY);
    Eigen::Vector3d leaf_size;

    _filtered = common::PointCloudRGB::Ptr(new common::PointCloudRGB);
    common::Timer timer;

    object_detector::ROIPtr roimsg = object_detector::ROIPtr(new object_detector::ROI);

    while(n.ok())
    {

        if (_pcloud != NULL && !_pcloud->empty())
        {
            timer.start();

            _filtered->clear();
            _pre_filter.set_frustum_culling(_frustum_near(), _frustum_far(), _frustum_horz_fov(), _frustum_vert_fov());
            _pre_filter.set_outlier_removal(_outlier_meanK(), _outlier_thresh());
            _pre_filter.set_voxel_leaf_size(_leaf_size(),_leaf_size(),_leaf_size());
            _pre_filter.enable_fast_downsampling(_fast_down_sampling(),_down_sample_target_n());
            _pre_filter.filter(_pcloud,_filtered);

            double t_prefilter = timer.elapsed();
            timer.start();

            _roi_extractor.set_cluster_constraints(_cluster_tolerance(), _cluster_min(), _cluster_max());

            leaf_size.setConstant(_leaf_size());
            common::vision::SegmentedPlane::ArrayPtr walls = _wall_extractor.extract(_filtered,_distance_threshold(),_halt_condition(),leaf_size);

            double t_walls = timer.elapsed();
            timer.start();

            common::vision::ROIArrayPtr rois = _roi_extractor.extract(walls,_filtered,_wall_thickness(),_max_object_height());
            double t_rois = timer.elapsed();

            //TODO: ------------------------------------------------------------
            //find closest object and publish distance to closest point

            double t_sum = t_prefilter+t_walls+t_rois;

#if ENABLE_TIME_PROFILING == 1
            ROS_INFO("Time spent on vision. Sum: %.3lf | Prefilter: %.3lf | Walls: %.3lf | Rois: %.3lf\n", t_sum, t_prefilter, t_walls, t_rois);
#endif

            roimsg->pointClouds.clear();
            common::vision::roiToMsg(rois,roimsg);
            pub_rois.publish(roimsg);

        }

        ros::spinOnce();
        loop_rate.sleep();
    }

    return 0;
}
