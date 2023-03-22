#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "NETWORK";
const char* password = "password";

const char* ntpServer = "pool.ntp.org";
const int ntpPort = 123;
const int timeZoneOffset = -8; // Pacific Time Zone (USA)

const uint16_t PANEL_RES_X = 64;
const uint16_t PANEL_RES_Y = 64;
const uint16_t PANEL_CHAIN = 1;

const uint8_t PADDLE_HEIGHT = 2;
const uint8_t PADDLE_WIDTH = 16;
const uint8_t BALL_SPEED = 1;
const uint8_t BRICK_WIDTH = 8;
const uint8_t BRICK_HEIGHT = 4;
const uint8_t BRICK_ROWS = 6;
const uint8_t BRICK_COLS = 8;

int16_t paddleX;
int16_t paddleY;
int16_t ballX;
int16_t ballY;
int8_t ballSpeedX;
int8_t ballSpeedY;
uint8_t bricks[BRICK_ROWS][BRICK_COLS];
bool gameMessageDisplayed = false;

MatrixPanel_I2S_DMA *dma_display = nullptr;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, timeZoneOffset * 3600, 60000);

void displayMessage() {
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 0, 255));

  dma_display->setCursor(PANEL_RES_X / 2 - 10, PANEL_RES_Y / 2 - 20);
  dma_display->print("GET");

  dma_display->setCursor(PANEL_RES_X / 2 - 17, PANEL_RES_Y / 2 - 10);
  dma_display->print("READY");

  dma_display->setCursor(PANEL_RES_X / 2 - 8, PANEL_RES_Y / 2);
  dma_display->print("FOR");

  dma_display->setCursor(PANEL_RES_X / 2 - 22, PANEL_RES_Y / 2 + 10);
  dma_display->print("ACTION");
}

void drawPaddle(int16_t x, int16_t y, uint16_t color) {
  dma_display->fillRect(x, y, PADDLE_WIDTH, PADDLE_HEIGHT, color);
}

void erasePaddle(int16_t x, int16_t y) {
  drawPaddle(x, y, dma_display->color565(0, 0, 0));
}

void drawBall(int16_t x, int16_t y, uint16_t color) {
  dma_display->fillCircle(x, y, 2, color);
}

void eraseBall(int16_t x, int16_t y) {
  drawBall(x, y, dma_display->color565(0, 0, 0));
}

void resetGame() {
  paddleX = PANEL_RES_X / 2 - PADDLE_WIDTH / 2;
  paddleY = PANEL_RES_Y - PADDLE_HEIGHT - 1;

  ballX = PANEL_RES_X / 2;
  ballY = PANEL_RES_Y - PADDLE_HEIGHT - 4;

  ballSpeedX = random(0, 2) == 0 ? -BALL_SPEED : BALL_SPEED;
  ballSpeedY = -BALL_SPEED;

  for (uint8_t i = 0; i < BRICK_ROWS; i++) {
    for (uint8_t j = 0; j < BRICK_COLS; j++) {
      bricks[i][j] = 1;
    }
  }
  gameMessageDisplayed = false;
}

void drawBricks() {
  for (int i = 0; i < BRICK_ROWS; i++) {
    for (int j = 0; j < BRICK_COLS; j++) {
      if (bricks[i][j]) {
        uint16_t brickColor;
        switch (i) {
          case 0:
            brickColor = dma_display->color565(255, 0, 0); // Red
            break;
          case 1:
            brickColor = dma_display->color565(0, 255, 0); // Green
            break;
          case 2:
            brickColor = dma_display->color565(0, 0, 255); // Blue
            break;
          case 3:
            brickColor = dma_display->color565(255, 255, 0); // Yellow
            break;
          case 4:
            brickColor = dma_display->color565(255, 165, 0); // Orange
            break;
          default:
            brickColor = dma_display->color565(255, 255, 255); // White
            break;
        }
        dma_display->fillRect(j * BRICK_WIDTH, i * BRICK_HEIGHT, BRICK_WIDTH - 1, BRICK_HEIGHT - 1, brickColor);
      }
    }
  }
}


