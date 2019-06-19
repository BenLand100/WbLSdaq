#include "LAPPDHighVoltageControl.h"

#include <thread>
#include <chrono>

LAPPDHighVoltageControl::LAPPDHighVoltageControl(HVInterface* a_interface, bool auto_init)
:
m_voltageController(a_interface),
m_relativeVoltages(5,0),
m_isPowered(false),
m_initialized(false)
{
  setDefaultVoltages();
  if (auto_init && !syncFromHWState()) gotoSafeHWState();
}

void LAPPDHighVoltageControl::setDefaultVoltages()
{
  m_relativeVoltages[0]=200.;
  m_relativeVoltages[1]=1000.;
  m_relativeVoltages[2]=200.;
  m_relativeVoltages[3]=1000.;
  m_relativeVoltages[4]=50;
}

bool LAPPDHighVoltageControl::syncFromHWState()
{
  bool all_on = true;
  bool all_off = true;
  for (int ch = 0; ch < 5; ch++) {
    bool powered = m_voltageController->isPowered(ch);
    all_on &= powered;
    all_off &= !powered;
  }
  if (all_on) {
    //The channels are already on, assume the LAPPD is on and store the measured 
    //voltages as the relative setpoints. This is necessary because the offset
    //to the setpoints to achieve the measured voltages are unknown.
    float lastHV = 0;
    for (int ch = 0; ch < 5; ch++) {
      float measuredHV = m_voltageController->getMeasuredHV(ch);
      m_relativeVoltages[ch] = measuredHV - lastHV;
      lastHV = measuredHV;
    }
    m_initialized = true;
    m_isPowered = true;
  } else if (all_off) {
    m_initialized = true;
    m_isPowered = false;
  }
  return all_on || all_off;
}

//Turns channels off set fixes setpoints
void LAPPDHighVoltageControl::gotoSafeHWState()
{
  for (int ch = 4; ch >= 0; ch--) {
    m_voltageController->setHV(ch,0);
    m_voltageController->powerOff(ch);
  }
  m_initialized = true;
  m_isPowered = false;
}

