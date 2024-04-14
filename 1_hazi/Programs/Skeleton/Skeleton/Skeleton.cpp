//=============================================================================================
// Mintaprogram: Z�ld h�romsz�g. Ervenyes 2019. osztol.
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
// Nev    : SChweitzer Andras Attila
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
#include "framework.h"
#include <vector>
#include <iostream>

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330
	precision highp float;		
	// normal floats, makes no difference on desktop computers

	layout(location = 0) in vec2 vertexPosition;
	void main() {
		gl_Position = vec4(vertexPosition, 0, 1); // in NDC
	}

)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330
	precision highp float;	
	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

enum Mode
{
	point_dr, line_dr, line_trans, intersect
};

Mode mode;

vec2 PixelToNDC(int pX, int pY) { // if full viewport!
	return vec2(2.0f * pX / windowWidth - 1, 1 - 2.0f * pY / windowHeight);
}

float calculateY(float x, float m, float b) {
	return m * x + b;
}

float calculateX(float y, float m, float b) {
	return (y - b) / m;
}

int isPointInsideSquare(vec2 p) {
	return (std::abs(p.x) <= 1 && std::abs(p.y) <= 1);
}

GPUProgram gpuProgram; // vertex and fragment shaders

class Object {
	unsigned int vao, vbo; // GPU
	std::vector<vec2> vtx; // CPU
public:
	Object() {
		glGenVertexArrays(1, &vao); glBindVertexArray(vao);
		glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	std::vector<vec2>& Vtx() { return vtx; }
	void updateGPU() { // CPU -> GPU
		glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec2), &vtx[0], GL_DYNAMIC_DRAW);
	}

	vec2* getPoint(float x, float y)
	{
		for (unsigned int i = 0; i < vtx.size(); i++)
		{
			if (vtx[i].x == x && vtx[i].y == y)
				return &vtx[i];
		}
		return nullptr;
	}

	void Draw(int type, vec3 color) {
		if (vtx.size() > 0) {
			glBindVertexArray(vao);
			gpuProgram.setUniform(color, "color");
			glDrawArrays(type, 0, vtx.size());
		}
	}
};

class LineObject {
	std::vector<vec2*> vtx; // CPU, endpoints of the line
	std::vector<vec2> dummyVTX = std::vector<vec2>();

	int vertical = false;

	Object* lines;

	float m = 0, b = 0; // y = mx + b
	// m = (y2 - y1) / (x2 - x1)
	// b = y1 - m * x1

	// Ax + By + C = 0
	// A = y2 - y1, B = x1 - x2, C = y1 × (x2 - x1) - (y2 - y1) × x1	

public:
	LineObject() {}

	std::vector<vec2*>& Vtx() { return vtx; }

	void setVertical(int v) {
		vertical = v;
		if(v)
			m = 1;
	}

	void setLinesOB(Object* l) {
		lines = l;
	}

	int isVertical() {
		return vertical;
	}

	int pointCloseToLine(vec2 p, float thres) {
		float A = vtx[1]->y - vtx[0]->y;
		float B = vtx[0]->x - vtx[1]->x;
		float C = vtx[0]->y * (vtx[1]->x - vtx[0]->x) - (vtx[1]->y - vtx[0]->y) * vtx[0]->x;
		float d = abs(A * p.x + B * p.y + C) / sqrt(A * A + B * B);
		if (d <= thres) {
			return 1;
		}
		return 0;
	}


	void setLine(float M, float B) {
		this->m = M;
		this->b = B;
	}

	float getM() {
		return m;
	}
	float getB() {
		return b;
	}

	void calcLine() {
		this->m = (vtx[1]->y - vtx[0]->y) / (vtx[1]->x - vtx[0]->x);
		this->b = vtx[0]->y - m * vtx[0]->x;
	}

	void moveVertically(float yy) {
		b += yy;
	}
	void moveHorizontally(float xx) {
		b += xx*m;
	}

