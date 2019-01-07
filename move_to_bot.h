//for compiling for real kilobots
#if !defined(NULL)
#define NULL ((void *)0)
#endif

//good values are 3 or 4
//2 leads to a spiral diverging too slowly
//values bigger than 5 leads to bots dispersing too much
#define SPIRAL_INCREMENT 5
// time in seconds after which the spiral movent of the bot will be reset
#define SECONDS_RESET_SPIRAL 150
// 2 seconds, after that time the bot should consider to be oout of communication range
// and start to move to search for other bots
#define TIME_TO_CONSIDER_OUT_OF_RANGE 64
// the threashold for considering 2 bots touching
#define RANGE_TO_TOUCH 

// declare motion variable type
typedef enum
{
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

// declare variables

typedef struct
{
    //ee
    uint16_t connections;
    //the color of this bot
    uint8_t my_color;
    //array of 10 elements (the number of bots) that is used in the phase one to verify if all other bots communicate with bot1
    uint8_t already_sent[10];
    //array of 10 elements (the number of bots) that is used in the phase two to verify if all the bots know the color of target bot
    uint8_t have_target[10];
    //current distance from another bot
    uint8_t cur_distance;
    //previous distance from another bot
    uint8_t cur_position;
    //distance from the target bot
    uint8_t distance_to_target;
    //distance from following bot to target
    uint8_t following_distance_to_target;
    //value of the clock when last message was received
    uint32_t last_reception_time;
    //whether there is a new received message or not
    bool is_new_message;
    //is true when the first phase of the game is terminated
    bool phase_one_end;
    //is used to print only one time the message that the kilobot is running the phase one
    bool phase_one_end_message;
    //is true when the second phase of the game is terminated
    bool phase_two_end;
    //is true if the kilobot know the target
    bool target_bool;
    //is true if the witch is chose
    bool witch_choose;
    //is true if the current kilobot is the witch
    bool i_am_the_witch;
    //used to print only one time when the phase one and for a bot
    bool print_phase_one;
    //is true if the target is chose
    bool target_chosen;
    //is true if the kilobot know the target
    bool get_target;
    //is true if the target is not sent by the bot 1 to the other
    bool send_target;
    //used to clean array of messages only one time and not forever
    bool clean_array;
    //is true if phase one is running
    bool phase_one_running;
    //number representing the id of the witch chose by bot1
    uint8_t witch;
    //the direction to which the bot is currently aiming
    motion_t curr_direction;
    //whether a bot should send messages or not
    bool stop_message;
    //whether a bot has catched its target
    bool target_catched;
    //for movement
    int t;
    //for spiral movement
    int i;
    //the color of the target bot
    uint8_t target_color;
    //the color of the bot that this bot is currently following (maybe is the target, maybe not)
    uint8_t following_color;
    //the last distance measurment received
    distance_measurement_t dist;
    //the message to be transmitted
    message_t transmit_msg;
    //last received message
    message_t received_msg;
    //for debugging
    char current_doing[50];
} USERDATA;
