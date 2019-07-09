#include <iostream>
#include <stdio.h>      
#include <math.h>    
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <array>
#include <vector>

using namespace std;
using namespace cv;

double KP = 5.0;  // ��������K
double ETA = 100000000.0; // ���������
const double robot_radius = 5.0;  //�����˰뾶
using position = std::vector<std::array<float, 2>>; //���xy�����vector�����㻭ͼ
position start({ {0.0,0.0} });//�������
position goal({ {250.0,250.0} });//�յ�����
// ����ϰ�����
position ob({ {{150.0, 150.0},
			   {50.0, 50.0},
			   {100.0, 92.0},
			   {200.0, 198.0}} });

//ÿ�����ض�Ӧ��ͼ�нڵ�
class MapNode {
public:
	int x = -1; 
	int y = -1;
	double ug = 0.0; //����
	double uo = 0.0; //����
	MapNode() { }

	MapNode(int x, int y,int ug, int uo) {
		this->x = x;
		this->y = y;
		this->ug = ug;
		this->uo = uo;
	}
	//�������
	double uf() {
		return ug + uo; 
	}
};

//��ͼΪopencv���ɵ�ͼƬ
Mat Map; 
//��ͼ��СΪ300*300��������
int mapSize = 300; 
//��ͼ��ÿ���ڵ����Ϣ ÿ��������Ӧһ���ڵ�  ����ת����ʽΪindex = 300*y+x��mapData[index]��Ϊ����xy�Ľڵ�Node
vector<MapNode> mapData; 

MapNode *startNode; // ָ�����Node
MapNode *targetNode;// ָ���յ�Node

MapNode *mapAt(int x, int y); //��XY����ת��Ϊ��Ӧ������

vector<MapNode *> neighbors(MapNode *node);//��ǰ�ڵ���ھӽڵ�

cv::Point2i cv_offset(float x, float y, int image_width, int image_height);

void drawMap(); //��ͼ����

void drawPath(Mat &map, vector<MapNode *> path);  //��������·��

double calc_attractive_potential(int x, int y); //��������
double calc_repulsive_potential(int x, int y); //�������
vector<MapNode *> potential_field_planning();  //�˹��Ƴ���

int main()
{
	//��ͼ����
	drawMap();
	//��ͼ��ÿ���ڵ����Ϣ��300*300����90000���ڵ�
	mapData = vector<MapNode>(90000);
	//��ȡ��ͼ��ÿ����ĺ���
	for (int y = 0; y < mapSize; y++) {
		for (int x = 0; x < mapSize; x++) {
			double ug = calc_attractive_potential(x, y);
			double uo = calc_repulsive_potential(x, y);
			mapData[y * mapSize + x] = MapNode(x, y, ug, uo);
		}	
	}
	startNode = &mapData[0 * 300 + 0];//���
	targetNode = &mapData[250 * 300 + 250];//�յ�
	cout << "startNode=(" << startNode->x << ", " << startNode->y << ")" << endl;
	cout << "endNode=(" << targetNode->x << ", " << targetNode->y << ")" << endl;
	vector<MapNode *> path = potential_field_planning();//�˹��Ƴ�Ѱ·
	drawPath(Map, path); //��������·��
	return 0;
}

////opencvͼ�������ͱ�������Map�����궨�岻ͬ���øú�����һ�������ת��Ϊͼ���ϵĵ�
cv::Point2i cv_offset(
	float x, float y, int image_width = 500, int image_height = 500) {
	cv::Point2i output;
	//output.x = int(x * 100) + image_width / 2;
	//output.y = image_height - int(y * 100) - image_height / 3;
	output.x = int(100 + x);
	output.y = int(image_height - y - 100);
	return output;
};

void drawMap() {
	// ���ô���
	Map = Mat::zeros(Size(500, 500), CV_8UC3);
	Map.setTo(255);              // ������ĻΪ��ɫ
	//�����ϰ� ��ɫ
	for (unsigned int j = 0; j < ob.size(); j++) {
		cv::circle(Map, cv_offset(ob[j][0], ob[j][1], Map.cols, Map.rows),
			1, cv::Scalar(0, 0, 0), 5);
	}
	//����Ŀ�� ��ɫ
	cv::circle(Map, cv_offset(goal[0][0], goal[0][1], Map.cols, Map.rows),
		2, cv::Scalar(255, 0, 0), 2);
	//������� ��ɫ
	cv::circle(Map, cv_offset(start[0][0], start[0][1], Map.cols, Map.rows),
		2, cv::Scalar(0, 255, 0), 2);
}

