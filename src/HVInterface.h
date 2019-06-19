#ifndef _HV_INTERFACE_H_
#define _HV_INTERFACE_H_

#include <vector>
using std::vector;
#include <string>
using std::string;
#include <iostream>
#include <fstream>

///this class is intended as an abstract base class to provide
///an interface between a specific controller and an application
///specific implementation
class HVInterface
{
public:
  ///used to construct the class for a specific number of channels
  HVInterface(int a_numChannels=4);
  ///used to set the high voltage set point in volts
  virtual int setHV(int a_channel, float a_voltage) = 0;
  ///used to get the high voltage set point in volts
  virtual float getHV(int a_channel) = 0;
  ///used for reading the applied high voltage in volts
  virtual float getMeasuredHV(int a_channel) = 0;
  ///used to find the setpoint that results in the desired measured voltage
  virtual int setHVExact(int    a_channel,
                         float  a_voltage,
                         float  v_tol=3,
                         double P=0.9,
                         double I=0.05,
                         int    min_steps=3,
                         int    max_steps=30
                        );
  ///used to set the current limit in milliamps
  virtual int setCurrent(int a_channel,float a_current) = 0;
  ///used to get the current limit in milliamps
  virtual float getCurrent(int a_channel) = 0;
  ///used for reading the sourced current in milliamps
  virtual float getMeasuredCurrent(int a_channel) = 0;
  ///used to set the ramp in V/s
  virtual int setRamp(int a_channel,float a_ramp) = 0;
  ///used to get the ramp in V/s
  virtual float getRamp(int a_channel) = 0;
  ///used to power on channels if -1 is sent all channels will be powered on
  virtual int powerOn(int a_channel = -1) = 0;
  ///used to power on channels if -1 is sent all channels will be powered on
  virtual int powerOff(int a_channel = -1) = 0;
  ///used to check whether a channel is on or off
  virtual bool isPowered(int a_channel) = 0;
  
  virtual bool isRamping(int a_channel) = 0;


  // ///used to read a setup
  // int readSetup(string a_file ="hvSetup.dat");
  // ///used to write the state to disk
  // int writeState(string a_file ="hvSetup.dat");

  void calibrate(int ch, bool danger=false);

protected:
  ///stores the number of channels
  int m_numChannels;

};

#endif