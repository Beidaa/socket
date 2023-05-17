//
// Created by beida on 2023/5/17.
//

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

// 定义三个多边形的顶点坐标
vector<Point> vertices1 = {Point(50, 50), Point(100, 50), Point(100, 100), Point(50, 100)};
vector<Point> vertices2 = {Point(200, 100), Point(250, 100), Point(250, 150), Point(200, 150)};
vector<Point> vertices3 = {Point(400, 200), Point(450, 200), Point(450, 250), Point(400, 250)};

int main() {
    // 读入图像
    Mat image = imread("input.jpg");
    if (image.empty()) {
        cerr << "Failed to read input image\n";
        return -1;
    }

    // 创建掩模图像并将其设为全黑
    Mat mask(image.size(), CV_8UC1, Scalar(0));

    // 创建三个多边形区域
    const Point* ppt[] = {vertices1.data(), vertices2.data(), vertices3.data()};
    int npt[] = {vertices1.size(), vertices2.size(), vertices3.size()};
    fillPoly(mask, ppt, npt, 3, Scalar(255));

    // 将三个多边形区域之外的像素置为黑色
    image.setTo(Scalar(0), ~mask);

    // 显示结果图像
    imshow("Result", image);
    waitKey(0);
    return 0;
}
