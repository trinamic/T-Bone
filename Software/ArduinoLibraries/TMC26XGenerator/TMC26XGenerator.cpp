/*
 TMC26XGenerator.cpp - - TMC26X Stepper library for Wiring/Arduino
 
 based on the stepper library by Tom Igoe, et. al.
 
 Copyright (c) 2011, Interactive Matter, Marcus Nowotny
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */

#if defined(ARDUINO) && ARDUINO >= 100
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif
#include <SPI.h>
#include "TMC26XGenerator.h"

//some default values used in initialization
#define DEFAULT_MICROSTEPPING_VALUE 32

//TMC26X register definitions
#define DRIVER_CONTROL_REGISTER 0x0ul
#define CHOPPER_CONFIG_REGISTER 0x80000ul
#define COOL_STEP_REGISTER  0xA0000ul
#define STALL_GUARD2_LOAD_MEASURE_REGISTER 0xC0000ul
#define DRIVER_CONFIG_REGISTER 0xE0000ul

#define REGISTER_BIT_PATTERN 0xFFFFFul

//definitions for the driver control register
#define MICROSTEPPING_PATTERN 0xFul
#define STEP_INTERPOLATION 0x200ul
#define DOUBLE_EDGE_STEP 0x100ul
#define VSENSE 0x40ul
#define READ_MICROSTEP_POSTION 0x0ul
#define READ_STALL_GUARD_READING 0x10ul
#define READ_STALL_GUARD_AND_COOL_STEP 0x20ul
#define READ_SELECTION_PATTERN 0x30ul

//definitions for the chopper config register
#define CHOPPER_MODE_STANDARD 0x0ul
#define CHOPPER_MODE_T_OFF_FAST_DECAY 0x4000ul
#define T_OFF_PATTERN 0xful
#define RANDOM_TOFF_TIME 0x2000ul
#define BLANK_TIMING_PATTERN 0x18000ul
#define BLANK_TIMING_SHIFT 15
#define HYSTERESIS_DECREMENT_PATTERN 0x1800ul
#define HYSTERESIS_DECREMENT_SHIFT 11
#define HYSTERESIS_LOW_VALUE_PATTERN 0x780ul
#define HYSTERESIS_LOW_SHIFT 7
#define HYSTERESIS_START_VALUE_PATTERN 0x78ul
#define HYSTERESIS_START_VALUE_SHIFT 4
#define T_OFF_TIMING_PATERN 0xFul

//definitions for cool step register
#define MINIMUM_CURRENT_FOURTH 0x8000ul
#define CURRENT_DOWN_STEP_SPEED_PATTERN 0x6000ul
#define SE_MAX_PATTERN 0xF00ul
#define SE_CURRENT_STEP_WIDTH_PATTERN 0x60ul
#define SE_MIN_PATTERN 0xful

//definitions for stall guard2 current register
#define STALL_GUARD_FILTER_ENABLED 0x10000ul
#define STALL_GUARD_TRESHHOLD_VALUE_PATTERN 0x17F00ul
#define CURRENT_SCALING_PATTERN 0x1Ful
#define STALL_GUARD_CONFIG_PATTERN 0x17F00ul
#define STALL_GUARD_VALUE_PATTERN 0x7F00ul

//definitions for the input from the TCM260
#define STATUS_STALL_GUARD_STATUS 0x1ul
#define STATUS_OVER_TEMPERATURE_SHUTDOWN 0x2ul
#define STATUS_OVER_TEMPERATURE_WARNING 0x4ul
#define STATUS_SHORT_TO_GROUND_A 0x8ul
#define STATUS_SHORT_TO_GROUND_B 0x10ul
#define STATUS_OPEN_LOAD_A 0x20ul
#define STATUS_OPEN_LOAD_B 0x40ul
#define STATUS_STAND_STILL 0x80ul
#define READOUT_VALUE_PATTERN 0xFFC00ul

//default values
#define INITIAL_MICROSTEPPING 0x3ul //32th microstepping

//debuging output
//#define DEBUG

/*
 * Constructor
 * number_of_steps - the steps per rotation
 * cs_pin - the SPI client select pin
 * dir_pin - the pin where the direction pin is connected
 * step_pin - the pin where the step pin is connected
 */
