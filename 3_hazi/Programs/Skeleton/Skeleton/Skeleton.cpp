//=============================================================================================
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
#include "framework.h"

// vertex shader in GLSL
const char* vertexSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0
	layout(location = 1) in vec2 vertexUV;			// Attrib Array 1

	out vec2 texCoord;								// output attribute

	void main() {
		texCoord = vertexUV;														// copy texture coordinates
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char* fragmentSource = R"(
	#version 330
    precision highp float;

	uniform sampler2D textureUnit;

	in vec2 texCoord;			// variable input: interpolated texture coordinates
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation


	void main() {
		fragmentColor = texture(textureUnit, texCoord);
	}
)";

// 2D camera
class Camera2D {
	vec2 wCenter; // center in world coordinates
	vec2 wSize;   // width and height in world coordinates
public:
	Camera2D() : wCenter(20, 30), wSize(150, 150) { }

	mat4 V() { return TranslateMatrix(-wCenter); }
	mat4 P() { return ScaleMatrix(vec2(2 / wSize.x, 2 / wSize.y)); }

	mat4 Vinv() { return TranslateMatrix(wCenter); }
	mat4 Pinv() { return ScaleMatrix(vec2(wSize.x / 2, wSize.y / 2)); }
};

class Circle {
	public:
	vec2 center;
	float radius;
};

GPUProgram gpuProgram; // vertex and fragment shaders

class PoincareTexture{
	unsigned int textureID;

	std::vector<Circle> circles;
	std::vector<vec4> colors;

	int linear = 1;

	int width = 300, height = 300;

	public:
		//makes the cirscles
		PoincareTexture() {

			for (int phi = 0; phi < 360; phi = phi + 40) {
				for (float dh = 0.5; dh <= 5.5; dh++) {
					
					float rad = phi * 3.1415 / 180;

					Circle circle;
					float dp = tanh(dh / 2); //distance from the center
					float r = ((1.0/dp) -dp )/2; //radius
					circle.center = vec2((r+dp) * cos(rad), (r+dp) * sin(rad)); //center
					circle.radius = r;
					circles.push_back(circle);
				}
			}
		}

		void recountPixels() {

			colors.clear();

			for (int y = 0; y < height; y++) {

				float he = 2 * (y / (float)height) - 1;

				for (int x = 0; x < width; x++) {
					vec2 p = vec2(2 * (x / (float)width) - 1, he);
					if (length(p) <= 1)
					{
						int num = 0;

						for(int i = 0; i < circles.size(); i++)
						{
							if (length(p - circles[i].center) <= circles[i].radius) {
								num++;
							}
						}
						if (num % 2 == 0)
						{
							colors.push_back(vec4(1, 1, 0, 1));
						}
						else
						{
							colors.push_back(vec4(0, 0, 1, 1));
						}
					}
					else
					{
						colors.push_back(vec4(0, 0, 0, 1));
					}
				}
			}
		}

		void RenderToTexture() {
			recountPixels();
			glGenTextures(1, &textureID);				// generate identifier for the texture
			glBindTexture(GL_TEXTURE_2D, textureID);	// make the texture active
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, &colors[0]);
			if (linear == 1) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			gpuProgram.setUniform(GL_TEXTURE0, "textureUnit");
		}

		void setLinear(int l) {
			linear = l;
			RenderToTexture();
		}

		void modifyRes(int n) {
			width += n;
			height += n;
			RenderToTexture();
		}
};

int animate = 0; // 0: no animation, 1: animate

Camera2D camera;       // 2D camera

PoincareTexture texture;

class Negyzet {
	unsigned int vao, vbo[2];
	vec2 vertices[10], uvs[10];

	int sp = 40;


	vec2 centerPoint = vec2(20,30);	// center of rotation
	float sugar = 30;	// translation

	vec2 wTranslate = vec2(30,0);	// translation

