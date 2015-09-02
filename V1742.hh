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
#include "Digitizer.hh"
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

class V1742Settings : public DigitizerSettings  {
    friend class V1742;

    public:
    
        V1742Settings();
        
        V1742Settings(RunTable &digitizer, RunDB &db);
        
        virtual ~V1742Settings();
        
        void validate();
        
        inline std::string getIndex() {
            return index;
        }
        
        inline uint32_t getNumSamples() {
            switch (card.custom_size) {
                case 0: return 1024;
                case 1: return 520;
                case 2: return 256;
                case 3: return 136;
                default: throw std::runtime_error("Invalid custom_size");
            }
        }
        
        inline bool getGroupEnabled(uint32_t gr) {
            return card.group_enable[gr];
        }
        
        inline bool getTRReadout() {
            return card.tr_readout;
        }
    
    protected:
    
        V1742_card_config card;
        
        void groupDefaults(uint32_t group);
        
};

class V1742 : public Digitizer {

    //system wide
    #define REG_GROUP_CONFIG 0x8000
    #define REG_CUSTOM_SIZE 0x8020
    #define REG_SAMPLE_FREQ 0x80D8
    #define REG_FRONT_PANEL_CONTROL 0x811C
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
    #define REG_MAX_EVENT_BLT 0xEF1C
    
    public:
        V1742(VMEBridge &_bridge, uint32_t _baseaddr);
        
        virtual ~V1742();
        
        virtual bool program(DigitizerSettings &settings);
        
        virtual void startAcquisition();
        
        virtual void stopAcquisition();
        
        virtual bool acquisitionRunning();
        
        virtual bool readoutReady();
        
        virtual bool checkTemps(std::vector<uint32_t> &temps, uint32_t danger);
        
};

class V1742Decoder : public Decoder {

    public: 
    
        V1742Decoder(size_t eventBuffer, V1742Settings &settings);
        
        virtual ~V1742Decoder();
        
        virtual void decode(Buffer &buffer);
        
        virtual size_t eventsReady();
        
        virtual void writeOut(H5::H5File &file, size_t nEvents);

    protected:
        
        size_t eventBuffer;
        V1742Settings &settings;
        
        size_t decode_size;
        size_t group_counter,event_counter,decode_counter;
        
        uint32_t nSamples;
        bool grActive[4];
        size_t grGrabbed[4];
        uint16_t *samples[4][8];
        bool trActive[2];
        uint16_t *tr_samples[2];
        
        uint32_t* decode_event_structure(uint32_t *event);
        
        uint32_t* decode_group_structure(uint32_t *group, uint32_t gr);

};

#endif