TMC26XGenerator::TMC26XGenerator(unsigned int current, unsigned int resistor)
{
    //by default cool step is not enabled
    cool_step_enabled=false;
    this->bridges_enabled=true;

	
    //store the current sense resistor value for later use
    this->resistor = resistor;
	
	//initialize register values
	driver_control_register_value=DRIVER_CONTROL_REGISTER | INITIAL_MICROSTEPPING;
	chopper_config_register=CHOPPER_CONFIG_REGISTER;
	
	//setting the default register values
	driver_control_register_value=DRIVER_CONTROL_REGISTER|INITIAL_MICROSTEPPING;
	microsteps = (1 << INITIAL_MICROSTEPPING);
	chopper_config_register=CHOPPER_CONFIG_REGISTER;
	cool_step_register_value=COOL_STEP_REGISTER;
	stall_guard2_current_register_value=STALL_GUARD2_LOAD_MEASURE_REGISTER;
	driver_configuration_register_value = DRIVER_CONFIG_REGISTER | READ_STALL_GUARD_READING;

	//set the current
	setCurrent(current);
	//set to a conservative start value
	setConstantOffTimeChopper(7, 54, 13,12,1);
    //set a nice microstepping value
    setMicrosteps(DEFAULT_MICROSTEPPING_VALUE);
}

void TMC26XGenerator::setCurrent(unsigned int current) {
	unsigned char current_scaling = this->getCurrentScaling(current, true);
	//check if the current scalingis too low
	if (current_scaling<16) {
        //set the csense bit to get a use half the sense voltage (to support lower motor currents)
		this->driver_configuration_register_value |= VSENSE;
        //and recalculate the current setting
        current_scaling = this->getCurrentScaling(current, false);
#ifdef DEBUG
		Serial.print("CS (Vsense=1): ");
		Serial.println(current_scaling);
	} else {
        Serial.print("CS: ");
        Serial.println(current_scaling);
#endif
    }
	//delete the old value
	stall_guard2_current_register_value &= ~(CURRENT_SCALING_PATTERN);
	//set the new current scaling
	stall_guard2_current_register_value |= current_scaling;
}

unsigned char TMC26XGenerator::getCurrentScaling(unsigned int current, boolean vsense_high) {
    unsigned char current_scaling = 0;
	//calculate the current scaling from the max current setting (in mA)
	double mASetting = (double)current;
    double resistor_value = (double) this->resistor;
	// remove vesense flag
	this->driver_configuration_register_value &= ~(VSENSE);
	//this is derrived from I=(cs+1)/32*(Vsense/Rsense)
    //leading to cs = CS = 32*R*I/V (with V = 0,31V oder 0,165V  and I = 1000*current)
	//with Rsense=0,15
	//for vsense = 0,310V (VSENSE not set)
	//or vsense = 0,165V (VSENSE set)
    if (vsense_high) {
        current_scaling = (byte)((resistor_value*mASetting*32.0/(0.31*1000.0*1000.0))-0.5); //theoretically - 1.0 for better rounding it is 0.5
    } else {
        current_scaling = (byte)((resistor_value*mASetting*32.0/(0.165*1000.0*1000.0))-0.5); //theoretically - 1.0 for better rounding it is 0.5
    }
	//do some sanity checks
	if (current_scaling>31) {
		current_scaling=31;
	}
    return current_scaling;
}

unsigned int TMC26XGenerator::getCurrent(void) {
    //we calculate the current according to the datasheet to be on the safe side
    //this is not the fastest but the most accurate and illustrative way
    double result = (double)(stall_guard2_current_register_value & CURRENT_SCALING_PATTERN);
    double resistor_value = (double)this->resistor;
    double voltage = (driver_configuration_register_value & VSENSE)? 0.165:0.31;
    result = (result+1.0)/32.0*voltage/resistor_value*1000.0*1000.0;
    return (unsigned int)result;
}

void TMC26XGenerator::setStallGuardThreshold(char stall_guard_threshold, char stall_guard_filter_enabled) {
	if (stall_guard_threshold<-64) {
		stall_guard_threshold = -64;
	//We just have 5 bits	
	} else if (stall_guard_threshold > 63) {
		stall_guard_threshold = 63;
	}
	//add trim down to 7 bits
	stall_guard_threshold &=0x7f;
	//delete old stall guard settings
	stall_guard2_current_register_value &= ~(STALL_GUARD_CONFIG_PATTERN);
	if (stall_guard_filter_enabled) {
		stall_guard2_current_register_value |= STALL_GUARD_FILTER_ENABLED;
	}
	//Set the new stall guard threshold
	stall_guard2_current_register_value |= (((unsigned long)stall_guard_threshold << 8) & STALL_GUARD_CONFIG_PATTERN);
}

