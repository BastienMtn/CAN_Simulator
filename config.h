#define OPERATION_MODE Normal
//#define TXLOG
#define RXLOG
#define DelayMeasurement

// Defines for nodes' periods
// Variable to define the period between two sends of SAS, in useconds
#define SAS_DATA_PERIOD 10000
// Variable to define the period between two sends of ECU1, in useconds
#define ECU_DATA1_PERIOD 10000
// Variable to define the period between two sends of ECU2, in useconds
#define ECU_DATA2_PERIOD 10000
// Variable to define the period between two sends of ECU3, in useconds
#define ECU_DATA3_PERIOD 10000
// Variable to define the period between two sends of ECU4, in useconds
#define ECU_DATA4_PERIOD 10000
// Variable to define the period between two sends of TCU1, in useconds
#define TCU_DATA1_PERIOD 10000
// Variable to define the period between two sends of TCU2, in useconds
#define TCU_DATA2_PERIOD 10000
// Variable to define the period between two sends of TCU3, in useconds
#define TCU_DATA3_PERIOD 20000
// Variable to define the period between two sends of ESP1, in useconds
#define ESP_DATA1_PERIOD 10000
// Variable to define the period between two sends of ESP2, in useconds
#define ESP_DATA2_PERIOD 40000
// Variable to define the period between two sends of ABS, in useconds
#define ABS_WHEEL_PERIOD 10000

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
#define ATTACK_FAKE_TPS_PERIOD (float)ECU_DATA2_PERIOD/2
#define ATTACK_FAKE_TPS_MODE Fast

// This attack has for objective to DOS all lower priority messages. Most effective if Id = 1
#define ATTACK_DOS_PERIOD 1000
#define ATTACK_DOS_ID 1

// Attack fuzzing. We can change only the data's position (mode 0), or 8 bytes of data (mode 1) 
#define ATTACK_FUZZ_PERIOD 100000
#define ATTACK_FUZZ_MOD 0 

// Attack replay
#define ATTACK_REPLAY_PERIOD SAS_DATA_PERIOD
