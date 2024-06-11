#define OPERATION_MODE Normal

// Defines for nodes' periods
// Variable to define the period between two sends of sas, in seconds
#define SAS_DATA_PERIOD 1
// Variable to define the period between two sends of ECU1, in seconds
#define ECU_DATA1_PERIOD 1
// Variable to define the period between two sends of ECU2, in seconds
#define ECU_DATA2_PERIOD 1
// Variable to define the period between two sends of TCU1, in seconds
#define TCU_DATA1_PERIOD 1
// Variable to define the period between two sends of TCU3, in seconds
#define TCU_DATA3_PERIOD 2
// Variable to define the period between two sends of ESP2, in seconds
#define ESP_DATA2_PERIOD 4

// This defines are the limits given by the DBC file of the vehicle, but they were not included in the vehicle.h files
// Maybe add an upgrade to put in a separate file
#define SAS_SPEED_MAX 255
#define SAS_TURN_MAX 327.6f
#define ECU1_APP_MAX 102
#define ECU1_MAX_RPM 65535
#define ECU1_MAX_REAL_RPM 7000
#define ECU1_MAX_TORQUE_REQ 255
#define ECU2_MAX_TPS 100
#define TCU3_MAX_GEAR 15
#define TCU3_REAL_MAX_GEAR 5

// Defines for different attacks
// This attack is gonna send fake Throttle Position Sensor data, with a period corresponding to a fraction of the real node.
// 3 modes : Fast which sends full speed all the time, slow which send 0 speed all the time, and OutOfBound which sends out of bound data to create an internal error.
//#define ATTACK_FAKE_TPS_NODE
#define ATTACK_FAKE_TPS_FREQUENCY (float)ECU_DATA2_PERIOD/(float)4
#define ATTACK_FAKE_TPS_MODE Fast

// This attack has for objective to DOS all lower priority messages. Most effective if Id = 1
//#define ATTACK_DOS_NODE
#define ATTACK_DOS_FREQUENCY 0.01f
#define ATTACK_DOS_ID 1