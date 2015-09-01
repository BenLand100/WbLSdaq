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
 
#include <map>
#include <vector>
#include <string>
 
#include "VMEBridge.hh"
#include "RunDB.hh"
#include "json.hh"

#ifndef V1742__hh
#define V1742__hh


typedef struct {
    uint16_t dc_offset[32]; //16 bit channel offsets
    uint16_t tr0_threshold; //16 bit tr0 threshold
    uint16_t tr1_threshold; //16 bit tr1 threshold
    uint16_t tr0_dc_offset; //16 bit tr0 dc offset
    uint16_t tr1_dc_offset; //16 bit tr1 dc offset
    uint8_t tr_enable; //1 bit tr enabled
    uint8_t tr_readout; //1 bit tr readout enabled
    uint8_t tr_polarity; //1 bit [positive,negative]
    uint8_t custom_size; //2 bit [1024, 520, 256, 136]
    uint8_t sample_freq; //2 bit [5, 2.5, 1]
    uint8_t software_trigger_enable; //1 bit bool
    uint8_t external_trigger_enable; //1 bit bool
    uint16_t post_trigger; //10 bit (8.5ns steps)
    uint8_t group_enable[4]; //1 bit bool
    uint8_t max_event_blt; //8 bit events per transfer
} V1742_card_config;

class V1742Settings {
    friend class V1742;

    public:
    
        V1742Settings();
        
        V1742Settings(json::Value &digitizer, RunDB &db);
        
        virtual ~V1742Settings();
        
        void validate();
        
    
    protected:
    
        V1742_card_config card;
        
};

class V1742 {

    //system wide
    #define REG_GROUP_CONFIG 0x8000
    #define REG_CUSTOM_SIZE 0x8020
    #define REG_SAMPLE_FREQ 0x80D8
    #define REG_DUMMY 0xEF20
    #define REG_SOFTWARE_RESET 0xEF24
    #define REG_SOFTWARE_CLEAR 0xEF28
    #define REG_BOARD_CONFIGURATION_RELOAD 0xEF34
    
    //per group, or with 0x0n00
    #define REG_GROUP_STATUS 0x1088
    #define REG_BUFFER_OCCUPANCY 0x1094
    #define REG_DC_OFFSET 0x1098
    #define REG_DC_SEL 0x10A4
    #define REG_DRS4_TEMP 0x10A0
    #define REG_TR_THRESHOLD 0x10D4
    #define REG_TR_DC_OFFSET 0x10DC

    //acquisition 
    #define REG_ACQUISITION_CONTROL 0x8100
    #define REG_ACQUISITION_STATUS 0x8104
    #define REG_SOFTWARE_TRIGGER 0x8108
    #define REG_TRIGGER_SOURCE 0x810C
    #define REG_POST_TRIGGER 0x8114
    #define REG_GROUP_ENABLE 0x8120

    //readout
    #define REG_EVENTS_STORED 0x812C
    #define REG_EVENT_SIZE 0x814C
    #define REG_READOUT_CONTROL 0xEF00
    #define REG_READOUT_STATUS 0xEF04
    #define REG_READOUT_BLT_AGGREGATE_NUMBE 0xEF1C
    
    public:
        V1742(VMEBridge &_bridge, uint32_t _baseaddr, bool busErrReadout = true);
        
        virtual ~V1742();
        
        bool program(V1742Settings &settings);
        
        inline void startAcquisition() {
            write32(REG_ACQUISITION_CONTROL,(1<<3)|(1<<2));
        }
        
        inline void stopAcquisition() {
            write32(REG_ACQUISITION_CONTROL,0);
        }
        
        inline bool acquisitionRunning() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 2);
        }
        
        inline bool readoutReady() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 3);
        }
        
        bool checkTemps(std::vector<uint32_t> &temps, uint32_t danger);
        
        inline size_t readoutBLT(char *buffer, size_t buffer_size) {
            return berr ? readoutBLT_berr(buffer, buffer_size) : readoutBLT_evtsz(buffer,buffer_size);
        }
        
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
        
    protected:
        
        bool berr;
        VMEBridge &bridge;
        uint32_t baseaddr;
        
        //for bus error read
        size_t readoutBLT_berr(char *buffer, size_t buffer_size);
        
        //for event size read
        size_t readoutBLT_evtsz(char *buffer, size_t buffer_size);
        
};

#endif

