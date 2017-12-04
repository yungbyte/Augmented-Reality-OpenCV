#include <stdio.h>
#include <iostream>
#include <cmath>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/xfeatures2d.hpp"
using namespace cv;
using namespace cv::xfeatures2d;

#define MIN_HESSIAN 400

int main()
{
  VideoCapture cap(0);
  VideoCapture vid("kendrick.mp4"); // load some fire
  Mat img_object, img_scene, descriptors_object, descriptors_scene, img_matches;;
  std::vector<KeyPoint> keypoints_object, keypoints_scene;
  FlannBasedMatcher matcher;

  //load template image
  img_object = imread("template.PNG", IMREAD_GRAYSCALE );
  resize(img_object, img_object, Size(500,500));
  //img_scene = imread( "test.jpg", IMREAD_GRAYSCALE );
  if( !img_object.data)
  { std::cout<< " --(!) Error reading image " << std::endl; return -1; }

  //-- Step 1: Detect the keypoints and extract descriptors using SURF
  Ptr<SURF> detector = SURF::create(MIN_HESSIAN);
  // detect the SURF keypoints in the object image
  detector->detectAndCompute( img_object, Mat(), keypoints_object, descriptors_object );

  Mat H_latest = Mat::eye(3, 3, CV_32F); // H is used to transform the video mat over the scene mat. H_old is used to store the last reasonable transform
  Mat scene_mask  = Mat::zeros(img_scene.rows, img_scene.cols, CV_8UC1);

  //main loop
  while(1){
    cap >> img_scene;
    if(!img_scene.data)
    { std::cout<< " --(!) Error reading camera " << std::endl; return -1; }

    // We can now initialize scene_mask with proper dimentions
    scene_mask = Mat::zeros(img_scene.rows, img_scene.cols, CV_8UC1);

    detector->detectAndCompute( img_scene, Mat(), keypoints_scene, descriptors_scene );
    //-- Step 2: Matching descriptor vectors using FLANN matcher

    std::vector< DMatch > matches;
    matcher.match( descriptors_object, descriptors_scene, matches );
    double max_dist = 0; double min_dist = 100;
    //-- Quick calculation of max and min distances between keypoints
    for( int i = 0; i < descriptors_object.rows; i++ )
    { double dist = matches[i].distance;
      if( dist < min_dist ) min_dist = dist;
      if( dist > max_dist ) max_dist = dist;
    }
    //printf("-- Max dist : %f \n", max_dist );
    //printf("-- Min dist : %f \n", min_dist );
    //-- Draw only "good" matches (i.e. whose distance is less than 3*min_dist )
    std::vector< DMatch > good_matches;
    for( int i = 0; i < descriptors_object.rows; i++ )
    { if( matches[i].distance < 3*min_dist )
       { good_matches.push_back( matches[i]); }
    }

    drawMatches( img_object, keypoints_object, img_scene, keypoints_scene,
                 good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
                 std::vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );
    if(img_matches.data) //check if there are any matches, if so then continue
    {
      //-- Localize the object
      std::vector<Point2f> obj;
      std::vector<Point2f> scene;
      for( size_t i = 0; i < good_matches.size(); i++ )
      {
        //-- Get the keypoints from the good matches
        obj.push_back( keypoints_object[ good_matches[i].queryIdx ].pt );
        scene.push_back( keypoints_scene[ good_matches[i].trainIdx ].pt );
      }
      //this returns a 3x3 transformation matrix
      Mat H = findHomography( obj, scene, RANSAC );
      //-- Get the corners from the image_1 ( the object to be "detected" )
      std::vector<Point2f> obj_corners(4);
      obj_corners[0] = cvPoint(0,0);
      obj_corners[1] = cvPoint( img_object.cols, 0 );
      obj_corners[2] = cvPoint( img_object.cols, img_object.rows );
      obj_corners[3] = cvPoint( 0, img_object.rows );
      std::vector<Point2f> scene_corners(4);
      perspectiveTransform( obj_corners, scene_corners, H);

      float hDet = abs(determinant(H));
      std::cout << "hDet=" << hDet << std::endl;
      if (hDet < 100 && hDet > 0.01){ // Good detection, reasonable transform
        std::cout << "Got match" << std::endl;
        H_latest = H;
        scene_mask = Mat::zeros(img_scene.rows, img_scene.cols, CV_8UC1);
        cv::Point nonfloat_corners[4];
        for(int i=0; i<4; i++){
         nonfloat_corners[i] = scene_corners[i];
        }
        fillConvexPoly(scene_mask, nonfloat_corners, 4, cv::Scalar(1));
      }

      //-- Draw lines between the corners (the mapped object in the scene - image_2 )

      std::vector<Point2f> match_corners(4);
      match_corners[0] = scene_corners[0] + Point2f( img_object.cols, 0);
      match_corners[1] = scene_corners[1] + Point2f( img_object.cols, 0);
      match_corners[2] = scene_corners[1] + Point2f( img_object.cols, 0);
      match_corners[3] = scene_corners[2] + Point2f( img_object.cols, 0);

      //line( img_matches, scene_corners[0] + Point2f( img_object.cols, 0), scene_corners[1] + Point2f( img_object.cols, 0), Scalar(0, 255, 0), 4 );
      line( img_matches, scene_corners[1] + Point2f( img_object.cols, 0), scene_corners[2] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
      line( img_matches, scene_corners[2] + Point2f( img_object.cols, 0), scene_corners[3] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
      line( img_matches, scene_corners[3] + Point2f( img_object.cols, 0), scene_corners[0] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
      //-- Show detected matches
      imshow( "SURF", img_matches );

      Mat img_video;
      vid >> img_video;
      //apply the homography transform matrix
      warpPerspective( img_video, img_video, H_latest, Size(img_video.cols,img_video.rows));

      std::cout << "Fixing to draw mask" << std::endl;
      imshow( "Test", scene_mask);
      std::cout << "Drew it" << std::endl;

      waitKey(1);
    }
  }
  return 0;
}
