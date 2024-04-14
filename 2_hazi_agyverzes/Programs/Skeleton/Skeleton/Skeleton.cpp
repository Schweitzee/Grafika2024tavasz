//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Schweitzer Andras Attila
// Neptun : TLEIB5
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
// 
// 
//=============================================================================================
// Bezier and Lagrange curve editor
//=============================================================================================
#include "framework.h"

enum Curvetypes {
	LAGRANGE, BEZIER, CATMULL,
};

Curvetypes curvetype = LAGRANGE;

float catmull_t = 0;



// vertex shader in GLSL
const char* vertexSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0

	void main() {
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char* fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	void main() {
		fragmentColor = vec4(color, 1); // extend RGB to RGBA
	}
)";

// 2D camera

class Camera2D {
	vec2 wCenter;
	vec2 wSize;

public:
	Camera2D() : wCenter(0, 0), wSize(30, 30) {}
	mat4 V() { return TranslateMatrix(-wCenter); }
	mat4 P() { return ScaleMatrix(vec2(2 / wSize.x, 2 / wSize.y)); }

	mat4 Vinv() { return TranslateMatrix(wCenter); }
	mat4 Pinv() { return ScaleMatrix(vec2(wSize.x / 2, wSize.y / 2)); }


	void Zoom(float s) { wSize = wSize * s; }
	void Pan(vec2 t) { wCenter = wCenter + t; }
};

Camera2D camera;	
GPUProgram gpuProgram; // vertex and fragment shaders

const int nTesselatedVertices = 100;

float distance(vec2 a, vec2 b) {
	return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

class Curve {
	unsigned int vaoVectorizedCurve, vboVectorizedCurve;
	unsigned int vaoCtrlPoints, vboCtrlPoints;
protected:
	std::vector<vec2> wCtrlPoints;		// coordinates of control points
public:
	Curve() {
		// Curve
		glGenVertexArrays(1, &vaoVectorizedCurve);
		glBindVertexArray(vaoVectorizedCurve);

		glGenBuffers(1, &vboVectorizedCurve); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboVectorizedCurve);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

		// Control Points
		glGenVertexArrays(1, &vaoCtrlPoints);
		glBindVertexArray(vaoCtrlPoints);

		glGenBuffers(1, &vboCtrlPoints); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset
	}

	~Curve() {
		glDeleteBuffers(1, &vboCtrlPoints); glDeleteVertexArrays(1, &vaoCtrlPoints);
		glDeleteBuffers(1, &vboVectorizedCurve); glDeleteVertexArrays(1, &vaoVectorizedCurve);
	}

	virtual vec2 r(float t) = 0;
	virtual float tStart() = 0;
	virtual float tEnd() = 0;

	virtual void AddControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints.push_back(vec2(wVertex.x, wVertex.y));
	}

	// Returns the selected control point or -1
	int PickControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		for (unsigned int p = 0; p < wCtrlPoints.size(); p++) {
			if (distance(wCtrlPoints[p], vec2(wVertex.x, wVertex.y)) < 0.1f)
				return p;
		}
		return -1;
	}

	void MoveControlPoint(int p, float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints[p] = vec2(wVertex.x, wVertex.y);
	}

	void Draw() {
		mat4 VPTransform = camera.V() * camera.P();
		gpuProgram.setUniform(VPTransform, "MVP");

		if (wCtrlPoints.size() > 0) {	// draw control points
			glBindVertexArray(vaoCtrlPoints);
			glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
			glBufferData(GL_ARRAY_BUFFER, wCtrlPoints.size() * sizeof(vec2), &wCtrlPoints[0], GL_DYNAMIC_DRAW);
			
			gpuProgram.setUniform(vec3(1, 0, 0), "color");

			glPointSize(10.0f);
			glDrawArrays(GL_POINTS, 0, wCtrlPoints.size());
		}

		if (wCtrlPoints.size() >= 2) {	// draw curve
			std::vector<vec2> vertexData;
			for (int i = 0; i < nTesselatedVertices; i++) {	// Tessellate
				float tNormalized = (float)i / (nTesselatedVertices - 1);
				float t = tStart() + (tEnd() - tStart()) * tNormalized;
				vec2 wVertex = r(t);
				vertexData.push_back(wVertex);
			}
			// copy data to the GPU
			glBindVertexArray(vaoVectorizedCurve);
			glBindBuffer(GL_ARRAY_BUFFER, vboVectorizedCurve);
			glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(vec2), &vertexData[0], GL_DYNAMIC_DRAW);
			gpuProgram.setUniform(vec3(1, 1, 0), "color");
			glLineWidth(2.0f);
			glDrawArrays(GL_LINE_STRIP, 0, nTesselatedVertices);
		}
	}
};

