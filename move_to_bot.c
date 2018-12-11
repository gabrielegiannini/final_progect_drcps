#include <math.h>

#include <kilombo.h>

#include "move_to_bot.h"

#ifdef SIMULATOR
#include <stdio.h> // for printf
#else
#include <avr/io.h> // for microcontroller register defs
//  #define DEBUG          // for printf to serial port
//  #include "debug.h"
#endif

REGISTER_USERDATA(USERDATA)

/* Helper function for setting motor speed smoothly
 */
void smooth_set_motors(uint8_t ccw, uint8_t cw)
{
  // OCR2A = ccw;  OCR2B = cw;
#ifdef KILOBOT
  uint8_t l = 0, r = 0;
  if (ccw && !OCR2A) // we want left motor on, and it's off
    l = 0xff;
  if (cw && !OCR2B) // we want right motor on, and it's off
    r = 0xff;
  if (l || r) // at least one motor needs spin-up
  {
    set_motors(l, r);
    delay(15);
  }
#endif
  // spin-up is done, now we set the real value
  set_motors(ccw, cw);
}

void set_motion(motion_t new_motion)
{
  switch (new_motion)
  {
  case STOP:
    smooth_set_motors(0, 0);
    mydata->actual_move = 1;
    break;
  case FORWARD:
    smooth_set_motors(kilo_turn_left, kilo_turn_right);
    mydata->actual_move = 2;
    break;
  case LEFT:
    smooth_set_motors(kilo_turn_left, 0);
    mydata->actual_move = 3;
    break;
  case RIGHT:
    smooth_set_motors(0, kilo_turn_right);
    mydata->actual_move = 4;
    break;
  }
}

void go_to_other_bot()
{
  if (mydata->cur_distance > mydata->cur_position && mydata->actual_move == 3)
  {
    set_motion(RIGHT);
    return;
  }

  if (mydata->cur_distance > mydata->cur_position && mydata->actual_move == 4)
  {
    set_motion(LEFT);
    return;
  }
}

void move_randomly_to_find_other_bot()
{
  if (kilo_ticks > 32 * (mydata->t) && kilo_ticks < 32 * (mydata->t + 3))
  {
    set_motion(RIGHT);
    mydata->actual_move = RIGHT;
    return;
  }

  if (kilo_ticks > 32 * (mydata->t + 3) && kilo_ticks < 32 * (mydata->t + 6))
  {
    set_motion(FORWARD);
    mydata->actual_move = FORWARD;
    mydata->t = mydata->t + mydata->i;
    //good values are i + 3 or i + 4
    mydata->i = mydata->i + 4;
    return;
  }
}

void loop()
{
  // Update distance estimate with every message
  if (!mydata->new_message)
  {
    move_randomly_to_find_other_bot();
    return;
  }
  else
  {
    set_motion(STOP);
    mydata->t = 2;
    mydata->i = 1;
  }
  /*if (mydata->new_message==1) {
        mydata->new_message = 0;
        mydata->cur_position = mydata->cur_distance;
        mydata->cur_distance = estimate_distance(&mydata->dist);
    }

    if(mydata->cur_distance < 40 && kilo_uid == 1){
      set_motion(STOP);
      set_color(RGB(0,0,3));
      mydata->stop_message = 1;
      mydata->new_message = 0;
      return;
    }

    // bot 0 is stationary. Other bots orbit around it.
    if (kilo_uid == 0)
      return;

    if(kilo_uid == 1)
      go_to_other_bot();*/
}

void message_rx(message_t *m, distance_measurement_t *d)
{
  mydata->new_message = 1;
  mydata->stop_message = 0;
  mydata->dist = *d;
}

void setup_message(void)
{
  mydata->transmit_msg.type = NORMAL;
  mydata->transmit_msg.data[0] = 1; //low byte of ID, currently not really used for anything
  //finally, calculate a message check sum
  mydata->transmit_msg.crc = message_crc(&mydata->transmit_msg);
}

message_t *message_tx()
{
  if (mydata->stop_message == 0)
  {
    return &mydata->transmit_msg;
  }
  else
  {
    return NULL;
  }
}

void setup()
{
  mydata->stop_message = kilo_uid == 0 ? 0 : 1;
  mydata->orbit_state = ORBIT_NORMAL;
  mydata->cur_distance = 0;
  mydata->new_message = 0;
  mydata->t = 0;
  mydata->i = 1;
  setup_message();
  if (kilo_uid == 0)
  {
    set_color(RGB(3, 0, 0)); // color of the stationary bot
    set_motion(LEFT);
  }
  else if (kilo_uid == 1)
  {
    set_color(RGB(0, 3, 0));
    set_motion(STOP);
  }
  else
  {
    set_color(RGB(kilo_uid, 255 - kilo_uid, (512 - kilo_uid) / 2));
  }
}

#ifdef SIMULATOR
/* provide a text string for the simulator status bar about this bot */
static char botinfo_buffer[10000];
char *cb_botinfo(void)
{
  char *p = botinfo_buffer;
  p += sprintf(p, "ID: %d \n", kilo_uid);
  if (mydata->actual_move == RIGHT)
    p += sprintf(p, "Direction: RIGHT\n");
  else if (mydata->actual_move == FORWARD)
    p += sprintf(p, "Direction: FORWARD\n");
  p += sprintf(p, "t: %i, i: %i\n", mydata->t, mydata->i);
  return botinfo_buffer;
}
#endif

int main()
{
  kilo_init();
  kilo_message_rx = message_rx;
  SET_CALLBACK(botinfo, cb_botinfo);
  // bot 0 is stationary and transmits messages. Other bots orbit around it.
  kilo_message_tx = message_tx;
  kilo_start(setup, loop);
  return 0;
}
