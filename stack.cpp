#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 800
#define Y_THRESHOLD 0
#define X_THRESHOLD 200
#define NEXT_OBJECT_Y 350
#define OBJECT_HEIGHT 25
#define STARTING_WIDTH_OF_OBJECT 150
#define MAX_NUM_OBJECTS ((Y_THRESHOLD + WINDOW_WIDTH / 2) / OBJECT_HEIGHT)
#define FEEDBACK_DURATION 50

#define TIMER_PERIOD  25 // Period for the timer.
#define TIMER_ON         1 // 0:disable timer, 1:enable timer

#define D2R 0.0174532

typedef struct {
	float x, y;
} point_t;

typedef struct{
	float r,g,b;
} color_t;

//the object the player sends
typedef struct {
	point_t coordinate;
	color_t color;
	float size;
} object_t;

typedef struct {
	point_t coordinate;
	point_t velocity;
	float rotate;
	color_t color;
	float size;
} overflowed_object_t;

//static objects 
typedef struct {
	float x,
		size;
	color_t color;
} static_object_t;

enum GameState { Running,	//no input is given
	ObjectMoving,			//after input, the rectangular object is moving towards the bottom
	Animating,				//after the rectangular object hit the static objects, the overflowing part is cut and moves towards out of the screen
	GameOver};				//game is over

/* Global Variables for Template File */
bool up = false, down = false, right = false, left = false;
int  winWidth, winHeight;					// current Window width and height
float lastSucceedSize;						//the size of the part of the last rectangular object which is succesfully landed
static_object_t objects[MAX_NUM_OBJECTS];	//static objects
object_t nextObject;						//the object which the player will send
overflowed_object_t overflowedObject;					//the object to be discarded
int objectCount;							//# of static objects
GameState gameState;						//controls the game state

//because I have an array with fixed size, I keep the index of the first object from top instead of shifting the whole array
int firstObjectIndex;		
float speed;				//the speed of the object
float currentThreshold;		//where the object should stop
int score;					//total score
bool isFeedBackActive;		//if active, feedback according to the last score is given in the top right
int feedBackTimer,			//controls the amount of time the feedback is shown
timer;						//counts # of frames passed
int feedback;				//score got from last hit

void initializeGlobals()
{
	gameState = GameOver;
	timer = 0;
	score = 0;
	speed = 5;
	firstObjectIndex = 0;
	objectCount = MAX_NUM_OBJECTS;
	for (int i = 0; i < objectCount; i++)
	{
		objects[i].x = -STARTING_WIDTH_OF_OBJECT / 2;
		objects[i].size = STARTING_WIDTH_OF_OBJECT;
		objects[i].color = { (rand() % 100) / 100.0f,(rand() % 100) / 100.0f,(rand() % 100) / 100.0f };
	}
	nextObject.coordinate.x = -STARTING_WIDTH_OF_OBJECT / 2;
	nextObject.coordinate.y = NEXT_OBJECT_Y;
	nextObject.size = lastSucceedSize = STARTING_WIDTH_OF_OBJECT;
	nextObject.color = { (rand() % 100) / 100.0f,(rand() % 100) / 100.0f,(rand() % 100) / 100.0f };
	currentThreshold = OBJECT_HEIGHT - WINDOW_HEIGHT / 2;
}

//
// to draw circle, center at (x,y)
// radius r
//
void circle(int x, int y, int r)
{
#define PI 3.1415
	float angle;
	glBegin(GL_POLYGON);
	for (int i = 0; i < 100; i++)
	{
		angle = 2 * PI*i / 100;
		glVertex2f(x + r * cos(angle), y + r * sin(angle));
	}
	glEnd();
}

void circle_wire(int x, int y, int r)
{
#define PI 3.1415
	float angle;

	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < 100; i++)
	{
		angle = 2 * PI*i / 100;
		glVertex2f(x + r * cos(angle), y + r * sin(angle));
	}
	glEnd();
}

void print(int x, int y, const char *string, void *font)
{
	int len, i;

	glRasterPos2f(x, y);
	len = (int)strlen(string);
	for (i = 0; i < len; i++)
	{
		glutBitmapCharacter(font, string[i]);
	}
}

