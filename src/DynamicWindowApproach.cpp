#include <iostream>
#include <Eigen/Eigen>
#include <stdio.h>      
#include <math.h>    
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <array>
#include <vector>

#define pi 3.14
using namespace std;
using namespace cv;

const double max_speed = 1.0; //[m/s] ����ٶ�
const double min_speed = -1.0; //[m/s] ��С�ٶ�
const double max_yawrate = 40.0 * pi / 180.0; //[rad/s] �����ٶ�
const double max_accel = 0.2;  //[m/ss] �����ٶ�
const double max_dyawrate = 40.0 * pi / 180.0; //[rad/ss] ���Ǽ��ٶ�
const double v_reso = 0.01;  //�����ٶȵĲ���
const double yawrate_reso = 0.1 * pi / 180.0;  //�������ٶȵĲ��� [rad / s]
const double dt = 0.1;  
const double predict_time = 5.0;  //�����켣��ʱ��
const double heading_cost_gain = 0.15; //Ŀ�꺯����heading��λ�����ۺ�����ϵ��
const double speed_cost_gain = 1.0;//Ŀ�꺯�����ٶ����ۺ�����ϵ��
const double obstacle_cost_gain = 1.0;  //Ŀ�꺯����dist���ۺ�����ϵ��
const double robot_radius = 1.0;  //�����˰뾶

Mat Map;  //��ͼΪopencv���ɵ�ͼƬ
int mapSize = 300;  //��ͼ��СΪ300*300��������
using position = std::vector<std::array<float, 2>>; //���xy�����vector�����㻭ͼ
using State = array<double, 5>; //״̬
using Traj = vector<array<double, 5>>; //���״̬�Ĺ켣
using Control = array<double, 2>; //�ٶ�
using Dynamic_Window = array<double, 4>; //�ٶȵĶ�̬����

position start({ {0.0,0.0} });//�������
position goal({ {100.0,100.0} });//�յ�����
// ����ϰ�����
position ob({ {{-10.0, -10.0},
			   {0.0, 20.0},
			   {40.0, 20.0},
			   {50.0, 40.0},
			   {50.0, 50.0},
			   {50.0, 60.0},
			   {50.0, 90.0},
			   {80.0, 90.0},
			   {70.0, 90.0}} });

//��ʼ��״̬[x(m), y(m), yaw(rad), v(m/s), omega(rad/s)]
State x{ 0.0, 0.0, pi / 2.0, 0.0, 0.0 };

cv::Point2i cv_offset(float x, float y, int image_width, int image_height);
void drawMap(Traj ptraj); 
State motion(State x, Control u);
Dynamic_Window calc_dynamic_window(State x);
Traj predict_trajectory(State x, double v, double w);
Traj calc_final_input(State x, Control& u, Dynamic_Window dw);
double calc_to_goal_cost(Traj traj);
double calc_to_ob_cost(Traj traj);
Traj dwa_control(State x, Control& u);

int main()
{
	//��ʼ���ٶ�
	Control u{ 0.0, 0.0 };
	//��ʼ������״̬�Ĺ켣
	Traj traj;
	traj.push_back(x);

	while (1)
	{
		Traj ptraj = dwa_control(x, u);
		x = motion(x, u);
		traj.push_back(x);
		//��ͼ����
		drawMap(ptraj);
		imshow("����", Map);
		waitKey(1);
		if (abs(x[0] - goal[0][0])<3 && abs(x[1] - goal[0][1]) < 3)
			break;
	}
	// ���ô���
	Map = Mat::zeros(Size(500, 500), CV_8UC3);
	Map.setTo(255);              // ������ĻΪ��ɫ
	//�����ϰ� ��ɫ
	for (unsigned int j = 0; j < ob.size(); j++) {
		cv::circle(Map, cv_offset(ob[j][0], ob[j][1], Map.cols, Map.rows),
			1, cv::Scalar(0, 0, 0), -2);
	}
	//����Ŀ�� ��ɫ
	cv::circle(Map, cv_offset(goal[0][0], goal[0][1], Map.cols, Map.rows),
		2, cv::Scalar(255, 0, 0), 2);
	//������� ��ɫ
	cv::circle(Map, cv_offset(start[0][0], start[0][1], Map.cols, Map.rows),
		2, cv::Scalar(0, 255, 0), 2);
	//���ƹ켣
	for (int j = 0; j < traj.size(); j++) {
		cv::circle(Map, cv_offset(traj[j][0], traj[j][1], Map.cols, Map.rows),
			1, cv::Scalar(0, 0, 255), 1);
	}
	imshow("����", Map);
	waitKey();
	return 0;
}

