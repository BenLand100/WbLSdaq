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
 
#include <sstream>
#include <stdexcept>
#include <CAENVMElib.h>

class VMEBridge {

    protected: 
        static const string error_codes[6] = {"Success","Bus Error","Comm Error","Generic Error","Invalid Param","Timeout Error"};  
    
    public:
        VMEBridge(int link, int board);
        
        virtual ~VMEBridge();

        inline void write32(uint32_t addr, uint32_t data) {
            //cout << "\twrite32@" << hex << addr << ':' << data << dec << endl;
            int res = CAENVME_WriteCycle(handle, addr, &data, cvA32_U_DATA, cvD32);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: write32 @ " << hex << addr << " : " << data;
                throw runtime_error(err.str());
            }
            usleep(10000);
        }        
        
        inline uint32_t read32(uint32_t addr) {
            uint32_t read;
            usleep(1);
            //cout << "\tread32@" << hex << addr << ':';
            int res = CAENVME_ReadCycle(handle, addr, &read, cvA32_U_DATA, cvD32);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: read32 @ " << hex << addr;
                throw runtime_error(err.str());
            }
            //cout << read << dec << endl;
            return read;
        }
        
        inline void write16(uint32_t addr, uint32_t data) {
            //cout << "\twrite16@" << hex << addr << ':' << data << dec << endl;
            int res = CAENVME_WriteCycle(handle, addr, &data, cvA32_U_DATA, cvD16);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: write32 @ " << hex << addr << " : " << data;
                throw runtime_error(err.str());
            }
            usleep(10000);
        }        
        
        inline uint32_t read16(uint32_t addr) {
            uint32_t read;
            //cout << "\tread16@" << hex << addr << ':';
            int res = CAENVME_ReadCycle(handle, addr, &read, cvA32_U_DATA, cvD16);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: read32 @ " << hex << addr;
                throw runtime_error(err.str());
            }
            //cout << read << dec << endl;
            return read;
        }
        
        inline uint32_t readBLT(uint32_t addr, void *buffer, uint32_t size) {
            uint32_t bytes;
            //cout << "\tBLT@" << hex << addr << " for " << dec << size << endl;
            int res = CAENVME_MBLTReadCycle(handle, addr, buffer, size, cvA32_U_MBLT, (int*)&bytes);
            if (res && (res != -1)) { //we ignore bus errors for BLT
                stringstream err;
                err << error_codes[-res] << " :: readBLT @ " << hex << addr;
                throw runtime_error(err.str());
            }
            return bytes;
        }
        
    protected:
        int handle;
};