	void reCalcPoints() {

		if (vertical) {
			vtx[0]->x = vtx[1]->x = b;
			vtx[0]->y = -1;
			vtx[1]->y = 1;
			return;
		}

		dummyVTX.clear();

		vec2 squareVertices[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
		for (int i = 0; i < 4; i++)
		{
			float y = calculateY(squareVertices[i].x, m, b);
			if (isPointInsideSquare({ squareVertices[i].x, y }))
			{
				if(dummyVTX.size() != 0)							
				{
					if (dummyVTX[0].x != squareVertices[i].x || dummyVTX[0].y != y)
					{
						dummyVTX.push_back({ squareVertices[i].x, y });
						//printf("Added endpoint at (%3.2f, %3.2f)\n", squareVertices[i].x, y);
					}
				}
				else
				{
					dummyVTX.push_back({ squareVertices[i].x, y });
					//printf("Added endpoint at (%3.2f, %3.2f)\n", squareVertices[i].x, y);
				}
			}
			float x = calculateX(squareVertices[i].y, m, b);
			if (isPointInsideSquare({ x, squareVertices[i].y }))
			{
				if (dummyVTX.size() != 0)
				{
					if (dummyVTX[0].x != x || dummyVTX[0].y != squareVertices[i].y)
					{
						dummyVTX.push_back({ x, squareVertices[i].y });
						//printf("Added endpoint at (%3.2f, %3.2f)\n", x, squareVertices[i].y);
					}
				}
				else
				{
					dummyVTX.push_back({ x, squareVertices[i].y });
					//printf("Added endpoint at (%3.2f, %3.2f)\n", x, squareVertices[i].y);
				}
			}
			if (dummyVTX.size() >= 2) {
				break;
			}
		}
		lines->getPoint(vtx[0]->x, vtx[0]->y)->x = dummyVTX[0].x;
		lines->getPoint(dummyVTX[0].x, vtx[0]->y)->y = dummyVTX[0].y;
		lines->getPoint(vtx[1]->x, vtx[1]->y)->x = dummyVTX[1].x;
		lines->getPoint(dummyVTX[1].x, vtx[1]->y)->y = dummyVTX[1].y;
		
		vtx[0]->x = dummyVTX[0].x;
		vtx[0]->y = dummyVTX[0].y;
		vtx[1]->x = dummyVTX[1].x;
		vtx[1]->y = dummyVTX[1].y;
	}

};



class World {
	Object points;
	Object lines;
	std::vector<LineObject> lineObjects;
public:
	void addPoint(vec2 p)
	{
		printf("Added point at (%3.2f, %3.2f)\n", p.x, p.y);
		points.Vtx().push_back(p);
	}
	void addLineObject(LineObject l) {
		lines.Vtx().push_back({l.Vtx()[0]->x, l.Vtx()[0]->y});
		lines.Vtx().push_back({ l.Vtx()[1]->x, l.Vtx()[1]->y });
		lineObjects.push_back(l);
		lineObjects[lineObjects.size() - 1].setLinesOB(&this->lines);
	}

	void update() {
		points.updateGPU();
		if(lines.Vtx().size() > 0)
			lines.updateGPU();
	}

	vec2* getPointOfLine(float x, float y) {
		for (unsigned int i = 0; i < lines.Vtx().size(); i++) {
			if (lines.Vtx()[i].x == x && lines.Vtx()[i].y == y)
				return &points.Vtx()[i];
		}
	}
	LineObject* pickLine(vec2 pp, float threshold)
	{
		for (unsigned int i = 0; i < lineObjects.size(); i++)
			if (lineObjects[i].pointCloseToLine(pp, threshold))
				return &lineObjects[i];
		return nullptr;
	}

	vec2* pickPoint(vec2 pp, float threshold)
	{
		for(unsigned int i = 0; i < points.Vtx().size(); i++)
			if (length(pp - points.Vtx()[i]) < threshold)
				return &points.Vtx()[i];
		return nullptr;
	}



	void Draw() {

		lines.Draw(GL_LINES, vec3(0.0f, 1.0f, 1.0f));

		points.Draw(GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
	}

};
LineObject* lineSelected;
vec2* pickedPoint;

World * world;

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(3);
	glPointSize(10);
	mode = point_dr;
	lineSelected = nullptr;
	pickedPoint = nullptr;
	world = new World();
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.8f, 0.8f, 0.8f, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	world->Draw();

