#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <pcl_ros/point_cloud.h>
#include <sensor_msgs/PointCloud2.h>
#include <pcl/point_types_conversion.h>
#include <pcl/point_types.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "color_classifier.h"
#include <object_detector/ROI.h>

static const char * ColorNames[] = {
    "red",
    "yellow",
    "blue",
    "green",
    "green_light",
    "orange",
    "plurple",
};

double _reference_hues[7] = {
    1.30828, // good
    34.643364, // good
    215.820892, // works for now. needs testing
    110,
    65,
    20,
    280,
};


std::vector<ColorClassifier> _classifiers;


pcl::PointCloud<pcl::PointXYZRGB>::Ptr _cloudrgb;
std::vector< pcl::PointCloud<pcl::PointXYZHSV>::Ptr > _hsv_clouds;
pcl::PointCloud<pcl::PointXYZHSV>::Ptr _cloudhsv;
std::vector<std::vector<double> > _hues;
int _flag=0;
int ref=0;

double red_compensation(double hue,int flag)
{
    if (flag==1)
    {
        if (hue < 50) return hue+360;
        else return hue;
    }
    else return hue;
}

void RoisCallBack(const object_detector::ROIConstPtr& cloudrois)
{

    int numclouds=1;//cloudrois->pointClouds.size();
    ////this is some magic that max did for creating a vector of hsv_clouds

    _hsv_clouds.reserve(numclouds);
    _hues.reserve(numclouds);
    while(_hsv_clouds.size() < numclouds) {
        _hsv_clouds.push_back(pcl::PointCloud<pcl::PointXYZHSV>::Ptr(new pcl::PointCloud<pcl::PointXYZHSV>));
    }
    while(_hues.size() < numclouds) {
        _hues.push_back(std::vector<double>());
    }
    ///////we get hsv hue (a row) from each pointcloud and put them into a matrix
    ///for each point cloud


    for (int i=0; i< numclouds;i++)
    {
        _hsv_clouds[i]->clear();
        _cloudrgb->clear();

        _cloudhsv->clear();

        pcl::fromROSMsg<pcl::PointXYZRGB>(cloudrois->pointClouds[i],*_cloudrgb);

        pcl::PointCloudXYZRGBtoXYZHSV(*_cloudrgb,*_cloudhsv);

        ////for points in one pointcloud
        int numpoints=_cloudhsv->size();

        // if(numpoints==0) continue; // check if the cloud has points?!?

        double sum=0;
        _hues[i].resize(numpoints);
        for(int j=0; j<numpoints; j++)
        {
            pcl::PointXYZHSV& pointhsv = _cloudhsv->at(j);
            ROS_INFO("Color of point %d \t:\t %f \t %f",j,red_compensation(pointhsv.h,0),pointhsv.v);
            // this value also wraps around 360 so the wrong value will be red! the rest of the colors have slight fluctuations but nothing that seems relevant
            sum=sum+red_compensation(pointhsv.h,0); // set second argument to 0 when getting reference values for colors different from red
            _hues[i][j]=pointhsv.h;
        }
        double average=sum/numpoints;
        double ref=average; // Check this value to get color specific reference value

        //ROS_ERROR("Reference color: %lf\n",ref);

        // Check this function! values seem to be 0-360.

        }
   _flag=1;

}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "color_detector");

    ros::NodeHandle n;


    _cloudrgb = pcl::PointCloud<pcl::PointXYZRGB>::Ptr(new pcl::PointCloud<pcl::PointXYZRGB>);
    _cloudhsv = pcl::PointCloud<pcl::PointXYZHSV>::Ptr(new pcl::PointCloud<pcl::PointXYZHSV>);


    ros::Subscriber sub_rois=n.subscribe<object_detector::ROI>
            ("/vision/obstacles/rois",1,RoisCallBack);

    ros::Rate loop_rate(10);

    // initialize reference hues classifers
    int num = sizeof(_reference_hues)/sizeof(double);
    for (int i = 0; i < num ; ++i)
    {
        _classifiers.push_back(ColorClassifier(_reference_hues[i]));
    }
    ///////////////
    while(ros::ok())
    {
        std::cout << "Waiting for callback" << std::endl;

        if (_flag!=0)
        {

            //            std::cout << "reference hue is " << ref<<std::endl;
            //            //std::cout <<  _cloudrgb->size() << "\n";
            //            /*
            //            // For reference hue values //////////////

            //            double sum=0;
            //            for(int i=0;i<_cloudhsv->size();++i)
            //            {
            //                pcl::PointXYZHSV& pointhsv = _cloudhsv->at(i);
            //                sum=sum+pointhsv.h;
            //            }

            //            double average=sum/_cloudhsv->size();
            //            int ref=round(average); // Check this value to get color specific reference value
            //            std::cout << "reference hue is " << ref;
            //            */
            //            //////////////////////////////////////////

            //            std::cout << "apple" <<std::endl;
            //            int max= _cloudhsv->size();
            //            std::vector<double> hues;
            ///*
            //            for (int i=0 ;i < max; ++i)
            //            {
            //                std::cout <<  _cloudrgb->size() << "\n";
            //                pcl::PointXYZHSV& pointhsv = _cloudhsv->at(i);
            //                int hue=round(pointhsv.h);
            //                hues[i]=hue;
            //                //std::cout << i << "\t" << hue << "\t" << "total" << "\t" << hues[hue] << "\n";
            //            }

            double max_probability = 0;
            int best_i = 0;
            num=7;
            double green_probability =0;
            for (int i = 0; i < num; ++i)
            {
                for (int j=0;j<_hues.size();j++)
                {
                    ColorClassifier& classifier = _classifiers[i];
                    double probability = classifier.classify(_hues[j]);

                    if (probability > max_probability)
                    {
                        max_probability = probability;
                        best_i = i;
                    }
                    if (i ==3) green_probability =probability;
                    std::cout << "Color: " << ColorNames[i] << " has probability: " << probability << std::endl;
                }
            }
            if (green_probability > 0.005) {std::cout << "It is green"  << std::endl;}
            else std::cout << "It is " << ColorNames[best_i] << std::endl;

            //            }

            //            /////////////////////////////////////////

        }

        ros::spinOnce();
        loop_rate.sleep();
    }

    return 0;
}
