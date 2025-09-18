#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>

void myReshape(int w, int h);
void display(void);

void myReshape(int w, int h) {
	glViewport(0, 0, w, h);
	glLoadIdentity();
	gluOrtho2D(0, 100, 0, 100);
}

void display(void) {
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(0, 1, 1);
	glRectf(10, 10, 90, 90);
	glutSwapBuffers();
}

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutCreateWindow("simple");
	glutReshapeFunc(myReshape);
	glutDisplayFunc(display);
	glutMainLoop();

	return 0;
}