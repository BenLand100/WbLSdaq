#include "HVInterface.hh"

#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

HVInterface::HVInterface(int a_numChannels)
://initializer list
m_numChannels(a_numChannels)
{
//nothing to do
}


// int HVInterface::writeState(string a_file)
// {
//   std::ofstream outFile;
//   outFile.open(a_file);
//   if(!outFile.good())
//   {
//     std::cout << "Not able to open file" << std::endl;
//     return 1;
//   }
//   for(int iC = 0; iC < m_numChannels;iC++)
//   {
//     outFile << iC << m_hvSetPoint[iC] << m_ISetPoint[iC] << m_rampSet[iC];
//     outFile << std::endl;
//   }
//   outFile.close();
// }

// int HVInterface::readSetup(string a_file)
// {
//   std::ifstream inFile;
//   inFile.open(a_file);
//   if(!inFile.good())
//   {
//     std::cout << "Not able to open file" << std::endl;
//     return 1;
//   }
//   std::string line;
//   vector<int>    channels;
//   vector<float> voltages;
//   vector<float> currents;
//   vector<float> rampRates;
//   while (std::getline(inFile, line)) 
//   {
//     std::istringstream iss(line);
//     int channel= -1;
//     float voltage = -1;
//     float current = -1;
//     float rampRate = -1;
//     iss >> channel >> voltage >> current >> rampRate;
//     channels.push_back(channel);
//     voltages.push_back(voltage);
//     currents.push_back(current);
//     rampRates.push_back(rampRate);
//   }
//   int maxChannel = *std::max_element(channels.begin(),channels.end());
//   m_hvSetPoint.clear();
//   m_ISetPoint.clear();
//   m_rampSet.clear();
//   m_voltage.clear();
//   m_current.clear();
//   m_power.clear();
//   m_hvSetPoint.resize(maxChannel,0);
//   m_ISetPoint.resize(maxChannel,0);
//   m_rampSet.resize(maxChannel,0);
//   m_voltage.resize(maxChannel,0);
//   m_current.resize(maxChannel,0);
//   m_power.resize(maxChannel,0);

//   for(int iC = 0; iC < channels.size(); iC++)
//   {
//     m_hvSetPoint[channels[iC]] = voltages[iC];
//     m_ISetPoint[channels[iC]] = currents[iC];
//     m_rampSet[channels[iC]] = rampRates[iC];
//   }
// }

int HVInterface::setHVExact(int a_channel, float a_voltage, float v_tol, double P, double I, int min_steps, int max_steps)
{
  float offP = 0;
  float offI = 0;
  float voltsNow = getMeasuredHV(a_channel);
  while(abs(voltsNow-a_voltage)>v_tol)
  {
    int loopCount = 0;
    setHV(a_channel,a_voltage+offP+offI);
    do {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      std::cout << "waiting for ramping to complete " << loopCount*3;
      std::cout << " seconds so far on channel " << a_channel;
      std::cout << "  volts =  " << a_voltage;
      std::cout << std::endl;
      loopCount++;
    } while(isRamping(a_channel));
    std::this_thread::sleep_for(std::chrono::seconds(3));
    // int goodTime = 0;
    // float last = getMeasuredHV(a_channel);
    // for (int i = 0; i < max_steps; i++) 
    // {
    //   std::this_thread::sleep_for(std::chrono::seconds(3));
    //   float cur = getMeasuredHV(a_channel);

    //   if (abs(cur-last) < v_tol) goodTime++;
    //   if (goodTime >= min_steps) break;

    //   last = cur;
    // }
    //if (goodTime < min_steps) return 1;
    float last = getMeasuredHV(a_channel);
    offP = (a_voltage-last)*P;
    offI += offP*I;
    voltsNow = last;
  }
  return 0;
}

void HVInterface::calibrate(int ch, bool danger) 
{
  if (!danger) {
    std::cout << "Set danger to true to confirm HV is disconnected before calibrating" << std::endl;
    return;
  }
  const float v_test[] = {100,500,1000,2000}; 
  const size_t npts = sizeof(v_test)/sizeof(v_test[0]);
  float *v_meas = new float[npts];

  float oldHV = getHV(ch);
  float oldRate = getRamp(ch);
  setRamp(ch,0); //assume this means max speed
  powerOn(ch);

  for (size_t vi = 0; vi < npts; vi++) {
    setHV(ch,v_test[vi]);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    float last = getMeasuredHV(ch);
    for (int i = 0; i < 30; i++) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      float cur = getMeasuredHV(ch);
      if (fabs(cur-last) < 1 && fabs((cur-v_test[vi])/v_test[vi]) < 0.2) break;
      last = cur;
    }
    v_meas[vi] = getMeasuredHV(ch);
    std::cout << "ch " << ch << " :: v_test = " << v_test[vi] << " -> v_meas = "  << v_meas[vi] << std::endl;
  }

  setHV(ch,oldHV);
  setRamp(ch,oldRate);
  powerOff(ch);
}

