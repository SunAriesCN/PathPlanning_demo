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

using namespace std;
using namespace cv;

const int ALLOW_VERTEX_PASSTHROUGH = 1; //�Ƿ�����б����
const int NODE_FLAG_CLOSED = -1; //�ýڵ���CLOSE��
const int NODE_FLAG_UNDEFINED = 0; //�ýڵ�û�б�����
const int NODE_FLAG_OPEN = 1;  //�ýڵ���OPEN��

const int NODE_TYPE_ZERO = 0;  //�ýڵ����ͨ��
const int NODE_TYPE_OBSTACLE = 1;  //�ýڵ����ϰ�
const int NODE_TYPE_START = 2;  //�ýڵ�Ϊ���
const int NODE_TYPE_END = 3;  //�ýڵ�Ϊ�յ�

//ÿ�����ض�Ӧ��ͼ�нڵ�
class MapNode {
public:
	int x = -1; 
	int y = -1;
	int h = 0; //����ֵ
	int g = 0; //��ɢֵ
	int type = NODE_TYPE_ZERO;
	int flag = NODE_FLAG_UNDEFINED;
	MapNode *parent = 0;

	MapNode() { }

	MapNode(int x, int y, int type = NODE_TYPE_ZERO, int flag = NODE_FLAG_UNDEFINED, MapNode *parent = 0) {
		this->x = x;
		this->y = y;
		this->type = type;
		this->flag = flag;
		this->parent = parent;
	}

	int f() {
		return g + h; //Astar
		return g; //ֻ���Ǻ�ɢֵ����ΪDijkstra
	}
};

//��ͼΪopencv���ɵ�ͼƬ
Mat Map; 
//��ͼ��СΪ300*300��������
int mapSize = 300; 
//��ͼ��ÿ���ڵ����Ϣ ÿ��������Ӧһ���ڵ�  ����ת����ʽΪindex = 300*y+x��mapData[index]��Ϊ����xy�Ľڵ�Node
vector<MapNode> mapData; 
//Open��
vector<MapNode *> openList;  

MapNode *startNode; // ָ�����Node
MapNode *targetNode;// ָ���յ�Node

MapNode *mapAt(int x, int y); //��XY����ת��Ϊ��Ӧ������

vector<MapNode *> neighbors(MapNode *node);//��ǰ�ڵ���ھӽڵ�

int computeH(MapNode *node1, MapNode *node2);//����H

int computeG(MapNode *node1, MapNode *node2);//����G

vector<MapNode *> Astar(); //Ѱ��·��������

cv::Point2i cv_offset(float x, float y, int image_width, int image_height);

void drawMap(); //��ͼ����

void drawPath(Mat &map, vector<MapNode *> path);  //��������·��

void drawOpenList(); //����Openlist�еĵ�

using position = std::vector<std::array<float, 2>>; //���xy�����vector�����㻭ͼ

int main()
{
	//��ͼ����
	drawMap();
	//��ͼ��ÿ���ڵ����Ϣ��300*300����90000���ڵ�
	mapData = vector<MapNode>(90000);
	//���ݵ�ͼ��ÿ��������ػ�ȡ�õ����Ϣ
	for (int y = 0; y < mapSize; y++) {
		for (int x = 0; x < mapSize; x++) {
			if (Map.at<Vec3b>(400 - y, 100 + x) == Vec3b(255, 255, 255)) {
				mapData[y * mapSize + x] = MapNode(x, y, NODE_TYPE_ZERO);
			}
			else if (Map.at<Vec3b>(400 - y, 100 + x) == Vec3b(0, 0, 0)) {
				mapData[y * mapSize + x] = MapNode(x, y, NODE_TYPE_OBSTACLE);
			}
			else if (Map.at<Vec3b>(400 - y, 100 + x) == Vec3b(0, 255, 0)) {
				MapNode node(x, y, NODE_TYPE_START);
				mapData[y * mapSize + x] = node;
				startNode = &mapData[y * 300 + x];
			}
			else if (Map.at<Vec3b>(400 - y, 100 + x) == Vec3b(255, 0, 0)) {
				MapNode node(x, y, NODE_TYPE_END);
				mapData[y * mapSize + x] = node;
				targetNode = &mapData[y * 300 + x];
			}
			else {
				Map.at<Vec3b>(400 - y, 100 + x) = Vec3b(0, 0, 0);
				mapData[y * mapSize + x] = MapNode(x, y, NODE_TYPE_OBSTACLE);
			}
		}
	}

	cout << "startNode=(" << startNode->x << ", " << startNode->y << ")" << endl;
	cout << "endNode=(" << targetNode->x << ", " << targetNode->y << ")" << endl;

	openList.push_back(startNode);//��������openlist
	vector<MapNode *> path = Astar();//A*�㷨Ѱ·
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
	position start({ {50.0,50.0} });//�������
	position goal({ {250.0,250.0} });//�յ�����
	// ����ϰ�����
	position ob({ {{0.0, 0.0}} });
	for (int i = 1; i < mapSize; i++)
	{
		ob.push_back({ 0.0, float(i) });
	}
	for (int i = 1; i < mapSize; i++)
	{
		ob.push_back({ 300.0, float(i) });
	}
	for (int i = 1; i < mapSize; i++)
	{
		ob.push_back({ float(i), 0.0 });
	}
	for (int i = 1; i < mapSize; i++)
	{
		ob.push_back({ float(i), 300.0 });
	}
	for (int i = 1; i < 200; i++)
	{
		ob.push_back({ 100, float(i) });
	}
	for (int i = 100; i < mapSize; i++)
	{
		ob.push_back({ 200, float(i) });
	}
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

}

