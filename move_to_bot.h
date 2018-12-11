
#if !defined(NULL)
#define NULL ((void *) 0)
#endif

// declare motion variable type
typedef enum {
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

// declare state variable type
typedef enum {
    ORBIT_TOOCLOSE,
    ORBIT_NORMAL,
} orbit_state_t;

// declare variables

typedef struct 
{
  orbit_state_t orbit_state;
  uint8_t cur_distance;
  uint8_t cur_position;
  uint8_t new_message;
  int actual_move;
  int stop_message;
  int t;
  int i;
  int rand;
  int first_phase;
  distance_measurement_t dist;

  message_t transmit_msg;
} USERDATA;




