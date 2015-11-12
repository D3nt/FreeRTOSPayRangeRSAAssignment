///-----------------------------------------------------------------------------
/// \file payrange_solution1.c
///-----------------------------------------------------------------------------
///
/// \brief Implementation of RSA Randomization - PayRange Challenge 1
///
/// \n <b> Owner: </b> aleksey.vlasov@gmail.com
///-----------------------------------------------------------------------------

///******************************************************************************
/// PayRange Challenge 1 Implementation using FreeRTOS
///
/// TASK: Please get back to us with the best architecture/design/code you can 
/// come up for the following requirements. I sincerely appreciate you taking time 
/// to program this. Create a program in ANSI C (C11) using GNU C libraries that 
/// does the following in clear/compact/efficiently written code: 
///  > Has a subsystem that creates a 12 digit random number every 250 ms (Call this A) 
///  > Has a separate subsystem that generates a 6 digit alphanumeric string every 5 seconds (Call this B). 
///  > Store a list of B, but only 5 qty of them. Every 5 seconds when a new B is created, 
/// randomly determine which of the existing list of B to replace with the new B. 
/// At any given time, there are only 5 B. When B is stored, save B and pair it with the time 
/// it was generated. So every B is paired with the string plus the time it was generated. 
/// > Take A and print to stdout, updating it every 250ms as a new one is generated. 
/// > Wait for user input. As the numbers are being displayed on stdout, when user presses C on keyboard, 
/// capture the A number at that time and store it to memory along with the time that C was pressed. Call this D.  
/// > The output of A doesn't stop. It keeps going even after the C key was pressed. 
/// > When C is pressed, take that D and pair it with one and only 1 B that is in the list of 5. 
/// > It is paired randomly. This combined D and B pair is E, and is stored in memory for every E that is generated. 
/// > Print each  E to a file E.txt with a line number. 
/// > Take every E and also store it in a separate list where it keeps only 7. Again each update is random to list. 
/// > The screen lists all E, but in this separate subsystem, there is a list of only 7 E's. This sub list we will call F. 
/// > When user presses G on the keyboard,  pause the A thread. 
/// Then ask user for input on which E on list they want by the12 digit A code.  
/// > Take the user's input, look up the E in list F. If that E is found, return B and B's time and print to screen. 
/// If it is not found, print on screen not found. Then resume the A thread.


/// Standard includes
#include <stdio.h>
#include <inttypes.h>
#include <conio.h>

/// Kernel includes
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>

/// Priorities at which the tasks are created
#define mainCHECK_TASK_PRIORITY			( configMAX_PRIORITIES - 2 )
#define ENABLE_DEBUG_PRINTS

/// Project Configurable defines
#define TASK_A_RUNTIME_IN_MS			( 250 )
#define TASK_B_RUNTIME_IN_MS			( 5000 )
#define KEYBOARD_TASK_DELAY_IN_MS       ( 5 )
#define NUMBER_OF_ALPHANUMERIC_DIGITS   ( 8 )
#define SIZE_OF_THE_TASK_B_ARRAY		( 5 )
#define SIZE_OF_VALUE_E_STRUCTURE       ( 7 )

#ifdef noENABLE_DEBUG_PRINTS
#define DEBUGPRINT						printf
#else
#define DEBUGPRINT
#endif ///ENABLE_DEBUG_PRINTS

/// Global Variables

/// Task function prototypes
/// A task that is created from the idle task to generate a 12 digit number
static void privateTaskA(void *pvParameters);
/// B task that is created from the idle task to generate a 8 digit alphanumeric
static void privateTaskB(void *pvParameters);
/// Keaboard Task Listener - can be substituted by an interrupt on a real system
static void keyboardTrackTask(void *pvParameters);
/// C Key Pressed handler
static void handleInterruptC(void);
/// G Key Pressed handler
static void handleInterruptG(void);
/// File Write Function
static void writeToFileE(int slotToWrite);
/// Structure for Task B array of alphanumerics and time
typedef struct
{
	TickType_t stringTime;
	char stringPlacer[8];
}taskBStructure_t;
/// Structure for D (Value A + Time)
typedef struct
{
	TickType_t randomNumberTime;
	int64_t    randomNumber;
}valueD_t;
/// Structure for E (Value B + Value D)
typedef struct
{
	taskBStructure_t randomValueB;
	valueD_t         currentValueD;
}valueE_t;