//�������յ�·��
void drawPath(Mat &map, vector<MapNode *> path) {
	//Path�����ÿһ��·���ϵĵ�
	for (int i = 0; i < path.size() - 1; i++) {
		MapNode *node = path[i];
		cv::circle(Map, cv_offset(node->x, node->y, Map.cols, Map.rows),
			2, cv::Scalar(0, 200, 155), 1);
		imshow("����", Map);
		waitKey(1);
		cout << "->(" << node->x << "," << node->y << ")";
	}
	imshow("����", Map);
	waitKey();
	cout << endl;
}

//�˹��Ƴ���
vector<MapNode *> potential_field_planning() {
	vector<MapNode *> path; //��������·���ϵĽڵ�
	cout << "Finding started!" << endl;
	MapNode *node = startNode;
	path.push_back(node);
	MapNode *next;
	while (1)
	{
		double minp = 1000000.0;
		//���������Ŀ��㣬����ѭ����
		if (abs(node->x-targetNode->x)<2&& abs(node->y - targetNode->y) < 2)
		{
			cout << "Reached the target node." << endl;
			break;
		}
		//�ҵ���ǰ�ڵ�����ڽڵ�
		vector<MapNode *> neighborNodes = neighbors(node);
		//�����������ڽڵ㣬ѡ���������
		for (int i = 0; i < neighborNodes.size(); i++) 
		{
			MapNode *_node = neighborNodes[i];
			double p = _node->uf();
			if (minp > p)
			{
				minp = p;
				next = _node;
			}
		}
		path.push_back(next);
		node = next;
		Map = Mat::zeros(Size(500, 500), CV_8UC3);
		Map.setTo(255);              // ������ĻΪ��ɫ
		//�����ϰ� ��ɫ
		for (unsigned int j = 0; j < ob.size(); j++) {
			cv::circle(Map, cv_offset(ob[j][0], ob[j][1], Map.cols, Map.rows),
				1, cv::Scalar(0, 0, 0), 2);
		}
		//����Ŀ�� ��ɫ
		cv::circle(Map, cv_offset(goal[0][0], goal[0][1], Map.cols, Map.rows),
			2, cv::Scalar(255, 0, 0), 2);
		//������� ��ɫ
		cv::circle(Map, cv_offset(start[0][0], start[0][1], Map.cols, Map.rows),
			2, cv::Scalar(0, 255, 0), 2);
		//���ƻ����� ��
		cv::circle(Map, cv_offset(node->x, node->y, Map.cols, Map.rows),
			2, cv::Scalar(0, 0, 255), 2);
		imshow("����", Map);
		waitKey(20);
	}
	//����·��
	return path;
}

// �����ھӽڵ�
vector<MapNode *> neighbors(MapNode *node) {
	vector<MapNode *> available;
	MapNode *_node;
	//����ýڵ���ھ��ڵ�ͼ�У���ô����
	// ��
	if ((_node = mapAt(node->x - 1, node->y)) != 0)available.push_back(_node);
	// ��
	if ((_node = mapAt(node->x, node->y - 1)) != 0)available.push_back(_node);
	// ��
	if ((_node = mapAt(node->x + 1, node->y)) != 0)available.push_back(_node);
	// ��
	if ((_node = mapAt(node->x, node->y + 1)) != 0)available.push_back(_node);
	// ����
	if ((_node = mapAt(node->x - 1, node->y - 1)) != 0)available.push_back(_node);
	// ����
	if ((_node = mapAt(node->x + 1, node->y - 1)) != 0)available.push_back(_node);
	// ����
	if ((_node = mapAt(node->x + 1, node->y + 1)) != 0)available.push_back(_node);
	// ����
	if ((_node = mapAt(node->x - 1, node->y + 1)) != 0)available.push_back(_node);
	return available;
}

// ������������Ӧ�����Ľڵ�
MapNode *mapAt(int x, int y) {
	if (x < 0 || y < 0 || x >= mapSize || y >= mapSize)return 0;
	return &mapData[y * mapSize + x];
}

//��������
double calc_attractive_potential(int x, int y) {
	return 0.5*KP*(sqrt(pow((x - goal[0][0]), 2) + pow((y - goal[0][1]), 2)));
}

//�������
double calc_repulsive_potential(int x, int y) {
	double dmin = 1000000.0;//���������������ϰ������
	for (int i = 0; i < ob.size(); i++)
	{
		double d = sqrt(pow((x - ob[i][0]), 2) + pow((y - ob[i][1]), 2));
		if (dmin >= d)
			dmin = d;
	}
	if (dmin <= robot_radius)
	{
		if (dmin <= 0.1)
			dmin = 0.1;
		return 0.5*ETA*pow((1.0 / dmin - 1.0 / robot_radius), 2);
	}
	else
		return 0.0;
}