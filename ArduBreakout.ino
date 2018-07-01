 /*
  Breakout
 Copyright (C) 2011 Sebastian Goscik
 All rights reserved.

 Modifications by Scott Allen 2016 (after previous changes by ???)
 Adapted for the Micro:Gamer by Dan O'Shea 2018

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 */

#include "MMA8653.h"

#include <MicroGamer.h>
#include "MicroGamerTones.h"
#include <MicroGamerMemoryCard.h>

#define BASE_FRAME_RATE (30)
#define MENU_FRAME_RATE (8)
#define NBR_HIGH_SCORES (7)

#define Y_BUTTON (1<<5)     /**< The Y button value for functions requiring a bitmask */
#define X_BUTTON (1<<6)     /**< The X button value for functions requiring a bitmask */

MMA8653 accel;

MicroGamer arduboy;
MicroGamerTones audio(arduboy.audio.enabled);
MicroGamerMemoryCard mem(64/4);

const unsigned int COLUMNS = 13; //Columns of bricks
const unsigned int ROWS = 4;     //Rows of bricks
int dx = -1;        //Initial movement of ball
int dy = -1;        //Initial movement of ball
int xb;           //Balls starting possition
int yb;           //Balls starting possition
boolean released;     //If the ball has been released by the player
boolean paused = false;   //If the game has been paused
byte xPaddle;       //X position of paddle
boolean isHit[ROWS][COLUMNS];   //Array of if bricks are hit or not
boolean bounced=false;  //Used to fix double bounce glitch
byte lives = 3;       //Amount of lives
byte level = 1;       //Current level
unsigned int score=0;   //Score for the game
unsigned int brickCount;  //Amount of bricks hit
boolean pad, pad2, pad3;  //Button press buffer used to stop pause repeating
boolean oldpad, oldpad2, oldpad3;
boolean start=false;    //If in menu or in game
boolean initialDraw=false;//If the inital draw has happened
char initials[3];     //Initials used in high score
char s[16]; // general string buffer

//Ball Bounds used in collision detection
byte leftBall;
byte rightBall;
byte topBall;
byte bottomBall;

//Brick Bounds used in collision detection
byte leftBrick;
byte rightBrick;
byte topBrick;
byte bottomBrick;

byte tick;

void setup()
{
  arduboy.boot();
  arduboy.setFrameRate(BASE_FRAME_RATE);

  accel.begin(false, 2); // 8-bit mode, 2g range
  accel.update();
  randomSeed(accel.getX() + accel.getY() + accel.getZ());
}

void loop()
{
  // pause render until it's time for the next frame
  if (!(arduboy.nextFrame()))
    return;

  //Title screen loop switches from title screen
  //and high scores until FIRE is pressed
  while (!start)
  {
    start = titleScreen();
    if (!start)
    {
      start = displayHighScores(1);
    }
  }

  //Initial level draw
  if (!initialDraw)
  {
    //Clears the screen
    arduboy.clear();
    //Selects Font
    //Draws the new level
    level = 1;
    initialDraw=true;
    newLevel();
    score = 0;
  }

  if (lives>0)
  {
    drawPaddle();

    //Pause game if FIRE pressed
    pad = arduboy.pressed(Y_BUTTON) || arduboy.pressed(X_BUTTON);

    if(pad == true && oldpad == false && released)
    {
      oldpad2 = false; //Forces pad loop 2 to run once
      pause();
    }

    oldpad = pad;
    drawBall();

    if(brickCount == ROWS * COLUMNS)
    {
      level++;
      newLevel();
    }
  }
  else
  {
    drawGameOver();
    if (score > 0)
    {
      enterHighScore(1);
    }

    arduboy.clear();
    initialDraw=false;
    start=false;
    lives=3;
    newLevel();
  }

  arduboy.display();
}

void movePaddle()
{
  arduboy.waitDisplayUpdate();
  accel.update();

  //Move right
  if(xPaddle < WIDTH - 12)
  {
    if (accel.getX() > 10)
    {
      xPaddle+=3;
    }
  }

  //Move left
  if(xPaddle > 0)
  {
    if (accel.getX() < -10)
    {
      xPaddle-=3;
    }
  }
}