/// Global task handles for suspension and other operations
TaskHandle_t xTaskAHandle;
TaskHandle_t xTaskBHandle;
TaskHandle_t xKeyboardTaskHandle;
/// Global access for randomly generated number from Task A
int64_t currentRandomNumberFromTaskA;
/// Global access for Task B Structure for pairing
taskBStructure_t taskBStructure[SIZE_OF_THE_TASK_B_ARRAY];
/// Global access for Value E structure
valueE_t valueEStructure[SIZE_OF_VALUE_E_STRUCTURE];
/// Global line number counter for file E.txt
int fileELineNumber;
///-----------------------------------------------------------
/// \brief This is the main function that starts the tasks
///        initiates the interrupts and starts the RTOS scheduler
///
/// @param 1 N/A
///
/// @return 0 - Should never get there.
///-----------------------------------------------------------
int main_payrange( void )
{
	/// Initialize the line number counter
	fileELineNumber = 0;
	/// Create the 2 random value generating tasks with default stack and priority
	xTaskCreate(privateTaskA, "TaskA", configMINIMAL_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, &xTaskAHandle);
	xTaskCreate(privateTaskB, "TaskB", configMINIMAL_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, &xTaskBHandle);
	xTaskCreate(keyboardTrackTask, "Keyboard", configMINIMAL_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, &xKeyboardTaskHandle);
	///Debug check for FreeRTOS. Fail in case any task/timer creation has failed.
	configASSERT(privateTaskA != NULL || privateTaskB != NULL);

	DEBUGPRINT("Tasks Created. Starting the multithreading! \n");
	/// Start the scheduler itself
	vTaskStartScheduler();

    /// Should never get here unless there was not enough heap space to create the idle and other system tasks
    return 0;
}
///-----------------------------------------------------------
/// \brief Generic function to return a random number based on
///        the max specified value
///
/// @param int - max - Maximum number to randomize on
///
/// @return int - randomly generated number
///-----------------------------------------------------------
static int generateIntRandomNumber(int max)
{
	/// Generate the random number with a standard C random generator
	return (rand() % max);
}
///-----------------------------------------------------------
/// \brief Generic function to return a random alphanumeric
///        based on the dictionary and specified length
///
/// @param1 char *generatedRandomString - Address of the generated string
/// @param2 int length - length of the alphanumeric
///
/// @return N/A
///-----------------------------------------------------------
void generateRandomString(char *generatedRandomString, int length)
{
	/// Character set dictionary, limited to small/large casing and numbers
	static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	/// Make sure the length is specified
	if (length)
	{
		/// Make sure that the address for the string is provided
		if (generatedRandomString)
		{
			/// Determine the length of the character set
			int charsetLength = (int)(sizeof(charset) - 1);
			/// Generate the random alphanumeric one by one
			for (int i = 0; i < length; i++)
			{
				int charKey = rand() % charsetLength;
				generatedRandomString[i] = charset[charKey];
			}
			/// Finish up the string with the line termination
			generatedRandomString[length] = '\0';
		}
	}
}

///-----------------------------------------------------------
/// \brief This is the handler for Task A, generates a random
///        12 digit number every 250 ms
///
/// @param 1 void *pvParameters - placeholder for FreeRTOS
///                               Task parameters
///
/// @return None - Task always runs without a return
///-----------------------------------------------------------
static void privateTaskA( void *pvParameters )
{
	/// Local Variables
	const TickType_t xCycleFrequency = TASK_A_RUNTIME_IN_MS / portTICK_PERIOD_MS;
	int64_t generatedRandomNumber;

	/// Just to remove compiler warning.
	( void ) pvParameters;

	/// Main Task endless loop
	for (;;)
	{
		/// Generate the 12 digit random number
		generatedRandomNumber = rand();
		generatedRandomNumber = (generatedRandomNumber << 32 | rand());
		generatedRandomNumber = (generatedRandomNumber % (899999999999)) + 100000000000;
		currentRandomNumberFromTaskA = generatedRandomNumber;
		///Debug assert if the generated number is not 12 digits
		configASSERT((generatedRandomNumber > 100000000000) || (generatedRandomNumber < 1000000000000));
		/// Required print per instructions
		printf("A-Thread Random Number: %" PRIu64 "\n", (int64_t)generatedRandomNumber);
		///Simulated Sleep for 250ms
		vTaskDelay(xCycleFrequency);
	}
}