// display text with variables.
// vprint(-winWidth / 2 + 10, winHeight / 2 - 20, GLUT_BITMAP_8_BY_13, "ERROR: %d", numClicks);
void vprint(int x, int y, void *font, const char *string, ...)
{
	va_list ap;
	va_start(ap, string);
	char str[1024];
	vsprintf_s(str, string, ap);
	va_end(ap);

	int len, i;
	glRasterPos2f(x, y);
	len = (int)strlen(str);
	for (i = 0; i < len; i++)
	{
		glutBitmapCharacter(font, str[i]);
	}
}

// vprint2(-50, 0, 0.35, "00:%02d", timeCounter);
void vprint2(int x, int y, float size, const char *string, ...) {
	va_list ap;
	va_start(ap, string);
	char str[1024];
	vsprintf_s(str, string, ap);
	va_end(ap);
	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(size, size, 1);

	int len, i;
	len = (int)strlen(str);
	for (i = 0; i < len; i++)
	{
		glutStrokeCharacter(GLUT_STROKE_ROMAN, str[i]);
	}
	glPopMatrix();
}

void rectBorder(float x1, float y1, float x2, float y2)
{
	glBegin(GL_LINE_LOOP);
	glVertex2f(x1, y1);
	glVertex2f(x1, y2);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
	glEnd();
}

void displayStaticObjects()
{
	int j;
	for (int i = 0; i < MAX_NUM_OBJECTS; i++)
	{
		j = (firstObjectIndex + i) % objectCount;
		glColor4f(objects[j].color.r, objects[j].color.g, objects[j].color.b, 0.5);
		glRectf(objects[j].x, i * OBJECT_HEIGHT - WINDOW_HEIGHT / 2,
			objects[j].x + objects[j].size, (i + 1) * OBJECT_HEIGHT - WINDOW_HEIGHT / 2);
		glColor4f(1 - objects[j].color.r, 1 - objects[j].color.g, 1 - objects[j].color.b, 0.5);
		rectBorder(objects[j].x, i * OBJECT_HEIGHT - WINDOW_HEIGHT / 2,
			objects[j].x + objects[j].size, (i + 1) * OBJECT_HEIGHT - WINDOW_HEIGHT / 2);
	}
}

void displayPlayerObject()
{
	glColor4f(nextObject.color.r, nextObject.color.g, nextObject.color.b, 0.5);

	glRectf(nextObject.coordinate.x, nextObject.coordinate.y,
		nextObject.coordinate.x + nextObject.size, nextObject.coordinate.y + OBJECT_HEIGHT);

	glColor3f(0, 0, 0);
	rectBorder(nextObject.coordinate.x, nextObject.coordinate.y,
		nextObject.coordinate.x + nextObject.size, nextObject.coordinate.y + OBJECT_HEIGHT);

	glBegin(GL_LINES);
	glColor4f(1, 1, 1, 0.2);
	glVertex2f(nextObject.coordinate.x, nextObject.coordinate.y + OBJECT_HEIGHT);
	glColor4f(1, 1, 1, 0);
	glVertex2f(nextObject.coordinate.x, 0);
	glColor4f(1, 1, 1, 0.2);
	glVertex2f(nextObject.coordinate.x + nextObject.size, nextObject.coordinate.y + OBJECT_HEIGHT);
	glColor4f(1, 1, 1, 0);
	glVertex2f(nextObject.coordinate.x + nextObject.size, 0);
	glEnd();
}

void displayScore()
{
	glColor3f(1, 1, 1);
	vprint(250, -350, GLUT_BITMAP_TIMES_ROMAN_24, "SCORE: %d", score);
	if (isFeedBackActive)
	{
		switch (feedback)
		{
		case 5:
			glColor3f(1, 0, 0);
			vprint(150, 350, GLUT_BITMAP_TIMES_ROMAN_24, "PERFECT : 5 POINTS");
			break;
		case 4:
			glColor3f(1, 0, 0);
			vprint(170, 350, GLUT_BITMAP_TIMES_ROMAN_24, "GOOD : 4 POINTS");
			break;
		case 3:
			glColor3f(1, 0, 0);
			vprint(150, 350, GLUT_BITMAP_TIMES_ROMAN_24, "NOT BAD : 3 POINTS");
			break;
		case 2:
			glColor3f(1, 0, 0);
			vprint(220, 350, GLUT_BITMAP_TIMES_ROMAN_24, "2 POINTS");
			break;
		case 1:
			glColor3f(1, 1, 1);
			vprint(220, 350, GLUT_BITMAP_TIMES_ROMAN_24, "1 POINT");
			break;
		default:
			break;
		}
	}
}

