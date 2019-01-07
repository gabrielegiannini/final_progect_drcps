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

void clear_message_array(){//clean the array of transmit message, except for the first position
	for(int i=1;i<4;i++){
		mydata->transmit_msg.data[i] = 0;
	}
  mydata->clean_array = true;
}

void clean_array_SR(){//clean the array of transmit message and the array of received message.
	for(int i=0;i<4;i++){
		mydata->transmit_msg.data[i] = 0;
		mydata->received_msg.data[i] = 0;
	}
}

void setup_message(void){//define the messages that kilobots send

	mydata->transmit_msg.type = NORMAL;
	if(mydata->phase_one_end){
		mydata->transmit_msg.data[0] =  mydata->connections & 255;
  		mydata->transmit_msg.data[1] =  mydata->connections >> 8;	
	}
	
	if(mydata->phase_one_end){
		mydata->transmit_msg.data[1] = mydata->received_msg.data[1];
	}
	
	/*if(kilo_uid == 1 && mydata->phase_one_end){//when the phase one finish, the bot1 choose randomly who is the witch and send it message[1]=witch
		mydata->transmit_msg.data[1] = mydata->witch;
  	}*/
	
  if(mydata->target_chosen){//if the witch chose the target, send it on message[2]. target_chosen is true only in the bot that is the witch and only if chose the target
		mydata->transmit_msg.data[1] = mydata->target_color;
  }

  mydata->transmit_msg.crc = message_crc(&mydata->transmit_msg);//checksum os messages
}

void assign_color(){
  mydata->my_color = kilo_uid == 0 ? RGB(3, 3, 3) : RGB(kilo_uid, (3 - kilo_uid), ((6 - kilo_uid) / 2));
  set_color(mydata->my_color);
}

void move_direction_for_seconds(motion_t direction, uint8_t seconds){
  if (mydata->t * 32 < kilo_ticks && kilo_ticks < (mydata->t + seconds / 2) * 32){
    set_motion(direction);
    //skip seconds
    mydata->t += seconds;
  }else if (kilo_ticks > (mydata->t + seconds) * 32){
    mydata->t = kilo_ticks / 32;
  }
}

void move_random_direction(){
  //actually, random int from 1 to 3, 1 = FORWARD, 2 = LEFT and 3 = RIGHT
  move_direction_for_seconds((rand_soft() % 3) + 1, 3);
}

void change_direction_based_on_distance(bool run_away){//change directions of kilobots based on the distance by the others and based on the status: is the target or a catcher
  if (run_away ? mydata->cur_distance < mydata->cur_position : mydata->cur_distance > mydata->cur_position){

    if (mydata->curr_direction == LEFT){
      move_direction_for_seconds(RIGHT, 4);
    }else if (mydata->curr_direction == RIGHT){
      move_direction_for_seconds(LEFT, 3);
    }else{
      move_direction_for_seconds((rand_soft() % 2 + 2), 2);
    }

  }else if (mydata->cur_distance == mydata->cur_position){
    move_direction_for_seconds(FORWARD, 2);
  }

}

void catch_other_bot(){//invokes the change_direction_based_on_distance with a boolean that will determine a different result of the conditional operator (? :) in the first if
  change_direction_based_on_distance(false);
}

void run_away_from_others(){//invokes the change_direction_based_on_distance with a boolean that will determine a different result of the conditional operator (? :) in the first if
  change_direction_based_on_distance(true);
}

void move_to_find_other_bots(){//define a spiral movement that it is the better way to find other bots having unknown positions
  if (kilo_ticks > 32 * (mydata->t) && kilo_ticks < 32 * (mydata->t + 3)){
    set_motion(RIGHT);
  }else if (kilo_ticks > 32 * (mydata->t + 3) && kilo_ticks < 32 * (mydata->t + 6)){
    set_motion(FORWARD);
    mydata->t = mydata->t + mydata->i;
    mydata->i = mydata->i + SPIRAL_INCREMENT;
  }else if (kilo_ticks > 32 * (mydata->t + mydata->i + 3)){//reset things if t has remained set to a previous value that is too low and would cause the bot to always turn right
    mydata->t = kilo_ticks / 32;
    mydata->i = 1;
  }
  /*if ((kilo_ticks - mydata->last_reception_time) % (SECONDS_RESET_SPIRAL * 32) < 3){//reset the spiral if it's too much that the bot is swirling around
    mydata->i = 1;
  }*/
}