void checkBrickCollisions() {
  uint8_t brickRow = ballY / BRICK_HEIGHT;
  uint8_t brickCol = ballX / BRICK_WIDTH;

  if (brickRow < BRICK_ROWS && brickCol < BRICK_COLS) {
    if (bricks[brickRow][brickCol]) {
      bricks[brickRow][brickCol] = 0;
      dma_display->fillRect(brickCol * BRICK_WIDTH, brickRow * BRICK_HEIGHT, BRICK_WIDTH - 1, BRICK_HEIGHT - 1, dma_display->color565(0, 0, 0));
      ballSpeedY = -ballSpeedY;
    }
  }
}

void setup() {
  Serial.begin(9600);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  // Configure NTP client
  timeClient.begin();

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN
  );
  mxconfig.gpio.e = 18;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();

  resetGame();
  displayMessage();
  delay(3000);

  dma_display->fillScreen(dma_display->color565(0, 0, 0)); // clear the screen
}

// Other functions and variables

bool checkGameOver() {
  for (uint8_t i = 0; i < BRICK_ROWS; i++) {
    for (uint8_t j = 0; j < BRICK_COLS; j++) {
      if (bricks[i][j]) {
        return false;
      }
    }
  }
  return true;
}


void loop() {
  // Update time from NTP server
  timeClient.update();

  if (!gameMessageDisplayed) {
    displayMessage();
    gameMessageDisplayed = true;
    delay(500);
    dma_display->fillScreen(dma_display->color565(0, 0, 0)); // clear the screen
  }

  // Display time in 12-hour format with leading zeroes
  String timeStr = timeClient.getFormattedTime();
  int hour = timeClient.getHours();
  bool isAM = true;
  if (hour > 12) {
    hour -= 12;
    isAM = false;
  } else if (hour == 0) {
    hour = 12;
  }

  if (hour < 10) {
    timeStr = "0" + String(hour) + timeStr.substring(2, 5);
  } else {
    timeStr = String(hour) + timeStr.substring(2, 5);
  }
  if (isAM) {
    timeStr += " AM";
  } else {
    timeStr += " PM";
  }

  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));

  int16_t x1, y1;
  uint16_t w, h;
  dma_display->getTextBounds(timeStr, 0, PANEL_RES_Y - 28, &x1, &y1, &w, &h);

  int timeStrX = (PANEL_RES_X - w) / 2; // Center time string horizontally

  dma_display->setCursor(timeStrX, PANEL_RES_Y - 28);
  dma_display->print(timeStr);

  // Update game logic
  eraseBall(ballX, ballY);
  ballX += ballSpeedX;
  ballY += ballSpeedY;
  drawBall(ballX, ballY, dma_display->color565(255, 255, 255));

  checkBrickCollisions();

  int16_t oldPaddleX = paddleX;
  int16_t oldPaddleY = paddleY;

  // Make the paddle track the ball for auto game play
  paddleX = ballX - PADDLE_WIDTH / 2;
  paddleY = PANEL_RES_Y - PADDLE_HEIGHT - 1;

  paddleX = constrain(paddleX, 0, PANEL_RES_X - PADDLE_WIDTH);

  erasePaddle(oldPaddleX, oldPaddleY);
  drawPaddle(paddleX, paddleY, dma_display->color565(0, 255, 0));

  // Ball hits the wall
  if (ballY <= 0 || ballY >= PANEL_RES_Y - 1) {
    ballSpeedY = -ballSpeedY;
  }

  if (ballX <= 0 || ballX >= PANEL_RES_X - 1) {
    ballSpeedX = -ballSpeedX;
    ballSpeedX += random(-1, 2); // Add random factor to horizontal speed
  }

  // Ball hits the paddle
  if (ballY >= paddleY - 2 && ballX >= paddleX && ballX <= paddleX + PADDLE_WIDTH) {
    ballSpeedY = -ballSpeedY;
  }

  // Ball goes out of bounds, reset game
  if (ballY >= PANEL_RES_Y) {
    resetGame();
  }

  drawBricks();

  // Limit horizontal speed and keep the ball within the matrix
  ballSpeedX = constrain(ballSpeedX, -3, 3);
  ballX = constrain(ballX, 0, PANEL_RES_X - 1);
  ballY = constrain(ballY, 0, PANEL_RES_Y - 1);

  delay(1000 / 30);
}
