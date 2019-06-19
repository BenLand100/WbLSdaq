#ifndef _LAPPD_HIGH_VOLTAGE_CONTROL_H_
#define _LAPPD_HIGH_VOLTAGE_CONTROL_H_

#include <iostream>
#include <vector> 
using std::vector;
#include "HVInterface.hh"

class LAPPDHighVoltageControl
{
public:
  //auto_init set to true will call syncFromHWState and if that fails, gotoSafeHWState
  LAPPDHighVoltageControl(HVInterface* a_interface, bool auto_init = false);

  void setDefaultVoltages();

  //Checks to see if all channels are on, and if so, store their measured HV values
  //as the relative voltage offsets and return true. This initilizes the class.
  //If all channels are not on, do nothing and return false.
  bool syncFromHWState();
  //If syncFromHWState fails, call this method to put the HW in a safe state with
  //all channels off and setpoints at zero. This initializes the class.
  void gotoSafeHWState();

  ///used to turn the power on
  int powerOn();
  ///used to turn the power off
  int powerOff();
  ///return the absolute set points of the individual power points of the lappd
  ///exitGapV exitMCPV entryGapV entryMCPV photoCathodeV
  vector<double> getAbsoluteVoltages();
  std::ostream& printAbsoluteVoltages(std::ostream& a_stream = std::cout);
  ///sets the voltage across the bottom gap in the mcp stack
  ///If power is on changes will be adopted immediately
  int setExitGapVoltage(double a_volts);
  ///set the voltage between the exit of the entry mcp and 
  ///the entry of the exit mcp
  ///If power is on changes will be adopted immediately
  int setEntryGapVoltage(double a_volts);
  ///sets the voltage for both gaps
  ///If power is on changes will be adopted immediately
  int setGapVoltages(double a_volts);
  ///set the voltage across the exit mcp
  ///If power is on changes will be adopted immediately
  int setExitMCPVoltage(double a_volts);
  ///set the voltage across the entry mcp
  ///If power is on changes will be adopted immediately
  int setEntryMCPVoltage(double a_volts);
  ///set the voltage for both mcps
  ///If power is on changes will be adopted immediately
  int setMCPVoltages(double a_volts);
  ///set the voltage for the photocathod 
  ///If power is on changes will be adopted immediately
  int setPhotocathodeVoltage(double a_volts);

  ///returns the currents or the power supplies vector has the form
  ///exitGapI exitMCPI entryGapI entryMCPI photoCathodeI
  vector<double> getCurrents();

  int updateVoltage(bool a_initialPowerOn = false);
  

private:
  ///need to supply a controller for a working class
  LAPPDHighVoltageControl(){};

  HVInterface* m_voltageController;
  ///this vector is used to store the voltages for the 
  ///the individual components of the mcp relative to the
  ///voltage of the last compontent 
  ///the individual components are 
  ///exitGapV exitMCPV entryGapV entryMCPV photoCathodeV 
  
  vector<float> m_relativeVoltages;
  
  //true if this class is synchronized with HW state and it's safe to do things
  //false means the class does not yet know the HW state and changes are unsafe
  //  syncFromHWState or gotoSafeHWState
  bool m_initialized; 

  //true if the LAPPD is known to be at HV
  bool m_isPowered;

};

#endif