char TMC26XGenerator::getStallGuardThreshold(void) {
    unsigned long stall_guard_threshold = stall_guard2_current_register_value & STALL_GUARD_VALUE_PATTERN;
    //shift it down to bit 0
    stall_guard_threshold >>=8;
    //convert the value to an int to correctly handle the negative numbers
    char result = stall_guard_threshold;
    //check if it is negative and fill it up with leading 1 for proper negative number representation
    if (result & _BV(6)) {
        result |= 0xC0;
    }
    return result;
}

char TMC26XGenerator::getStallGuardFilter(void) {
    if (stall_guard2_current_register_value & STALL_GUARD_FILTER_ENABLED) {
        return -1;
    } else {
        return 0;
    }
}
/*
 * Set the number of microsteps per step.
 * 0,2,4,8,16,32,64,128,256 is supported
 * any value in between will be mapped to the next smaller value
 * 0 and 1 set the motor in full step mode
 */
void TMC26XGenerator::setMicrosteps(int number_of_steps) {
	long setting_pattern;
	//poor mans log
	if (number_of_steps>=256) {
		setting_pattern=0;
		microsteps=256;
	} else if (number_of_steps>=128) {
		setting_pattern=1;
		microsteps=128;
	} else if (number_of_steps>=64) {
		setting_pattern=2;
		microsteps=64;
	} else if (number_of_steps>=32) {
		setting_pattern=3;
		microsteps=32;
	} else if (number_of_steps>=16) {
		setting_pattern=4;
		microsteps=16;
	} else if (number_of_steps>=8) {
		setting_pattern=5;
		microsteps=8;
	} else if (number_of_steps>=4) {
		setting_pattern=6;
		microsteps=4;
	} else if (number_of_steps>=2) {
		setting_pattern=7;
		microsteps=2;
    //1 and 0 lead to full step
	} else if (number_of_steps<=1) {
		setting_pattern=8;
		microsteps=1;
	}
#ifdef DEBUG
	Serial.print("Microstepping: ");
	Serial.println(microsteps);
#endif
	//delete the old value
	this->driver_control_register_value &=0xFFFF0ul;
	//set the new value
	this->driver_control_register_value |=setting_pattern;
}

/*
 * returns the effective number of microsteps at the moment
 */
int TMC26XGenerator::getMicrosteps(void) {
	return microsteps;
}

/*
 * constant_off_time: The off time setting controls the minimum chopper frequency. 
 * For most applications an off time within	the range of 5μs to 20μs will fit.
 *		2...15: off time setting
 *
 * blank_time: Selects the comparator blank time. This time needs to safely cover the switching event and the
 * duration of the ringing on the sense resistor. For
 *		0: min. setting 3: max. setting
 *
 * fast_decay_time_setting: Fast decay time setting. With CHM=1, these bits control the portion of fast decay for each chopper cycle.
 *		0: slow decay only
 *		1...15: duration of fast decay phase
 *
 * sine_wave_offset: Sine wave offset. With CHM=1, these bits control the sine wave offset. 
 * A positive offset corrects for zero crossing error.
 *		-3..-1: negative offset 0: no offset 1...12: positive offset
 *
 * use_current_comparator: Selects usage of the current comparator for termination of the fast decay cycle. 
 * If current comparator is enabled, it terminates the fast decay cycle in case the current 
 * reaches a higher negative value than the actual positive value.
 *		1: enable comparator termination of fast decay cycle
 *		0: end by time only
 */
