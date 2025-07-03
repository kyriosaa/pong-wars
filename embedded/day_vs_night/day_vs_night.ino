// For ESP32

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// component defines
#define BUTTON_PIN      5       // D5
#define BUZZER_PIN      2       // D2 - PWM
#define BUZZER_DAY      1250    // Hz noise of day circle when it hits a cell
#define BUZZER_NIGHT    750     // Hz noise of night circle when it hits a cell
#define POT_DAY_PIN     32      // ADC1_CH5
#define POT_NIGHT_PIN   33      // ADC1_CH4
#define POT_THRESHOLD   2       // min change to update circle angle
#define POT_MIN_ANGLE   0       
#define POT_MAX_ANGLE   157     // maps to π radians
#define POT_DIVISOR     100.0f  // the thing dividing the max angle
#define ADC_MIN         0       // Analog2Digital Converter min reading
#define ADC_MAX         1023    // Analog2Digital Converter max reading

// screen defines
#define SCREEN_WIDTH    128     // OLED screen length
#define SCREEN_HEIGHT   64      // OLED screen height
#define SCREEN_ADDRESS  0x3C    // A4->DATA | A5->CLOCK
#define OLED_RESET      -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// game defines
#define PI              3.1415f // π π π pi π π π
#define GRID_SIZE_X     32      // overall length of grid (128x64 OLED -> 32x16 grid)
#define GRID_SIZE_Y     16      // overall height of grid (128x64 OLED -> 32x16 grid)
#define CELL_SIZE       4       // size of grid cells
#define CIRCLE_RADIUS   2       // radius of circle
#define CIRCLE_SPEED    0.5f    // speed of circle
#define ANGLE_RAND      0.2f    // rand for wall bounces
#define COLLISION_RAND  0.3f    // rand for cell collisions
#define AIM_LINE_LENGTH 15      // length of the aim line
#define CIRCLE_BOUND    1       // distance from walls
#define COLOR_WHITE     SSD1306_WHITE
#define COLOR_BLACK     SSD1306_BLACK

// seed defines
#define RAND_MULTIPLIER 1000    // for the RandomFloat() func
#define RAND_DIVISOR    1000.0f


// white = 0
// black = 1
int grid[GRID_SIZE_Y][GRID_SIZE_X];

// circle positions & velocities
float leftCircleX = 8.0f, leftCircleY = 8.0f;     
float leftVelX = 0.8f, leftVelY = -0.8f;
float rightCircleX = 24.0f, rightCircleY = 8.0f;  
float rightVelX = -0.8f, rightVelY = 0.8f;
float dayCircleAngle = 0.0f;
float nightCircleAngle = 0.0f;
bool dayAngleChanged = false;
bool nightAngleChanged = false;

// game state
bool gameRunning = false;
bool lastButtonState = false;
unsigned long lastPressTime = 0;
const int buttonDebounceDelay = 200;

// counters
int dayCount = 0;
int nightCount = 0;

// potentiometer
int lastDayPotValue = 0;
int lastNightPotValue = 0;

unsigned long lastUpdate = 0;
const int UPDATE_INTERVAL = 16; // framerate basically (lower num = higher framerate) (its currently ~60FPS)

float RandomFloat() {
    return (float)random(0, RAND_MULTIPLIER) / RAND_DIVISOR;
}

void InitializeGrid() {
  // i want the balls to be the same speed when the program starts
  float speed = CIRCLE_SPEED;
  
  // creates random angles for each circle using pi
  float leftAngle = RandomFloat() * 2 * PI; 
  float rightAngle = RandomFloat() * 2 * PI;
  
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
          // RIGHT
          } else {
              grid[i][j] = 1; // black = 1
          }
      }
  }
}

