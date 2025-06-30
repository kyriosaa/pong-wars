#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCREEN_ADDRESS 0x3C
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// for pong game
#define GRID_SIZE_X 32
#define GRID_SIZE_Y 16
#define CELL_SIZE 4
#define CIRCLE_RADIUS 2

// white = 0
// black = 1
int grid[GRID_SIZE_Y][GRID_SIZE_X];

// circle positions & velocities
float leftCircleX = 8.0f, leftCircleY = 8.0f;     
float leftVelX = 0.8f, leftVelY = -0.8f;
float rightCircleX = 24.0f, rightCircleY = 8.0f; 
float rightVelX = -0.8f, rightVelY = 0.8f;

unsigned long lastUpdate = 0;
const int UPDATE_INTERVAL = 16; // framerate basically (lower num = higher framerate) (its currently ~60FPS)

float RandomFloat() {
    return (float)random(0, 1000) / 1000.0f;
}

void InitializeGrid() {
  // i want the balls to be the same speed when the program starts
  float speed = 0.5f;
  
  // creates random angles for each circle using pi
  float pi = 3.14159f;
  float leftAngle = RandomFloat() * 2 * pi; 
  float rightAngle = RandomFloat() * 2 * pi;
  
  // calc velocities w same speed but different directions
  leftVelX = cos(leftAngle) * speed;
  leftVelY = sin(leftAngle) * speed;
  rightVelX = cos(rightAngle) * speed;
  rightVelY = sin(rightAngle) * speed;
  
  // create the grid
  int middleColumn = GRID_SIZE_X / 2;

  for (int i = 0; i < GRID_SIZE_Y; i++) {
      for (int j = 0; j < GRID_SIZE_X; j++) {
          // LEFT
          if (j < middleColumn) {
              grid[i][j] = 0; // white = 0
          // MIDDLE - half white, half black
          } else if (j == middleColumn) {
              if (i < GRID_SIZE_Y / 2) {
                  grid[i][j] = 0; // top half = white (0)
              } else {
                  grid[i][j] = 1; // bottom half = black (1)
              }
          // RIGHT
          } else {
              grid[i][j] = 1; // black = 1
          }
      }
  }
}

void UpdateCircles() {
    // THIS IS FOR THE LEFT HALF (WHITE CIRCLE)
    leftCircleX += leftVelX;
    leftCircleY += leftVelY;
    
    // handles the wall bounces
    if (leftCircleX <= 1 || leftCircleX >= GRID_SIZE_X - 1) {
        leftVelX = -leftVelX;

        // this stuff makes sure the balls stay in bounds
        if (leftCircleX <= 1) leftCircleX = 1;
        if (leftCircleX >= GRID_SIZE_X - 1) leftCircleX = GRID_SIZE_X - 1;
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
        float angle = atan2(leftVelY, leftVelX) + (RandomFloat() - 0.5f) * 0.2f;
        leftVelX = cos(angle) * speed;
        leftVelY = sin(angle) * speed;
    }
    if (leftCircleY <= 1 || leftCircleY >= GRID_SIZE_Y - 1) {
        leftVelY = -leftVelY;

        if (leftCircleY <= 1) leftCircleY = 1;
        if (leftCircleY >= GRID_SIZE_Y - 1) leftCircleY = GRID_SIZE_Y - 1;
        
        float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
        float angle = atan2(leftVelY, leftVelX) + (RandomFloat() - 0.5f) * 0.2f;
        leftVelX = cos(angle) * speed;
        leftVelY = sin(angle) * speed;
    }
    
    // checks collision with grid cells
    int gridX = (int)round(leftCircleX);
    int gridY = (int)round(leftCircleY);
    if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
        if (grid[gridY][gridX] == 1) { // if a black cell is hit...
            grid[gridY][gridX] = 0; // turn it white

            // same angle change code
            float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
            float angle = atan2(-leftVelY, -leftVelX) + (RandomFloat() - 0.5f) * 0.3f;
            leftVelX = cos(angle) * speed;
            leftVelY = sin(angle) * speed;
        }
    }
    // END LEFT HALF
    
    // THIS IS FOR THE RIGHT HALF (BLACK CIRCLE)
    rightCircleX += rightVelX;
    rightCircleY += rightVelY;
    
    // handles the wall bounces
    if (rightCircleX <= 1 || rightCircleX >= GRID_SIZE_X - 1) {
        rightVelX = -rightVelX;
        // this stuff makes sure the balls stay in bounds
        if (rightCircleX <= 1) rightCircleX = 1;
        if (rightCircleX >= GRID_SIZE_X - 1) rightCircleX = GRID_SIZE_X - 1;
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
        float angle = atan2(rightVelY, rightVelX) + (RandomFloat() - 0.5f) * 0.2f;
        rightVelX = cos(angle) * speed;
        rightVelY = sin(angle) * speed;
    }
    if (rightCircleY <= 1 || rightCircleY >= GRID_SIZE_Y - 1) {
        rightVelY = -rightVelY;
       
        if (rightCircleY <= 1) rightCircleY = 1;
        if (rightCircleY >= GRID_SIZE_Y - 1) rightCircleY = GRID_SIZE_Y - 1;
        
        float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
        float angle = atan2(rightVelY, rightVelX) + (RandomFloat() - 0.5f) * 0.2f;
        rightVelX = cos(angle) * speed;
        rightVelY = sin(angle) * speed;
    }
    
    // checks collision with grid cells
    gridX = (int)rightCircleX;
    gridY = (int)rightCircleY;
    if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
        if (grid[gridY][gridX] == 0) { // if a white cell is hit...
            grid[gridY][gridX] = 1; // turn it black
            
            // same angle change code
            float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
            float angle = atan2(-rightVelY, -rightVelX) + (RandomFloat() - 0.5f) * 0.3f;
            rightVelX = cos(angle) * speed;
            rightVelY = sin(angle) * speed;
        }
    }
    // END RIGHT HALF
}

void DrawGame() {
    display.clearDisplay();
    
    // draw the grid
    for (int i = 0; i < GRID_SIZE_Y; i++) {
        for (int j = 0; j < GRID_SIZE_X; j++) {
            int x = j * CELL_SIZE;
            int y = i * CELL_SIZE;
            
            if (grid[i][j] == 0) {
                // white cells (day) - draw white rectangles
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
            } else {
                // black cells (night) - draw black rectangles
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, SSD1306_BLACK);
            }
        }
    }
    
    // draw circles
    int leftX = (int)(leftCircleX * CELL_SIZE);
    int leftY = (int)(leftCircleY * CELL_SIZE);
    int rightX = (int)(rightCircleX * CELL_SIZE);
    int rightY = (int)(rightCircleY * CELL_SIZE);
    
    // left circle (black on white background)
    display.fillCircle(leftX, leftY, CIRCLE_RADIUS, SSD1306_BLACK);
    
    // right circle (white on black background)
    display.fillCircle(rightX, rightY, CIRCLE_RADIUS, SSD1306_WHITE);
    
    display.display();
}

void setup() {
    Serial.begin(115200);
    
    // init random seed
    randomSeed(analogRead(0));
    
    // init display
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("Pong Wars"));
    display.println(F("Starting..."));
    display.display();
    delay(2000);
    
    InitializeGrid();
    lastUpdate = millis();
}

void loop() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastUpdate >= UPDATE_INTERVAL) {
        UpdateCircles();
        DrawGame();
        lastUpdate = currentTime;
    }
}