void set_catched(){//set the status of a bot to "catched"
  mydata->target_catched = true;
  set_motion(STOP);
  set_color(RGB(0, 0, 3));
}

void update_distance_estimate(){
  uint8_t distance_estimate = estimate_distance(&mydata->dist);
  if (mydata->target_color == kilo_uid){//the target consider everyone else a target (whoever catches him, he must stop)
    mydata->distance_to_target = distance_estimate < mydata->distance_to_target ? distance_estimate : mydata->distance_to_target;
    mydata->cur_position = mydata->cur_distance;
    mydata->cur_distance = distance_estimate;
    mydata->following_color = mydata->received_msg.data[0];
  }else{
    
    if (mydata->received_msg.data[1] < mydata->following_distance_to_target){// choose whether to update who to follow (the bot who is believed to be the closest to target)
      mydata->following_color = mydata->received_msg.data[0];
      mydata->following_distance_to_target = mydata->received_msg.data[1];
    }
    
    if (mydata->received_msg.data[0] == mydata->following_color){//then it update the distance estimate
      mydata->cur_position = mydata->cur_distance;
      mydata->cur_distance = distance_estimate;
      if (mydata->following_distance_to_target + distance_estimate < 255){//update distance to target: it consider the sum of the distance from the bot 
        mydata->distance_to_target = mydata->following_distance_to_target + distance_estimate;//it's currently following plus its distance from the target
      }else{//prevent overflow (maybe also use uint16_t instead of 8?)
        mydata->distance_to_target = 255;
      }
    }
  }
  printf("%idtt: %i\n",kilo_uid ,mydata->distance_to_target);
  if (mydata->distance_to_target < 50){
    set_catched();
  }
}

bool first_phase(){//while the array already_set don't have all elements equals to 1 return false, else return true. return true when all kilobots communicate with the bot1

	if ((mydata->received_msg.data[0] >> 1) & 1){
		set_motion(STOP);
	}
		
	if(kilo_uid != 1 && mydata->curr_direction != STOP){
		move_to_find_other_bots();
		return false;
	}
	
	return mydata->connections == 1023;

}

bool all_have_target(){//is useful to verify if every kilobots know who is their target (chose by the witch)

	for(int i=0; i<10; i++){
		if(mydata->received_msg.data[0] == i && mydata->received_msg.data[3] == 1){
			mydata->have_target[i]=1;
		}
		mydata->have_target[1]=1;
	}

	for(int i=0;i<10;i++){
		if(mydata->have_target[i] == 1){
			//condition satistied
		}else{
			return false;
		}
	}

	return true;

}

void loop(){

	//first phase (make all communicate)
	if(!mydata->phase_one_end){
		if(mydata->phase_one_running){
			printf("Kilobot %i: Phase 1 running...\n", kilo_uid);
			mydata->phase_one_running = false;
		}
		mydata->phase_one_end = first_phase();
		return;
	} 
	
	if(mydata->print_phase_one){
		printf("###Kilobot %i: Phase 1 terminated###\n",kilo_uid);
		mydata->print_phase_one = false;
	}
        
	//second phase (random choise of witch. One bot make this choise and then will broadcast the ID on the network created)
	if(!mydata->phase_two_end && mydata->phase_one_end && kilo_uid == 1){
                rand_seed(3);
		mydata->witch = rand_soft()%10;
		printf("Witch: %i\n", mydata->witch);
		clean_array_SR();
		mydata->transmit_msg.data[1] = mydata->witch;
		mydata->transmit_msg.data[0] = 2;//phase two
		mydata->phase_two_end = true;
		return;
	}

	//third phase (random choise of the target color. the witch choise this color and brodcast it on the network)
	if(mydata->received_msg.data[1] == kilo_uid)
		mydata->i_am_the_witch = true;

	if(mydata->i_am_the_witch && !mydata->target_chosen){
		mydata->target_color = rand_soft()%10;
		printf("Target, ID: %i, %i\n", mydata->target_color, kilo_uid);
		mydata->target_chosen = true;
		mydata->i_am_the_witch = false;
		return;
	}

  	/*if(kilo_uid == 1 && mydata->send_target){//the bot1 receive the target from the witch because the bot1 communicate for sure with all oterh bot, and brodcast to all the other
		if(mydata->received_msg.data[2]<10 && mydata->received_msg.data[2] != 0 && mydata->send_target){
			mydata->target_color = mydata->received_msg.data[2];
			mydata->transmit_msg.data[2] = mydata->target_color;
			mydata->send_target = false;
		}
	}

	if(kilo_uid != 1 && mydata->get_target){//every kilobots receive the target color by message and save it in mydata->target_color
		if(mydata->received_msg.data[2]<10 && mydata->received_msg.data[2] != 0 && mydata->get_target){
			mydata->target_color = mydata->received_msg.data[2];
			mydata->transmit_msg.data[3] = mydata->target_color;
			mydata->get_target = false;
		}
	}
	
	if(kilo_uid == 1 && all_have_target() && mydata->target_bool){//condition to wait that all kilobots received the color of the target
		mydata->transmit_msg.data[2] = 11;
		mydata->target_bool = false;
	}
	if(mydata->received_msg.data[2] == 11 || mydata->transmit_msg.data[2] == 11){//if all the other receive 11 from bot1 it means that all have the target
		//condition satisfied
	}else{
		return;
	}

	//from here is what Cosimo wrote
	mydata->transmit_msg.data[1] = mydata->my_color == mydata->target_color ? 0 : mydata->distance_to_target;

	update_distance_estimate();	

	if (mydata->target_catched){
  	sprintf(mydata->current_doing, "CATCHED");
    return;
  }
	
	if (kilo_uid == mydata->target_color){
		//printf("%i RUN\n", kilo_uid);
  	run_away_from_others();
    sprintf(mydata->current_doing, "RUNNING AWAY from: %i", mydata->following_color);
	}else{
		//printf("%i CATCH\n", kilo_uid);
      		catch_other_bot();
      		sprintf(mydata->current_doing, "CATCHING target: %i, following: %i", mydata->target_color, mydata->following_color);
  }*/

}