void UpdateCircles() {
    // THIS IS FOR THE DAY HALF (WHITE CIRCLE)
    if(dayAngleChanged) {
        float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
        leftVelX = cos(dayCircleAngle) * speed;
        leftVelY = sin(dayCircleAngle) * speed;
        dayAngleChanged = false;
    }
    leftCircleX += leftVelX;
    leftCircleY += leftVelY;
    
    // handles the wall bounces
    if (leftCircleX <= CIRCLE_BOUND || leftCircleX >= GRID_SIZE_X - CIRCLE_BOUND) {
        leftVelX = -leftVelX;

        // this stuff makes sure the balls stay in bounds
        if (leftCircleX <= CIRCLE_BOUND) leftCircleX = CIRCLE_BOUND;
        if (leftCircleX >= GRID_SIZE_X - CIRCLE_BOUND) leftCircleX = GRID_SIZE_X - CIRCLE_BOUND;
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
        float angle = atan2(leftVelY, leftVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
        leftVelX = cos(angle) * speed;
        leftVelY = sin(angle) * speed;
    }
    if (leftCircleY <= CIRCLE_BOUND || leftCircleY >= GRID_SIZE_Y - CIRCLE_BOUND) {
        leftVelY = -leftVelY;

        if (leftCircleY <= CIRCLE_BOUND) leftCircleY = CIRCLE_BOUND;
        if (leftCircleY >= GRID_SIZE_Y - CIRCLE_BOUND) leftCircleY = GRID_SIZE_Y - CIRCLE_BOUND;
        
        float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
        float angle = atan2(leftVelY, leftVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
        leftVelX = cos(angle) * speed;
        leftVelY = sin(angle) * speed;
    }
    
    // checks collision with grid cells
    int gridX = (int)round(leftCircleX);
    int gridY = (int)round(leftCircleY);
    if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
        if (grid[gridY][gridX] == 1) { // if a black cell is hit...
            grid[gridY][gridX] = 0; // turn it white
            tone(BUZZER_PIN, BUZZER_DAY);
            delay(10);
            tone(BUZZER_PIN, 0);

            // same angle change code
            float speed = sqrt(leftVelX * leftVelX + leftVelY * leftVelY);
            float angle = atan2(-leftVelY, -leftVelX) + (RandomFloat() - 0.5f) * COLLISION_RAND;
            leftVelX = cos(angle) * speed;
            leftVelY = sin(angle) * speed;
        }
    }
    // END DAY HALF
    
    // THIS IS FOR THE NIGHT HALF (BLACK CIRCLE)
    if(nightAngleChanged) {
        float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
        rightVelX = cos(nightCircleAngle) * speed;
        rightVelY = sin(nightCircleAngle) * speed;
        nightAngleChanged = false;
    }
    rightCircleX += rightVelX;
    rightCircleY += rightVelY;
    
    // handles the wall bounces
    if (rightCircleX <= CIRCLE_BOUND || rightCircleX >= GRID_SIZE_X - CIRCLE_BOUND) {
        rightVelX = -rightVelX;
        // this stuff makes sure the balls stay in bounds
        if (rightCircleX <= CIRCLE_BOUND) rightCircleX = CIRCLE_BOUND;
        if (rightCircleX >= GRID_SIZE_X - CIRCLE_BOUND) rightCircleX = GRID_SIZE_X - CIRCLE_BOUND;
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
        float angle = atan2(rightVelY, rightVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
        rightVelX = cos(angle) * speed;
        rightVelY = sin(angle) * speed;
    }
    if (rightCircleY <= CIRCLE_BOUND || rightCircleY >= GRID_SIZE_Y - CIRCLE_BOUND) {
        rightVelY = -rightVelY;
       
        if (rightCircleY <= CIRCLE_BOUND) rightCircleY = CIRCLE_BOUND;
        if (rightCircleY >= GRID_SIZE_Y - CIRCLE_BOUND) rightCircleY = GRID_SIZE_Y - CIRCLE_BOUND;
        
        float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
        float angle = atan2(rightVelY, rightVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
        rightVelX = cos(angle) * speed;
        rightVelY = sin(angle) * speed;
    }
    
    // checks collision with grid cells
    gridX = (int)round(rightCircleX);  
    gridY = (int)round(rightCircleY);
    if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
        if (grid[gridY][gridX] == 0) { // if a white cell is hit...
            grid[gridY][gridX] = 1; // turn it black
            tone(BUZZER_PIN, BUZZER_NIGHT);
            delay(10);
            tone(BUZZER_PIN, 0);
            
            // same angle change code
            float speed = sqrt(rightVelX * rightVelX + rightVelY * rightVelY);
            float angle = atan2(-rightVelY, -rightVelX) + (RandomFloat() - 0.5f) * COLLISION_RAND;
            rightVelX = cos(angle) * speed;
            rightVelY = sin(angle) * speed;
        }
    }
    // END NIGHT HALF
}

// counts the squares for the counters
void CountSquares(int* dayCount, int* nightCount) {
    *dayCount = 0;
    *nightCount = 0;
    
    for (int i = 0; i < GRID_SIZE_Y; i++) {
        for (int j = 0; j < GRID_SIZE_X; j++) {
            if (grid[i][j] == 0) {
                (*dayCount)++;
            } else {
                (*nightCount)++;
            }
        }
    }
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
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, COLOR_WHITE);
            } else {
                // black cells (night) - draw black rectangles
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, COLOR_BLACK);
            }
        }
    }
    
    // draw circles
    int leftX = (int)(leftCircleX * CELL_SIZE);
    int leftY = (int)(leftCircleY * CELL_SIZE);
    int rightX = (int)(rightCircleX * CELL_SIZE);
    int rightY = (int)(rightCircleY * CELL_SIZE);
    
    // left circle (black on white background)
    display.fillCircle(leftX, leftY, CIRCLE_RADIUS, COLOR_BLACK);
    
    // right circle (white on black background)
    display.fillCircle(rightX, rightY, CIRCLE_RADIUS, COLOR_WHITE);

    // draw angle line
    if(!gameRunning) {
        // DAY
        int dayEndX = leftX + (int)(cos(dayCircleAngle) * AIM_LINE_LENGTH);
        int dayEndY = leftY + (int)(sin(dayCircleAngle) * AIM_LINE_LENGTH);
        display.drawLine(leftX, leftY, dayEndX, dayEndY, COLOR_BLACK);

        // NIGHT
        int nightEndX = rightX + (int)(cos(nightCircleAngle) * AIM_LINE_LENGTH);
        int nightEndY = rightY + (int)(sin(nightCircleAngle) * AIM_LINE_LENGTH);
        display.drawLine(rightX, rightY, nightEndX, nightEndY, COLOR_WHITE);
    }
}

