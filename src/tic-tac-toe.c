/**
 * tic-tac-toe
 *
 * Author: Michael Rosa
 * Date: June 2016
 * 
 * Play tic-tac-toe against a computer or another person.
 * 
 * Uses an Atmega328P for I/O but can probably be modified to use other MCUs.
 * The game "board" is represented by red and green LEDs arranged in a grid. 
 * A player inputs their move with a button matrix, the buttons corresponding
 * to the appropriate place.
 *
 * For more circuit/layout details see the appropriate files.
 */
  

// ------ Preamble ------ //
#include <avr/io.h>                 // pins, ports, etc.
#include <util/delay.h>             // delay functions
#include <avr/interrupt.h>          // interrupts
#include <avr/sleep.h>              // sleep modes

#define  LED_PORT       PORTC       // some defines
#define  LED_DDR        DDRC
#define  CPU_PORT       PORTC       // for readability
#define  CPU_PIN        PINC
#define  CPU_DDR        DDRC
#define  BTN_PORT       PORTD
#define  BTN_PIN        PIND
#define  BTN_DDR        DDRD

#define  CPU_SW         PC5
  
#define DEBOUNCE_DELAY  10000       // microseconds
#define LED_DELAY         800 

#define TRUE                1
#define FALSE               0

FUSES = 
{
  .low = LFUSE_DEFAULT,
  .high = HFUSE_DEFAULT,
  .extended = EFUSE_DEFAULT,
};


uint8_t board[9] = {0, 0, 0,        // 0 - not played
                    0, 0, 0,        // 1 - player 1
                    0, 0, 0};       // 2 - player 2

uint8_t cpu_is_playing;
uint8_t player_turn = 1;

uint8_t count = 0;
uint8_t LED_state = 0;

uint8_t winner = 0;
              // 0 - game not over
              // 1 - player 1
              // 2 - player 2
              // 3 - tie


// ------ Interrupt ------ //
/**
 * Interrupt to drive the LEDs
 * 
 * For the board:
 *   for n[0..8]
 *     set LED registers to drive nth red/green pair
 *     if position n on the board has been played
 *       drive appropriate LED for the player
 *
 * For the player LEDs:
 *   if game has a winner / tie
 *     drive LED(s) for the winner / tie
 *   else
 *     count with a variable 
 *     when the limit is reached toggle the LEDs state
 *     if the LED state is on
 *       drive the LED according to the current player's turn
 */
ISR(TIMER2_OVF_vect)
{
  uint8_t i;
  
  // ------ Main board LEDs ------ //
  LED_DDR &= ~(0x1F);
  LED_DDR |=   0x03;
  for(i = 0; i < 4; i++)
  {
    if(board[i] != 0)
    {
      if(board[i] == 2)
      {
        LED_PORT |= 1 << i;
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << i);
      }
      else if(board[i] == 1)
      {
        LED_PORT |= 1 << (i + 1);
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << (i + 1));
      }
    }
    
    LED_DDR = LED_DDR << 1;
  }
  
  LED_DDR &= ~(0x1F);
  LED_DDR |=   0x05;
  for(i = 0; i < 3; i++)
  {
    if(board[i + 4] != 0)
    {
      if(board[i + 4] == 2)
      {
        LED_PORT |= 1 << i;
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << i);
      }
      else if(board[i + 4] == 1)
      {
        LED_PORT |= 1 << (i + 2);
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << (i + 2));
      }
    }
    
    LED_DDR = LED_DDR << 1;
  }
  
  LED_DDR &= ~(0x1F);
  LED_DDR |=   0x09;
  for(i = 0; i < 2; i++)
  {
    if(board[i + 7] != 0)
    {
      if(board[i + 7] == 2)
      {
        LED_PORT |= 1 << i;
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << i);
      }
      else if(board[i+7] == 1)
      {
        LED_PORT |= 1 << (i + 3);
        _delay_us(LED_DELAY);
        LED_PORT &= ~(1 << (i + 3));
      }
    }
    
    LED_DDR = LED_DDR << 1;
  }
  
  
  // ------ Player LEDs ------ //
  LED_DDR &= ~(0x1F);
  LED_DDR |=   0x11;
  if(winner != 0)
  {
    if(winner == 1)
    {
      LED_PORT |= 0x10;
      _delay_us(LED_DELAY);
      LED_PORT &= ~(0x10);
    }
    else if(winner == 2)
    {
      LED_PORT |= 1;
      _delay_us(LED_DELAY);
      LED_PORT &= ~(1);
    }
    else
    {
      LED_PORT |= 1;
      _delay_us(LED_DELAY);
      LED_PORT &= ~(1);
      LED_PORT |= 0x10;
      _delay_us(LED_DELAY);
      LED_PORT &= ~(0x10);
    }
  }
  else   // else blink player turn LED
  {
    if(++count == 46)  // ~377 ms
    {
      count = 0;
      LED_state ^= 1;
    }
    
    if(LED_state == 1) 
    {
      if(player_turn == 1)
      {
        LED_PORT |= 0x10;
        _delay_us(LED_DELAY);
        LED_PORT &= ~0x10;
      }
      else
      {
        LED_PORT |= 1;
        _delay_us(LED_DELAY);
        LED_PORT &= ~1;
      }
    }
  }
  
  LED_DDR &= ~(0x1F);
  TCNT2 = 0;
}