///used to turn the power on
int LAPPDHighVoltageControl::powerOn()
{
  if (!m_initialized) {
    std::cout << "Class not initialized, giving up." << std::endl;
    return 1;
  }
  if(!m_isPowered)
  {
    updateVoltage(); //this is _critical_ before turning on
    for(int iC = 0; iC < 5; iC++)
    {
      m_voltageController->powerOn(iC);
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
      updateVoltage(true);
    m_isPowered = true;
    return 0;
  }
  else
  {
    std::cout << "Device Appears to already be powered" << std::endl;
    return 1;
  }
  //how would I get here?
  return 1;
}
int LAPPDHighVoltageControl::powerOff()
{
  if (!m_initialized) {
    std::cout << "Class not initialized, giving up." << std::endl;
    return 1;
  }
  if(m_isPowered)
  {
    double volts =0;
    for(int iC = 4; iC >=0; iC--)
    {
      m_voltageController->powerOff(iC);
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    m_isPowered = false;
    return 0;
  }
  else
  {
    std::cout << "device appears to already be off " << std::endl;
    return 1;
  }
  //how would I get here
  return 1;
}
///return the absolute set points of the individual power points of the lappd
///exitGapV exitMCPV entryGapV entryMCPV photoCathodeV
vector<double> LAPPDHighVoltageControl::
getAbsoluteVoltages()
{
  double volts = 0;
  vector<double> absVolts;
  for(int iC = 0; iC < 5;iC++)
  {
    volts+=m_relativeVoltages[iC];
    absVolts.push_back(volts);
  }
  return absVolts;
}
std::ostream& LAPPDHighVoltageControl:: 
printAbsoluteVoltages(std::ostream& a_stream)
{
  auto absVolts = getAbsoluteVoltages();
  for (auto & voltage : absVolts)
  {
    a_stream << voltage << " ";
  }
  return a_stream;
}
///sets the voltage across the bottom gap in the mcp stack
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setExitGapVoltage(double a_volts)
{
  if(a_volts < 250 && a_volts >= 0)
  {
    m_relativeVoltages[0] = a_volts;
  }
  updateVoltage();
}
///set the voltage between the exit of the entry mcp and 
///the entry of the exit mcp
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setEntryGapVoltage(double a_volts)
{
  if(a_volts < 250 && a_volts >= 0)
  {
    m_relativeVoltages[2] = a_volts;
  }
  updateVoltage();
}
///sets the voltage for both gaps
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setGapVoltages(double a_volts)
{
  if(a_volts < 250 && a_volts >= 0)
  {
    m_relativeVoltages[0] = a_volts;
    m_relativeVoltages[2] = a_volts;
  }
  updateVoltage();
}
///set the voltage across the exit mcp
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setExitMCPVoltage(double a_volts)
{
  if(a_volts < 1150 && a_volts >= 0)
  {
    m_relativeVoltages[1] = a_volts;
  }
  updateVoltage();
}
///set the voltage across the entry mcp
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setEntryMCPVoltage(double a_volts)
{
  if(a_volts < 1150 && a_volts >= 0)
  {
    m_relativeVoltages[3] = a_volts;
  }
  updateVoltage();
}
///set the voltage for both mcps
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setMCPVoltages(double a_volts)
{
  if(a_volts < 1150 && a_volts >= 0)
  {
    m_relativeVoltages[1] = a_volts;
    m_relativeVoltages[3] = a_volts;

  }
  updateVoltage();
}
///set the voltage for the photocathod 
///If power is on changes will be adopted immediately
int LAPPDHighVoltageControl::setPhotocathodeVoltage(double a_volts)
{
  if(a_volts < 100 && a_volts > -10)
  {
    m_relativeVoltages[4] = a_volts;
  }
  updateVoltage();
}
///returns the currents or the power supplies vector has the form
///exitGapI exitMCPI entryGapI entryMCPI photoCathodeI
vector<double> LAPPDHighVoltageControl::getCurrents()
{
  vector<double> currents;
  for(int iC = 0; iC < 5; iC++)
  {
    currents.push_back(m_voltageController->getMeasuredCurrent(iC));
  }
  return currents;
}

int LAPPDHighVoltageControl::updateVoltage(bool a_initialPowerOn)
{

  if (!m_initialized) {
    std::cout << "Device not initialized, giving up." << std::endl;
    return 1;
  }

  double volts =0;
  ///ensure the class is synced with the hv
  for(int iC = 0; iC < 5; iC++)
  {
    volts+=m_relativeVoltages[iC];
    if(m_voltageController!=NULL)
    {
      if(!m_voltageController->isPowered(iC))
      {
        //std::cout << "Setting voltage on ch" << iC << std::endl;
        auto ret = m_voltageController->setHV(iC,volts);
        if(ret)
        {
          std::cerr << "Not able to set a HV" << std::endl;
          return 1;
        }
      }
      else if(a_initialPowerOn)
      {
        //std::cout << "Refining voltage on ch" << iC << std::endl;
        for(int iC2 = iC+1; iC2 < 5; iC2++)
        {
          auto ret = m_voltageController->setHV(iC,volts);
        }
        auto ret = m_voltageController->setHVExact(iC,volts);
        if (ret)
        {
          std::cerr << "Not able to set a HV exactly" << std::endl;
          return 1;
        }
      }
      else
      {
        //std::cout << "Refining voltage on ch" << iC << std::endl;
        
        auto ret = m_voltageController->setHVExact(iC,volts);
        if (ret)
        {
          std::cerr << "Not able to set a HV exactly" << std::endl;
          return 1;
        }
      }
    }
    else
    {
      std::cout << "No HV Interface only class internal state is changed";
      std::cout << std::endl;
    }
  }
  return 0;
}