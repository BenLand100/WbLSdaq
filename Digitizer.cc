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
 
#include "Digitizer.hh"

    
DigitizerSettings::DigitizerSettings(std::string _index) : index(_index) {

}

DigitizerSettings::~DigitizerSettings() {

}

Digitizer::Digitizer(VMEBridge &_bridge, uint32_t _baseaddr) : bridge(_bridge), baseaddr(_baseaddr) {

}

Digitizer::~Digitizer() {

}

size_t Digitizer::readoutBLT(char *buffer, size_t buffer_size) {
    size_t offset = 0, size = 0;
    while (offset < buffer_size && (size = readBLT(0x0000, buffer+offset, 4093))) {
        offset += size;
    }
    return offset;
}
