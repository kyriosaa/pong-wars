// For ESP32

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// component defines
#define BTN_DAY_PIN     18      // D18
#define BTN_NIGHT_PIN   5       // D5
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
float dayCircleStartX = 8.0f, dayCircleStartY = 8.0f;       // starting location for day circle
float dayCircleVelX = 0.8f, dayCircleVelY = -0.8f;
float nightCircleStartX = 24.0f, nightCircleStartY = 8.0f;  // starting location for night circle
float nightCircleVelX = -0.8f, nightCircleVelY = 0.8f;
float dayCircleAngle = 0.0f;
float nightCircleAngle = 0.0f;
bool dayAngleChanged = false;
bool nightAngleChanged = false;

// game state
bool gameStarted = false;               // tracks whether the game has started (for sync start)
bool initialStart = true;               // tracks whether this is the first start since reset (for sync start)
bool dayRunning = false;                // tracks the day circle's state
bool nightRunning = false;              // tracks the night circle's state
bool lastDayButtonState = false;        // tracks day circle's input management
bool lastNightButtonState = false;      // tracks night circle's input management
unsigned long lastDayPressTime = 0;     // how long ago was the day button pressed? (for debounce)
unsigned long lastNightPressTime = 0;   // how long ago was the night button pressed? (for debounce)
const int buttonDebounceDelay = 1000;   // debounce delay

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
    if(dayRunning) {
        if(dayAngleChanged) {
            float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
            dayCircleVelX = cos(dayCircleAngle) * speed;
            dayCircleVelY = sin(dayCircleAngle) * speed;
            dayAngleChanged = false;
        }
        dayCircleStartX += dayCircleVelX;
        dayCircleStartY += dayCircleVelY;
        
        // handles the wall bounces
        if (dayCircleStartX <= CIRCLE_BOUND || dayCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) {
            dayCircleVelX = -dayCircleVelX;

            // this stuff makes sure the balls stay in bounds
            if (dayCircleStartX <= CIRCLE_BOUND) dayCircleStartX = CIRCLE_BOUND;
            if (dayCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) dayCircleStartX = GRID_SIZE_X - CIRCLE_BOUND;
            
            // everytime the balls bounce it changes the angle a bit
            // bcs otherwise the balls would draw an identical path
            float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
            float angle = atan2(dayCircleVelY, dayCircleVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
            dayCircleVelX = cos(angle) * speed;
            dayCircleVelY = sin(angle) * speed;
        }
        if (dayCircleStartY <= CIRCLE_BOUND || dayCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) {
            dayCircleVelY = -dayCircleVelY;

            if (dayCircleStartY <= CIRCLE_BOUND) dayCircleStartY = CIRCLE_BOUND;
            if (dayCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) dayCircleStartY = GRID_SIZE_Y - CIRCLE_BOUND;
            
            float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
            float angle = atan2(dayCircleVelY, dayCircleVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
            dayCircleVelX = cos(angle) * speed;
            dayCircleVelY = sin(angle) * speed;
        }
        
        // checks collision with grid cells
        int gridX = (int)round(dayCircleStartX);
        int gridY = (int)round(dayCircleStartY);
        if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
            if (grid[gridY][gridX] == 1) { // if a black cell is hit...
                grid[gridY][gridX] = 0; // turn it white
                tone(BUZZER_PIN, BUZZER_DAY);
                delay(10);
                tone(BUZZER_PIN, 0);

                // same angle change code
                float speed = sqrt(dayCircleVelX * dayCircleVelX + dayCircleVelY * dayCircleVelY);
                float angle = atan2(-dayCircleVelY, -dayCircleVelX) + (RandomFloat() - 0.5f) * COLLISION_RAND;
                dayCircleVelX = cos(angle) * speed;
                dayCircleVelY = sin(angle) * speed;
            }
        }
    }    
    // END DAY HALF
    
    // THIS IS FOR THE NIGHT HALF (BLACK CIRCLE)
    if(nightRunning) {
        if(nightAngleChanged) {
            float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
            nightCircleVelX = cos(nightCircleAngle) * speed;
            nightCircleVelY = sin(nightCircleAngle) * speed;
            nightAngleChanged = false;
        }
        nightCircleStartX += nightCircleVelX;
        nightCircleStartY += nightCircleVelY;
        
        // handles the wall bounces
        if (nightCircleStartX <= CIRCLE_BOUND || nightCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) {
            nightCircleVelX = -nightCircleVelX;
            // this stuff makes sure the balls stay in bounds
            if (nightCircleStartX <= CIRCLE_BOUND) nightCircleStartX = CIRCLE_BOUND;
            if (nightCircleStartX >= GRID_SIZE_X - CIRCLE_BOUND) nightCircleStartX = GRID_SIZE_X - CIRCLE_BOUND;
            
            // everytime the balls bounce it changes the angle a bit
            // bcs otherwise the balls would draw an identical path
            float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
            float angle = atan2(nightCircleVelY, nightCircleVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
            nightCircleVelX = cos(angle) * speed;
            nightCircleVelY = sin(angle) * speed;
        }
        if (nightCircleStartY <= CIRCLE_BOUND || nightCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) {
            nightCircleVelY = -nightCircleVelY;
        
            if (nightCircleStartY <= CIRCLE_BOUND) nightCircleStartY = CIRCLE_BOUND;
            if (nightCircleStartY >= GRID_SIZE_Y - CIRCLE_BOUND) nightCircleStartY = GRID_SIZE_Y - CIRCLE_BOUND;
            
            float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
            float angle = atan2(nightCircleVelY, nightCircleVelX) + (RandomFloat() - 0.5f) * ANGLE_RAND;
            nightCircleVelX = cos(angle) * speed;
            nightCircleVelY = sin(angle) * speed;
        }
        
        // checks collision with grid cells
        int gridX = (int)round(nightCircleStartX);  
        int gridY = (int)round(nightCircleStartY);
        if (gridX >= 0 && gridX < GRID_SIZE_X && gridY >= 0 && gridY < GRID_SIZE_Y) {
            if (grid[gridY][gridX] == 0) { // if a white cell is hit...
                grid[gridY][gridX] = 1; // turn it black
                tone(BUZZER_PIN, BUZZER_NIGHT);
                delay(10);
                tone(BUZZER_PIN, 0);
                
                // same angle change code
                float speed = sqrt(nightCircleVelX * nightCircleVelX + nightCircleVelY * nightCircleVelY);
                float angle = atan2(-nightCircleVelY, -nightCircleVelX) + (RandomFloat() - 0.5f) * COLLISION_RAND;
                nightCircleVelX = cos(angle) * speed;
                nightCircleVelY = sin(angle) * speed;
            }
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
    int leftX = (int)(dayCircleStartX * CELL_SIZE);
    int leftY = (int)(dayCircleStartY * CELL_SIZE);
    int rightX = (int)(nightCircleStartX * CELL_SIZE);
    int rightY = (int)(nightCircleStartY * CELL_SIZE);
    
    // left circle (black on white background)
    display.fillCircle(leftX, leftY, CIRCLE_RADIUS, COLOR_BLACK);
    
    // right circle (white on black background)
    display.fillCircle(rightX, rightY, CIRCLE_RADIUS, COLOR_WHITE);

    // draw angle lines
    // DAY
    if(!dayRunning) {
        int dayEndX = leftX + (int)(cos(dayCircleAngle) * AIM_LINE_LENGTH);
        int dayEndY = leftY + (int)(sin(dayCircleAngle) * AIM_LINE_LENGTH);
        display.drawLine(leftX, leftY, dayEndX, dayEndY, COLOR_BLACK);
    }
    // NIGHT
    if(!nightRunning) {
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
    // DAY
    if(!dayRunning) {
        int dayPotValue = analogRead(POT_DAY_PIN);
        // only update if there's a significant change
        if(abs(dayPotValue - lastDayPotValue) > POT_THRESHOLD) {
            // map the potentiometer value (0-1023) to angle (0 to π)
            float unmappedDayCircleAngle = map(dayPotValue, ADC_MIN, ADC_MAX, POT_MIN_ANGLE, POT_MAX_ANGLE) / 100.0f;
            dayCircleAngle = PI - unmappedDayCircleAngle;   // so the aim is flipped horizontally
            lastDayPotValue = dayPotValue;
        }
    }  
    // NIGHT
    if(!nightRunning) {
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

    // sync start at the beginning of the game
    // either button will unpause both circles for a fair start
    if(!gameStarted) {
        bool currentDayButtonState = digitalRead(BTN_DAY_PIN) == LOW;
        bool currentNightButtonState = digitalRead(BTN_NIGHT_PIN) == LOW;

        // check either button press to start both circles
        if((currentDayButtonState && !lastDayButtonState && millis() - lastDayButtonState > buttonDebounceDelay) ||
           (currentNightButtonState && !lastNightButtonState && millis() - lastNightButtonState > buttonDebounceDelay)) {
            dayRunning = true;
            nightRunning = true;
            dayAngleChanged = true;
            nightAngleChanged = true;
            gameStarted = true;

            lastDayPressTime = millis();
            lastNightPressTime = millis();
        }
        lastDayButtonState = currentDayButtonState;
        lastNightButtonState = currentNightButtonState;
    } else {
        // handle DAY button
        bool currentDayButtonState = digitalRead(BTN_DAY_PIN) == LOW;
        if(currentDayButtonState && !lastDayButtonState && millis() - lastDayPressTime > buttonDebounceDelay) {
            lastDayPressTime = millis();
            dayRunning = !dayRunning;

            if(dayRunning) {
                dayAngleChanged = true;
            }
        }
        lastDayButtonState = currentDayButtonState;

        // handle NIGHT button
        bool currentNightButtonState = digitalRead(BTN_NIGHT_PIN) == LOW;
        if(currentNightButtonState && !lastNightButtonState && millis() - lastNightPressTime > buttonDebounceDelay) {
            lastNightPressTime = millis();
            nightRunning = !nightRunning;

            if(nightRunning) {
                nightAngleChanged = true;
            }
        }
        lastNightButtonState = currentNightButtonState;
    }

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