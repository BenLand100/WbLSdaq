/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <cmath>
#include <string>
#include <stdexcept>
#include <iostream>

#include "V65XX.hh"
#include "LAPPDHighVoltageControl.h"

using namespace std;

V65XX::V65XX(VMEBridge &bridge, uint32_t baseaddr) : VMECard(bridge,baseaddr) {
    nChans = read16(REG_NUM_CHANS);
    vmax = read16(REG_BOARD_VMAX);
    imax = read16(REG_BOARD_IMAX);
    positive.resize(nChans);
    for (uint32_t ch = 0; ch < nChans; ch++) {
        positive[ch] = read16((0x80*(ch+1))|REG_POLARITY);
    }
}
        
V65XX::~V65XX() {

}

void V65XX::set(RunTable &config) {

    if (config.isMember("lappd")) { 
        //a card can control a SINGLE lappd
        //this could potentially be broken out into a different class
        //but I wanted to ensure that channels aren't configured twice
        json::Value &conf = config["lappd"];
        
        hvinterface_map.clear();
        
        //The LAPPDHighVoltage class assumes the HVInterface channels are 0-4 
        //this maps between HVInterface channels and the actual channels being 
        //used since they need not be the same.
        hvinterface_map[0] = conf["mcp_lower_bottom_channel"].cast<int>();
        hvinterface_map[1] = conf["mcp_lower_top_channel"].cast<int>();
        hvinterface_map[2] = conf["mcp_upper_bottom_channel"].cast<int>();
        hvinterface_map[3] = conf["mcp_upper_top_channel"].cast<int>();
        hvinterface_map[4] = conf["photocathode_channel"].cast<int>();
        
        double pc = conf["photocathode_bias"].cast<double>();
        double mcpu = conf["mcp_upper_bias"].cast<double>();
        double gapu = conf["gap_upper_bias"].cast<double>();
        double mcpl = conf["mcp_lower_bias"].cast<double>();
        double gapl = conf["gap_lower_bias"].cast<double>();
        
        bool enabled = conf["enabled"].cast<bool>();
        
        LAPPDHighVoltageControl lappd(this);
        
        if (enabled) {
            lappd.setExitGapVoltage(gapl);
            lappd.setExitMCPVoltage(mcpl);
            lappd.setEntryGapVoltage(gapu);
            lappd.setEntryMCPVoltage(mcpu);
            lappd.setPhotocathodeVoltage(pc);
            lappd.powerOn();
        } else {
            lappd.powerOff();
        }
       
    }

    for (uint32_t ch = 0; ch < nChans; ch++) {
        string field = "ch"+to_string(ch);
        if (config.isMember(field)) {
            json::Value &chan = config[field];
            if (chan.isMember("enabled") && !chan["enabled"].cast<bool>()) setEnabled(ch,false);
            if (chan.isMember("v_set")) setVSet(ch,chan["v_set"].cast<double>());
            if (chan.isMember("v_max")) setVMax(ch,chan["v_max"].cast<double>());
            if (chan.isMember("i_max")) setIMax(ch,chan["i_max"].cast<double>());
            if (chan.isMember("r_up")) setUpRate(ch,chan["r_up"].cast<int>());
            if (chan.isMember("r_down")) setDownRate(ch,chan["r_down"].cast<int>());
            if (chan.isMember("trip")) setTripTime(ch,chan["trip"].cast<double>());
            if (chan.isMember("ramp_off")) setDownMode(ch,chan["ramp_off"].cast<bool>());
            if (chan.isMember("enabled") && chan["enabled"].cast<bool>()) setEnabled(ch,true);
        }
    
    }

}

bool V65XX::isHVOn() {
    bool on = false;
    for (uint32_t ch = 0; ch < nChans; ch++) {
        on |= isOn(ch);
    }
    return on;
}

bool V65XX::isBusy() {
    bool busy = false;
    for (uint32_t ch = 0; ch < nChans; ch++) {
        busy |= getStatus(ch) & (CH_UP | CH_DOWN);
    }
    return busy;
}

bool V65XX::isWarning() {
    bool warning = false;
    for (uint32_t ch = 0; ch < nChans; ch++) {
        warning |= getStatus(ch) & (CH_OVERI | CH_OVERV | CH_UNDERV | CH_MAXV | CH_MAXI | CH_TRIP | CH_OVER_POWER | CH_OVER_TEMP);
    }
    return warning;
}

void V65XX::powerDown() {
    for (uint32_t ch = 0; ch < nChans; ch++) {
        setEnabled(ch,false);
    }
}

void V65XX::kill() {
    for (uint32_t ch = 0; ch < nChans; ch++) {
        setDownMode(ch,false);
        setEnabled(ch,false);
    }
}