void TMC26XGenerator::setConstantOffTimeChopper(char constant_off_time, char blank_time, char fast_decay_time_setting, char sine_wave_offset, unsigned char use_current_comparator) {
	//perform some sanity checks
	if (constant_off_time<2) {
		constant_off_time=2;
	} else if (constant_off_time>15) {
		constant_off_time=15;
	}
    //save the constant off time
    this->constant_off_time = constant_off_time;
	char blank_value;
	//calculate the value acc to the clock cycles
	if (blank_time>=54) {
		blank_value=3;
	} else if (blank_time>=36) {
		blank_value=2;
	} else if (blank_time>=24) {
		blank_value=1;
	} else {
		blank_value=0;
	}
	if (fast_decay_time_setting<0) {
		fast_decay_time_setting=0;
	} else if (fast_decay_time_setting>15) {
		fast_decay_time_setting=15;
	}
	if (sine_wave_offset < -3) {
		sine_wave_offset = -3;
	} else if (sine_wave_offset>12) {
		sine_wave_offset = 12;
	}
	//shift the sine_wave_offset
	sine_wave_offset +=3;
	
	//calculate the register setting
	//first of all delete all the values for this
	chopper_config_register &= ~((1<<12) | BLANK_TIMING_PATTERN | HYSTERESIS_DECREMENT_PATTERN | HYSTERESIS_LOW_VALUE_PATTERN | HYSTERESIS_START_VALUE_PATTERN | T_OFF_TIMING_PATERN);
	//set the constant off pattern
	chopper_config_register |= CHOPPER_MODE_T_OFF_FAST_DECAY;
	//set the blank timing value
	chopper_config_register |= ((unsigned long)blank_value) << BLANK_TIMING_SHIFT;
	//setting the constant off time
    if (this->bridges_enabled) {
        chopper_config_register |= constant_off_time;
    }
	//set the fast decay time
	//set msb
	chopper_config_register |= (((unsigned long)(fast_decay_time_setting & 0x8))<<HYSTERESIS_DECREMENT_SHIFT);
	//other bits
	chopper_config_register |= (((unsigned long)(fast_decay_time_setting & 0x7))<<HYSTERESIS_START_VALUE_SHIFT);
	//set the sine wave offset
	chopper_config_register |= (unsigned long)sine_wave_offset << HYSTERESIS_LOW_SHIFT;
	//using the current comparator?
	if (!use_current_comparator) {
		chopper_config_register |= (1<<12);
	}
}

/*
 * constant_off_time: The off time setting controls the minimum chopper frequency. 
 * For most applications an off time within	the range of 5μs to 20μs will fit.
 *		2...15: off time setting
 *
 * blank_time: Selects the comparator blank time. This time needs to safely cover the switching event and the
 * duration of the ringing on the sense resistor. For
 *		0: min. setting 3: max. setting
 *
 * hysteresis_start: Hysteresis start setting. Please remark, that this value is an offset to the hysteresis end value HEND.
 *		1...8
 *
 * hysteresis_end: Hysteresis end setting. Sets the hysteresis end value after a number of decrements. Decrement interval time is controlled by HDEC. 
 * The sum HSTRT+HEND must be <16. At a current setting CS of max. 30 (amplitude reduced to 240), the sum is not limited.
 *		-3..-1: negative HEND 0: zero HEND 1...12: positive HEND
 *
 * hysteresis_decrement: Hysteresis decrement setting. This setting determines the slope of the hysteresis during on time and during fast decay time.
 *		0: fast decrement 3: very slow decrement
 */

void TMC26XGenerator::setSpreadCycleChopper(char constant_off_time, char blank_time, char hysteresis_start, char hysteresis_end, char hysteresis_decrement) {
	//perform some sanity checks
	if (constant_off_time<2) {
		constant_off_time=2;
	} else if (constant_off_time>15) {
		constant_off_time=15;
	}
    //save the constant off time
    this->constant_off_time = constant_off_time;
	char blank_value;
	//calculate the value acc to the clock cycles
	if (blank_time>=54) {
		blank_value=3;
	} else if (blank_time>=36) {
		blank_value=2;
	} else if (blank_time>=24) {
		blank_value=1;
	} else {
		blank_value=0;
	}
	if (hysteresis_start<1) {
		hysteresis_start=1;
	} else if (hysteresis_start>8) {
		hysteresis_start=8;
	}
	hysteresis_start--;

	if (hysteresis_end < -3) {
		hysteresis_end = -3;
	} else if (hysteresis_end>12) {
		hysteresis_end = 12;
	}
	//shift the hysteresis_end
	hysteresis_end +=3;

	if (hysteresis_decrement<0) {
		hysteresis_decrement=0;
	} else if (hysteresis_decrement>3) {
		hysteresis_decrement=3;
	}
	
	//first of all delete all the values for this
	chopper_config_register &= ~(CHOPPER_MODE_T_OFF_FAST_DECAY | BLANK_TIMING_PATTERN | HYSTERESIS_DECREMENT_PATTERN | HYSTERESIS_LOW_VALUE_PATTERN | HYSTERESIS_START_VALUE_PATTERN | T_OFF_TIMING_PATERN);

	//set the blank timing value
	chopper_config_register |= ((unsigned long)blank_value) << BLANK_TIMING_SHIFT;
	//setting the constant off time
    if (this->bridges_enabled) {
        chopper_config_register |= constant_off_time;
    }
	//set the hysteresis_start
	chopper_config_register |= ((unsigned long)hysteresis_start) << HYSTERESIS_START_VALUE_SHIFT;
	//set the hysteresis end
	chopper_config_register |= ((unsigned long)hysteresis_end) << HYSTERESIS_LOW_SHIFT;
	//set the hystereis decrement
	chopper_config_register |= ((unsigned long)blank_value) << BLANK_TIMING_SHIFT;
}