void displayOverflowingPart()
{
	if (gameState == Animating)
	{
		glColor3f(overflowedObject.color.r, overflowedObject.color.g, overflowedObject.color.b);
		glTranslatef(overflowedObject.coordinate.x + overflowedObject.size / 2,
			overflowedObject.coordinate.y - OBJECT_HEIGHT / 2,
			0);
		glRotatef(overflowedObject.rotate, 0, 0, 1);
		glRectf(-overflowedObject.size / 2, OBJECT_HEIGHT / 2, overflowedObject.size / 2, -OBJECT_HEIGHT / 2);
		glLoadIdentity();
	}
}

void displayStartScreen()
{
	glColor4f(0, 0, 0, 0.7);
	glRectf(-WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, WINDOW_WIDTH / 2, -WINDOW_HEIGHT / 2);
	glColor3f(1, 1, 1);
	vprint2(-200, 0, 0.5, "F1 to Start");
	vprint2(-250, -100, 0.3, "Space to send the object");
	if(score != 0)
		vprint2(-200, -200, 0.2, "Last Score: %d", score);
}

//
// To display onto window using OpenGL commands
//
void display() {
	//
	// clear window to black
	//
	glClearColor(0.2, 0.2, 0.2, 0.7);
	glClear(GL_COLOR_BUFFER_BIT);
	
	displayStaticObjects();
	if(gameState != Animating)
		displayPlayerObject();
	else
		displayOverflowingPart();
	displayScore();
	if (gameState == GameOver)
		displayStartScreen();


	glutSwapBuffers();
}

//
// key function for ASCII charachters like ESC, a,b,c..,A,B,..Z
//
void onKeyDown(unsigned char key, int x, int y)
{
	// exit when ESC is pressed.
	if (key == 27)
		exit(0);
	if (gameState == Running && key == ' ')
		gameState = ObjectMoving;

	// to refresh the window it calls display() function
	glutPostRedisplay();
}

void onKeyUp(unsigned char key, int x, int y)
{
	// exit when ESC is pressed.
	if (key == 27)
		exit(0);

	// to refresh the window it calls display() function
	glutPostRedisplay();
}

//
// Special Key like GLUT_KEY_F1, F2, F3,...
// Arrow Keys, GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_RIGHT, GLUT_KEY_RIGHT
//
void onSpecialKeyDown(int key, int x, int y)
{
	if (key == GLUT_KEY_F1)
	{
		initializeGlobals();
		gameState = Running;
	}
	// to refresh the window it calls display() function
	glutPostRedisplay();
}

//
// Special Key like GLUT_KEY_F1, F2, F3,...
// Arrow Keys, GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_RIGHT, GLUT_KEY_RIGHT
//
void onSpecialKeyUp(int key, int x, int y)
{
	// Write your codes here.
	switch (key) {
	case GLUT_KEY_UP: up = false; break;
	case GLUT_KEY_DOWN: down = false; break;
	case GLUT_KEY_LEFT: left = false; break;
	case GLUT_KEY_RIGHT: right = false; break;
	}

	// to refresh the window it calls display() function
	glutPostRedisplay();
}

//
// When a click occurs in the window,
// It provides which button
// buttons : GLUT_LEFT_BUTTON , GLUT_RIGHT_BUTTON
// states  : GLUT_UP , GLUT_DOWN
// x, y is the coordinate of the point that mouse clicked.
//
void onClick(int button, int stat, int x, int y)
{
	// Write your codes here.



	// to refresh the window it calls display() function
	glutPostRedisplay();
}

//
// This function is called when the window size changes.
// w : is the new width of the window in pixels.
// h : is the new height of the window in pixels.
//
void onResize(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-w / 2, w / 2, -h / 2, h / 2, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	display(); // refresh window.
}

void onMoveDown(int x, int y) {
	// Write your codes here.



	// to refresh the window it calls display() function   
	glutPostRedisplay();
}

// GLUT to OpenGL coordinate conversion:
//   x2 = x1 - winWidth / 2
//   y2 = winHeight / 2 - y1
void onMove(int x, int y) {
	// Write your codes here.



	// to refresh the window it calls display() function
	glutPostRedisplay();
}

