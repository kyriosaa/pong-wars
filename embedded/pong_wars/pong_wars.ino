// For ESP32

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// component defines
#define BTN_DAY_PIN     5       // D5
#define BTN_NIGHT_PIN   18      // D18
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
#define CIRCLE_BOUND    0       // distance from walls
#define ANGLE_RAND      0.0f    // rand for wall bounces
#define COLLISION_RAND  0.3f    // rand for cell collisions
#define COLLISION_CD    100     // circle collision cooldown
#define AIM_LINE_LENGTH 15      // length of the aim line
#define PADDLE_WIDTH    1       // width of paddle
#define PADDLE_HEIGHT   4       // height of paddle
#define PADDLE_OFFSET   3       // distance from left/right edge (depends on side)
#define PADDLE_SENS     350.0f  // keeps the paddle from going off screen by slowing it down (idk if this changes depending on the screen size)
#define PADDLE_MIN_BND  0.0f    // min bound for paddle (edges of screen)
#define COLOR_WHITE     SSD1306_WHITE
#define COLOR_BLACK     SSD1306_BLACK

// seed defines
#define RAND_MULTIPLIER 1000    // for the RandomFloat() func
#define RAND_DIVISOR    1000.0f


// white = 0
// black = 1
int grid[GRID_SIZE_Y][GRID_SIZE_X];

// circle positions & velocities
float dayCircleStartX = 8.0f, dayCircleStartY = 8.0f;       // starting location for day circle
float dayCircleVelX = 0.8f, dayCircleVelY = -0.8f;
float nightCircleStartX = 24.0f, nightCircleStartY = 8.0f;  // starting location for night circle
float nightCircleVelX = -0.8f, nightCircleVelY = 0.8f;
float dayCircleAngle = 0.0f;
float nightCircleAngle = 0.0f;
bool dayAngleChanged = false;
bool nightAngleChanged = false;

// paddle positions
float dayPaddleY = 6.0f;
float nightPaddleY = 6.0f;

// game state
bool gameRunning = false;                   // tracks whether the game has started (for sync start)
bool lastDayButtonState = false;            // tracks button input management
bool lastNightButtonState = false;  
bool dayCircleActive = false;               // circle states
bool nightCircleActive = false;
const int buttonDebounceDelay = 200;        // debounce delay
unsigned long lastDayButtonPressTime = 0;   // button debounce timing
unsigned long lastNightButtonPressTime = 0;
unsigned long dayCircleCooldownEnd = 0;     // cooldown timer for grid cell changes
unsigned long nightCircleCooldownEnd = 0;  

// counters
int dayCount = 0;       // counts how many grid cells is white
int nightCount = 0;     // counts how many grid cells is black

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
  dayCircleVelX = cos(leftAngle) * speed;
  dayCircleVelY = sin(leftAngle) * speed;
  nightCircleVelX = cos(rightAngle) * speed;
  nightCircleVelY = sin(rightAngle) * speed;
  
  // create the grid
  int middleColumn = GRID_SIZE_X / 2;

  for(int i = 0; i < GRID_SIZE_Y; i++) {
      for(int j = 0; j < GRID_SIZE_X; j++) {
          // LEFT
          if(j < middleColumn) {
              grid[i][j] = 0; // white = 0
          // RIGHT
          } else {
              grid[i][j] = 1; // black = 1
          }
      }
  }
}

