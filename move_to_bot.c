#include <math.h>
#include <stdbool.h>

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
    break;
  case FORWARD:
    smooth_set_motors(kilo_turn_left, kilo_turn_right);
    break;
  case LEFT:
    smooth_set_motors(kilo_turn_left, 0);
    break;
  case RIGHT:
    smooth_set_motors(0, kilo_turn_right);
    break;
  }
  mydata->curr_direction = new_motion;
}

void move_random_direction()
{
  if (mydata->t * 32 < kilo_ticks && kilo_ticks < (mydata->t + 2) * 32)
  {
    //actually, random int from 1 to 3, 1 = FORWARD, 2 = LEFT and 3 = RIGHT
    set_motion((rand_soft() % 3) + 1);
    //skip 2 seconds
    mydata->t += 4;
  }
}

void catch_other_bot()
{
  if (mydata->cur_distance > mydata->cur_position)
  {
    if (mydata->curr_direction == LEFT)
    {
      set_motion(RIGHT);
    }
    else if (mydata->curr_direction == RIGHT)
    {
      set_motion(LEFT);
    }
    else
    {
      set_motion((rand_soft() % 2 + 2));
    }
  }
  else if (mydata->cur_distance == mydata->cur_position)
  {
    set_motion(FORWARD);
  }
}

void move_to_find_other_bots()
{
  if (kilo_ticks > 32 * (mydata->t) && kilo_ticks < 32 * (mydata->t + 3))
  {
    set_motion(RIGHT);
  }
  else if (kilo_ticks > 32 * (mydata->t + 3) && kilo_ticks < 32 * (mydata->t + 6))
  {
    set_motion(FORWARD);
    mydata->t = mydata->t + mydata->i;
    mydata->i = mydata->i + SPIRAL_INCREMENT;
  }
  //reset the spiral if it's too much that the bot is swirling around
  if ((kilo_ticks - mydata->last_reception_time) % (200 * 32) < 3)
  {
    mydata->i = 1;
  }
}

void update_distance_estimate()
{
  uint8_t distance_estimate = estimate_distance(&mydata->dist);
  if (mydata->target_uid == kilo_uid)
  {
    //the target consider everyone else a target (whoever catches him, he must stop)
    mydata->distance_to_target = distance_estimate < mydata->distance_to_target ? distance_estimate : mydata->distance_to_target;
  }
  else
  {
    //a catcher update distance_to_target with the estimate only if received directly from target
    if (mydata->received_msg.data[0] == mydata->target_uid)
    {
      //update distance estimate
      mydata->cur_position = mydata->cur_distance;
      mydata->cur_distance = distance_estimate;
      mydata->distance_to_target = distance_estimate;
    }
    else
    {
      // choose whether to update who to follow (the bot who is believed to be the closest to target)
      if (mydata->received_msg.data[1] < mydata->following_distance_to_target)
      {
        mydata->following_uid = mydata->received_msg.data[0];
        mydata->following_distance_to_target = mydata->received_msg.data[1];
      }

      //then it update the distance estimate
      if (mydata->received_msg.data[0] == mydata->following_uid)
      {
        //update distance estimate
        mydata->cur_position = mydata->cur_distance;
        mydata->cur_distance = distance_estimate;
        //update distance to target: it consider the sum of the distance from the bot
        // it's currently following plus its distance from the target
        mydata->distance_to_target = mydata->following_distance_to_target + mydata->cur_distance;
      }
    }
  }
}

