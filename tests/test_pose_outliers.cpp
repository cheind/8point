/**
    This file is part of Deformable Shape Tracking (DEST).

    Copyright(C) 2015/2016 Christoph Heindl
    All rights reserved.

    This software may be modified and distributed under the terms
    of the BSD license.See the LICENSE file for details.
*/


#include "catch.hpp"

#include <eight/fundamental.h>
#include <eight/normalize.h>
#include <eight/distance.h>
#include <eight/essential.h>
#include <eight/project.h>
#include <eight/select.h>
#include <random>
#include <iostream>
#include "utils.h"

TEST_CASE("test_pose_outliers")
{
    const double foc = 530.0;
    const int width = 640;
    const int height = 480;
    const int nPoints = 60;

    Eigen::Matrix3d k;
    k << 
        foc, 0.0, 0.5 *(width - 1),
        0.0, foc, 0.5 *(height - 1),
        0.0, 0.0, 1.0;

    // Generate random 3D points
    Eigen::Matrix<double, 3, Eigen::Dynamic> points = eight::utils::samplePointsInBox(Eigen::Vector3d(-500, -500, 300), Eigen::Vector3d(500, 500, 1500), nPoints);

    // Assume the first camera at origin and the second freely transformed.
    Eigen::AffineCompact3d t0;
    t0.setIdentity();

    Eigen::AffineCompact3d t1;
    t1 = Eigen::Translation3d(15.0, 0.0, 3.5) * Eigen::AngleAxisd(0.25*M_PI, Eigen::Vector3d(0.5, -0.3, 0.2).normalized());

    // Generate projected image points
    Eigen::Matrix<double, 3, 4> cam0 = eight::cameraMatrix(k, t0);
    Eigen::Matrix<double, 3, 4> cam1 = eight::cameraMatrix(k, t1);

    Eigen::Matrix<double, 2, Eigen::Dynamic> image0 = eight::perspectiveProject(points, cam0).colwise().hnormalized();
    Eigen::Matrix<double, 2, Eigen::Dynamic> image1 = eight::perspectiveProject(points, cam1).colwise().hnormalized();

    // Disturb around 10% of outliers
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Eigen::DenseIndex> dis(0, image0.cols() - 1);
    for (int i = 0; i < nPoints / 10; i++) {
        Eigen::DenseIndex idx = dis(gen);
        image0.col(idx) += Eigen::Vector2d::Random() * 20.0;
        image1.col(idx) += Eigen::Vector2d::Random() * 20.0;
    }

    std::vector<Eigen::DenseIndex> inliers;
    Eigen::Matrix3d F = eight::fundamentalMatrixRobust(image0, image1, inliers, 0.1);
    Eigen::Matrix3d E = eight::essentialMatrix(k, F);

    image0 = eight::selectColumnsByIndex(image0, inliers.begin(), inliers.end());
    image1 = eight::selectColumnsByIndex(image1, inliers.begin(), inliers.end());

    Eigen::Matrix<double, 3, 4> pose = eight::pose(E, k, image0, image1);

    std::cout << "Should be: " << std::endl <<  t1.matrix() << std::endl;
    std::cout << "Pose: " << std::endl << pose << std::endl;

    // Note: Translation can only be compared up to scale.
    Eigen::Matrix<double, 3, 4> tm = t1.matrix();
    tm.block<3, 1>(0, 3).normalize();

    REQUIRE(pose.isApprox(tm, 1e-3));
}