//�������յ�·��
void drawPath(Mat &map, vector<MapNode *> path) {
	//Path�����ÿһ��·���ϵĵ�
	for (int i = 0; i < path.size() - 1; i++) {
		MapNode *node = path[i];
		cv::circle(Map, cv_offset(node->x, node->y, Map.cols, Map.rows),
			2, cv::Scalar(0, 200, 155), 2);
		imshow("����", Map);
		waitKey(1);
		cout << "->(" << node->x << "," << node->y << ")";
	}
	imshow("����", Map);
	waitKey();
	cout << endl;
}

//����Openlist�еĵ�
void drawOpenList() {
	for (int i = 0; i < openList.size(); i++) {
		MapNode *node = openList[i];
		if (node == startNode || node == targetNode)continue;
		cv::circle(Map, cv_offset(node->x, node->y, Map.cols, Map.rows),
			2, cv::Scalar(210, 210, 210), 2);
		imshow("����", Map);
		waitKey(1);
	}
}

//A*�㷨
vector<MapNode *> Astar() {
	vector<MapNode *> path; //��������·���ϵĽڵ�
	cout << "Finding started!" << endl;
	int iteration = 0;
	MapNode *node;
	MapNode *reversedPtr = 0;
	while (openList.size() > 0) {
		node = openList.at(0);
		//�ҵ�openlist��f(n)��С�Ľڵ�
		for (int i = 0, max = openList.size(); i < max; i++) {
			if ((openList[i]->f() + openList[i]->h)<= (node->f() + node->h)) {
				node = openList[i];
			}
		}
		//������ڵ��open�����Ƴ������ұ��ΪClose
		openList.erase(remove(openList.begin(), openList.end(), node), openList.end());
		node->flag = NODE_FLAG_CLOSED;

		//���������Ŀ��㣬����ѭ����reversedPtr��ΪĿ���
		if (node == targetNode) {
			cout << "Reached the target node." << endl;
			reversedPtr = node;
			break;
		}
		//�ҵ���ǰ�ڵ�����ڽڵ�
		vector<MapNode *> neighborNodes = neighbors(node);
		//�����������ڽڵ�
		for (int i = 0; i < neighborNodes.size(); i++) {
			MapNode *_node = neighborNodes[i];
			//������ڽڵ���close���Ѿ����ʹ����л���Ϊ�ϰ�����������ж���һ�����ڽڵ�
			if (_node->flag == NODE_FLAG_CLOSED || _node->type == NODE_TYPE_OBSTACLE) {
				continue;
			}
			int g = node->g + computeG(_node, node);//��������ڽڵ�ĺ�ɢֵ
			//��������ڽڵ㲻��open�У��ͽ������open
			if (_node->flag != NODE_FLAG_OPEN) {
				_node->g = g;
				_node->h = computeH(_node, targetNode);
				_node->parent = node;
				_node->flag = NODE_FLAG_OPEN;
				openList.push_back(_node);
			}
		}
		/*drawOpenList();*/
		if (openList.size() <= 0) break;
	}
	if (reversedPtr == 0) {
		cout << "Target node is unreachable." << endl;
	}
	//���յ���׷��parent�ڵ㵽���ó�·��
	else {
		MapNode *_node = reversedPtr;
		while (_node->parent != 0) {
			path.push_back(_node);
			_node = _node->parent;
		}
		reverse(path.begin(), path.end());
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

	if (ALLOW_VERTEX_PASSTHROUGH) { //�������б����
		// ����
		if ((_node = mapAt(node->x - 1, node->y - 1)) != 0)available.push_back(_node);
		// ����
		if ((_node = mapAt(node->x + 1, node->y - 1)) != 0)available.push_back(_node);
		// ����
		if ((_node = mapAt(node->x + 1, node->y + 1)) != 0)available.push_back(_node);
		// ����
		if ((_node = mapAt(node->x - 1, node->y + 1)) != 0)available.push_back(_node);
	}

	return available;
}

//����Hֵ �ڵ㵽Ŀ���Ĺ��ƴ���
int computeH(MapNode *node1, MapNode *node2) {
	//�������б���ߣ�����ŷ�Ͼ���
	if (ALLOW_VERTEX_PASSTHROUGH) {
		double x_distance = double(node2->x) - double(node1->x);
		double y_distance = double(node2->y) - double(node1->y);
		double distance2 = pow(x_distance, 2) + pow(y_distance, 2);
		return int(sqrt(distance2));
	}
	//������������پ���
	else {
		return abs(node2->x - node1->x) + abs(node2->y - node1->y);
	}
}

//����Gֵ ����㵽��ǰ���ʵ�ʴ���
int computeG(MapNode *node1, MapNode *node2) {
	//�������б���ߣ�����ŷ�Ͼ���
	if (ALLOW_VERTEX_PASSTHROUGH) {
		double x_distance = double(node2->x) - double(node1->x);
		double y_distance = double(node2->y) - double(node1->y);
		double distance2 = pow(x_distance, 2) + pow(y_distance, 2);
		return int(sqrt(distance2));
	}
	//������������پ���
	else {
		return abs(node2->x - node1->x) + abs(node2->y - node1->y);
	}
}

// ������������Ӧ�����Ľڵ�
MapNode *mapAt(int x, int y) {
	if (x < 0 || y < 0 || x >= mapSize || y >= mapSize)return 0;
	return &mapData[y * mapSize + x];
}