void loop()
{
  mydata->current_doing[0] = "\0";
  if (mydata->target_catched)
  {
    sprintf(mydata->current_doing, "CATCHED");
    return;
  }
  //if no message has been received for a long time
  if (mydata->last_reception_time + TIME_TO_CONSIDER_OUT_OF_RANGE <= kilo_ticks && kilo_uid != mydata->target_uid)
  {
    move_to_find_other_bots();
    sprintf(mydata->current_doing, "SEARCHING OTHERS");
    mydata->cur_distance = UINT8_MAX;
    mydata->cur_position = UINT8_MAX;
    mydata->distance_to_target = UINT8_MAX;
    mydata->following_distance_to_target = UINT8_MAX;
    mydata->stop_message = true;
    return;
  }
  if (mydata->is_new_message)
  {
    mydata->is_new_message = false;
    update_distance_estimate();
  }

  if (mydata->distance_to_target < 45)
  {
    set_motion(STOP);
    set_color(RGB(0, 0, 3));
    mydata->target_catched = true;
    printf("ID: %i CATCHED TARGET\n", kilo_uid);
    // mydata->stop_message = true;
    // mydata->is_new_message = false;
    return;
  }

  // target bot is running away, other ones are trying to catch up (katchup?)
  if (kilo_uid == mydata->target_uid)
  {
    move_random_direction();
    sprintf(mydata->current_doing, "MOVING RANDOMLY");
  }
  else
  {
    catch_other_bot();
    sprintf(mydata->current_doing, "CATCHING target: %i, following: %i", mydata->target_uid, mydata->following_uid);
  }
}

void message_rx(message_t *m, distance_measurement_t *d)
{
  mydata->is_new_message = true;
  //start transmitting
  mydata->stop_message = false;
  //set the time of reception of this message
  mydata->last_reception_time = kilo_ticks;
  mydata->dist = *d;
  mydata->received_msg = *m;
  // printf("ID: %i, target: %i, rec from: %i, dtt: %i\n", kilo_uid, mydata->target_uid, m->data[0], m->data[1]);
}

void setup_message(void)
{
  mydata->transmit_msg.type = NORMAL;
  mydata->transmit_msg.data[0] = kilo_uid & 0xff; //low byte of ID
  mydata->transmit_msg.data[1] = kilo_uid == mydata->target_uid ? 0 : mydata->distance_to_target;
  //finally, calculate a message check sum
  mydata->transmit_msg.crc = message_crc(&mydata->transmit_msg);
}

message_t *message_tx()
{
  if (!mydata->stop_message)
  {
    setup_message();
    return &mydata->transmit_msg;
  }
  else
  {
    return NULL;

  }
}

void setup()
{
  mydata->target_uid = 0;
  mydata->stop_message = kilo_uid == mydata->target_uid ? 0 : 1;
  mydata->cur_distance = 0;
  mydata->is_new_message = false;
  mydata->distance_to_target = UINT8_MAX;
  mydata->following_distance_to_target = UINT8_MAX;
  mydata->last_reception_time = 0;
  mydata->target_catched = false;
  mydata->t = 0;
  mydata->i = 1;
  setup_message();
  if (kilo_uid == mydata->target_uid)
  {
    set_color(RGB(3, 0, 0));
    set_motion(LEFT);
  }
  // else if (kilo_uid == 1)
  // {
  //   set_color(RGB(0, 3, 0));
  //   set_motion(FORWARD);
  // }
  else
  {
    //just to be colorful!
    set_color(RGB(kilo_uid, (255 - kilo_uid), ((512 - kilo_uid) / 2)));
    set_motion(STOP);
  }
}

#ifdef SIMULATOR
/* provide a text string for the simulator status bar about this bot */
static char botinfo_buffer[10000];
char *cb_botinfo(void)
{
  char *p = botinfo_buffer;
  p += sprintf(p, "ID: %d, ", kilo_uid);
  switch (mydata->curr_direction)
  {
  case STOP:
    p += sprintf(p, "Direction: STOP");
    break;
  case FORWARD:
    p += sprintf(p, "Direction: FORWARD");
    break;
  case LEFT:
    p += sprintf(p, "Direction: LEFT");
    break;
  case RIGHT:
    p += sprintf(p, "Direction: RIGHT");
    break;
  }
  p += sprintf(p, ", Distance to target: %i\n", mydata->distance_to_target);
  p += sprintf(p, "target: %i, following: %i, t: %i, i: %i, doing %s\n", mydata->target_uid, mydata->following_uid, mydata->t, mydata->i, mydata->current_doing);
  return botinfo_buffer;
}
#endif

int main()
{
  kilo_init();
  kilo_message_rx = message_rx;
  SET_CALLBACK(botinfo, cb_botinfo);
  kilo_message_tx = message_tx;
  kilo_start(setup, loop);
  return 0;
}