void V65XX::setVSet(uint32_t ch, double V) {
    if (ch >= nChans) throw runtime_error("Cannot setVSet on ch " + to_string(ch) + " - channel out of range.");
    if (positive[ch] && V < 0.0) throw runtime_error("Trying to set negative voltage on a positive channel.");
    if (!positive[ch] && V > 0.0) throw runtime_error("Trying to set positive voltage on a negative channel.");
    uint32_t data = round(abs(V)/VRES);
    if (data > 40000)  throw runtime_error("Voltage " + to_string(V) + " out of bounds");
    write16((0x80*(ch+1))|REG_VSET,data);
}

void V65XX::setIMax(uint32_t ch, double uA) {
    if (ch >= nChans) throw runtime_error("Cannot setIMax on ch " + to_string(ch) + " - channel out of range.");
    if (uA < 0.0) throw runtime_error("IMax must be positive");
    uint32_t data = round(uA/IRESH);
    if (data > 62000)  throw runtime_error("IMax " + to_string(uA) + " out of bounds");
    write16((0x80*(ch+1))|REG_IMAX,data);
}

void V65XX::setVMax(uint32_t ch, double V) {
    if (ch >= nChans) throw runtime_error("Cannot setVmax on ch " + to_string(ch) + " - channel out of range.");
    if (positive[ch] && V < 0.0) throw runtime_error("Trying to set negative VMax on a positive channel.");
    if (!positive[ch] && V > 0.0) throw runtime_error("Trying to set positive VMax on a negative channel.");
    uint32_t data = round(abs(V)/VRES);
    if (data > 40000)  throw runtime_error("VMax " + to_string(V) + " out of bounds");
    write16((0x80*(ch+1))|REG_VMAX,data);
}

void V65XX::setEnabled(uint32_t ch, bool on) {
    if (ch >= nChans) throw runtime_error("Cannot setEnabled ch " + to_string(ch) + " - channel out of range.");
    write16((0x80*(ch+1))|REG_ENABLE,on?0x1:0x0);
}

void V65XX::setTripTime(uint32_t ch, double s) {
    if (ch >= nChans) throw runtime_error("Cannot setTripTime on ch " + to_string(ch) + " - channel out of range.");
    if (s < 0.0) throw runtime_error("TripTime must be positive");
    uint32_t data = round(s/TRES);
    if (data > 10000)  throw runtime_error("TripTime " + to_string(s) + " out of bounds");
    write16((0x80*(ch+1))|REG_TRIP_TIME,data);
}

void V65XX::setDownRate(uint32_t ch, uint32_t Vps) {
    if (ch >= nChans) throw runtime_error("Cannot setDownRate on ch " + to_string(ch) + " - channel out of range.");
    if (Vps < 0.0) throw runtime_error("DownRate must be positive");
    if (Vps > 500)  throw runtime_error("DownRate " + to_string(Vps) + " out of bounds");
    write16((0x80*(ch+1))|REG_DOWN_RATE,Vps);
}

void V65XX::setUpRate(uint32_t ch, uint32_t Vps) {
    if (ch >= nChans) throw runtime_error("Cannot setUpRate on ch " + to_string(ch) + " - channel out of range.");
    if (Vps < 0.0) throw runtime_error("UpRate must be positive");
    if (Vps > 500)  throw runtime_error("UpRate " + to_string(Vps) + " out of bounds");
    write16((0x80*(ch+1))|REG_UP_RATE,Vps);
}

void V65XX::setDownMode(uint32_t ch, bool ramp) {
    if (ch >= nChans) throw runtime_error("Cannot setDownMode ch " + to_string(ch) + " - channel out of range.");
    write16((0x80*(ch+1))|REG_DOWN_MODE,ramp?0x1:0x0);
}

void V65XX::setIMonRange(uint32_t ch, bool low) {
    if (ch >= nChans) throw runtime_error("Cannot setIMonRange ch " + to_string(ch) + " - channel out of range.");
    write16((0x80*(ch+1))|REG_DOWN_MODE,low?0x1:0x0);
}

double V65XX::getVSet(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getVSet on ch " + to_string(ch) + " - channel out of range.");
    uint32_t data = read16((0x80*(ch+1))|REG_VSET);
    return (positive[ch] ? 1.0 : -1.0)*data*VRES;
}

double V65XX::getV(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getV on ch " + to_string(ch) + " - channel out of range.");
    uint32_t data = read16((0x80*(ch+1))|REG_VMON);
    return (positive[ch] ? 1.0 : -1.0)*data*VRES;
}

double V65XX::getI(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getI on ch " + to_string(ch) + " - channel out of range.");
    if (isIMonLow(ch)) {
        uint32_t data = read16((0x80*(ch+1))|REG_IMONL);
        return data*IRESL;
    } else {
        uint32_t data = read16((0x80*(ch+1))|REG_IMONH);
        return data*IRESH;
    }
}