/*
 * In a constant off time chopper scheme both coil choppers run freely, i.e. are not synchronized. 
 * The frequency of each chopper mainly depends on the coil current and the position dependant motor coil inductivity, thus it depends on the microstep position. 
 * With some motors a slightly audible beat can occur between the chopper frequencies, especially when they are near to each other. This typically occurs at a 
 * few microstep positions within each quarter wave. This effect normally is not audible when compared to mechanical noise generated by ball bearings, etc. 
 * Further factors which can cause a similar effect are a poor layout of sense resistor GND connection.
 * Hint: A common factor, which can cause motor noise, is a bad PCB layout causing coupling of both sense resistor voltages 
 * (please refer to sense resistor layout hint in chapter 8.1).
 * In order to minimize the effect of a beat between both chopper frequencies, an internal random generator is provided. 
 * It modulates the slow decay time setting when switched on by the RNDTF bit. The RNDTF feature further spreads the chopper spectrum, 
 * reducing electromagnetic emission on single frequencies.
 */
void TMC26XGenerator::setRandomOffTime(char value) {
	if (value) {
		chopper_config_register |= RANDOM_TOFF_TIME;
	} else {
		chopper_config_register &= ~(RANDOM_TOFF_TIME);
	}
}	

void TMC26XGenerator::setCoolStepConfiguration(unsigned int lower_SG_threshold, unsigned int SG_hysteresis, unsigned char current_decrement_step_size,
                              unsigned char current_increment_step_size, unsigned char lower_current_limit) {
    //sanitize the input values
    if (lower_SG_threshold>480) {
        lower_SG_threshold = 480;
    }
    //divide by 32
    lower_SG_threshold >>=5;
    if (SG_hysteresis>480) {
        SG_hysteresis=480;
    }
    //divide by 32
    SG_hysteresis >>=5;
    if (current_decrement_step_size>3) {
        current_decrement_step_size=3;
    }
    if (current_increment_step_size>3) {
        current_increment_step_size=3;
    }
    if (lower_current_limit>1) {
        lower_current_limit=1;
    }
    //store the lower level in order to enable/disable the cool step
    this->cool_step_lower_threshold=lower_SG_threshold;
    //if cool step is not enabled we delete the lower value to keep it disabled
    if (!this->cool_step_enabled) {
        lower_SG_threshold=0;
    }
    //the good news is that we can start with a complete new cool step register value
    //and simply set the values in the register
    cool_step_register_value = ((unsigned long)lower_SG_threshold) | (((unsigned long)SG_hysteresis)<<8) | (((unsigned long)current_decrement_step_size)<<5)
        | (((unsigned long)current_increment_step_size)<<13) | (((unsigned long)lower_current_limit)<<15)
        //and of course we have to include the signature of the register
        | COOL_STEP_REGISTER;
    //Serial.println(cool_step_register_value,HEX);
 }

void TMC26XGenerator::setCoolStepEnabled(boolean enabled) {
    //simply delete the lower limit to disable the cool step
    cool_step_register_value &= ~SE_MIN_PATTERN;
    //and set it to the proper value if cool step is to be enabled
    if (enabled) {
        cool_step_register_value |=this->cool_step_lower_threshold;
    }
    //and save the enabled status
    this->cool_step_enabled = enabled;
  }

