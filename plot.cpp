#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <vector>

// Setup
SDL_Renderer* gRenderer = nullptr;
const int windowHeight = 1080;
const int windowWidth = 1920;
const int configUpdateInterval = 100;
const int FPS = 60;
const int particleNum =400000;
int sensorReach = 6;
float ANGLE_CHANGE = 30.0f / 360.0f * 2.0f * M_PI;
const int spawnRadius =200;
const int spawnAngleDeviation = 30;
const int darkening = 16;
const int initBlurReset = 5;
const bool renderToFile = false;
int frameCounter = 0;
float angleChange;

const std::string outputDirectory = "Frames";

// Trail Matrix Vector
std::vector<std::vector<int>> trailMatrix(windowHeight, std::vector<int>(windowWidth, 0));
std::vector<std::vector<int>> tempTrailMatrix(windowHeight, std::vector<int>(windowWidth, 0));  

// config.txt
void loadConfig() {
	std::ifstream configFile("config.txt");
	std::string line;
	
	while (std::getline(configFile, line)) {
		if (line.find("angleChange=") != std::string::npos) {
			angleChange = std::stof(line.substr(line.find('=') + 1));
			ANGLE_CHANGE = angleChange/360*2*M_PI;
		} else if (line.find("sensorReach=") != std::string::npos) {
			sensorReach = std::stoi(line.substr(line.find('=') + 1));
		}
	}
}

void blur() {
	for (int y = 1; y < windowHeight - 1; ++y) {
		for (int x = 1; x < windowWidth - 1; ++x) {
			float sum = trailMatrix[y + 1][x] + trailMatrix[y - 1][x] + trailMatrix[y][x + 1] + trailMatrix[y][x - 1];
			if (sum>darkening) {
				tempTrailMatrix[y][x] = (sum - darkening) / 4.0f;
			} else {tempTrailMatrix[y][x]=0;}
		}
	}
    
	// Edge Pixel
	for (int y = 0; y < windowHeight; ++y) {
		tempTrailMatrix[y][0] = 0;
		tempTrailMatrix[y][windowWidth - 1] = 0;
	}
	for (int x = 0; x < windowWidth; ++x) {
		tempTrailMatrix[0][x] = 0;
		tempTrailMatrix[windowHeight - 1][x] = 0;
	}

	// matrix austauschen
	trailMatrix.swap(tempTrailMatrix);
}

// semiRandom um mit fmod schneller eine "zufällige" Richting zu kriegen
double semiRandom = 0;

class Particle {
private:
	float x, y;
	float angle;
public:
	Particle() {
		// Partikel in spawn Radius mit zuf. Winkel
		float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;
		float theta = static_cast<float>(rand()) / RAND_MAX * 2 * M_PI;
	
		// Polar Koordinaten
		x = windowWidth / 2 + radius * cos(theta);
		y = windowHeight / 2 + radius * sin(theta);
        
		// Zur Mitte ausrichten
		float centerAngle = atan2(windowHeight / 2 - y, windowWidth / 2 - x);
		float deviation = (static_cast<float>(rand()) / RAND_MAX * 2 * spawnAngleDeviation - spawnAngleDeviation) * (M_PI / 180.0f); // Convert to radians
		angle = centerAngle + deviation;

	}