void moveBall()
{
  tick++;
  if(released)
  {
    //Move ball
    if (abs(dx)==2) {
      xb += dx/2;
      // 2x speed is really 1.5 speed
      if (tick%2==0)
        xb += dx/2;
    } else {
      xb += dx;
    }
    yb=yb + dy;

    //Set bounds
    leftBall = xb;
    rightBall = xb + 2;
    topBall = yb;
    bottomBall = yb + 2;

    //Bounce off top edge
    if (yb <= 0)
    {
      yb = 2;
      dy = -dy;
      audio.tone(523, 250);
    }

    //Lose a life if bottom edge hit
    if (yb >= 64)
    {
      arduboy.drawRect(xPaddle, 63, 11, 1, 0);
      xPaddle = 54;
      yb=60;
      released = false;
      lives--;
      audio.tone(175, 250);
      if (random(0, 2) == 0)
      {
        dx = 1;
      }
      else
      {
        dx = -1;
      }
    }

    //Bounce off left side
    if (xb <= 0)
    {
      xb = 2;
      dx = -dx;
      audio.tone(523, 250);
    }

    //Bounce off right side
    if (xb >= WIDTH - 2)
    {
      xb = WIDTH - 4;
      dx = -dx;
      audio.tone(523, 250);
    }

    //Bounce off paddle
    if (xb+1>=xPaddle && xb<=xPaddle+12 && yb+2>=63 && yb<=64)
    {
      dy = -dy;
      dx = ((xb-(xPaddle+6))/3); //Applies spin on the ball
      // prevent straight bounce
      if (dx == 0) {
        dx = (random(0,2) == 1) ? 1 : -1;
      }
      audio.tone(200, 250);
    }

    //Bounce off Bricks
    for (byte row = 0; row < ROWS; row++)
    {
      for (byte column = 0; column < COLUMNS; column++)
      {
        if (!isHit[row][column])
        {
          //Sets Brick bounds
          leftBrick = 10 * column;
          rightBrick = 10 * column + 10;
          topBrick = 6 * row + 1;
          bottomBrick = 6 * row + 7;

          //If A collison has occured
          if (topBall <= bottomBrick && bottomBall >= topBrick &&
              leftBall <= rightBrick && rightBall >= leftBrick)
          {
            Score();
            brickCount++;
            isHit[row][column] = true;
            arduboy.drawRect(10*column, 2+6*row, 8, 4, 0);

            //Vertical collision
            if (bottomBall > bottomBrick || topBall < topBrick)
            {
              //Only bounce once each ball move
              if(!bounced)
              {
                dy =- dy;
                yb += dy;
                bounced = true;
                audio.tone(261, 250);
              }
            }

            //Hoizontal collision
            if (leftBall < leftBrick || rightBall > rightBrick)
            {
              //Only bounce once brick each ball move
              if(!bounced)
              {
                dx =- dx;
                xb += dx;
                bounced = true;
                audio.tone(261, 250);
              }
            }
          }
        }
      }
    }
    //Reset Bounce
    bounced = false;
  }
  else
  {
    //Ball follows paddle
    xb=xPaddle + 5;

    //Release ball if FIRE pressed
    pad3 = arduboy.pressed(Y_BUTTON) || arduboy.pressed(X_BUTTON);
    if (pad3 == true && oldpad3 == false)
    {
      released = true;

      //Apply random direction to ball on release
      if (random(0, 2) == 0)
      {
        dx = 1;
      }
      else
      {
        dx = -1;
      }
      //Makes sure the ball heads upwards
      dy = -1;
    }
    oldpad3 = pad3;
  }
}

void drawBall()
{
  // arduboy.setCursor(0,0);
  // arduboy.print(arduboy.cpuLoad());
  // arduboy.print("  ");
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb+1, yb,   0);
  arduboy.drawPixel(xb,   yb+1, 0);
  arduboy.drawPixel(xb+1, yb+1, 0);

  moveBall();

  arduboy.drawPixel(xb,   yb,   1);
  arduboy.drawPixel(xb+1, yb,   1);
  arduboy.drawPixel(xb,   yb+1, 1);
  arduboy.drawPixel(xb+1, yb+1, 1);
}

void drawPaddle()
{
  arduboy.drawRect(xPaddle, 63, 11, 1, 0);
  movePaddle();
  arduboy.drawRect(xPaddle, 63, 11, 1, 1);
}

void drawGameOver()
{
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb+1, yb,   0);
  arduboy.drawPixel(xb,   yb+1, 0);
  arduboy.drawPixel(xb+1, yb+1, 0);
  arduboy.setCursor(37, 42);
  arduboy.print("Game Over");
  arduboy.setCursor(31, 56);
  arduboy.print("Score: ");
  arduboy.print(score);
  arduboy.display();
  delay(4000);
}