///-----------------------------------------------------------
/// \brief This is the handler for Task B, generates a random
///        alphanumeric value and stores it into the array
///        with the time value (tick).
///
/// @param 1 void *pvParameters - placeholder for FreeRTOS
///                               Task parameters
///
/// @return None - Task always runs without a return
///-----------------------------------------------------------
static void privateTaskB( void *pvParameters )
{
	///Local Variables
	char taskBRandomString[NUMBER_OF_ALPHANUMERIC_DIGITS - 1];
	char(*randomStringPointer);
	randomStringPointer = taskBRandomString;
	int randomSlot;
	TickType_t currentTickTime;

	/// Just to remove compiler warnings
	( void ) pvParameters;

	/// Main Task endless loop
	for(;;)
	{
		/// Generate the 8 digit alphanumeric
		generateRandomString(randomStringPointer, NUMBER_OF_ALPHANUMERIC_DIGITS);
		DEBUGPRINT("Random String generated by Task B is %s \n", taskBRandomString);

		/// Get Current Timer/Tick Count
		currentTickTime = xTaskGetTickCount();
		/// Determine the random slot for storing - total 5 slots (0-4)
		randomSlot = generateIntRandomNumber(SIZE_OF_THE_TASK_B_ARRAY - 1);
		/// Copy over the generated string and time to the array
		taskBStructure[randomSlot].stringTime = currentTickTime;
		strcpy(taskBStructure[randomSlot].stringPlacer, taskBRandomString);

#ifdef ENABLE_DEBUG_PRINTS
		/// Debug Check
		for (int i = 0; i < SIZE_OF_THE_TASK_B_ARRAY;i++)
		{
			DEBUGPRINT("String %d is %d %s \n", i, taskBStructure[i].stringTime, taskBStructure[i].stringPlacer);
		}
#endif //ENABLE_DEBUG_PRINTS
		///Simulated Sleep for 5 seconds
		vTaskDelay(TASK_B_RUNTIME_IN_MS);
	}
}
///-----------------------------------------------------------
/// \brief This is the keyboard listener task that checks
///        the keyboard buffer for any key presses
///
/// @param 1 void *pvParameters - placeholder for FreeRTOS
///                               Task parameters
///
/// @return None - Task always runs without a return
///-----------------------------------------------------------
static void keyboardTrackTask(void *pvParameters)
{
	/// Just to remove compiler warnings
	(void)pvParameters;

	/// Main Task endless loop
	for (;;)
	{
		/// Delay the task immediatelly. Most Frequest listener task
		vTaskDelay(KEYBOARD_TASK_DELAY_IN_MS);
		/// Wait for the keyboard press - not a standard embedded implementation,
		/// but for all intents and purposes of this challenge, it works.
		if (_kbhit())
		{
			/// Extract the pressed keyboard key. Again, not a standard embedded FW way
			/// but this works in the Visual Studio simulation mode
			int keyboardKey = _getch();
			switch (keyboardKey)
			{
				/// Cases for C key pressed
				case 67:
				case 99:
					/// Get Current Timer/Tick Count
					handleInterruptC();
					break;
                /// Cases for G key pressed
				case 71:
				case 103:
					handleInterruptG();
					break;
                /// Catch all the rest of the keys, just in case
				default:
					DEBUGPRINT("Illegal Key. The key pressed was %d\n", keyboardKey);
					break;
			}
		}
	}
}
///-----------------------------------------------------------
/// \brief This is the handler for Interrupt C - C key pressed
///         on the keyboard. Saves Value D and E. All storage 
///         and handling is local.
///
/// @param N/A
///
/// @return N/A
///-----------------------------------------------------------
static void handleInterruptC(void)
{
	int randomSlotB;
	int randomSlotF;
	BOOL foundValidValueB = FALSE;
	/// Local Variables
	valueD_t valueDStructure;

	TickType_t currentTickTime;
	/// Get the current timer
	currentTickTime = xTaskGetTickCount();
	/// Save off the Value D
	valueDStructure.randomNumber = currentRandomNumberFromTaskA;
	valueDStructure.randomNumberTime = currentTickTime;

	/// Create value E (pair value D with Value B)
	/// Find a valid Value B
	while (!foundValidValueB)
	{
		randomSlotB = generateIntRandomNumber(SIZE_OF_THE_TASK_B_ARRAY);
		if (taskBStructure[randomSlotB].stringTime == 0)
		{
			DEBUGPRINT("Random Slot B value invalid %d \n", randomSlotB);
			/// Do nothing. Structure B slot hasn't been populated
		}
		else
		{
			/// Exit the randomization loop. Value is found.
			DEBUGPRINT("Found a valid Slot B Value %d \n", randomSlotB);
			foundValidValueB = TRUE;
		}
	}

	/// Determine the random slot for storing - total 5 slots (0-4)
	randomSlotF = generateIntRandomNumber(SIZE_OF_VALUE_E_STRUCTURE - 1);
	/// Store the vales into the E structure into the List F slot
	valueEStructure[randomSlotF].randomValueB.stringTime = taskBStructure[randomSlotB].stringTime;
	strcpy(valueEStructure[randomSlotF].randomValueB.stringPlacer, taskBStructure[randomSlotB].stringPlacer);
	valueEStructure[randomSlotF].currentValueD.randomNumber = valueDStructure.randomNumber;
	valueEStructure[randomSlotF].currentValueD.randomNumberTime = valueDStructure.randomNumberTime;

	/// Write the contents of E into the E.txt
	writeToFileE(randomSlotF);

	DEBUGPRINT("C Key pressed! Time: %d, Value: %" PRIu64 "\n", valueDStructure.randomNumberTime, valueDStructure.randomNumber);
	DEBUGPRINT("Selected B is %d String %s Time %d slot %d\n", randomSlotB, valueEStructure[randomSlotF].randomValueB.stringPlacer, valueEStructure[randomSlotF].randomValueB.stringTime, randomSlotF);
}