// Bezier using Bernstein polynomials
class BezierCurve : public Curve {
	float B(int i, float t) {
		int n = wCtrlPoints.size() - 1; // n deg polynomial = n+1 pts!
		float choose = 1;
		for (int j = 1; j <= i; j++) choose *= (float)(n - j + 1) / j;
		return choose * pow(t, i) * pow(1 - t, n - i);
	}
public:
	float tStart() { return 0; }
	float tEnd() { return 1; }


	virtual vec2 r(float t) {
		vec2 wPoint = vec2(0, 0);
		for (unsigned int n = 0; n < wCtrlPoints.size(); n++) wPoint = wPoint + wCtrlPoints[n] * B(n, t);
		return wPoint;
	}
};

// Lagrange curve
class LagrangeCurve : public Curve {
	std::vector<float> ts;  // knots
	float L(unsigned int i, float t) {
		float Li = 1.0f;
		for (unsigned int j = 0; j < wCtrlPoints.size(); j++)
			if (j != i) Li *= (t - ts[j]) / (ts[i] - ts[j]);
		return Li;
	}
public:
	void AddControlPoint(float cX, float cY) {
		Curve::AddControlPoint(cX, cY);

		if (wCtrlPoints.size() > 1) {
			ts.clear();
			float tot_length = 0;

			for (unsigned int i = 1; i <= wCtrlPoints.size() - 1; i++) {
				float Distance = distance(wCtrlPoints[i - 1], wCtrlPoints[i]);
				tot_length += Distance;
			}

			ts.push_back(0);
			printf("number of control points: %d\n", wCtrlPoints.size());
			printf("ts 0 = 0\n");

			float partial_length = 0;
			for (unsigned int i = 1; i <= wCtrlPoints.size() - 1; i++) {
				float Distance = distance(wCtrlPoints[i], wCtrlPoints[i - 1]);
				partial_length += Distance;
				ts.push_back(partial_length / tot_length);
				printf("ts %d = %f\n", i, partial_length / tot_length);
			}
		}
		else {
			ts.push_back(0);
		}
	}
	float tStart() { return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size() - 1]; }

	virtual vec2 r(float t) {
		vec2 wPoint = vec2(0, 0);
		for (unsigned int n = 0; n < wCtrlPoints.size(); n++) wPoint = wPoint + wCtrlPoints[n] * L(n, t);
		return wPoint;
	}
};

class CatmullRom : public Curve {
	std::vector<float> ts; // parameter (knot) values
	vec2 Hermite(vec2 p0, vec2 v0, float t0, vec2 p1, vec2 v1, float t1,
		float t) {
		float deltat = t1 - t0;
		t -= t0;
		float deltat2 = deltat * deltat;
		float deltat3 = deltat2 * deltat;
		vec2 a0 = p0, a1 = v0;
		vec2 a2 = ((3 * (p1 - p0)) / deltat2) - ((v1 + 2 * v0) / deltat);
		vec2 a3 = ((2 * (p0 - p1)) / deltat3) + ((v1 + v0) / deltat2);

		return ((a3 * t + a2) * t + a1) * t + a0;
	}
public:

	void AddControlPoint(float cX, float cY) {
		Curve::AddControlPoint(cX, cY);
		if (wCtrlPoints.size() > 1) {
			ts.clear();
			float tot_length = 0;

			for (unsigned int i = 1; i <= wCtrlPoints.size() - 1; i++) {
				float Distance = distance(wCtrlPoints[i - 1], wCtrlPoints[i]);
				tot_length += Distance;
			}

			ts.push_back(0);
			printf("number of control points: %d\n", wCtrlPoints.size());
			printf("ts 0 = 0\n");

			float partial_length = 0;
			for (unsigned int i = 1; i <= wCtrlPoints.size() - 1; i++) {
				float Distance = distance(wCtrlPoints[i], wCtrlPoints[i - 1]);
				partial_length += Distance;
				ts.push_back(partial_length / tot_length);
				printf("ts %d = %f\n", i, partial_length / tot_length);
			}
		}
		else {
			ts.push_back(0);
		}

	}