void pause()
{
  paused = true;
  //Draw pause to the screen
  arduboy.setCursor(52, 45);
  arduboy.print("PAUSE");
  arduboy.display();
  while (paused)
  {
    delay(150);
    //Unpause if FIRE is pressed
    pad2 = arduboy.pressed(Y_BUTTON) || arduboy.pressed(X_BUTTON);
    if (pad2 == true && oldpad2 == false && released)
    {
        arduboy.fillRect(52, 45, 30, 11, 0);

        paused=false;
    }
    oldpad2 = pad2;
  }
}

void Score()
{
  score += (level*10);
}

void newLevel()
{
  //Undraw paddle
  arduboy.drawRect(xPaddle, 63, 11, 1, 0);

  //Undraw ball
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb+1, yb,   0);
  arduboy.drawPixel(xb,   yb+1, 0);
  arduboy.drawPixel(xb+1, yb+1, 0);

  //Alter various variables to reset the game
  xPaddle = 54;
  yb = 60;
  brickCount = 0;
  released = false;

  //Draws new bricks and resets their values
  for (byte row = 0; row < 4; row++) {
    for (byte column = 0; column < 13; column++)
    {
      isHit[row][column] = false;
      arduboy.drawRect(10*column, 2+6*row, 8, 4, 1);
    }
  }

  if (!initialDraw) arduboy.clear();
  else arduboy.display();
}

//Used to delay images while reading button input
boolean pollFireButton(int n)
{
  for(int i = 0; i < n; i++)
  {
    delay(15);
    pad = arduboy.pressed(Y_BUTTON) || arduboy.pressed(X_BUTTON);
    if(pad == true && oldpad == false)
    {
      oldpad3 = true; //Forces pad loop 3 to run once
      return true;
    }
    oldpad = pad;
  }
  return false;
}

//Function by nootropic design to display highscores
boolean displayHighScores(byte file)
{
  byte y = 8;
  byte x = 24;
  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = 0;
  byte hi, lo;

  arduboy.fillScreen(BLACK);
  arduboy.setTextColor(WHITE);
  arduboy.setCursor(32,0);
  arduboy.print("HIGH SCORES");

  mem.load();

  for (int i = 0; i < NBR_HIGH_SCORES; i++) {
    sprintf(s, "%2d", i + 1);

    arduboy.setCursor(x, y + (i * 8));
    arduboy.print(s);

    hi = mem.read(address + (5 * i));
    lo = mem.read(address + (5 * i) + 1);
    if ((hi == 0xFF) && (lo == 0xFF)) {
      score = 0;
    } else {
      score = (hi << 8) | lo;
    }
    initials[0] = (char)mem.read(address + (5 * i) + 2);
    initials[1] = (char)mem.read(address + (5 * i) + 3);
    initials[2] = (char)mem.read(address + (5 * i) + 4);

    if (score > 0) {
      sprintf(s, "%c%c%c %u", initials[0], initials[1], initials[2], score);
      arduboy.setCursor(x + 24, y + (i * 8));
      arduboy.print(s);
    }
  }

  arduboy.display();

  delay(300);

  if (pollFireButton(300)) {
    return true;
  }
  return false;
}

boolean titleScreen()
{
  //Clears the screen
  arduboy.clear();
  arduboy.setCursor(16,22);
  arduboy.setTextSize(2);
  arduboy.print("BREAKOUT");
  arduboy.setTextSize(1);
  arduboy.display();
  if (pollFireButton(25))
  {
    return true;
  }

  //Flash "Press FIRE" 5 times
  for(byte i = 0; i < 5; i++)
  {
    //Draws "Press FIRE"
    arduboy.setCursor(31, 53);
    arduboy.print("PRESS FIRE!");
    arduboy.display();

    if (pollFireButton(50))
    {
      return true;
    }

    //Removes "Press FIRE"
    arduboy.setCursor(31, 53);
    arduboy.print("           ");
    arduboy.display();

    if (pollFireButton(25))
    {
      return true;
    }
  }

  return false;
}

