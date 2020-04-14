#include <iostream>
#include <vector>
#include <chrono>
#include <glut.h>
#include <freeglut.h>
#define resX 600
#define scoreWidth 200
#define resY 600
#define GAME_OVER 10
#define MAX_GAME_TIME 10 // not used at present
#define len_of_half_square float(30)/resX
#define main_space_offset float(scoreWidth)/(resX+scoreWidth)
using namespace std;

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds ms;
typedef std::chrono::duration<float> fsec;

class Point {
public:
	float x, y;
	Point() {
		x = 0;
		y = 0;
	}
	Point(int x, int y) {
		this->x = float(x) / resX * 2 - 1;
		this->y = 1 - float(y) / resY * 2;
	}
	Point(float x, float y) {
		this->x = x;
		this->y = y;
	}
	void p();
};

class TimeStats {
public:
	long long times[40] = { 0 };
	bool positive[40] = { 0 };
	int index = -1;
	std::chrono::steady_clock::time_point time_start;
	TimeStats() {
		time_start = Time::now();
	}
	void update(long long duration, bool positive) {
		if (index < 0)return;
		this->times[index] = duration;// -this->times[index > 0 ? index - 1 : 0];
		this->positive[index] = positive;
		this->index++;
	}
	long latestTime() {
		return times[index];
	}
	long latestSplit() {
		return times[index > 0 ? index - 1 : 0] - this->times[index - 1 > 0 ? index - 2 : 0];
	}
	bool latestPositive() {
		return positive[index > 0 ? index - 1 : 0];
	}
	void print() {
		cout << "------------------------------------split times------------------------------------" << endl;
		cout << "click" << "\t|\t" << "time" << "\t|\t" << "split" << "(ms)\t|\t" << "hit/miss" << endl;
		for (int i = 0; i < index; i++) {
			cout << i+1 << "\t|\t" <<times[i]<<"\t|\t"<< times[i] - times[i > 0 ? i - 1 : 0] << " ms\t\t|\t";
			if (positive[i]) {
				cout << "hit" << endl;
			} else {
				cout << "miss" << endl;
			}
		}
		cout << "-----------------------------------------------------------------------------------" << endl;
		cout << "total time : " << times[index - 1] << "ms" << endl;
		cout << "-----------------------------------------------------------------------------------" << endl;
	}
};

TimeStats timeKeeper;
Point target;
vector<Point*> points;
uint32_t clicks = 0;
int positiveScore = 0;
int negativeScore = 0;

void mouse(int button, int state, int px, int py);
void renderPoints();
void drawTarget(float x, float y);
void drawCharacter(int num,Point p,int width,int height,bool positive);
void drawSplitTime();

void display() {
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(0, 1, 0);
	glLineWidth(2);
	glBegin(GL_LINE_LOOP);
	glVertex2f(1 - main_space_offset, 1);
	glVertex2f(1 - main_space_offset, -1);
	glEnd();
	drawTarget(target.x,target.y);
	if (positiveScore / 10 != 0) { 
		drawCharacter(positiveScore / 10, Point(1 - float(main_space_offset) / 4 * 3, 0.5), 80, 112, true); 
	}
	drawCharacter(positiveScore%10, Point(1 - float(main_space_offset) / 4 , 0.5), 80, 112, true);

	if (negativeScore / 10 != 0) {
		drawCharacter(negativeScore / 10, Point(1 - float(main_space_offset) / 4 * 3, 0.2), 80, 112, false);
	}
	drawCharacter(negativeScore % 10, Point(1 - float(main_space_offset) / 4, 0.2), 80, 112, false);
	drawSplitTime();
	glutSwapBuffers();
}

int main(int argv, char** argc) {
	cout << len_of_half_square << endl;
	glutInit(&argv, argc);
	glutInitWindowSize(resX + scoreWidth, resY);
	glutInitWindowPosition(600,100);
	glutCreateWindow("reaction-game");
	glutDisplayFunc(display);
	glutMouseFunc(&mouse);
	glutMainLoop();
	getchar();
	return 0;
}

