
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define DEG2RAD (M_PI / 180.0f)

typedef float mat4[4][4];
typedef float vec4[4];

static vec4 cubeVerts[8];      // transformed vertices
static vec4 originalVerts[8];  // original cube vertices (homogeneous)
static mat4 model;             // cumulative model matrix (applied to original to get cubeVerts)

static int win_w = 800, win_h = 600;

// camera rotation for viewing
static float viewRotX = 20.0f;
static float viewRotY = -30.0f;
static int lastX = -1, lastY = -1;
static int leftButtonDown = 0;

// Utility: set mat to identity
void mat_identity(mat4 m) {
    memset(m, 0, sizeof(mat4));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}

// Multiply a * b -> out (4x4)
void mat_mul(const mat4 a, const mat4 b, mat4 out) {
    mat4 tmp;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            tmp[r][c] = 0.0f;
            for (int k = 0; k < 4; ++k) tmp[r][c] += a[r][k] * b[k][c];
        }
    }
    memcpy(out, tmp, sizeof(mat4));
}

// Multiply matrix m by vector v (4x4 * 4x1) -> out
void mat_vec_mul(const mat4 m, const vec4 v, vec4 out) {
    for (int r = 0; r < 4; ++r) {
        out[r] = m[r][0]*v[0] + m[r][1]*v[1] + m[r][2]*v[2] + m[r][3]*v[3];
    }
}

// Build translation matrix
void translation_matrix(float tx, float ty, float tz, mat4 out) {
    mat_identity(out);
    out[0][3] = tx;
    out[1][3] = ty;
    out[2][3] = tz;
}

// Build scaling matrix
void scaling_matrix(float sx, float sy, float sz, mat4 out) {
    mat_identity(out);
    out[0][0] = sx;
    out[1][1] = sy;
    out[2][2] = sz;
}

// Build rotation about X
void rotation_x_matrix(float angleDeg, mat4 out) {
    mat_identity(out);
    float a = angleDeg * DEG2RAD;
    out[1][1] = cosf(a); out[1][2] = -sinf(a);
    out[2][1] = sinf(a); out[2][2] = cosf(a);
}

// Rotation about Y
void rotation_y_matrix(float angleDeg, mat4 out) {
    mat_identity(out);
    float a = angleDeg * DEG2RAD;
    out[0][0] = cosf(a); out[0][2] = sinf(a);
    out[2][0] = -sinf(a); out[2][2] = cosf(a);
}

// Rotation about Z
void rotation_z_matrix(float angleDeg, mat4 out) {
    mat_identity(out);
    float a = angleDeg * DEG2RAD;
    out[0][0] = cosf(a); out[0][1] = -sinf(a);
    out[1][0] = sinf(a); out[1][1] = cosf(a);
}

// Reflection: reflect about plane by flipping sign on axis
void reflection_matrix(char axis, mat4 out) {
    mat_identity(out);
    if (axis == 'x' || axis == 'X') out[0][0] = -1.0f;
    if (axis == 'y' || axis == 'Y') out[1][1] = -1.0f;
    if (axis == 'z' || axis == 'Z') out[2][2] = -1.0f;
}

// Shearing (six types). See mapping in comments below.
void shear_matrix(int shearType, float sh, mat4 out) {
    mat_identity(out);
    switch (shearType) {
        case 1: out[0][1] = sh; break; // x += sh*y
        case 2: out[0][2] = sh; break; // x += sh*z
        case 3: out[1][0] = sh; break; // y += sh*x
        case 4: out[1][2] = sh; break; // y += sh*z
        case 5: out[2][0] = sh; break; // z += sh*x
        case 6: out[2][1] = sh; break; // z += sh*y
    }
}

// Apply matrix M to cumulative model: model = M * model (pre-multiply)
void apply_transform(const mat4 M) {
    mat4 tmp;
    mat_mul(M, model, tmp);
    memcpy(model, tmp, sizeof(mat4));

    // update transformed vertices
    for (int i = 0; i < 8; ++i) {
        mat_vec_mul(model, originalVerts[i], cubeVerts[i]);
    }
}