boolean TMC26XGenerator::isCoolStepEnabled(void) {
    return this->cool_step_enabled;
}

unsigned int TMC26XGenerator::getCoolStepLowerSgThreshold() {
    //we return our internally stored value - in order to provide the correct setting even if cool step is not enabled
    return this->cool_step_lower_threshold<<5;
}

unsigned int TMC26XGenerator::getCoolStepUpperSgThreshold() {
    return (unsigned char)((cool_step_register_value & SE_MAX_PATTERN)>>8)<<5;
}

unsigned char TMC26XGenerator::getCoolStepCurrentIncrementSize() {
    return (unsigned char)((cool_step_register_value & CURRENT_DOWN_STEP_SPEED_PATTERN)>>13);
}

unsigned char TMC26XGenerator::getCoolStepNumberOfSGReadings() {
    return (unsigned char)((cool_step_register_value & SE_CURRENT_STEP_WIDTH_PATTERN)>>5);
}

unsigned char TMC26XGenerator::getCoolStepLowerCurrentLimit() {
    return (unsigned char)((cool_step_register_value & MINIMUM_CURRENT_FOURTH)>>15);
}

void TMC26XGenerator::setEnabled(boolean enabled) {
    //delete the t_off in the chopper config to get sure
    chopper_config_register &= ~(T_OFF_PATTERN);
    if (enabled) {
        //and set the t_off time
        chopper_config_register |= this->constant_off_time;
    }
    this->bridges_enabled = enabled;
}

boolean TMC26XGenerator::isEnabled() {
    return this->bridges_enabled;
}

/*
 * reads a value from the TMC26X status register. The value is not obtained directly but can then 
 * be read by the various status routines.
 *
 */
unsigned long TMC26XGenerator::setReadStatus(char read_value) {
    //reset the readout configuration
	driver_configuration_register_value &= ~(READ_SELECTION_PATTERN);
	//this now equals TMC26X_READOUT_POSITION - so we just have to check the other two options
	if (read_value == TMC26X_READOUT_STALLGUARD) {
		driver_configuration_register_value |= READ_STALL_GUARD_READING;
	} else if (read_value == TMC26X_READOUT_CURRENT) {
		driver_configuration_register_value |= READ_STALL_GUARD_AND_COOL_STEP;
	}
	//all other cases are ignored to prevent funny values
}

int TMC26XGenerator::getMotorPosition(void) {
	//we read it out even if we are not started yet - perhaps it is useful information for somebody
	//readStatus(TMC26X_READOUT_POSITION);
    return getReadoutValue();
}

//reads the stall guard setting from last status
//returns -1 if stallguard information is not present
int TMC26XGenerator::getCurrentStallGuardReading(void) {
	//not time optimal, but solution optiomal:
	//first read out the stall guard value
	//readStatus(TMC26X_READOUT_STALLGUARD);
	return getReadoutValue();
}

unsigned char TMC26XGenerator::getCurrentCSReading(void) {
	//not time optimal, but solution optiomal:
	//first read out the stall guard value
	//readStatus(TMC26X_READOUT_CURRENT);
	return (getReadoutValue() & 0x1f);
}

unsigned int TMC26XGenerator::getCurrentCurrent(void) {
    double result = (double)getCurrentCSReading();
    double resistor_value = (double)this->resistor;
    double voltage = (driver_configuration_register_value & VSENSE)? 0.165:0.31;
    result = (result+1.0)/32.0*voltage/resistor_value*1000.0*1000.0;
    return (unsigned int)result;
}

/*
 return true if the stallguard threshold has been reached
*/
boolean TMC26XGenerator::isStallGuardOverThreshold(void) {
	return (driver_status_result & STATUS_STALL_GUARD_STATUS);
}

/*
 returns if there is any over temperature condition:
 OVER_TEMPERATURE_PREWARING if pre warning level has been reached
 OVER_TEMPERATURE_SHUTDOWN if the temperature is so hot that the driver is shut down
 Any of those levels are not too good.
*/
char TMC26XGenerator::getOverTemperature(void) {
	if (driver_status_result & STATUS_OVER_TEMPERATURE_SHUTDOWN) {
		return TMC26X_OVERTEMPERATURE_SHUTDOWN;
	}
	if (driver_status_result & STATUS_OVER_TEMPERATURE_WARNING) {
		return TMC26X_OVERTEMPERATURE_PREWARING;
	}
	return 0;
}