// ------ Input ------ //
/**
 * Get a board location from a (r, c) coordinate
 * If the coordinate isn't valid return 255
 *
 * Board / coordinate key:
 *  [0][1][2]      [0,0][0,1][0,2]
 *  [3][4][5]      [1,0][1,1][1,2]
 *  [6][7][8]      [2,0][2,1][2,2]
 */
uint8_t location(uint8_t r, uint8_t c)
{
  if(r < 3 && c < 3) return r * 3 + c;
  else return 0xFF;
}


/**
 * Get what number button was pressed
 * as long as the corresponding board 
 * location has not been played
 * If not button pressed return 255
 *
 * Buttons are arranged in this fashion:
 *  [0][1][2]
 *  [3][4][5]
 *  [6][7][8]
 *
 * IMPORTANT NOTE:
 * This function will hang the AVR if the 
 * timer 2 overflow interrupt occurs too often
 * and if the LED_DELAY is too long
 */
uint8_t get_btn_pressed(void)
{
  uint8_t r, c;
  
  for(c = 0; c < 3; c++)
  {
    BTN_DDR &= ~0x38;         // set cols as inputs for z-state
    BTN_DDR |=  0x08 << c;    // bit shift to set one pin as output (low)
    
    for(r = 0; r < 3; r++)
    {
      // if the pin is being pulled low and the location hasn't been played
      if(!(BTN_PIN & (0x04 >> r)) && board[location(r, c)] == 0)
      { 
        _delay_us(DEBOUNCE_DELAY);    // delay for debounce
        if(!(BTN_PIN & (0x04 >> r)))  // still pressed after debounce
        {
          return location(r, c);
        }
      }
    }
  }
  
  return 0xFF;  // no button pressed
}


// ------ Game over and win states ------ //
/**
 * Return whether or not a certain 
 * player is the winner of the game
 */
uint8_t game_winner(uint8_t is_player_1)
{
  uint8_t b, i;
  if(is_player_1) b = 1;
  else b = 2;
  
  // check rows and cols
  for(i = 0; i < 3; i++)
  {
    if(board[location(i, 0)] == b &&
       board[location(i, 1)] == b &&
       board[location(i, 2)] == b)     return TRUE;
       
    if(board[location(0, i)] == b &&
       board[location(1, i)] == b &&
       board[location(2, i)] == b)     return TRUE;
  }
  
  // check diagonals
  if(board[4] == b && ((board[0] == b && board[8] == b) || (board[2] == b && board[6] == b))) 
    return TRUE;
  
  // not winner
  return FALSE;
}

/**
 * Return whether or not the game is over
 * Also set the "winner" variable so the 
 * player LEDs can be driven appropriately
 */
uint8_t game_is_over(void)
{
  // if game has winner, game is over
  if(game_winner(TRUE))
  {
    winner = 1;
    return TRUE;
  }
  else if(game_winner(FALSE))
  {
    winner = 2;
    return TRUE;
  }
  
  // if board has empty space, game is not over
  for(uint8_t i = 0; i < 9; i++)
  {
    if(board[i] == 0) 
    {
      return FALSE;
    }
  }
  
  // tie
  winner = 3;
  return TRUE;
}