void message_rx(message_t *m, distance_measurement_t *d){

  mydata->is_new_message = true;
  //start transmitting
  mydata->stop_message = false;
  //set the time of reception of this message
  mydata->last_reception_time = kilo_ticks;
  mydata->dist = *d;
	mydata->received_msg = *m;
  if(!mydata->phase_one_end && (m->data[0] >> 1) & 1)
	mydata->connections = mydata->connections | ((m->data[1] << 8 ) | m->data[0]);

}

message_t *message_tx(){

  if (!mydata->stop_message){
    setup_message();
    return &mydata->transmit_msg;
  }else{
    return NULL;
  }

}

void setup(){

  mydata->target_color = RGB(3, 3, 3);
  mydata->cur_distance = 0;
  mydata->is_new_message = false;
  mydata->distance_to_target = UINT8_MAX;
  mydata->following_distance_to_target = UINT8_MAX;
  mydata->last_reception_time = 0;
  mydata->target_catched = false;
  mydata->phase_one_end = false;
  mydata->phase_two_end = false;
  mydata->witch_choose = false;
  mydata->target_bool = true;
  mydata->i_am_the_witch = false;
  mydata->print_phase_one = true;
  mydata->phase_one_running = true;
  mydata->target_chosen = false;
  mydata->clean_array = false;
  mydata->get_target = true;
  mydata->send_target = true;
  mydata->witch = rand_soft()%10;
  mydata->t = 0;
  mydata->i = 1;
  mydata->connections = 1 << kilo_uid;
  setup_message();
  assign_color(); 
  clean_array_SR();
  if(kilo_uid != 1)
  	set_motion(FORWARD);
  if(kilo_uid == 1)
  	mydata->stop_message = false;

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
  p += sprintf(p, ", Dtt: %i, sending: [%i, %i,%i,%i]\n", mydata->distance_to_target, mydata->transmit_msg.data[0], mydata->transmit_msg.data[1],mydata->transmit_msg.data[2],mydata->transmit_msg.data[3]);
  p += sprintf(p, ", Dtt: %i, receiving: [%i, %i,%i,%i]\n", mydata->distance_to_target, mydata->received_msg.data[0], mydata->received_msg.data[1],mydata->received_msg.data[2],mydata->received_msg.data[3]);
  p += sprintf(p, "target: %i, following: %i, t: %i, i: %i, doing %s\n", mydata->connections, mydata->following_color, mydata->t, mydata->i, mydata->current_doing);
  return botinfo_buffer;
}
#endif

int main(){

  kilo_init();
  kilo_message_rx = message_rx;
  SET_CALLBACK(botinfo, cb_botinfo);
  kilo_message_tx = message_tx;
  kilo_start(setup, loop);
  return 0;

}
