/**
 * Copyright (C) 2022, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <cmath>
#include <iostream>
#include <map>
//#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "common.h"
#include "gauge.h"

using namespace cv;
using namespace std;

Gauge::Gauge(
    const Mat &img,
    const Point &point_min,
    const Point &point_center,
    const Point &point_max,
    const bool clockwise)
    : clockwise(clockwise), img_size(img.size())
{
    // Calculate angles and radiuses
    angle_min = GetDegree(point_center, point_min);
    angle_max = GetDegree(point_center, point_max);
    angle_min_max = AngleDifference(angle_min, angle_max);
    auto d1 = EuclidianDistance(point_center, point_min);
    auto d2 = EuclidianDistance(point_center, point_max);
    unsigned int radii = round((d1 + d2) / 2);
    big_radii = round(radii * 0.75);
    small_radii = round(radii / 3);

    // Crop to avoid processing pixels outside gauge area
    auto crop_radii = max(d1, d2);
    int max_x = point_center.x + crop_radii;
    int max_y = point_center.y + crop_radii;
    int min_x = point_center.x - crop_radii;
    int min_y = point_center.y - crop_radii;
    if (img.size().width < max_x)
    {
        max_x = img.size().width;
    }
    if (img.size().height < max_y)
    {
        max_y = img.size().height;
    }
    if (0 > min_x)
    {
        min_x = 0;
    }
    if (0 > min_y)
    {
        min_y = 0;
    }
    croprange_x = Range(min_x, max_x);
    croprange_y = Range(min_y, max_y);
    const Point offset(croprange_x.start, croprange_y.start);
    this->point_min = point_min - offset;
    this->point_center = point_center - offset;
    this->point_max = point_max - offset;
    Mat cropped_img = img(croprange_y, croprange_x);
    // imwrite("cropped_img_c++.jpg", cropped_img);

    // Create gauge masks
    CreateMask(cropped_img, big_mask, big_radii);
    CreateMask(cropped_img, small_mask, small_radii);
    bitwise_xor(big_mask, small_mask, global_mask);
    // imwrite("mask_0_big_c++.png", big_mask);
    // imwrite("mask_1_small_c++.png", small_mask);
    // imwrite("mask_2_global_c++.png", global_mask);

    LOG_I("%s/%s: img size: (%u, %u)", __FILE__, __FUNCTION__, img_size.width, img_size.height);
}

Gauge::~Gauge()
{
}

double Gauge::ComputeGaugeValue(const Mat &img) const
{
    // Make sure input image has the same size as the gague was set up for
    assert(img_size == img.size());

    // Use 2 matrices for work back and forth
    Mat mat_a;
    Mat mat_b;

    // Crop
    mat_a = img(croprange_y, croprange_x);

    // Convert to grayscale
    // cvtColor(mat_b, mat_a, COLOR_BGR2GRAY);
    // imwrite("compute_gauge_value_0_gray_c++.jpg", mat_a);

    // Always do dark check for handling shifting light conditions over time
    if (IsDark(mat_a, big_mask))
    {
        // Invert
        InvertImg(mat_a);
    }
    // imwrite("compute_gauge_value_1_gray_after_dark_c++.jpg", mat_a);

    // Prepare image for contour detection
    GaussianBlur(mat_a, mat_b, Size(5, 5), 0);
    // imwrite("compute_gauge_value_2_mat_a_mat_b_c++.jpg", mat_b);
    adaptiveThreshold(mat_b, mat_a, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 11, 2);
    InvertImg(mat_a);
    // imwrite("compute_gauge_value_3_mat_a_c++.jpg", mat_a);
    Mat kernel(2, 2, CV_8U, 1);
    morphologyEx(mat_a, mat_b, MORPH_CLOSE, kernel);
    // imwrite("compute_gauge_value_4_mat_b_c++.jpg", mat_b);
    bitwise_and(mat_b, global_mask, mat_a);
    // imwrite("compute_gauge_value_5_mat_a_c++.jpg", mat_a);

    Point pointer_edge;
    if (!ContourEdgePoint(mat_a, pointer_edge))
    {
        LOG_E("%s/%s: ContourEdgePoint FAILED", __FILE__, __FUNCTION__);
        return -1;
    }

    // Calculate and return value (percent)
    auto angle_pointer = GetDegree(point_center, pointer_edge);
    auto min_pointer_angle = AngleDifference(angle_min, angle_pointer);
    if (350 < min_pointer_angle && 360 > min_pointer_angle)
    {
        // pointer slightly below min => minimum value
        return 0;
    }
    else if (min_pointer_angle > angle_min_max)
    {
        return 100;
    }
    return 100 * min_pointer_angle / angle_min_max;
}

double Gauge::EuclidianDistance(const Point &a, const Point &b) const
{
    const Point dp = a - b;
    return sqrt(pow(abs(dp.x), 2) + pow(abs(dp.y), 2));
}

double Gauge::GetDegree(const Point &origo, const Point &point) const
{
    const Point dp = point - origo;
    double angle = atan2(dp.y, dp.x) * 180 / M_PI;
    if (0 > angle)
    {
        angle += 360;
    }
    return angle;
}

bool Gauge::IsDark(const Mat &img, const Mat &mask) const
{
    unsigned int number_of_light_pix = 0;
    unsigned int number_of_dark_pix = 0;
    for (auto i = 0; i < img.rows; i++)
    {
        for (auto j = 0; j < img.cols; j++)
        {
            if (255 == mask.at<uchar>(i, j))
            {
                if (100 < img.at<uchar>(i, j))
                {
                    number_of_light_pix++;
                }
                else
                {
                    number_of_dark_pix++;
                }
            }
        }
    }

    return number_of_light_pix < number_of_dark_pix;
}

double Gauge::AngleDifference(const double base_point, const double mesh_point) const
{
    double angle = mesh_point - base_point;
    if (!clockwise)
    {
        angle = 0 - angle;
    }
    return angle > 0 ? angle : angle + 360;
}

void Gauge::CreateMask(const Mat &img, Mat &mask, const unsigned int radii) const
{
    mask = Mat::zeros(img.size(), CV_8U);
    double ellipse_min = angle_min - 10;
    double ellipse_max = angle_max + 10;
    if (clockwise && ellipse_max < ellipse_min)
    {
        ellipse_max += 360;
    }
    else if (!clockwise && ellipse_max > ellipse_min)
    {
        ellipse_max -= 360;
    }
    ellipse(
        mask, point_center, Size(radii, radii), 0.0, ellipse_min, ellipse_max, Scalar(255, 255, 255), -1, LINE_8, 0);
}

inline void Gauge::InvertImg(Mat &img) const
{
    img.forEach<Point_<uchar>>(
        [&img](Point_<uchar> &pixel, const int *position) -> void
        {
            // TODO: Remove unused variable pixel
            (void)pixel;
            img.at<uchar>(position) ^= 0xff;
        });
}

bool Gauge::ContourEdgePoint(const Mat &img, Point &edge_point) const
{
    vector<vector<Point>> cnts;
    vector<Vec4i> hierarchy;
    findContours(img, cnts, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    sort(
        cnts.begin(),
        cnts.end(),
        [](const vector<Point> &c1, const vector<Point> &c2)
        { return contourArea(c1, false) > contourArea(c2, false); });
    Mat big_circle_mask = Mat::zeros(img.size(), CV_8U);
    Mat small_circle_mask = Mat::zeros(img.size(), CV_8U);
    circle(big_circle_mask, point_center, big_radii, Scalar(255), 2);
    circle(small_circle_mask, point_center, small_radii, Scalar(255), 2);
    // imwrite("contour_edge_point_1_big_circle_mask_c++.jpg", big_circle_mask);
    // imwrite("contour_edge_point_2_small_circle_mask_c++.jpg", small_circle_mask);
    for (unsigned int i = 0; i < cnts.size(); i++)
    {
        Mat object_mask = Mat::zeros(img.size(), CV_8U);
        drawContours(object_mask, cnts, i, Scalar(255), 2);
        // imwrite("contour_edge_point_3_object_mask_c++.jpg", object_mask);
        Mat big_object_mask;
        bitwise_and(object_mask, big_circle_mask, big_object_mask);
        Mat small_object_mask;
        bitwise_and(object_mask, small_circle_mask, small_object_mask);
        // No further action if there is no content
        // imwrite("contour_edge_point_4_big_object_mask_c++.jpg", big_object_mask);
        // imwrite("contour_edge_point_5_small_object_mask_c++.jpg", small_object_mask);
        if (0 < sum(big_object_mask)[0] && 0 < sum(small_object_mask)[0])
        {
            // evaluate every contour distance to center point and return
            // pointer coordinates with largest distance to center point
            map<double, Point> distances;
            for (auto p : cnts.at(i))
            {
                distances.emplace(EuclidianDistance(point_center, p), p);
            }
            // Maps sort the largest key values at the end
            edge_point = distances.rbegin()->second;
            return true;
        }
    }

    return false;
}