// Initialize cube vertices (centered at origin, size s*2)
void init_cube() {
    float s = 0.7f; // half-size
    vec4 v[8] = {
        {-s, -s, -s, 1.0f}, { s, -s, -s, 1.0f},
        { s,  s, -s, 1.0f}, {-s,  s, -s, 1.0f},
        {-s, -s,  s, 1.0f}, { s, -s,  s, 1.0f},
        { s,  s,  s, 1.0f}, {-s,  s,  s, 1.0f}
    };
    for (int i = 0; i < 8; ++i) memcpy(originalVerts[i], v[i], sizeof(vec4));
    mat_identity(model);
    for (int i = 0; i < 8; ++i) mat_vec_mul(model, originalVerts[i], cubeVerts[i]);
}

// Draw axes for reference
void draw_axes(float len) {
    glBegin(GL_LINES);
      glColor3f(1, 0, 0); glVertex3f(0,0,0); glVertex3f(len,0,0); // X
      glColor3f(0, 1, 0); glVertex3f(0,0,0); glVertex3f(0,len,0); // Y
      glColor3f(0, 0, 1); glVertex3f(0,0,0); glVertex3f(0,0,len); // Z
    glEnd();
}

// Draw a colored cube using transformed vertices
void draw_cube() {
    int faces[6][4] = {
        {4, 5, 6, 7}, // Front (+Z)
        {0, 1, 2, 3}, // Back  (-Z)
        {0, 4, 7, 3}, // Left  (-X)
        {1, 5, 6, 2}, // Right (+X)
        {3, 2, 6, 7}, // Top   (+Y)
        {0, 1, 5, 4}  // Bottom(-Y)
    };

    float colors[6][3] = {
        {1,0,0}, {0,1,0}, {0,0,1},
        {1,1,0}, {1,0,1}, {0,1,1}
    };

    for (int f = 0; f < 6; f++) {
        glColor3fv(colors[f]);
        glBegin(GL_QUADS);
        for (int v = 0; v < 4; v++) {
            int idx = faces[f][v];
            glVertex3f(cubeVerts[idx][0], cubeVerts[idx][1], cubeVerts[idx][2]);
        }
        glEnd();
    }

    // draw edges (black)
    glColor3f(0,0,0);
    glBegin(GL_LINES);
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i) {
        int a = edges[i][0], b = edges[i][1];
        glVertex3f(cubeVerts[a][0], cubeVerts[a][1], cubeVerts[a][2]);
        glVertex3f(cubeVerts[b][0], cubeVerts[b][1], cubeVerts[b][2]);
    }
    glEnd();
}

// GLUT display
void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // simple camera translate back
    glTranslatef(0.0f, 0.0f, -4.0f);

    // apply view rotations (mouse drag)
    glRotatef(viewRotX, 1, 0, 0);
    glRotatef(viewRotY, 0, 1, 0);

    draw_axes(1.5f);
    draw_cube();

    glutSwapBuffers();
}

