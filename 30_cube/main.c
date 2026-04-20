#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <stdlib.h>
// #include <GL/gl.h>
// #include <GLUT/GLUT.h>


static int WindW = 800, WindH = 600;
static float angle = 0.0f;
static char *window_title = NULL;

static void cleanup(void) {
    free(window_title);
}


static void Reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)width / (float)height, 0.1f,
        100.0f);
    glMatrixMode(GL_MODELVIEW);
    WindW = width;
    WindH = height;
}

static void Draw(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glLoadIdentity();
    gluLookAt(0.0, 0.0, 5.0,  0.0, 0.0,
        0.0,  0.0, 1.0, 0.0);

    glRotatef(angle, 1.0f, 1.0f, 0.0f);

    glColor3f(0.2f, 0.6f, 0.9f);
    glutSolidCube(1.8);

    glDepthMask(GL_FALSE);
    glColor3f(0.3f, 0.3f, 0.3f);
    glutWireCube(1.8);
    glDepthMask(GL_TRUE);

    glutSwapBuffers();
}


static void Keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    if (key == 27)   /* Esc */
        exit(EXIT_SUCCESS);
}


static void Timer(int value) {
    (void)value;
    angle += 2.0f; /* скорость вращения */
    if (angle >= 360.0f)
        angle -= 360.0f;
    glutPostRedisplay();
    glutTimerFunc(40, Timer, 0);
}

int main(int argc, char *argv[] __attribute__((unused))) {
    if (atexit(cleanup) != 0) {
        perror("atexit");
        exit(EXIT_FAILURE);
    }

    window_title = malloc(32);
    if (window_title == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(window_title, 32, "Вращающийся куб");

    glutInit(&argc, argv);
    glutInitWindowSize(WindW, WindH);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow(window_title);

    glutReshapeFunc(Reshape);
    glutDisplayFunc(Draw);
    glutKeyboardFunc(Keyboard);
    glutTimerFunc(40, Timer, 0);

    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    glutMainLoop();

    return EXIT_SUCCESS;
}