	void move() {
		// Pos updaten
		x += cos(angle);
		y += sin(angle);

		// Rand
		if (x < 1 || x >= windowWidth-1 || y < 1 || y >= windowHeight-1) {
			if (x <1) {
				angle = 0;
			} else if (x >= windowWidth-1) {
				angle = M_PI;
			}
			if (y < 15) {
				angle = M_PI/2;
			} else if (y >= windowHeight-5) {
				angle = M_PI*3/2;
			}
		}

		// Trail 255
		if (x >= 0 && x < windowWidth && y >= 0 && y < windowHeight) {
			trailMatrix[int(y)][int(x)] = 255;
		}


		// Sensor Positionen
		int xForward = int(x + cos(angle) * sensorReach);
		int yForward = int(y + sin(angle) * sensorReach);
		int xLeft = int(x + cos(angle - ANGLE_CHANGE) * sensorReach);
		int yLeft = int(y + sin(angle - ANGLE_CHANGE) * sensorReach);
		int xRight = int(x + cos(angle + ANGLE_CHANGE) * sensorReach);
		int yRight = int(y + sin(angle + ANGLE_CHANGE) * sensorReach);

		// Sensoren Auswerten
		int intensityForward = (yForward >= 10 && yForward < windowHeight-10 && xForward >= 10 && xForward < windowWidth-10) ? trailMatrix[yForward][xForward] : -1;
		int intensityLeft = (yLeft >= 10 && yLeft < windowHeight-10 && xLeft >= 10 && xLeft < windowWidth-10) ? trailMatrix[yLeft][xLeft] : -1;
		int intensityRight = (yRight >= 10 && yRight < windowHeight-10 && xRight >= 10 && xRight < windowWidth-10) ? trailMatrix[yRight][xRight] : -1;

		// Werte Vergleichen
		if (intensityForward == -1 and intensityLeft == -1 and intensityRight == -1) {
			if (angle>M_PI) {angle-=M_PI;
			} else {angle+=M_PI;}
		} else if (intensityForward+intensityLeft+intensityRight<8) {
			semiRandom++;
			angle+=(fmod(semiRandom,11)-5)/50;
		} else if (intensityForward >= intensityLeft && intensityForward >= intensityRight) {
		} else if (intensityLeft > intensityForward && intensityLeft >= intensityRight) {
			angle -= ANGLE_CHANGE;
		} else if (intensityRight > intensityForward && intensityRight > intensityLeft) {
			angle += ANGLE_CHANGE;
		}

		// Nochmal Kantenanstoßverhinderung
		if (x <= 0 || x >= windowWidth - 1 || y <= 0 || y >= windowHeight - 1) {
			angle = (float(rand()) / RAND_MAX) * M_PI * 2;
		}
	}
};

int blurReset = initBlurReset;

void saveFrame(const std::string& filename) {
	std::string fullPath = outputDirectory + "/" + filename;
	unsigned char* buffer = new unsigned char[windowWidth * windowHeight * 4];
	SDL_RenderReadPixels(gRenderer, NULL, SDL_PIXELFORMAT_RGBA8888, buffer, windowWidth * 4);
	stbi_write_png(fullPath.c_str(), windowWidth, windowHeight, 4, buffer, windowWidth * 4);
	delete[] buffer;
}

int main(int argc, char* argv[]) {
	loadConfig();
	/* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	/* Fenster */
	SDL_Window* window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
	if (window == nullptr) {
		std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	/* Renderer */
	gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (gRenderer == nullptr) {
		std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	/* FPS */
	const int frameDelay = 1000 / FPS;
	Uint32 frameStart;
	int frameTime;

	/* Main loop flag */
	bool quit = false;
	SDL_Event event;

	// Partikel spawnen
	srand(static_cast<unsigned int>(time(0)));
	Particle particles[particleNum];

	while (!quit) {
		frameStart = SDL_GetTicks();

		while (SDL_PollEvent(&event) != 0) {
			if (event.type == SDL_QUIT) {
				quit = true;
			}
		}

		// Hintergrund
		SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
		SDL_RenderClear(gRenderer);
	
		// Trails aus der trailMatrix
		for (int i = 0; i < windowHeight; i++) {
			for (int j = 0; j < windowWidth; j++) {
				Uint8 intensity = Uint8(trailMatrix[i][j]);
				if (intensity > 0) {
					SDL_SetRenderDrawColor(gRenderer, intensity, intensity, intensity, 255);
					SDL_RenderDrawPoint(gRenderer, j, i);
				}
			}
		}
	
		if (frameCounter % configUpdateInterval == 0) {
			loadConfig();
		}
		frameCounter++;
	
		// Blur
		if (blurReset == 0) {
			blur();
			blurReset = initBlurReset;
		}
		blurReset -= 1;
	
		// Partikel
		for (int i = 0; i < particleNum; ++i) {
			particles[i].move();
		}
	
		// Frame Speichern
		if (renderToFile) {
			saveFrame("frame_" + std::to_string(SDL_GetTicks()) + ".png");
		}
	
		// Bildschirm Update
		SDL_RenderPresent(gRenderer);

		// Frame rate
		frameTime = SDL_GetTicks() - frameStart;
		if (frameDelay > frameTime) {
			SDL_Delay(frameDelay - frameTime);
		}
	}

	// Kehrschaufel
	SDL_DestroyRenderer(gRenderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