void mouse(int button, int state, int px, int py) {
	switch (button) {
	case GLUT_LEFT_BUTTON:
		if (state == GLUT_DOWN) {
			if (positiveScore + negativeScore == GAME_OVER) {
				cout << "game over" << endl;
				timeKeeper.print();
				glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
				glutLeaveMainLoop();
				//glutDestroyWindow(glutGetWindow());
				return;
			}
			float x=0,y=0;
			x = float(px) / (resX + scoreWidth) * 2 - 1;
			y = 1 - float(py) / resY * 2;
			//cout << x << "|" << y << endl;
			//cout << (len_of_half_square * resX / (resX + scoreWidth)) << "|" << len_of_half_square << " ! " << target.x << "|" << target.y << endl;
			//cout << (x >= target.x - (len_of_half_square*resX/(resX+scoreWidth)) && x <= target.x + (len_of_half_square * resX / (resX + scoreWidth)) && y >= target.y - len_of_half_square && y <= target.y + len_of_half_square) << endl;
			if (x >= target.x - (len_of_half_square * resX / (resX + scoreWidth)) && x <= target.x + (len_of_half_square * resX / (resX + scoreWidth)) && y >= target.y - len_of_half_square && y <= target.y + len_of_half_square) {
				positiveScore++;
				if (positiveScore == 1) {
					timeKeeper.time_start = Time::now();
					timeKeeper.index = 0;
				}
				if (timeKeeper.index >= 0) {
					auto t0 = Time::now();
					fsec fs = t0 - timeKeeper.time_start;
					ms d = std::chrono::duration_cast<ms>(fs);
					timeKeeper.update(d.count(),true);
				}
			} else if(positiveScore>0){
				negativeScore++;
				if (timeKeeper.index >= 0) {
					auto t0 = Time::now();
					fsec fs = t0 - timeKeeper.time_start;
					ms d = std::chrono::duration_cast<ms>(fs);
					timeKeeper.update(d.count(),false);
				}
			}
			clicks++;
			//points.push_back(new Point(px, py));
			auto now = std::chrono::high_resolution_clock::now();
			auto mls = std::chrono::time_point_cast<std::chrono::microseconds>(now);
			auto epoch = mls.time_since_epoch();
			auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);

			srand(value.count() + int(target.x*1000) + clicks);
			rand();
			x = float(rand())/RAND_MAX;
			y = float(rand())/RAND_MAX;
			//x = 2 * x * (1 - len_of_half_square*resX/(resX+scoreWidth)) + len_of_half_square * resX / (resX + scoreWidth) - 1 + scoreWidth/(resX+scoreWidth);
			//y = 2 * y * (1 - len_of_half_square) + len_of_half_square - 1;
			x = 2 * x * (1 - main_space_offset / 2 - len_of_half_square) + len_of_half_square - 1;
			y = 2 * y * (1 - len_of_half_square) + len_of_half_square - 1;
			target.x = x;
			target.y = y;
			glutPostRedisplay();
		}
	}
}

void renderPoints() {	
	glColor3f(0, 0, 0);
	glPointSize(2);
	glBegin(GL_POINTS);
	for (Point* p : points) {
		glVertex2f(p->x, p->y);
		p->p();
	}
	glEnd();
}

void drawTarget(float x, float y) {
	glColor3f(1, 0, 0);
	glBegin(GL_POLYGON);
	glVertex2f(x - (len_of_half_square * resX / (resX + scoreWidth)), y + len_of_half_square);
	glVertex2f(x + (len_of_half_square * resX / (resX + scoreWidth)), y + len_of_half_square);
	glVertex2f(x + (len_of_half_square * resX / (resX + scoreWidth)), y - len_of_half_square);
	glVertex2f(x - (len_of_half_square * resX / (resX + scoreWidth)), y - len_of_half_square);
	glEnd();
}

void Point::p() {
	cout << "point (" << this->x << "," << this->y << ")" << endl;
}