	float tStart() { return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size() - 1]; }

	vec2 r(float t) {
		for (unsigned int i = 0; i < wCtrlPoints.size() - 1; i++)
			if (ts[i] <= t && t <= ts[i + 1]) {

				// v(i) = (1-t/2)*( ( (ri+1 - ri) / (ti+1 - ti) ) + (( ri - ri-1) / ( ti - ti-1 ))
				vec2 v0, v1;

				if(i != 0 && i != wCtrlPoints.size() - 1)
				{
					vec2 vPrev = (wCtrlPoints[i] - wCtrlPoints[i - 1]) / (ts[i] - ts[i - 1]);
					vec2 vCur = (wCtrlPoints[i + 1] - wCtrlPoints[i]) / (ts[i + 1] - ts[i]);
					vec2 vNext = (wCtrlPoints[i + 2] - wCtrlPoints[i + 1]) / (ts[i + 2] - ts[i + 1]);

					v0 = (vPrev + vCur) * ((1 - catmull_t) / 2);
					v1 = (vCur + vNext) * ((1 - catmull_t) / 2);
				}
				else if (i == 0)
				{
					vec2 vCur = (wCtrlPoints[i + 1] - wCtrlPoints[i]) / (ts[i + 1] - ts[i]);
					vec2 vNext = (wCtrlPoints[i + 2] - wCtrlPoints[i + 1]) / (ts[i + 2] - ts[i + 1]);

					v0 = vCur * (1 - catmull_t);
					v1 = (vCur + vNext) * ((1 - catmull_t) / 2);
				}
				else if(i == wCtrlPoints.size()-1)
				{
					vec2 vPrev = (wCtrlPoints[i] - wCtrlPoints[i - 1]) / (ts[i] - ts[i - 1]);
					vec2 vCur = (wCtrlPoints[i + 1] - wCtrlPoints[i]) / (ts[i + 1] - ts[i]);

					v0 = (vPrev + vCur) * ((1 - catmull_t) / 2);
					v1 = vCur * (1 - catmull_t);
				}	

				//return (Hermite(cps[i], v0, ts[i], cps[i + 1], v1, ts[i + 1], t));

				return  Hermite(wCtrlPoints[i], v0, ts[i], wCtrlPoints[i + 1], v1, ts[i + 1], t);
			}
		return wCtrlPoints[0];
	}
};


// The virtual world: collection of two objects
Curve* curve;


// Initialization, create an OpenGL context
void onInitialization() {

	glViewport(0, 0, windowWidth, windowHeight);

	curve = new LagrangeCurve();

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

	curve->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {

	switch (key) {
	case 'c':
		delete curve;
		curve = new CatmullRom();
		curvetype = CATMULL;
		printf("Catmull-Rom spline\n");
		break;
	case 'l':
		delete curve;
		curve = new LagrangeCurve();
		curvetype = LAGRANGE;
		printf("Lagrange curve\n");
		break;
	case 'b':
		delete curve;
		curve = new BezierCurve();
		curvetype = BEZIER;
		printf("Bezier curve\n");
		break;
	case 'Z':
		camera.Zoom(1.1);
		break;
	case 'z':
		camera.Zoom(1 / 1.1);
		break;
	case 'P':
		camera.Pan(vec2(1, 0));
		break;
	case 'p':
		camera.Pan(vec2(-1, 0));
		break;
	case 'T':
		catmull_t += 0.1;
		break;
	case 't':
		catmull_t -= 0.1;
		break;
	case 'd':
		break;
	}

	glutPostRedisplay();        // redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {

}

int pickedControlPoint = -1;

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
		curve->AddControlPoint(cX, cY);
		glutPostRedisplay();     // redraw
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
		pickedControlPoint = curve->PickControlPoint(cX, cY);
		glutPostRedisplay();     // redraw
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		pickedControlPoint = -1;
	}
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;
	if (pickedControlPoint >= 0) curve->MoveControlPoint(pickedControlPoint, cX, cY);
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
}