//is motor channel A shorted to ground
boolean TMC26XGenerator::isShortToGroundA(void) {
	return (driver_status_result & STATUS_SHORT_TO_GROUND_A);
}

//is motor channel B shorted to ground
boolean TMC26XGenerator::isShortToGroundB(void) {
	return (driver_status_result & STATUS_SHORT_TO_GROUND_B);
}

//is motor channel A connected
boolean TMC26XGenerator::isOpenLoadA(void) {
	return (driver_status_result & STATUS_OPEN_LOAD_A);
}

//is motor channel B connected
boolean TMC26XGenerator::isOpenLoadB(void) {
	return (driver_status_result & STATUS_OPEN_LOAD_B);
}

//is chopper inactive since 2^20 clock cycles - defaults to ~0,08s
boolean TMC26XGenerator::isStandStill(void) {
	return (driver_status_result & STATUS_STAND_STILL);
}

//is chopper inactive since 2^20 clock cycles - defaults to ~0,08s
boolean TMC26XGenerator::isStallGuardReached(void) {
	return (driver_status_result & STATUS_STALL_GUARD_STATUS);
}

//reads the stall guard setting from last status
//returns -1 if stallguard inforamtion is not present
int TMC26XGenerator::getReadoutValue(void) {
	return (int)(driver_status_result >> 10);
}

int TMC26XGenerator::getResistor() {
    return this->resistor;
}

boolean TMC26XGenerator::isCurrentScalingHalfed() {
    if (this->driver_configuration_register_value & VSENSE) {
        return true;
    } else {
        return false;
    }
}

void TMC26XGenerator::debugLastStatus() {
#ifdef DEBUG    
		if (this->getOverTemperature()&TMC26X_OVERTEMPERATURE_PREWARING) {
			Serial.println("WARNING: Overtemperature Prewarning!");
		} else if (this->getOverTemperature()&TMC26X_OVERTEMPERATURE_SHUTDOWN) {
			Serial.println("ERROR: Overtemperature Shutdown!");
		}
		if (this->isShortToGroundA()) {
			Serial.println("ERROR: SHORT to ground on channel A!");
		}
		if (this->isShortToGroundB()) {
			Serial.println("ERROR: SHORT to ground on channel A!");
		}
		if (this->isOpenLoadA()) {
			Serial.println("ERROR: Channel A seems to be unconnected!");
		}
		if (this->isOpenLoadB()) {
			Serial.println("ERROR: Channel B seems to be unconnected!");
		}
		if (this->isStallGuardReached()) {	
			Serial.println("INFO: Stall Guard level reached!");
		}
		if (this->isStandStill()) {
			Serial.println("INFO: Motor is standing still.");
		}
		unsigned long readout_config = driver_configuration_register_value & READ_SELECTION_PATTERN;
		int value = getReadoutValue();
		if (readout_config == READ_MICROSTEP_POSTION) {
			Serial.print("Microstep postion phase A: ");
			Serial.println(value);
		} else if (readout_config == READ_STALL_GUARD_READING) {
			Serial.print("Stall Guard value:");
			Serial.println(value);
		} else if (readout_config == READ_STALL_GUARD_AND_COOL_STEP) {
			int stallGuard = value & 0xf;
			int current = value & 0x1F0;
			Serial.print("Approx Stall Guard: ");
			Serial.println(stallGuard);
			Serial.print("Current level");
			Serial.println(current);
		}
#endif
}

unsigned long TMC26XGenerator::getDriverControlRegisterValue() {
	return this->driver_control_register_value;
}

unsigned long TMC26XGenerator::getChopperConfigRegisterValue() {
	return this->chopper_config_register;
}

unsigned long TMC26XGenerator::getCoolStepConfigRegisterValue() {
	return this->cool_step_register_value;
}

unsigned long TMC26XGenerator::getStallGuard2RegisterValue() {
	return this->stall_guard2_current_register_value;
}

unsigned long TMC26XGenerator::getDriverConfigurationRegisterValue(){
	return this->driver_configuration_register_value;
}

void TMC26XGenerator::setDriverStatusResult(unsigned long driver_status_result) {
	this->driver_status_result = driver_status_result;
}