	float phi = 0;			// angle of rotation

public:
	Negyzet(int width, int height, const std::vector<vec4>& image){

		recountVerticies();

		uvs[0] = vec2(0.5, 0.5);	//kozep
		uvs[1] = vec2(0, 0.5);		//bal kozep
		uvs[2] = vec2(0, 1);		//
		uvs[3] = vec2(0.5, 1);		//felso
		uvs[4] = vec2(1, 1);		//
		uvs[5] = vec2(1, 0.5);		//jobb kozep	
		uvs[6] = vec2(1, 0);		//	
		uvs[7] = vec2(0.5, 0);		//also
		uvs[8] = vec2(0, 0);		//	
		uvs[9] = vec2(0, 0.5);		//bal kozep


		glGenVertexArrays(1, &vao);	// create 1 vertex array object
		glBindVertexArray(vao);		// make it active

		glGenBuffers(2, vbo);	// Generate 1 vertex buffer objects

		// vertex coordinates: vbo[0] -> Attrib Array 0 -> vertexPosition of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); // make it active, it is an array
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);     // stride and offset: it is tightly packed

		// vertex coordinates: vbo[1] -> Attrib Array 1 -> vertexUV of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); // make it active, it is an array
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);     // stride and offset: it is tightly packed		
	}

	void recountVerticies() {
		vertices[0] = vec2(0, 0);		//kozep
		vertices[1] = vec2(-sp, 0);		//bal kozep
		vertices[2] = vec2(-40, 40);	//
		vertices[3] = vec2(0, sp);		//felso
		vertices[4] = vec2(40, 40);		//
		vertices[5] = vec2(sp, 0);		//jobb kozep
		vertices[6] = vec2(40, -40);	//	
		vertices[7] = vec2(0, -sp);		//also
		vertices[8] = vec2(-40, -40);	//	
		vertices[9] = vec2(-sp, 0);		//bal kozep
	}

	void Draw() {
		mat4 MVPTransform = M() * camera.V() * camera.P();
		gpuProgram.setUniform(MVPTransform, "MVP");
		texture.RenderToTexture();   

		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 10);	// draw two triangles forming a quad
	}

	void sharpen() {
		sp -= 10;
		recountVerticies();
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);	   // copy to that part of the memory which is modified 
	}

	void widen() {
		sp += 10;
		recountVerticies();
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);	   // copy to that part of the memory which is modified 
	}


	void Animate(float t) {
		phi = t;
		wTranslate =  vec2(cos(phi) * sugar, sin(phi) * sugar);
	}

	mat4 M() {

		mat4 Mrotate(cosf(phi*2), sinf(phi * 2), 0, 0,
			-sinf(phi * 2), cosf(phi * 2), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1); // rotation

		mat4 Mtranslate(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 0,
			wTranslate.x + centerPoint.x, wTranslate.y + centerPoint.y, 0, 1); // translation

		return Mrotate * Mtranslate;	// model transformation
	}


};

Negyzet* quad; // The virtual world: one object

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	int width = 300, height = 300; 
	std::vector<vec4> image(width * height);
	//HASONLÍTSD OSSZE AZ EREDETI KOODDAL

	quad = new Negyzet(width, height, image);

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "fragmentColor");
	glutPostRedisplay();
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	quad->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

long timeSetter;

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key){
		case 'r':
			texture.modifyRes(-100);
			break;
		case 'R':
			texture.modifyRes(100);
			break;
		case 't':
			texture.setLinear(0);
			break;
		case 'T':
			texture.setLinear(1);
			break;
		case 'h':
			quad->sharpen();
			glutPostRedisplay();
			break;
		case 'H':
			quad->widen();
			glutPostRedisplay();
			break;
		case 'a':
			if (animate == 0) {
				animate = 1;
				timeSetter = glutGet(GLUT_ELAPSED_TIME);
			}
			else {
				animate = 0;
				glutPostRedisplay();
			}
			break;
		default:
			break;

	}
	glutPostRedisplay();
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}
// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
}



// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	if (animate == 1) {
		long time = glutGet(GLUT_ELAPSED_TIME) - timeSetter; // elapsed time since the start of the program
		float sec = time / (500.0f * 3.1415);				// convert msec to sec
		quad->Animate(sec);					// animate the triangle object
		glutPostRedisplay();
	}
}
