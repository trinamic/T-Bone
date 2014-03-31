//here we go with the real homing action
void find_sg_value() {
  writeRegister(motor_to_test, TMC4361_START_CONFIG_REGISTER,0 
  | _BV(5) //start is start
  | _BV(0) //X_TARGET needs start
  | _BV(1) //V_MAX needs start
  ); 
  writeRegister(motor_to_test, TMC4361_RAMP_MODE_REGISTER,0); //nice S-Ramps in velocity mode

  motors[motor_to_test].tmc260.setReadStatus(TMC26X_READOUT_STALLGUARD);
  set260Register(motor_to_test,motors[motor_to_test].tmc260.getDriverConfigurationRegisterValue() | 0x80);

  writeRegister(motor_to_test, TMC4361_A_MAX_REGISTER, FIXED_22_2_MAKE(fast_run*10));
  writeRegister(motor_to_test, TMC4361_BOW_1_REGISTER, fast_run*10);
  writeRegister(motor_to_test, TMC4361_BOW_2_REGISTER, fast_run*10);
  writeRegister(motor_to_test, TMC4361_BOW_3_REGISTER, fast_run*10);
  writeRegister(motor_to_test, TMC4361_BOW_4_REGISTER, fast_run*10);

  writeRegister(motor_to_test, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(slow_run));
  signal_start();

  for (char threshold=-63;threshold<=64;threshold++) {
    motors[motor_to_test].tmc260.setStallGuardThreshold(threshold,0);
    set260Register(motor_to_test,motors[motor_to_test].tmc260.getStallGuard2RegisterValue());
    long result = (long)readRegister(motor_to_test,TMC4361_COVER_DRIVER_LOW_REGISTER);
    result = (result >> 10);
    Serial.print(threshold,DEC);
    Serial.print(" -> ");
    Serial.println(result,HEX);
  }
  Serial.println();

  writeRegister(motor_to_test, TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(0));
  signal_start();

  writeRegister(motor_to_test, TMC4361_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps
}