void drawMap(Traj ptraj) {
	// ���ô���
	Map = Mat::zeros(Size(500, 500), CV_8UC3);
	Map.setTo(255);              // ������ĻΪ��ɫ
	//�����ϰ� ��ɫ
	for (unsigned int j = 0; j < ob.size(); j++) {
		cv::circle(Map, cv_offset(ob[j][0], ob[j][1], Map.cols, Map.rows),
			1, cv::Scalar(0, 0, 0), -2);
	}
	//����Ŀ�� ��ɫ
	cv::circle(Map, cv_offset(goal[0][0], goal[0][1], Map.cols, Map.rows),
		2, cv::Scalar(255, 0, 0), 2);
	//������� ��ɫ
	cv::circle(Map, cv_offset(start[0][0], start[0][1], Map.cols, Map.rows),
		2, cv::Scalar(0, 255, 0), 2);
	//���Ƶ�ǰ���� ��ɫ
	cv::circle(Map, cv_offset(x[0], x[1], Map.cols, Map.rows),
		2, cv::Scalar(0, 0, 255), 1);
	//�����ٶȴ��ڹ켣
	for (int j = 0; j < ptraj.size(); j++) {
		cv::circle(Map, cv_offset(ptraj[j][0], ptraj[j][1], Map.cols, Map.rows),
			1, cv::Scalar(210, 0, 0), 1);
	}
}

//opencvͼ�������ͱ�������Map�����궨�岻ͬ���øú�����һ�������ת��Ϊͼ���ϵĵ�
cv::Point2i cv_offset(
	float x, float y, int image_width = 500, int image_height = 500) {
	cv::Point2i output;
	//output.x = int(x * 100) + image_width / 2;
	//output.y = image_height - int(y * 100) - image_height / 3;
	output.x = int(100 + x);
	output.y = int(image_height - y - 100);
	return output;
};

//�����ٶȼ�����һʱ�̵�״̬
State motion(State x, Control u) {
	x[2] += u[1] * dt; //thetaƫ����
	x[0] += u[0] * cos(x[2])*dt; //x����
	x[1] += u[0] * sin(x[2])*dt; //y����
	x[3] = u[0]; //���ٶ�
	x[4] = u[1]; //���ٶ�
	return x;
}

//�����ٶȵĶ�̬����
Dynamic_Window calc_dynamic_window(State x) {
	return { {max(min_speed,x[3] - max_accel * dt),
			  min(max_speed,x[3] + max_accel * dt),
			  max(-max_yawrate,x[4] - max_dyawrate * dt),
			  min(max_yawrate,x[4] + max_dyawrate * dt)} };
}

//�����ٶ�Ԥ��һ��ʱ���ڵĹ켣
Traj predict_trajectory(State x, double v, double w) {
	Traj traj;
	traj.push_back(x);
	Control c = { v,w };
	double time = 0;
	for (double time = 0; time <= predict_time; time += dt)
	{
		x = motion(x, c);
		traj.push_back(x);
	}
	return traj;
}

//�ö�̬���ڷ�����������ٶȺ͹켣
Traj calc_final_input(State x, Control& u, Dynamic_Window dw) {
	double min_cost = 1000000.0;
	Control best_u = { 0.0,0.0 };
	Traj best_traj;
	//������̬������ÿһ���켣
	for (double v = dw[0]; v <= dw[1]; v += v_reso)
	{
		for (double w = dw[2]; w <= dw[3]; w += yawrate_reso)
		{
			Traj traj = predict_trajectory(x, v, w);
			//����cost
			double to_goal_cost = heading_cost_gain * calc_to_goal_cost(traj);
			double speed_cost = speed_cost_gain * (max_speed - traj[traj.size() - 1][3]);
			double ob_cost = obstacle_cost_gain * calc_to_ob_cost(traj);
			double final_cost = to_goal_cost + speed_cost + ob_cost;
			if (min_cost >= final_cost) {
				min_cost = final_cost;
				best_u = { v,w };
				best_traj = traj;
			}
		}
	}
	u = best_u;
	return best_traj;
}

//���㵽Ŀ���cost
double calc_to_goal_cost(Traj traj){
	//�켣�յ��ƫ���Ǻͻ����˵�Ŀ���н�֮��
	double dx = goal[0][0] - traj[traj.size() - 1][0];
	double dy = goal[0][1] - traj[traj.size() - 1][1];
	double error_angle = atan2(dy, dx);
	double cost = abs(error_angle - traj[traj.size() - 1][2]);
	return cost;
}

//�����ϰ����cost
double calc_to_ob_cost(Traj traj) {
	//���㵱ǰ�켣���ϰ������С����
	int skip_n = 2;
	double minr = 1000000.0;
	for (int i = 0; i < traj.size(); i += skip_n) 
	{
		for (int j = 0; j < ob.size(); j++) 
		{
			double dx = traj[i][0] - ob[j][0];
			double dy = traj[i][1] - ob[j][1];
			double r = sqrt(pow(dx, 2) + pow(dy, 2));
			if(r<= robot_radius)
				return 1000000.0;
			if (minr >= r)
				minr = r;
		}
	}
	return 1.0 / minr;
}

//��̬���ڷ����ٶȷ��ع켣
Traj dwa_control(State x, Control& u) {
	Dynamic_Window dw = calc_dynamic_window(x);
	Traj traj = calc_final_input(x, u, dw);
	return traj;
}