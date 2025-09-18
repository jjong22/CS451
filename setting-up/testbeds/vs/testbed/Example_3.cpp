#include <GL/glew.h>
#include <GL/freeglut.h>
#include <windows.h>
#include <iostream>

void init(void);
void display(void);
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void specialKeys(int key, int x, int y);

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(600, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow(argv[0]);
	init();

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(specialKeys);
	glutMainLoop();
	return 0;
}

typedef struct rect {
	float x, y;
	float width, height;
} rect;
rect rectangle;

void init(void) {
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);

	rectangle.x = 0.45;
	rectangle.y = 0.48;
	rectangle.width = 0.1;
	rectangle.height = 0.15;
}

void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(rectangle.x, rectangle.y);
		glVertex2f(rectangle.x + rectangle.width, rectangle.y);
		glVertex2f(rectangle.x + rectangle.width, rectangle.y + rectangle.height);
		glVertex2f(rectangle.x, rectangle.y + rectangle.height);
	glEnd();
	
	glutSwapBuffers();
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, 1.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 'w':
		if (rectangle.y + rectangle.height < 1.0) // º® Á¦ÇÑ
			rectangle.y += 0.01;
		break;
	case 's':
		if (rectangle.y > 0.0)
			rectangle.y -= 0.01;
		break;
	case 'a':
		if (rectangle.x > 0.0)
			rectangle.x -= 0.01;
		break;
	case 'd':
		if (rectangle.x + rectangle.width < 1.0)
			rectangle.x += 0.01;
		break;
	default:
		break;
	}
	glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP: // Up arrow key
		if (rectangle.y + rectangle.height < 1.0)
			rectangle.y += 0.01;
		break;
	case GLUT_KEY_DOWN:
		if (rectangle.y > 0.0)
			rectangle.y -= 0.01;
		break;
	case GLUT_KEY_LEFT:
		if (rectangle.x > 0.0)
			rectangle.x -= 0.01;
		break;
	case GLUT_KEY_RIGHT:
		if (rectangle.x + rectangle.width < 1.0)
			rectangle.x += 0.01;
		break;
	default:
		break;
	}
	glutPostRedisplay();
}