// ------ Board manipulation ------ //
/**
 * Return a pointer to a copy of the game board
 * so that the computer can rotate the board freely
 * without modifying the actual game
 */
uint8_t * copy_board(void)
{
  static uint8_t copy[9];
  uint8_t i;
  
  for(i = 0; i < 9; i++)
  {
    copy[i] = board[i];
  }
  
  return copy;
}

/**
 * Rotate a game board once clockwise
 * This allows for a reduced search space and
 * in effect, shorter code
 *
 * Rotation essentially has the following effect:
 *
 *  [0][1][2]         [6][3][0]
 *  [3][4][5]   -->   [7][4][1]
 *  [6][7][8]         [8][5][2]
 */
void rotate(uint8_t *board)
{
  uint8_t temp5 = *(board + 5);
  uint8_t temp8 = *(board + 8);
  
  *(board + 8) = *(board + 2);
  *(board + 5) = *(board + 1);
  *(board + 2) = *(board + 0);
  
  *(board + 1) = *(board + 3);
  *(board)     = *(board + 6);
  
  *(board + 3) = *(board + 7);
  *(board + 6) = temp8;
  
  *(board + 7) = temp5;
}


// ------ Computer ------ //
/**
 * Return a side that can be played
 */
uint8_t check_sides(void)
{
  if(board[1] == 0) return 1;
  else if(board[5] == 0) return 5;
  else if(board[7] == 0) return 7;
  else if(board[3] == 0) return 3;
  else return 0xFF;
}

/**
 * Return a corner that can be played
 * depending on whether or not player 1 
 * is in the opposite one
 */
uint8_t check_corners(uint8_t player_1_opposite)
{
  if(player_1_opposite)
  {
    if(board[0] == 0 && board[8] == 1) return 0;
    else if(board[2] == 0 && board[6] == 1) return 2;
    else if(board[8] == 0 && board[0] == 1) return 8;
    else if(board[6] == 0 && board[2] == 1) return 6;
  }
  else
  {
    if(board[0] == 0) return 0;
    else if(board[2] == 0) return 2;
    else if(board[8] == 0) return 8;
    else if(board[6] == 0) return 6;
  }
  
  if(player_1_opposite)
    return check_corners(FALSE);
  else return 0xFF;
}

/**
 * Return a location that either wins the
 * game or blocks player 1 from winning
 *
 * A win condition is three consecutive locations
 * held by one player.
 */
uint8_t check_wins_or_losses(uint8_t is_computer)
{
  uint8_t b, i;
  if(is_computer) b = 2;
  else b = 1;
  
  uint8_t *t = copy_board();
  for(i = 0; i < 4; i++)
  {
    if(i != 0) rotate(t);
    
    // sides
    if(*t == 0 && *(t + 1) == b && *(t + 2) == b)
    {
      if(i == 0) return 0;
      else if(i == 1) return 6;
      else if(i == 2) return 8;
      else return 2;
    }
    else if(*t == b  && *(t + 1) == 0 && *(t + 2) == b)
    {
      if(i == 0) return 1;
      else if(i == 1) return 3;
      else if(i == 2) return 7;
      else return 5; 
    } 
    else if(*t == b && *(t + 1) == b && *(t + 2) == 0)
    {
      if(i == 0) return 2;
      else if(i == 1) return 0;
      else if(i == 2) return 6;
      else return 8;
    }
    
    // middles
    else if(*(t +1) == b && *(t + 4) == b && *(t + 7) == 0){
      if(i == 0) return 7;
      else if(i == 1) return 5;
      else if(i == 2) return 1;
      else return 3;
    }
    else if(*(t + 1) == b && *(t + 4) == 0 && *(t + 7) == b)
      return 4;
    
    // diagonals
    else if(*t == b && *(t + 4) == b && *(t + 8) == 0){
      if(i == 0) return 8;
      else if(i == 1) return 2;
      else if(i == 2) return 0;
      else return 6;
    }
    else if(*t == b && *(t + 4) == 0 && *(t + 8) == b)
      return 4;
  }
  
  if(is_computer)
    return check_wins_or_losses(FALSE);
  else return 0xFF;
}

