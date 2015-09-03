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

#ifndef VMECard__hh
#define VMECard__hh

class VMECard {

    public:   
    
        VMECard(VMEBridge &bridge, uint32_t baseaddr);
        
        virtual ~VMECard();
    
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

#endif