double V65XX::getIMax(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getIMax on ch " + to_string(ch) + " - channel out of range.");
    uint32_t data = read16((0x80*(ch+1))|REG_IMAX);
    return data*IRESH;
}

double V65XX::getVMax(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getVMax on ch " + to_string(ch) + " - channel out of range.");
    uint32_t data = read16((0x80*(ch+1))|REG_VMAX);
    return data*VRES;
}

bool V65XX::getEnabled(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getEnabled on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_ENABLE);
}

double V65XX::getTripTime(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getTripTime on ch " + to_string(ch) + " - channel out of range.");
    uint32_t data = read16((0x80*(ch+1))|REG_TRIP_TIME);
    return data*TRES;
}

uint32_t V65XX::getDownRate(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getDownRate on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_DOWN_RATE);
}

uint32_t V65XX::getUpRate(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getUpRate on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_UP_RATE);
}

int V65XX::getTemp(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getTemp on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_TEMP);
}

uint32_t V65XX::getStatus(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot getStatus on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_STATUS);
}

bool V65XX::isDownModeLow(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot isDownModeLow on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_DOWN_MODE);
}

bool V65XX::isIMonLow(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot isIMonLow on ch " + to_string(ch) + " - channel out of range.");
    return read16((0x80*(ch+1))|REG_IMON_RANGE);
}

bool V65XX::isPositive(uint32_t ch) {
    if (ch >= nChans) throw runtime_error("Cannot isPositive on ch " + to_string(ch) + " - channel out of range.");
    return positive[ch];
}

bool V65XX::isOn(uint32_t ch) {
    return getStatus(ch) & CH_ON;
}

bool V65XX::isBusy(uint32_t ch) {
    return getStatus(ch) & (CH_UP | CH_DOWN);
}

bool V65XX::isWarning(uint32_t ch) {
    return getStatus(ch) & (CH_OVERI | CH_OVERV | CH_UNDERV | CH_MAXV | CH_MAXI | CH_TRIP | CH_OVER_POWER | CH_OVER_TEMP);
}

///used to set the high voltage set point in volts
int V65XX::setHV(int a_channel, float a_voltage) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    //N.B. by convention the HVInterface class uses POSTIIVE numbers to represent NEGATIVE voltages 
    setVSet(hvinterface_map[a_channel],-a_voltage);
    return 0;
}

///used to get the high voltage set point in volts
float V65XX::getHV(int a_channel) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    //N.B. by convention the HVInterface class uses POSTIIVE numbers to represent NEGATIVE voltages 
    return -getVSet(hvinterface_map[a_channel]);
}

///used for reading the applied high voltage in volts
float V65XX::getMeasuredHV(int a_channel) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    return getV(hvinterface_map[a_channel]);
}

///used to set the current limit in milliamps
int V65XX::setCurrent(int a_channel,float a_current) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    setIMax(hvinterface_map[a_channel],a_current*1000.0);
    return 0;
}

///used to get the current limit in milliamps
float V65XX::getCurrent(int a_channel) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    return getIMax(hvinterface_map[a_channel])/1000.0;
}

///used for reading the sourced current in milliamps
float V65XX::getMeasuredCurrent(int a_channel) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    return getI(hvinterface_map[a_channel])/1000.0;
}

///used to set the ramp in V/s
int V65XX::setRamp(int a_channel,float a_ramp) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    setUpRate(hvinterface_map[a_channel],a_ramp);
    setDownRate(hvinterface_map[a_channel],a_ramp);
    return 0;
}

///used to get the ramp in V/s
float V65XX::getRamp(int a_channel) {
    if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
    return getUpRate(hvinterface_map[a_channel]);
}

///used to power on channels if -1 is sent all channels will be powered on
int V65XX::powerOn(int a_channel) {
    if (a_channel == -1) {
    } else { 
        if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
        setEnabled(hvinterface_map[a_channel],true);
        return 0;
    }
    return 1;
}

///used to power on channels if -1 is sent all channels will be powered on
int V65XX::powerOff(int a_channel) {
    if (a_channel == -1) {
    } else { 
        if (!hvinterface_map.count(a_channel)) throw runtime_error("Unmapped HVInterface channel: "+to_string(a_channel));
        setEnabled(hvinterface_map[a_channel],false);
        return 0;
    }
    return 1;
}

///used to check whether a channel is on or off
bool V65XX::isPowered(int a_channel) {
    return isOn(hvinterface_map[a_channel]);
}

///used to check whether a channel is still changing voltage
bool V65XX::isRamping(int a_channel) {
    return isBusy(hvinterface_map[a_channel]);
}
        