	glutSwapBuffers();
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
	case 'p': // Point drawing
		mode = point_dr;
		printf("Point making\n");
		pickedPoint = nullptr;
		break;
	case 'l': // Line drawing
		mode = line_dr;
		printf("Line making\n");
		pickedPoint = nullptr;
		break;
	case 'm': // Line translation
		mode = line_trans;
		printf("Line translation\n");
		lineSelected = nullptr;
		break;
	case 'i': // Intersection
		mode = intersect;
		printf("Intersection making\n");
		lineSelected = nullptr;
		break;
	default:
		break;
	}
	glutPostRedisplay(); // Invalidate display
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

vec2* mousePos;

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {	// pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;
	//printf("Mouse moved to (%3.2f, %3.2f)\n", cX, cY);

	if (lineSelected != nullptr && mode == line_trans) {

		lineSelected->moveVertically(cY - mousePos->y);
		lineSelected->moveHorizontally(mousePos->x - cX);

		lineSelected->reCalcPoints();
		
		world->update(); glutPostRedisplay();

		float baseB = lineSelected->getB();
		float baseM = lineSelected->getM();

		// Ax + By + C = 0
		// A = y2 - y1, B = x1 - x2, C = y1 × (x2 - x1) - (y2 - y1) × x1
		if (!lineSelected->isVertical()) {
			float A = lineSelected->Vtx()[1]->y - lineSelected->Vtx()[0]->y;
			float B = lineSelected->Vtx()[0]->x - lineSelected->Vtx()[1]->x;
			float C = lineSelected->Vtx()[0]->y * (lineSelected->Vtx()[1]->x - lineSelected->Vtx()[0]->x) - (lineSelected->Vtx()[1]->y - lineSelected->Vtx()[0]->y) * lineSelected->Vtx()[0]->x;

			printf("Implicit line equation: %3.2f * x + %3.2f * y + %3.2f = 0\n", A, B, C);
			printf("Parametric line equation:\n x = 0 + t\n y = %3.2f + %3.2f * t\n", baseB, baseM);
		}


		mousePos->y = cY;
		mousePos->x = cX;
	}
}

vec2 lineIntersection(LineObject l1, LineObject l2) {

	float m1 = l1.getM();
	float b1 = l1.getB();
	float m2 = l2.getM();
	float b2 = l2.getB();
	
	float x = (b1 - b2) / (m2 - m1);
	float y = m2 * x + b2;

	return vec2(x, y);
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		if (mode == point_dr) {
			//printf("Point added at (%3.2f, %3.2f)\n", cX, cY);
			world->addPoint(vec2(cX, cY));
			world->update(); glutPostRedisplay();
		}
		else if (mode == line_dr) {
			if (pickedPoint == nullptr) {
				pickedPoint = world->pickPoint(PixelToNDC(pX, pY), 0.05f);
				if (pickedPoint != nullptr) {
					//printf("Point 1 picked at (%3.2f, %3.2f)\n", pickedPoint->x, pickedPoint->y);
				}
			}
			else {
				vec2* pickedPoint2 = world->pickPoint(PixelToNDC(pX, pY), 0.05f);
				if (pickedPoint2 != nullptr) {
					//printf("Point 2 picked at (%3.2f, %3.2f)\n", pickedPoint2->x, pickedPoint2->y);
					LineObject line;
					vec2 p1;
					vec2 p2;
					if (pickedPoint->x > pickedPoint2->x)
					{
						p1 = *pickedPoint2;
						p2 = *pickedPoint;
					}
					else
					{
						p1 = *pickedPoint;
						p2 = *pickedPoint2;
					}
					if (p1.x == p2.x) //vertical line (how and why??)
					{
						vec2 upper =  vec2{ p1.x, 1 };
						vec2 lower = vec2{ p1.x, -1 };

						line.Vtx().push_back(&upper);
						line.Vtx().push_back(&lower);

						world->addLineObject(line);
						printf("x = %3.2f\n", p1.x);

						world->update(); glutPostRedisplay();
						return;
					}
					
					float m = (p2.y - p1.y) / (p2.x - p1.x);
					float b = p1.y - m * p1.x;

					line.setLine(m, b);

					vec2 squareVertices[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
					for (int i = 0; i < 4; i++)
					{
						float y = calculateY(squareVertices[i].x, m, b);
						if (isPointInsideSquare({ squareVertices[i].x, y })) {

							if (line.Vtx().size() != 0)
							{
								if (line.Vtx()[0]->x != squareVertices[i].x || line.Vtx()[0]->y != y)
								{
									line.Vtx().push_back(new vec2{ squareVertices[i].x, y });
									//printf("Added endpoint at (%3.2f, %3.2f)\n", squareVertices[i].x, y);
								}
							}
							else
							{
								line.Vtx().push_back(new vec2 {squareVertices[i].x, y});
								//printf("Added endpoint at (%3.2f, %3.2f)\n", squareVertices[i].x, y);
							}
						}
						float x = calculateX(squareVertices[i].y, m, b);
						if (isPointInsideSquare({ x, squareVertices[i].y })) {

							if (line.Vtx().size() != 0)
							{
								if (line.Vtx()[0]->x != y || line.Vtx()[0]->y != squareVertices[i].y)
								{
									line.Vtx().push_back( new vec2{ x, squareVertices[i].y});
									//printf("Added endpoint at (%3.2f, %3.2f)\n", x, squareVertices[i].y);
								}
							}
							else
							{
								line.Vtx().push_back(new vec2{ x, squareVertices[i].y });
								//printf("Added endpoint at (%3.2f, %3.2f)\n", x, squareVertices[i].y);
							}
						}
						if (line.Vtx().size() == 2)
						{
							break;
						}
					}
						//printf("Line begin at (%3.2f, %3.2f)\n", p1.x, p1.y);
						//printf("Line end at (%3.2f, %3.2f)\n", p2.x, p2.y);
						world->addLineObject(line);

						float A = line.Vtx()[1]->y - line.Vtx()[0]->y;
						float B = line.Vtx()[0]->x - line.Vtx()[1]->x;
						float C = line.Vtx()[0]->y * (line.Vtx()[1]->x - line.Vtx()[0]->x) - (line.Vtx()[1]->y - line.Vtx()[0]->y) * line.Vtx()[0]->x;

						printf("Implicit line equation: %3.2f * x + %3.2f * y + %3.2f = 0\n", A, B, C);
						printf("Parametric line equation:\n x = 0 + t\n y = %3.2f + %3.2f * t\n", b, m);

						world->update(); glutPostRedisplay();
						pickedPoint = nullptr;



				}
			}
		}
		else if (mode == line_trans) {
			if (lineSelected == nullptr) {
				lineSelected = world->pickLine(PixelToNDC(pX, pY), 0.05f);
				if (lineSelected != nullptr) {
					printf("Line selected\n");

					mousePos = new vec2(cX, cY);
				}
			}
		}
		else if (mode == intersect) {
			if (lineSelected != nullptr) {
				LineObject* line2 = world->pickLine(PixelToNDC(pX, pY), 0.05f);
				if (line2 != nullptr) {
					printf("Line 2 selected\n");

					if (lineSelected->getM() != line2->getM())
					{
						world->addPoint(lineIntersection(*lineSelected, *line2));
						world->update(); glutPostRedisplay();
					}
					lineSelected = nullptr;
					line2 = nullptr;
					return;
				}
			}
			else if (lineSelected == nullptr) {
				lineSelected = world->pickLine(PixelToNDC(pX, pY), 0.05f);
				if (lineSelected != nullptr) {
					printf("Line 1 selected\n");
				}
			}


		}
	}
	if (button == GLUT_LEFT_BUTTON && state == GLUT_UP){
		if (mode == line_trans) {
			lineSelected = nullptr;
		}

	}
}



// Idle event indicating that some time elapsed: do animation here
void onIdle() {
}


					//AUTH-os előadás: "Hát..... Ez nagyon fura"
					//"............oké."