// reshape
void reshape(int w, int h) {
    win_w = w; win_h = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w/h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// print instructions
void print_instructions() {
    printf("\n3D Hybrid Transformations (homogeneous coords)\n\n");
    printf("Transform keys (press):\n");
    printf("  Arrow keys / PageUp / PageDown  : Translate (small steps)\n");
    printf("  + / -                           : Uniform scale up / down\n");
    printf("  x / X  : Rotate +10 / -10 deg about X\n");
    printf("  y / Y  : Rotate +10 / -10 deg about Y\n");
    printf("  z / Z  : Rotate +10 / -10 deg about Z\n");
    printf("\nReflection (no conflict with rotation keys):\n");
    printf("  F : Reflect about YZ plane (invert X)\n");
    printf("  G : Reflect about XZ plane (invert Y)\n");
    printf("  H : Reflect about XY plane (invert Z)\n");
    printf("\nShearing (press number 1..6):\n");
    printf("  1: x += sh*y    2: x += sh*z\n");
    printf("  3: y += sh*x    4: y += sh*z\n");
    printf("  5: z += sh*x    6: z += sh*y\n");
    printf("  (default sh = 0.3)\n");
    printf("\nOther:\n  c : Reset model to identity\n  q or Esc : Quit\n");
    printf("Mouse left-drag : Rotate view (orbit)\n\n");
}

// keyboard handler
void keyboard(unsigned char key, int x, int y) {
    mat4 M;
    switch (key) {
        case 27: case 'q': exit(0); break;
        case 'c':
            mat_identity(model);
            for (int i = 0; i < 8; ++i) mat_vec_mul(model, originalVerts[i], cubeVerts[i]);
            printf("Reset model matrix to identity.\n");
            break;
        case '+': case '=':
            scaling_matrix(1.1f, 1.1f, 1.1f, M); apply_transform(M);
            printf("Scaled by 1.1 uniformly.\n"); break;
        case '-':
            scaling_matrix(0.9f, 0.9f, 0.9f, M); apply_transform(M);
            printf("Scaled by 0.9 uniformly.\n"); break;
        case 'x':
            rotation_x_matrix(10.0f, M); apply_transform(M); printf("Rotated +10 deg about X.\n"); break;
        case 'X':
            rotation_x_matrix(-10.0f, M); apply_transform(M); printf("Rotated -10 deg about X.\n"); break;
        case 'y':
            rotation_y_matrix(10.0f, M); apply_transform(M); printf("Rotated +10 deg about Y.\n"); break;
        case 'Y':
            rotation_y_matrix(-10.0f, M); apply_transform(M); printf("Rotated -10 deg about Y.\n"); break;
        case 'z':
            rotation_z_matrix(10.0f, M); apply_transform(M); printf("Rotated +10 deg about Z.\n"); break;
        case 'Z':
            rotation_z_matrix(-10.0f, M); apply_transform(M); printf("Rotated -10 deg about Z.\n"); break;

        // Reflection keys (capital letters to avoid conflict)
        case 'F':
            reflection_matrix('x', M); apply_transform(M); printf("Reflected about YZ (invert X).\n"); break;
        case 'G':
            reflection_matrix('y', M); apply_transform(M); printf("Reflected about XZ (invert Y).\n"); break;
        case 'H':
            reflection_matrix('z', M); apply_transform(M); printf("Reflected about XY (invert Z).\n"); break;

        // Shears: '1'..'6'
        case '1': case '2': case '3': case '4': case '5': case '6': {
            int t = key - '0';
            shear_matrix(t, 0.3f, M);
            apply_transform(M);
            printf("Applied shear type %d with sh = 0.3\n", t);
            break;
        }

        default:
            break;
    }
    glutPostRedisplay();
}

// special keys (arrow keys, PageUp/PageDown)
void special(int key, int x, int y) {
    mat4 M;
    switch (key) {
        case GLUT_KEY_LEFT:
            translation_matrix(-0.1f, 0.0f, 0.0f, M); apply_transform(M); printf("Translated -0.1 in X.\n"); break;
        case GLUT_KEY_RIGHT:
            translation_matrix(0.1f, 0.0f, 0.0f, M); apply_transform(M); printf("Translated +0.1 in X.\n"); break;
        case GLUT_KEY_UP:
            translation_matrix(0.0f, 0.1f, 0.0f, M); apply_transform(M); printf("Translated +0.1 in Y.\n"); break;
        case GLUT_KEY_DOWN:
            translation_matrix(0.0f, -0.1f, 0.0f, M); apply_transform(M); printf("Translated -0.1 in Y.\n"); break;
        case GLUT_KEY_PAGE_UP:
            translation_matrix(0.0f, 0.0f, 0.1f, M); apply_transform(M); printf("Translated +0.1 in Z.\n"); break;
        case GLUT_KEY_PAGE_DOWN:
            translation_matrix(0.0f, 0.0f, -0.1f, M); apply_transform(M); printf("Translated -0.1 in Z.\n"); break;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            leftButtonDown = 1;
            lastX = x; lastY = y;
        } else leftButtonDown = 0;
    }
}

void motion(int x, int y) {
    if (!leftButtonDown) return;
    int dx = x - lastX;
    int dy = y - lastY;
    viewRotY += dx * 0.5f;
    viewRotX += dy * 0.5f;
    lastX = x; lastY = y;
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    printf("Initializing 3D Hybrid Transformation Demo\n");
    print_instructions();
    init_cube();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(win_w, win_h);
    glutCreateWindow("3D Hybrid Transformations (Homogeneous Coordinates)");

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.92f, 0.92f, 0.95f, 1.0f);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