void DrawNames() {
  // tweaks the position of the names
  int dayPosX = 12;
  int dayPosY = 117;
  int nightPosX = 5;
  int nightPosY = 120;

  display.setTextSize(1);

  display.setRotation(1); // 90 deg
  display.setCursor(dayPosX, dayPosY);
  display.setTextColor(COLOR_BLACK);
  display.print(F("Day:"));
  display.println(dayCount);

  display.setRotation(3); // 270 deg
  display.setCursor(nightPosX, nightPosY);
  display.setTextColor(COLOR_WHITE);
  display.print(F("Night:"));
  display.println(nightCount);

  display.setRotation(0); // reset
}

// use potentiometer to change angle of circles
void ReadPotentiometer() {
    if(!gameRunning) {
        // DAY
        int dayPotValue = analogRead(POT_DAY_PIN);
        // only update if there's a significant change
        if(abs(dayPotValue - lastDayPotValue) > POT_THRESHOLD) {
            // map the potentiometer value (0-1023) to angle (0 to π)
            float unmappedDayCircleAngle = map(dayPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
            dayCircleAngle = PI - unmappedDayCircleAngle;   // so the aim is flipped horizontally
            lastDayPotValue = dayPotValue;
        }
        // NIGHT
        int nightPotValue = analogRead(POT_NIGHT_PIN);
        // only update if there's a significant change
        if(abs(nightPotValue - lastNightPotValue) > POT_THRESHOLD) {
            // map the potentiometer value (0-1023) to angle (0 to π)
            nightCircleAngle = map(nightPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
            lastNightPotValue = nightPotValue;
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // init random seed
    randomSeed(analogRead(0));
    
    // init display
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    // init potentiometer
    lastDayPotValue = analogRead(POT_DAY_PIN);
    lastNightPotValue = analogRead(POT_NIGHT_PIN);
    dayCircleAngle = 0.0f;
    nightCircleAngle = 0.0f;
    
    // boot screen
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(COLOR_WHITE);
    display.setCursor(0,17);
    display.println(F("Pong"));
    display.println(F("Wars"));
    display.display();
    delay(2000);
    
    InitializeGrid();
    lastUpdate = millis();
}

void loop() {
    ReadPotentiometer();

    bool currentButtonState = digitalRead(BUTTON_PIN) == LOW;
    if(currentButtonState && !lastButtonState && millis() - lastPressTime > buttonDebounceDelay) {
        lastPressTime = millis();
        gameRunning = !gameRunning;

        if(gameRunning) {
            dayAngleChanged = true;
            nightAngleChanged = true;
        }
    }
    
    lastButtonState = currentButtonState;

    unsigned long currentTime = millis();
    if (currentTime - lastUpdate >= UPDATE_INTERVAL) {
        if(gameRunning) {
            UpdateCircles();
        }
        CountSquares(&dayCount, &nightCount);
        DrawGame();
        DrawNames();
        display.display();
        lastUpdate = currentTime;
    }
}