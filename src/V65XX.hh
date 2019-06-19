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

#include <vector>

#include "VMECard.hh"
#include "RunDB.hh"
#include "HVInterface.h"

#ifndef V6533__hh
#define V6533__hh

class V65XX : public VMECard, virtual public HVInterface {
    
    public:
        //board status registers
        static constexpr uint32_t REG_NUM_CHANS = 0x8100;
        static constexpr uint32_t REG_BOARD_VMAX = 0x0050;
        static constexpr uint32_t REG_BOARD_IMAX = 0x0054;
        static constexpr uint32_t REG_BOARD_STATUS = 0x0058;
        
        //board status masks (first bits are channel alarms)
        static constexpr uint32_t BD_POWER_FAIL = (1<<8);
        static constexpr uint32_t BD_OVER_POWER = (1<<9);
        static constexpr uint32_t BD_VMAX_UNCAL = (1<<10);
        static constexpr uint32_t BD_IMAX_UNCAL = (1<<11);
        
    
        //channel control offsets, add to channel base (0x80 * ch)
        static constexpr uint32_t REG_VSET = 0x0000;
        static constexpr uint32_t REG_IMAX = 0x0004;
        static constexpr uint32_t REG_VMON = 0x0008;
        static constexpr uint32_t REG_IMONH = 0x000C;
        static constexpr uint32_t REG_ENABLE = 0x0010;
        static constexpr uint32_t REG_STATUS = 0x0014;
        static constexpr uint32_t REG_TRIP_TIME = 0x0018;
        static constexpr uint32_t REG_VMAX = 0x001C;
        static constexpr uint32_t REG_DOWN_RATE = 0x0020;
        static constexpr uint32_t REG_UP_RATE = 0x0024;
        static constexpr uint32_t REG_DOWN_MODE = 0x0028;
        static constexpr uint32_t REG_POLARITY = 0x002C;
        static constexpr uint32_t REG_TEMP = 0x0030;
        static constexpr uint32_t REG_IMON_RANGE = 0x0034;
        static constexpr uint32_t REG_IMONL = 0x0038;
        
        //channel status masks
        static constexpr uint32_t CH_ON = (1<<0);
        static constexpr uint32_t CH_UP = (1<<1);
        static constexpr uint32_t CH_DOWN = (1<<2);
        static constexpr uint32_t CH_OVERI = (1<<3);
        static constexpr uint32_t CH_OVERV = (1<<4);
        static constexpr uint32_t CH_UNDERV = (1<<5);
        static constexpr uint32_t CH_MAXV = (1<<6);
        static constexpr uint32_t CH_MAXI = (1<<7);
        static constexpr uint32_t CH_TRIP = (1<<8);
        static constexpr uint32_t CH_OVER_POWER = (1<<9);
        static constexpr uint32_t CH_OVER_TEMP = (1<<10);
        static constexpr uint32_t CH_DISABLED = (1<<11);
        static constexpr uint32_t CH_INTERLOCK = (1<<12);
        static constexpr uint32_t CH_UNCAL = (1<<13);
        
        //useful constants
        static constexpr double VRES = 0.1; // V
        static constexpr double IRESH = 0.05; // for most registers uA
        static constexpr double IRESL = 0.005; // for low range registers uA
        static constexpr double TRES = 0.1; // s
        
        V65XX(VMEBridge &bridge, uint32_t baseaddr);
        
        virtual ~V65XX();
        
        void set(RunTable &config);
        
        bool isHVOn();
        
        bool isBusy();
        
        bool isWarning();
        
        void powerDown();
        
        void kill();
        
        void setVSet(uint32_t ch, double V);
        void setIMax(uint32_t ch, double uA);
        void setVMax(uint32_t ch, double V);
        void setEnabled(uint32_t ch, bool on);
        void setTripTime(uint32_t ch, double s);
        void setDownRate(uint32_t ch, uint32_t Vps);
        void setUpRate(uint32_t ch, uint32_t Vps);
        void setDownMode(uint32_t ch, bool ramp);
        void setIMonRange(uint32_t ch, bool low);
        
        double getVSet(uint32_t ch);
        double getV(uint32_t ch);
        double getI(uint32_t ch);
        double getIMax(uint32_t ch);
        double getVMax(uint32_t ch);
        bool getEnabled(uint32_t ch);
        double getTripTime(uint32_t ch);
        uint32_t getDownRate(uint32_t ch);
        uint32_t getUpRate(uint32_t ch);
        int getTemp(uint32_t ch);
        uint32_t getStatus(uint32_t ch);
        
        bool isIMonLow(uint32_t ch);
        bool isDownModeLow(uint32_t ch);
        bool isPositive(uint32_t ch);
        bool isOn(uint32_t ch);
        bool isBusy(uint32_t ch);
        bool isWarning(uint32_t ch);
        
        //HVInterface methods
        //N.B. by convention the HVInterface class uses POSTIIVE numbers to represent NEGATIVE voltages 
        
        ///used to set the high voltage set point in volts
        int setHV(int a_channel, float a_voltage);
        ///used to get the high voltage set point in volts
        float getHV(int a_channel);
        ///used for reading the applied high voltage in volts
        float getMeasuredHV(int a_channel);
        ///used to set the current limit in milliamps
        int setCurrent(int a_channel,float a_current);
        ///used to get the current limit in milliamps
        float getCurrent(int a_channel);
        ///used for reading the sourced current in milliamps
        float getMeasuredCurrent(int a_channel);
        ///used to set the ramp in V/s
        int setRamp(int a_channel,float a_ramp);
        ///used to get the ramp in V/s
        float getRamp(int a_channel);
        ///used to power on channels if -1 is sent all channels will be powered on
        int powerOn(int a_channel = -1);
        ///used to power on channels if -1 is sent all channels will be powered on
        int powerOff(int a_channel = -1);
        ///used to check whether a channel is on or off
        bool isPowered(int a_channel);
        ///used to check whether a channel is still changing voltage
        bool isRamping(int a_channel);
        
    protected:
    
        uint32_t vmax, imax;
        uint32_t nChans;
        
        std::vector<bool> positive;
        
        std::map<int,uint32_t> hvinterface_map;
    
};

#endif