/**
 * Return a location that either creates a fork
 * state or blocks a fork state of player 1
 * 
 * A fork state is a condition in which there 
 * are two possible ways to win.
 */
uint8_t check_fork_states(uint8_t is_computer)
{
  uint8_t b, i;
  if(is_computer) b = 2;
  else b = 1;
  
  uint8_t *t = copy_board();
  for(i = 0; i < 4; i++)
  {
    if(i != 0) rotate(t);
    
    if(*(t + 2) == 0 && *(t + 4) == b && *(t + 6) == 0 &&
       *(t + 7) == 0 && *(t + 8) == b)
    {
      if(i == 0) return 6;
      else if(i == 1) return 8;
      else if(i == 2) return 2;
      else return 0;
    }
    else if(*t == b && *(t + 3) == 0 && *(t + 6) == 0 &&
            *(t + 7) == 0 && *(t + 8) == b){
      if(is_computer)
      {
        if(i == 0) return 6;
        else if(i == 1) return 8;
        else if(i == 2) return 2;
        else return 0;
      }
      else
      {
        uint8_t location = check_sides();
        if(location <= 8) return location;
      }
    }
  }
  
  if(is_computer)
    return check_fork_states(FALSE);
  else return 0xFF;
}

/**
 * Let's the computer read the board and play a location
 * appropriate toward the goal of either winning or tying
 */
void cpu_turn(void)
{
  uint8_t location;
  
  _delay_ms(1000);    // decision is made quickly
                      // this gives the impression of "thinking"
  
  // check for possible wins or losses
  // wins first
  location = check_wins_or_losses(TRUE);
  if(location <= 8)
  {
    board[location] = 2;
    return;
  }
  
  // check for possible fork states
  // fork states for self first
  location = check_fork_states(TRUE);
  if(location <= 8)
  {
    board[location] = 2;
    return;
  }
  
  // play the center if it's clear
  if(board[4] == 0)
  {
    board[4] = 2;
    return;
  }
  else
  {
    // play on a corner opposite of the player
    // or any corner that's free
    location = check_corners(TRUE);
    if(location <= 8)
    {
      board[location] = 2;
      return;
    }
    
    // play on an empty side
    location = check_sides();
    if(location <= 8)
    {
      board[location] = 2;
      return;
    }
  }
  
  return;
}


// ------ Main ------ //
int main(void) 
{
  // ------ Inits ------ //
  // determine if playing against cpu
  CPU_DDR  &= ~(1 << CPU_SW);  // set as input
  CPU_PORT |=   1 << CPU_SW;   // enable pull-up
  _delay_us(1);                // if statement won't evaluate proplerly w/o
                               // check pin and set flag
  if(CPU_PIN & (1 << CPU_SW)) cpu_is_playing = TRUE;   
  else cpu_is_playing = FALSE;     
  
  CPU_PORT &= ~(1 << CPU_SW);  // pull-up can be safely disabled
  
  
  BTN_DDR &=  ~0x07;           // set rows as inputs
  BTN_PORT |=  0x07;           // enable pull-ups for rows
  
  
  // timer interrupts to drive LEDs
  TCCR2B  |= (1 << CS21) | (1 << CS20); // prescaler 32
  TIMSK2  |= (1 << TOIE2);    // enable interrupts
  TCNT2   =  0;               // every 8.2 ms or so
  sei();                      // global interrupt enable
 
 
  // ------ Main loop ------ //
  uint8_t btn;
  while(game_is_over() == FALSE) 
  {
    btn = get_btn_pressed();
    if(btn <= 8)
    {
      board[btn] = player_turn;
      if(player_turn == 1) player_turn = 2;
      else player_turn = 1;
    }
    
    if(cpu_is_playing == TRUE && player_turn == 2 && game_is_over() == FALSE)
    {
      cpu_turn();
      player_turn = 1;
    }
  }
  
  // go into sleep mode after game is over
  cli();
  sleep_enable();
  sei();
  while(TRUE){sleep_cpu();}
  
  return 0;
}
