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

#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

class Gauge
{
  public:
    Gauge(
        const cv::Mat &img,
        const cv::Point &point_center,
        const cv::Point &point_min,
        const cv::Point &point_max,
        const bool clockwise = true);
    ~Gauge();
    double ComputeGaugeValue(const cv::Mat &img) const;

  private:
    bool clockwise;
    cv::Mat big_mask;
    cv::Mat global_mask;
    cv::Mat small_mask;
    cv::Point point_center;
    cv::Point point_min;
    cv::Point point_max;
    cv::Range croprange_x;
    cv::Range croprange_y;
    cv::Size img_size;
    double angle_max = 0;
    double angle_min = 0;
    double angle_min_max = 0;
    unsigned int big_radii;
    unsigned int small_radii;
    double EuclidianDistance(const cv::Point &a, const cv::Point &b) const;
    double GetDegree(const cv::Point &origo, const cv::Point &point) const;
    bool IsDark(const cv::Mat &img, const cv::Mat &mask) const;
    double AngleDifference(const double base_point, const double mesh_point) const;
    void CreateMask(const cv::Mat &img, cv::Mat &mask, const unsigned int radii) const;
    inline void InvertImg(cv::Mat &img) const;
    bool ContourEdgePoint(const cv::Mat &img, cv::Point &edge_point) const;
};
