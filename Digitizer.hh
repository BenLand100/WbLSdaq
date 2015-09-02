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

#include "VMEBridge.hh"
#include "Buffer.hh"
#include "H5Cpp.h"

#ifndef Digitizer__hh
#define Digitizer__hh

class DigitizerSettings {

    public:
    
        DigitizerSettings(std::string index);
        
        virtual ~DigitizerSettings();
        
        inline std::string getIndex() { return index; }
        
    protected:
    
        std::string index;

};

class Digitizer {
    public:   
        Digitizer(VMEBridge &bridge, uint32_t baseaddr);
        
        virtual ~Digitizer();
    
        virtual bool program(DigitizerSettings &settings) = 0;
        
        virtual bool checkTemps(std::vector<uint32_t> &temps, uint32_t danger) = 0;
        
        virtual void startAcquisition() = 0;
        
        virtual void stopAcquisition() = 0;
        
        virtual bool acquisitionRunning() = 0;
        
        virtual bool readoutReady() = 0;
        
        virtual size_t readoutBLT(char *buffer, size_t buffer_size);
    
    protected:
        
        VMEBridge &bridge;
        uint32_t baseaddr;
        
        inline void write16(uint32_t reg, uint32_t data) {
            bridge.write16(baseaddr|reg,data);
        }
        
        inline uint32_t read16(uint32_t reg) {
            return bridge.read16(baseaddr|reg);
        }
        
        inline void write32(uint32_t reg, uint32_t data) {
            bridge.write32(baseaddr|reg,data);
        }
        
        inline uint32_t read32(uint32_t reg) {
            return bridge.read32(baseaddr|reg);
        }
        
        inline uint32_t readBLT(uint32_t addr, void *buffer, uint32_t size) {
            return bridge.readBLT(baseaddr|addr,buffer,size);
        }
        
};

class Decoder {
    
    public:
        
        virtual void decode(Buffer &buffer) = 0;
        
        virtual size_t eventsReady() = 0;
        
        virtual void writeOut(H5::H5File &file, size_t nEvents) = 0;
};

#endif