void UpdateCircles() {
    // only update the circles if the game has started
    if(!gameRunning) {
        return;
    }

    // THIS IS FOR THE DAY HALF (WHITE CIRCLE)
    if(dayAngleChanged) {
        float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
        dayCircleVelX = cos(dayCircleAngle) * speed;
        dayCircleVelY = sin(dayCircleAngle) * speed;
        dayAngleChanged = false;
    }

    if(dayCircleActive) {
        dayCircleStartX += dayCircleVelX;
        dayCircleStartY += dayCircleVelY;

        bool shouldDayBounce = false;
        float dayCircleRadius = 0.5f;

        // check the cells that the circle might be touching
        int dayMinX = (int)floor(dayCircleStartX - dayCircleRadius);
        int dayMaxX = (int)ceil(dayCircleStartX + dayCircleRadius);
        int dayMinY = (int)floor(dayCircleStartY - dayCircleRadius);
        int dayMaxY = (int)ceil(dayCircleStartY + dayCircleRadius);

        for(int checkY = dayMinY; checkY <= dayMaxY; checkY++) {
            for(int checkX = dayMinX; checkX <= dayMaxX; checkX++) {
                if(checkX >= 0 && checkX < GRID_SIZE_X && checkY >= 0 && checkY < GRID_SIZE_Y) {
                    if(grid[checkY][checkX] == 1) { // if a black cell is hit...
                        float cellCenterX = checkX + 0.5f;
                        float cellCenterY = checkY + 0.5f;
                        float deltaX = dayCircleStartX - cellCenterX;
                        float deltaY = dayCircleStartY - cellCenterY;
                        float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
                        
                        // if circle is close enough to cell, then its a collision
                        if(distance < dayCircleRadius + 0.5f) { 
                            shouldDayBounce = true;
                            
                            if(millis() > dayCircleCooldownEnd) {
                                grid[checkY][checkX] = 0; // ...turn it white
                                tone(BUZZER_PIN, BUZZER_DAY);
                                delay(10);
                                tone(BUZZER_PIN, 0);
                                dayCircleCooldownEnd = millis() + COLLISION_CD;
                            }
                            
                            // bounce away from cell
                            if(abs(deltaX) > abs(deltaY)) {
                                dayCircleVelX = -abs(dayCircleVelX); 
                            } else {
                                dayCircleVelY = -dayCircleVelY; 
                            }
                            
                            // push circle away from cell
                            if(deltaX != 0 || deltaY != 0) {
                                float pushDistance = (dayCircleRadius + 0.5f) - distance + 0.1f;
                                float pushX = (deltaX / distance) * pushDistance;
                                float pushY = (deltaY / distance) * pushDistance;
                                dayCircleStartX += pushX;
                                dayCircleStartY += pushY;
                            }
                            
                            goto dayCollisionDone;
                        }
                    }
                }
            }
        }
        dayCollisionDone:

        // handles day paddle bounces
        float dayPaddleX = PADDLE_OFFSET;
        if(dayCircleStartX >= dayPaddleX && dayCircleStartX <= dayPaddleX + PADDLE_WIDTH) {
            if(dayCircleStartY >= dayPaddleY && dayCircleStartY <= dayPaddleY + PADDLE_HEIGHT) {
                // bounce back
                dayCircleVelX = abs(dayCircleVelX);
                // angle variation for chaos otherwise its gonna be boring
                float hitPosition = (dayCircleStartY - dayPaddleY) / PADDLE_HEIGHT;
                float angleVariation = (hitPosition - 0.5f) * 0.5f;

                float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
                float currentAngle = atan2(dayCircleVelY, dayCircleVelX);
                float newAngle = currentAngle + angleVariation;
                dayCircleVelX = cos(newAngle) * speed;
                dayCircleVelY = sin(newAngle) * speed;
                
                // move circle away from paddle so it doesnt stick
                dayCircleStartX = dayPaddleX + PADDLE_WIDTH + 0.1f;
            }
        }
        
        // handles the wall bounces
        bool dayBounced = false;
        if(dayCircleStartX <= CIRCLE_BOUND || dayCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) {
            // vertical wall collision
            dayCircleVelX = -dayCircleVelX;
            dayBounced = true;

            // this stuff makes sure the balls stay in bounds
            if(dayCircleStartX <= CIRCLE_BOUND) dayCircleStartX = CIRCLE_BOUND;
            if(dayCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) dayCircleStartX = GRID_SIZE_X - CIRCLE_BOUND;
        }
        if(dayCircleStartY <= CIRCLE_BOUND || dayCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) {
            // horizontal wall collision
            dayCircleVelY = -dayCircleVelY;
            dayBounced = true;

            if(dayCircleStartY <= CIRCLE_BOUND) dayCircleStartY = CIRCLE_BOUND;
            if(dayCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) dayCircleStartY = GRID_SIZE_Y - CIRCLE_BOUND;
        }
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        if(dayBounced && ANGLE_RAND > 0) {
            float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
            float currentAngle = atan2(dayCircleVelY, dayCircleVelX);
            float newAngle = currentAngle + (RandomFloat() - 0.5f) * ANGLE_RAND;
            dayCircleVelX = cos(newAngle) * speed;
            dayCircleVelY = sin(newAngle) * speed;
        }
    }
    // END DAY HALF
    
    // THIS IS FOR THE NIGHT HALF (BLACK CIRCLE
    if(nightAngleChanged) {
        float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
        nightCircleVelX = cos(nightCircleAngle) * speed;
        nightCircleVelY = sin(nightCircleAngle) * speed;
        nightAngleChanged = false;
    }

    if(nightCircleActive) {
        nightCircleStartX += nightCircleVelX;
        nightCircleStartY += nightCircleVelY;

        bool shouldNightBounce = false;
        float nightCircleRadius = 0.5f;

        // check the cells that the circle might be touching
        int nightMinX = (int)floor(nightCircleStartX - nightCircleRadius);
        int nightMaxX = (int)ceil(nightCircleStartX + nightCircleRadius);
        int nightMinY = (int)floor(nightCircleStartY - nightCircleRadius);
        int nightMaxY = (int)ceil(nightCircleStartY + nightCircleRadius);

        for(int checkY = nightMinY; checkY <= nightMaxY; checkY++) {
            for(int checkX = nightMinX; checkX <= nightMaxX; checkX++) {
                if(checkX >= 0 && checkX < GRID_SIZE_X && checkY >= 0 && checkY < GRID_SIZE_Y) {
                    if(grid[checkY][checkX] == 0) { // if a white cell is hit...
                        // Calculate distance from circle center to cell center
                        float cellCenterX = checkX + 0.5f;
                        float cellCenterY = checkY + 0.5f;
                        float deltaX = nightCircleStartX - cellCenterX;
                        float deltaY = nightCircleStartY - cellCenterY;
                        float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
                        
                        // if circle is close enough to cell, then its a collision
                        if(distance < nightCircleRadius + 0.5f) { 
                            shouldNightBounce = true;
                            
                            if(millis() > nightCircleCooldownEnd) {
                                grid[checkY][checkX] = 1; // ...turn it black
                                tone(BUZZER_PIN, BUZZER_NIGHT);
                                delay(10);
                                tone(BUZZER_PIN, 0);
                                nightCircleCooldownEnd = millis() + COLLISION_CD;
                            }
                            
                            // Bounce away from the cell
                            if(abs(deltaX) > abs(deltaY)) {
                                nightCircleVelX = abs(nightCircleVelX); 
                            } else {
                                nightCircleVelY = -nightCircleVelY; 
                            }
                            
                            // Push circle away from cell
                            if(deltaX != 0 || deltaY != 0) {
                                float pushDistance = (nightCircleRadius + 0.5f) - distance + 0.1f;
                                float pushX = (deltaX / distance) * pushDistance;
                                float pushY = (deltaY / distance) * pushDistance;
                                nightCircleStartX += pushX;
                                nightCircleStartY += pushY;
                            }
                            
                            goto nightCollisionDone; 
                        }
                    }
                }
            }
        }
        nightCollisionDone:

        // handles night paddle bounces
        float nightPaddleX = GRID_SIZE_X - PADDLE_OFFSET - PADDLE_WIDTH;
        if (nightCircleStartX >= nightPaddleX && nightCircleStartX <= nightPaddleX + PADDLE_WIDTH) {
            if (nightCircleStartY >= nightPaddleY && nightCircleStartY <= nightPaddleY + PADDLE_HEIGHT) {
                // bounce back
                nightCircleVelX = -abs(nightCircleVelX); 
                
                // angle variation for chaos or else its boring
                float hitPosition = (nightCircleStartY - nightPaddleY) / PADDLE_HEIGHT;
                float angleVariation = (hitPosition - 0.5f) * 0.5f; 
                
                float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
                float currentAngle = atan2(nightCircleVelY, nightCircleVelX);
                float newAngle = currentAngle + angleVariation;
                nightCircleVelX = cos(newAngle) * speed;
                nightCircleVelY = sin(newAngle) * speed;
                
                // move circle away from paddle so it doesnt stick
                nightCircleStartX = nightPaddleX - 0.1f;
            }
        }
        
        // handles the wall bounces
        bool nightBounced = false;
        if(nightCircleStartX <= CIRCLE_BOUND || nightCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) {
            // vertical wall collision
            nightCircleVelX = -nightCircleVelX;
            nightBounced = true;
            
            // this stuff makes sure the balls stay in bounds
            if(nightCircleStartX <= CIRCLE_BOUND) nightCircleStartX = CIRCLE_BOUND;
            if(nightCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) nightCircleStartX = GRID_SIZE_X - CIRCLE_BOUND;
        }
        if(nightCircleStartY <= CIRCLE_BOUND || nightCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) {
            // horizontal wall collision
            nightCircleVelY = -nightCircleVelY;
            nightBounced = true;
        
            if(nightCircleStartY <= CIRCLE_BOUND) nightCircleStartY = CIRCLE_BOUND;
            if(nightCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) nightCircleStartY = GRID_SIZE_Y - CIRCLE_BOUND;
        }
        
        // everytime the balls bounce it changes the angle a bit
        // bcs otherwise the balls would draw an identical path
        if(nightBounced && ANGLE_RAND > 0) {
            float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
            float currentAngle = atan2(nightCircleVelY, nightCircleVelX);
            float newAngle = currentAngle + (RandomFloat() - 0.5f) * ANGLE_RAND;
            nightCircleVelX = cos(newAngle) * speed;
            nightCircleVelY = sin(newAngle) * speed;
        }
    }
    // END NIGHT HALF
}