void drawSplitTime() {
	if (timeKeeper.index < 0) {
		return;
	}
	long lastSplit = timeKeeper.latestSplit();
	bool positive = timeKeeper.latestPositive();
	cout << "last split : " << lastSplit << endl;
	drawCharacter(lastSplit % 10, Point(1 - float(main_space_offset) * 5 / 4, 0.8), 80, 112, positive);
	drawCharacter(lastSplit / 10 % 10, Point(1 - float(main_space_offset) * 7 / 4, 0.8), 80, 112, positive);
	drawCharacter(lastSplit /100 % 10, Point(1 - float(main_space_offset) * 9 / 4, 0.8), 80, 112, positive);
	drawCharacter(lastSplit /1000 % 10, Point(1 - float(main_space_offset) * 11 / 4, 0.8), 80, 112, positive);
	drawCharacter(lastSplit /10000 % 10, Point(1 - float(main_space_offset) * 13 / 4, 0.8), 80, 112, positive);
	drawCharacter(lastSplit /100000 % 10, Point(1 - float(main_space_offset) * 15 / 4, 0.8), 80, 112, positive);
}

void drawCharacter(int num, Point p, int width, int height, bool positive) {
	float wunit = float(width) / (resX + scoreWidth) / 10;
	float hunit = float(height) / resY / 14;
	//hexGrid
	/*
	     a
	   -----
	f |     | b
	   -----
	e |  g  | c
	   -----
	     d
	a=0,2,3,5,6,7,8,9
	b=0,1,2,3,4,7,8,9
	c=0,1,3,4,5,6,7,8,9
	d=0,2,3,5,6,8
	e=0,2,6,8
	f=0,4,5,6,8,9
	g=2,3,4,5,6,8,9
	*/
	float a[4] = { p.x - wunit * 5 ,p.y + hunit * 7 ,p.x + wunit * 5,p.y + hunit * 7 };
	float b[4] = { p.x + wunit * 5 ,p.y + hunit * 7 ,p.x + wunit * 5,p.y };
	float c[4] = { p.x + wunit * 5 ,p.y ,p.x + wunit * 5,p.y - hunit * 7 };
	float d[4] = { p.x - wunit * 5 ,p.y - hunit * 7 ,p.x + wunit * 5,p.y - hunit * 7 };
	float e[4] = { p.x - wunit * 5 ,p.y ,p.x - wunit * 5,p.y - hunit * 7 };
	float f[4] = { p.x - wunit * 5 ,p.y + hunit * 7 ,p.x - wunit * 5,p.y };
	float g[4] = { p.x - wunit * 5 ,p.y ,p.x + wunit * 5,p.y };
	

	if (positive) {
		glColor3f(0, 0, 1);
	} else {
		glColor3f(1, 0, 0);
	}
	glLineWidth(2);

	if (num == 0 || num == 2 || num == 3 || num == 5 || num == 6 || num == 7 || num == 8 || num == 9) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(a[0], a[1]);
		glVertex2f(a[2], a[3]);
		glEnd();
	} 
	if (num == 0 || num == 2 || num == 3 || num == 1 || num == 4 || num == 7 || num == 8 || num == 9) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(b[0], b[1]);
		glVertex2f(b[2], b[3]);
		glEnd();
	}  
	if (num == 0 || num == 1 || num == 3 || num == 4 || num == 5 || num == 6 || num == 7 || num == 8 || num == 9) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(c[0], c[1]);
		glVertex2f(c[2], c[3]);
		glEnd();
	}  
	if (num == 0 || num == 2 || num == 3 || num == 5 || num == 6 || num == 8) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(d[0], d[1]);
		glVertex2f(d[2], d[3]);
		glEnd();
	}  
	if (num == 0 || num == 2 || num == 6 || num == 8) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(e[0], e[1]);
		glVertex2f(e[2], e[3]);
		glEnd();
	}  
	if (num == 0 || num == 4 || num == 5 || num == 6 || num == 8 || num == 9) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(f[0], f[1]);
		glVertex2f(f[2], f[3]);
		glEnd();
	}  
	if (num == 2 || num == 3 || num == 4 || num == 5 || num == 6 || num == 8 || num == 9) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(g[0], g[1]);
		glVertex2f(g[2], g[3]);
		glEnd();
	}
}