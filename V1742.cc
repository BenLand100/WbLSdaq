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
#include <iostream>
#include <stdexcept>
 
#include "V1742.hh"

using namespace std;

V1742Settings::V1742Settings() {
    //These are "do nothing" defaults
    
    
}

V1742Settings::V1742Settings(json::Value &digitizer, RunDB &db) {
    
}

V1742Settings::~V1742Settings() {

}
        
void V1742Settings::validate() {

}

V1742::V1742(VMEBridge &_bridge, uint32_t _baseaddr, bool busErrReadout) : berr(busErrReadout), bridge(_bridge), baseaddr(_baseaddr) {

}

V1742::~V1742() {
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
}

bool V1742::program(V1742Settings &settings) {
    try {
        settings.validate();
    } catch (runtime_error &e) {
        cout << "Could not program V1742: " << e.what() << endl;
        return false;
    }
    
    return true;
}

bool V1742::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(4);
    bool over = false;
    for (int gr = 0; gr < 4; gr++) {
        temps[gr] = read32(REG_DRS4_TEMP|(gr<<8))&0xFF;
        if (temps[gr] >= danger) over = true;
    }
    return over;
}

size_t V1742::readoutBLT_berr(char *buffer, size_t buffer_size) {
    size_t offset = 0, size = 0;
    while (offset < buffer_size && (size = readBLT(0x0000, buffer+offset, buffer_size))) {
        offset += size;
    }
    return offset;
}

size_t V1742::readoutBLT_evtsz(char *buffer, size_t buffer_size) {
    size_t offset = 0, total = 0;
    while (readoutReady()) {
        uint32_t next = read32(REG_EVENT_SIZE);
        total += 4*next;
        if (offset+total > buffer_size) throw runtime_error("Readout buffer too small!");
        size_t lastoff = offset;
        while (offset < total) {
            size_t remaining = total-offset, read;
            if (remaining > 4090) {
                read = readBLT(0x0000, buffer+offset, 4090);
            } else {
                remaining = 8*(remaining%8 ? remaining/8+1 : remaining/8); // needs to be multiples of 8 (64bit)
                read = readBLT(0x0000, buffer+offset, remaining);
            }
            offset += read;
            if (!read) {
                cout << "\tfailed event size " << offset-lastoff << " / " << next*4 << endl;
                break;
            }
        }
    }
    return total;
}