// counts the squares for the counters
void CountSquares(int* dayCount, int* nightCount) {
    *dayCount = 0;
    *nightCount = 0;
    
    for(int i = 0; i < GRID_SIZE_Y; i++) {
        for(int j = 0; j < GRID_SIZE_X; j++) {
            if(grid[i][j] == 0) {
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
    for(int i = 0; i < GRID_SIZE_Y; i++) {
        for (int j = 0; j < GRID_SIZE_X; j++) {
            int x = j * CELL_SIZE;
            int y = i * CELL_SIZE;
            
            if(grid[i][j] == 0) {
                // white cells (day) - draw white rectangles
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, COLOR_WHITE);
            } else {
                // black cells (night) - draw black rectangles
                display.fillRect(x, y, CELL_SIZE, CELL_SIZE, COLOR_BLACK);
            }
        }
    }

    // draw paddles
    if(gameRunning) {
        // DAY PADDLE
        int dayPaddleX = PADDLE_OFFSET * CELL_SIZE;
        int dayPaddleYPixel = (int)(dayPaddleY * CELL_SIZE);
        display.fillRect(dayPaddleX, dayPaddleYPixel, PADDLE_WIDTH * CELL_SIZE, PADDLE_HEIGHT * CELL_SIZE, COLOR_BLACK);
        // NIGHT PADDLE
        int nightPaddleX = (GRID_SIZE_X - PADDLE_OFFSET - PADDLE_WIDTH) * CELL_SIZE;
        int nightPaddleYPixel = (int)(nightPaddleY * CELL_SIZE);
        display.fillRect(nightPaddleX, nightPaddleYPixel, PADDLE_WIDTH * CELL_SIZE, PADDLE_HEIGHT * CELL_SIZE, COLOR_WHITE);
    }
    
    // draw circles
    int leftX = (int)(dayCircleStartX * CELL_SIZE);
    int leftY = (int)(dayCircleStartY * CELL_SIZE);
    int rightX = (int)(nightCircleStartX * CELL_SIZE);
    int rightY = (int)(nightCircleStartY * CELL_SIZE);
    
    // left circle (black on white background)
    display.fillCircle(leftX, leftY, CIRCLE_RADIUS, COLOR_BLACK);
    
    // right circle (white on black background)
    display.fillCircle(rightX, rightY, CIRCLE_RADIUS, COLOR_WHITE);

    // draw angle lines
    // DAY ANGLE LINE
    if(!gameRunning || !dayCircleActive) {
        int dayEndX = leftX + (int)(cos(dayCircleAngle) * AIM_LINE_LENGTH);
        int dayEndY = leftY + (int)(sin(dayCircleAngle) * AIM_LINE_LENGTH);
        display.drawLine(leftX, leftY, dayEndX, dayEndY, COLOR_BLACK);
    }
    // NIGHT ANGLE LINE
    if(!gameRunning || !nightCircleActive) {
        int nightEndX = rightX + (int)(cos(nightCircleAngle) * AIM_LINE_LENGTH);
        int nightEndY = rightY + (int)(sin(nightCircleAngle) * AIM_LINE_LENGTH);
        display.drawLine(rightX, rightY, nightEndX, nightEndY, COLOR_WHITE);
    }
}

void DrawNames() {
  // tweaks the position of the names
  int dayPosX = 12;
  int dayPosY = 119;
  int nightPosX = 5;
  int nightPosY = 119;

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

// potentiometer control (angle & paddle)
void ReadPotentiometer() {
    if(!gameRunning) {
        // use potentiometer to change angle of circles at the beginning of the game
        // DAY ANGLE LINE
        int dayPotValue = analogRead(POT_DAY_PIN);
        // only update if there's a significant change
        if(abs(dayPotValue - lastDayPotValue) > POT_THRESHOLD) {
            // map the potentiometer value (0-1023) to angle (0 to π)
            float unmappedDayCircleAngle = map(dayPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
            dayCircleAngle = PI - unmappedDayCircleAngle;   // so the aim is flipped horizontally
            lastDayPotValue = dayPotValue;
        }
        // NIGHT ANGLE LINE
        int nightPotValue = analogRead(POT_NIGHT_PIN);
        // only update if there's a significant change
        if(abs(nightPotValue - lastNightPotValue) > POT_THRESHOLD) {
            // map the potentiometer value (0-1023) to angle (0 to π)
            nightCircleAngle = map(nightPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
            lastNightPotValue = nightPotValue;
        }
    } else {
        // use potentiometer to move the paddles when the game starts
        // DAY PADDLE
        int dayPotValue = analogRead(POT_DAY_PIN);
        // only update if there's a significant change
        if(abs(dayPotValue - lastDayPotValue) > POT_THRESHOLD) {
            if(!dayCircleActive) {
                // if the circle isnt active then control both the aim and paddle
                float unmappedDayCircleAngle = map(dayPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
                dayCircleAngle = PI - unmappedDayCircleAngle;

                float mappedValue = map(dayPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
                dayPaddleY = mappedValue / PADDLE_SENS;
                dayPaddleY = constrain(dayPaddleY, PADDLE_MIN_BND, GRID_SIZE_Y - PADDLE_HEIGHT - PADDLE_MIN_BND);
                lastDayPotValue = dayPotValue;
            } else {
                float mappedValue = map(dayPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
                dayPaddleY = mappedValue / PADDLE_SENS;
                dayPaddleY = constrain(dayPaddleY, PADDLE_MIN_BND, GRID_SIZE_Y - PADDLE_HEIGHT - PADDLE_MIN_BND);
            }
            lastDayPotValue = dayPotValue;
        }
        // NIGHT PADDLE
        int nightPotValue = analogRead(POT_NIGHT_PIN);
        if(abs(nightPotValue - lastNightPotValue) > POT_THRESHOLD) {
            if(!nightCircleActive) {
                // if the circle isnt active then control both the aim and paddle
                nightCircleAngle = map(nightPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;

                float mappedValue = map(nightPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
                nightPaddleY = mappedValue / PADDLE_SENS;
                nightPaddleY = constrain(nightPaddleY, PADDLE_MIN_BND, GRID_SIZE_Y - PADDLE_HEIGHT - PADDLE_MIN_BND);
                lastNightPotValue = nightPotValue;
            } else {
                float mappedValue = map(nightPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
                nightPaddleY = mappedValue / PADDLE_SENS;
                nightPaddleY = constrain(nightPaddleY, PADDLE_MIN_BND, GRID_SIZE_Y - PADDLE_HEIGHT - PADDLE_MIN_BND);
            }
            lastNightPotValue = nightPotValue;
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN_DAY_PIN, INPUT_PULLUP);
    pinMode(BTN_NIGHT_PIN, INPUT_PULLUP);
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

    // calculate initial aim line angles by looking at the potentiometer pos
    float unmappedDayCircleAngle = map(lastDayPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
    dayCircleAngle = PI - unmappedDayCircleAngle;
    nightCircleAngle = map(lastNightPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
    
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

    // handle day button
    bool currentDayButtonState = digitalRead(BTN_DAY_PIN) == LOW;
    if(currentDayButtonState && !lastDayButtonState && millis() - lastDayButtonPressTime > buttonDebounceDelay) {
        lastDayButtonPressTime = millis();
        
        if(!gameRunning) {
            // start the game
            gameRunning = true;
            dayCircleActive = true;
            nightCircleActive = true;
            dayAngleChanged = true;
            nightAngleChanged = true;

            // init paddle pos with current potentiometer angles
            int dayPotValue = analogRead(POT_DAY_PIN);
            float dayMappedValue = map(dayPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
            dayPaddleY = dayMappedValue / PADDLE_SENS;
            dayPaddleY = constrain(dayPaddleY, 1.0f, GRID_SIZE_Y - PADDLE_HEIGHT - 1.0f);
            
            int nightPotValue = analogRead(POT_NIGHT_PIN);
            float nightMappedValue = map(nightPotValue, ADC_MIN, ADC_MAX, 100, (GRID_SIZE_Y - PADDLE_HEIGHT - 1) * 100);
            nightPaddleY = nightMappedValue / PADDLE_SENS;
            nightPaddleY = constrain(nightPaddleY, 1.0f, GRID_SIZE_Y - PADDLE_HEIGHT - 1.0f);
        } else {
            if(dayCircleActive) {
                // reset day circle
                dayCircleActive = false;
                dayCircleStartX = 8.0f;
                dayCircleStartY = 8.0f;
                // velocity will be calc when relaunched
            } else {
                // launch day circle
                dayCircleActive = true;
                dayAngleChanged = true;
            }
        }
    }
    lastDayButtonState = currentDayButtonState;

    // handle night button
    bool currentNightButtonState = digitalRead(BTN_NIGHT_PIN) == LOW;
    if(currentNightButtonState && !lastNightButtonState && millis() - lastNightButtonPressTime > buttonDebounceDelay) {
        lastNightButtonPressTime = millis();
        
        if(!gameRunning) {
            // start game
            gameRunning = true;
            dayCircleActive = true;
            nightCircleActive = true;
            dayAngleChanged = true;
            nightAngleChanged = true;

            // init paddle pos with current potentiometer angles
            int dayPotValue = analogRead(POT_DAY_PIN);
            float dayMappedValue = map(dayPotValue, ADC_MIN, ADC_MAX, 0, (GRID_SIZE_Y - PADDLE_HEIGHT) * 100);
            dayPaddleY = dayMappedValue / 100.0f;
            dayPaddleY = constrain(dayPaddleY, 0.0f, GRID_SIZE_Y - PADDLE_HEIGHT);
            
            int nightPotValue = analogRead(POT_NIGHT_PIN);
            float nightMappedValue = map(nightPotValue, ADC_MIN, ADC_MAX, 0, (GRID_SIZE_Y - PADDLE_HEIGHT) * 100);
            nightPaddleY = nightMappedValue / 100.0f;
            nightPaddleY = constrain(nightPaddleY, 0.0f, GRID_SIZE_Y - PADDLE_HEIGHT);
        } else {
            if(nightCircleActive) {
                // reset night circle
                nightCircleActive = false;
                nightCircleStartX = 24.0f;
                nightCircleStartY = 8.0f;
                // velocity will be calc when relaunched
            } else {
                // launch night circle
                nightCircleActive = true;
                nightAngleChanged = true;
            }
        }
    }
    lastNightButtonState = currentNightButtonState;

    // update display
    unsigned long currentTime = millis();
    if (currentTime - lastUpdate >= UPDATE_INTERVAL) {
        UpdateCircles();
        CountSquares(&dayCount, &nightCount);
        DrawGame();
        DrawNames();
        display.display();
        lastUpdate = currentTime;
    }
}