void scoreHandler(float dif)
{
	//the percentage of the overflowing part's size over the whole size of the object
	float percent = dif / lastSucceedSize;
	if (dif == 0)
		feedback = 5;
	else if (percent < 0.10)
		feedback = 4;
	else if (percent < 0.20)
		feedback = 3;
	else if (percent < 0.30)
		feedback = 2;
	else
		feedback = 1;
	score += feedback;
	feedBackTimer = timer;
	isFeedBackActive = true;
}

#if TIMER_ON == 1
void onTimer(int v) {

	glutTimerFunc(TIMER_PERIOD, onTimer, 0);
	// Write your codes here.
	timer++;
	if (isFeedBackActive && feedBackTimer + FEEDBACK_DURATION < timer)
	{
		isFeedBackActive = false;
	}
	//the object constantly moves left to right & right to left
	if (gameState == Running)
	{
		nextObject.coordinate.x += speed;
		if (nextObject.coordinate.x + nextObject.size >= X_THRESHOLD || nextObject.coordinate.x <= -X_THRESHOLD)
			speed *= -1;
	}
	else if(gameState == ObjectMoving)
	{
		if (!(nextObject.coordinate.y <= 0))
			nextObject.coordinate.y -= OBJECT_HEIGHT;
		else
		{
			//the index of the object at the top
			int lastIndex = (firstObjectIndex - 1 + objectCount) % objectCount;
			//if the sent object outside the area of the last object, game is over
			if (nextObject.coordinate.x + nextObject.size < objects[lastIndex].x
				|| nextObject.coordinate.x > objects[lastIndex].x + objects[lastIndex].size)
				gameState = GameOver;
			else
			{
				//the size of the overflowing part
				float difference = fabs(objects[lastIndex].x - nextObject.coordinate.x);
				scoreHandler(difference);
				lastSucceedSize -= difference;

				overflowedObject.size = difference;
				overflowedObject.color = nextObject.color;
				//if the overflowing part is on the left
				if (nextObject.coordinate.x < objects[lastIndex].x)
				{
					overflowedObject.coordinate.x = nextObject.coordinate.x;
					overflowedObject.velocity = { -3, 5 };
				}
				//if on the right
				else
				{
					overflowedObject.coordinate.x = objects[lastIndex].x + objects[lastIndex].size;
					overflowedObject.velocity = { 3, 5 };
				}
				overflowedObject.coordinate.y = 0;
				overflowedObject.rotate = 0;

				//insert the sent object to the static objects
				objects[firstObjectIndex].size = lastSucceedSize;
				objects[firstObjectIndex].x = fmax(objects[lastIndex].x, nextObject.coordinate.x);
				objects[firstObjectIndex].color = nextObject.color;

				firstObjectIndex = (firstObjectIndex + 1) % objectCount;
				//reset nextObject's values
				nextObject.coordinate.x = -STARTING_WIDTH_OF_OBJECT / 2;
				nextObject.coordinate.y = NEXT_OBJECT_Y;
				nextObject.size = lastSucceedSize;
				nextObject.color = { (rand() % 100) / 100.0f,(rand() % 100) / 100.0f,(rand() % 100) / 100.0f };
				gameState = Animating;
			}
		}
	}
	else if (gameState == Animating)
	{
		overflowedObject.velocity.y -= 0.5;
		overflowedObject.rotate += 5 * overflowedObject.velocity.x;

		overflowedObject.coordinate.y += overflowedObject.velocity.y;
		overflowedObject.coordinate.x += overflowedObject.velocity.x;
		if (overflowedObject.coordinate.y < -WINDOW_HEIGHT / 2)
			gameState = Running;
	}


	// to refresh the window it calls display() function
	glutPostRedisplay(); // display()

}
#endif

void Init() {

	// Smoothing shapes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

void main(int argc, char *argv[]) {
	srand(time(NULL));
	initializeGlobals();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	//glutInitWindowPosition(100, 100);
	glutCreateWindow("Template File");

	glutDisplayFunc(display);
	glutReshapeFunc(onResize);

	//
	// keyboard registration
	//
	glutKeyboardFunc(onKeyDown);
	glutSpecialFunc(onSpecialKeyDown);

	glutKeyboardUpFunc(onKeyUp);
	glutSpecialUpFunc(onSpecialKeyUp);

	//
	// mouse registration
	//
	glutMouseFunc(onClick);
	glutMotionFunc(onMoveDown);
	glutPassiveMotionFunc(onMove);

#if  TIMER_ON == 1
	// timer event
	glutTimerFunc(TIMER_PERIOD, onTimer, 0);
#endif

	Init();

	glutMainLoop();
}