//Function by nootropic design to add high scores
void enterInitials()
{
  int index = 0;

  // Lower the frame rate otherwise the intials input is too fast
  arduboy.setFrameRate(MENU_FRAME_RATE);

  arduboy.fillScreen(BLACK);
  arduboy.setTextColor(WHITE);
  arduboy.setCursor(16, 0);
  arduboy.print("HIGH SCORE");
  sprintf(s, "%u", score);
  arduboy.setCursor(88, 0);
  arduboy.print(s);

  initials[0] = ' ';
  initials[1] = ' ';
  initials[2] = ' ';

  delay(300);

  while (true) {
    if (!arduboy.nextFrame()) {
      continue; // go back to the start of the loop
    }

    arduboy.setCursor(56, 20);
    arduboy.print(initials[0]);
    arduboy.setCursor(64, 20);
    arduboy.print(initials[1]);
    arduboy.setCursor(72, 20);
    arduboy.print(initials[2]);
    for (byte i = 0; i < 3; i++) {
      arduboy.drawLine(56 + (i * 8), 27, 56 + (i * 8) + 6, 27, 1);
    }
    arduboy.drawLine(56, 28, 88, 28, 0);
    arduboy.drawLine(56 + (index * 8), 28, 56 + (index * 8) + 6, 28, 1);

    delay(50);
    if (arduboy.pressed(LEFT_BUTTON)) {
      index--;
      if (index < 0) {
        index = 0;
      } else {
        audio.tone(1046, 20);
      }
    }
    if (arduboy.pressed(RIGHT_BUTTON)) {
      index++;
      if (index > 2) {
        index = 2;
      } else {
        audio.tone(1046, 20);
      }
    }
    if (arduboy.pressed(UP_BUTTON)) {
      initials[index]++;
      audio.tone(523, 20);
      // A-Z 0-9 :-? !-/ ' '
      if (initials[index] == '0') {
        initials[index] = ' ';
      }
      if (initials[index] == '!') {
        initials[index] = 'A';
      }
      if (initials[index] == '[') {
        initials[index] = '0';
      }
      if (initials[index] == '@') {
        initials[index] = '!';
      }
    }
    if (arduboy.pressed(DOWN_BUTTON)) {
      initials[index]--;
      audio.tone(523, 20);
      if (initials[index] == ' ') {
        initials[index] = '?';
      }
      if (initials[index] == '/') {
        initials[index] = 'Z';
      }
      if (initials[index] == 31) {
        initials[index] = '/';
      }
      if (initials[index] == '@') {
        initials[index] = ' ';
      }
    }
    if (arduboy.pressed(Y_BUTTON) || arduboy.pressed(X_BUTTON)) {
      if (index < 2) {
        index++;
        audio.tone(1046, 20);
      } else {
        audio.tone(1046, 20);

        // Go back to standard frame rate
        arduboy.setFrameRate(BASE_FRAME_RATE);
        return;
      }
    }
    arduboy.display();
  }
}

void enterHighScore(byte file)
{
  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = 0;
  uint8_t hi, lo;
  char tmpInitials[3];
  unsigned int tmpScore = 0;

  mem.load();

  // High score processing
  for (byte i = 0; i < NBR_HIGH_SCORES; i++) {
    hi = mem.read(address + (5 * i));
    lo = mem.read(address + (5 * i) + 1);
    if ((hi == 0xFF) && (lo == 0xFF)) {
      // The values are uninitialized, so treat this entry
      // as a score of 0.
      tmpScore = 0;
    } else {
      tmpScore = (hi << 8) | lo;
    }
    if (score > tmpScore) {
      enterInitials();
      for (byte j = i; j < 10; j++) {
        hi = mem.read(address + (5 * j));
        lo = mem.read(address + (5 * j) + 1);
        if ((hi == 0xFF) && (lo == 0xFF)) {
          tmpScore = 0;
        } else {
          tmpScore = (hi << 8) | lo;
        }
        tmpInitials[0] = (char)mem.read(address + (5 * j) + 2);
        tmpInitials[1] = (char)mem.read(address + (5 * j) + 3);
        tmpInitials[2] = (char)mem.read(address + (5 * j) + 4);

        // tmpScore and tmpInitials now hold what we want to write in the next slot.

        // write score and initials to current slot
        mem.write(address + (5 * j), ((score >> 8) & 0xFF));
        mem.write(address + (5 * j) + 1, (score & 0xFF));
        mem.write(address + (5 * j) + 2, initials[0]);
        mem.write(address + (5 * j) + 3, initials[1]);
        mem.write(address + (5 * j) + 4, initials[2]);
        score = tmpScore;
        initials[0] = tmpInitials[0];
        initials[1] = tmpInitials[1];
        initials[2] = tmpInitials[2];
      }
      score = 0;
      initials[0] = ' ';
      initials[1] = ' ';
      initials[2] = ' ';
      mem.save();
      return;
    }
  }
}