///-----------------------------------------------------------
/// \brief This is the function that saves value E to "E.txt"
///        along with the line number
///
/// @param int slotToWrite
///
/// @return N/A
///-----------------------------------------------------------
static void writeToFileE(int slotToWrite)
{
	FILE *fileE;
	/// Handle file operations in a critical section to avoid corruption
	/// Check if the E.txt file already exists

	portENTER_CRITICAL();
	
	fileE = fopen("E.txt", "r");
	if ((fileE != NULL) && (fileELineNumber != 0))
	{
		/// File exists from the current session, need to append
		fclose(fileE);
		fileE = fopen("E.txt", "a");
	}
	else
	{
		/// File doesn't exist or an old version of the file is present, create new and overwrite
		fclose(fileE);
		fileE = fopen("E.txt", "w");
	}
	
	configASSERT(fileE != NULL);
	
	///No Specific order has been listed for Value E, so writing the contents in structure order
	fprintf(fileE, "Line %d: %d %s %d %"PRIu64"\n", fileELineNumber,
		valueEStructure[slotToWrite].randomValueB.stringTime,
		valueEStructure[slotToWrite].randomValueB.stringPlacer,
		valueEStructure[slotToWrite].currentValueD.randomNumberTime,
		valueEStructure[slotToWrite].currentValueD.randomNumber);
	fileELineNumber++;
    fclose(fileE);

	/// Exit the critical session. File operations are over
	portEXIT_CRITICAL();
}

///-----------------------------------------------------------
/// \brief This is the handler for Interrupt G - G key pressed
///         on the keyboard. Pauses A Thread, does E look-up
///         based on the value of A
///
/// @param N/A
///
/// @return N/A
///-----------------------------------------------------------
static void handleInterruptG(void)
{
	int64_t userInputAValue;
	BOOL userInputFound = FALSE;
	/// Suspend Task A
	vTaskSuspend(xTaskAHandle);
	/// Prompt the user for the A Value
	printf("\n Please enter A 12-digit code: ");
	scanf("%lld", &userInputAValue);

	DEBUGPRINT("Value inputted is %" PRIu64 "\n", userInputAValue);

	/// Look up the E based on the A number
	for (int i = 0; i < (SIZE_OF_VALUE_E_STRUCTURE - 1); i++)
	{
		if (userInputAValue == valueEStructure[i].currentValueD.randomNumber)
		{
			///Found the value, print it
			printf("E Value Found. Corresponding B Time = %d, B String = %s", valueEStructure[i].randomValueB.stringTime, valueEStructure[i].randomValueB.stringPlacer);
			userInputFound = TRUE;
		}
	}
	/// Print "Not Found" if the value was not found
	if (userInputFound == FALSE)
	{
		
		printf("Value %" PRIu64 " was not found in List F \n", userInputAValue);
	}
	/// Resume the A Thread
	vTaskResume(xTaskAHandle);
}