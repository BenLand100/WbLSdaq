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
#include <ctime>

#include <CAENDigitizer.h>
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
    uint8_t software_trigger_out; //1 bit bool
    uint8_t external_trigger_out; //1 bit bool
    uint16_t post_trigger; //10 bit (8.5ns steps)
    uint8_t group_enable[4]; //1 bit bool
    uint8_t max_event_blt; //8 bit events per transfer
} V1742_card_config;

enum V1742SampleFreq {GHz_5, GHz_2_5, GHz_1};

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
        
        inline double nsPerSample() {
            switch (card.sample_freq) {
                case 0: return 0.2;
                case 1: return 0.4;
                case 2: return 1.0;
                default: throw std::runtime_error("Invalid custom_size");
            }
        }
        
        inline V1742SampleFreq sampleFreq() {
            switch (card.sample_freq) {
                case 0: return GHz_5;
                case 1: return GHz_2_5;
                case 2: return GHz_1;
                default: throw std::runtime_error("Invalid custom_size");
            }
        }
        
        inline uint32_t getDCOffset(uint32_t ch) {
            return card.dc_offset[ch];
        }
    
    protected:
    
        V1742_card_config card;
        
        void groupDefaults(uint32_t group);
        
};

class V1742calib {

    public:
        V1742calib(CAEN_DGTZ_DRS4Correction_t *dat);
        
        virtual ~V1742calib();
        
        virtual void calibrate(uint16_t *samples[4][8], size_t sampPerEv, uint16_t *start_index[4], bool grActive[4], size_t num);
        
    protected:
        struct {
            struct {
                int cell_offset[1024], seq_offset[1024];
            } chans[8]; 
            int cell_delay[1024];   
        } groups[4];

};

class V1742 : public Digitizer {

    //system wide
    static constexpr uint32_t REG_GROUP_CONFIG = 0x8000;
    static constexpr uint32_t REG_CUSTOM_SIZE = 0x8020;
    static constexpr uint32_t REG_SAMPLE_FREQ = 0x80D8;
    static constexpr uint32_t REG_FRONT_PANEL_CONTROL = 0x811C;
    static constexpr uint32_t REG_DUMMY = 0xEF20;
    static constexpr uint32_t REG_SOFTWARE_RESET = 0xEF24;
    static constexpr uint32_t REG_SOFTWARE_CLEAR = 0xEF28;
    static constexpr uint32_t REG_BOARD_CONFIGURATION_RELOAD = 0xEF34;
    
    //per group, or with 0x0n00
    static constexpr uint32_t REG_GROUP_STATUS = 0x1088;
    static constexpr uint32_t REG_BUFFER_OCCUPANCY = 0x1094;
    static constexpr uint32_t REG_DC_OFFSET = 0x1098;
    static constexpr uint32_t REG_DC_SEL = 0x10A4;
    static constexpr uint32_t REG_DRS4_TEMP = 0x10A0;
    static constexpr uint32_t REG_TR_THRESHOLD = 0x10D4;
    static constexpr uint32_t REG_TR_DC_OFFSET = 0x10DC;

    //acquisition 
    static constexpr uint32_t REG_ACQUISITION_CONTROL = 0x8100;
    static constexpr uint32_t REG_ACQUISITION_STATUS = 0x8104;
    static constexpr uint32_t REG_SOFTWARE_TRIGGER = 0x8108;
    static constexpr uint32_t REG_TRIGGER_SOURCE = 0x810C;
    static constexpr uint32_t REG_TRIGGER_OUT = 0x8110;
    static constexpr uint32_t REG_POST_TRIGGER = 0x8114;
    static constexpr uint32_t REG_GROUP_ENABLE = 0x8120;

    //readout
    static constexpr uint32_t REG_EVENTS_STORED = 0x812C;
    static constexpr uint32_t REG_EVENT_SIZE = 0x814C;
    static constexpr uint32_t REG_READOUT_CONTROL = 0xEF00;
    static constexpr uint32_t REG_READOUT_STATUS = 0xEF04;
    static constexpr uint32_t REG_MAX_EVENT_BLT = 0xEF1C;
    
    public:
        V1742(VMEBridge &bridge, uint32_t baseaddr);
        
        virtual ~V1742();
        
        virtual bool program(DigitizerSettings &settings);
        
        virtual void softTrig();
        
        virtual void startAcquisition();
        
        virtual void stopAcquisition();
        
        virtual bool acquisitionRunning();
        
        virtual bool readoutReady();
        
        virtual bool checkTemps(std::vector<uint32_t> &temps, uint32_t danger);
        
        virtual V1742calib* getCalib(V1742SampleFreq freq);
        
        static V1742calib* staticGetCalib(V1742SampleFreq freq, int link, uint32_t baseaddr);
        
};

class V1742Decoder : public Decoder {

    public: 
    
        V1742Decoder(size_t eventBuffer, V1742calib *calib, V1742Settings &settings);
        
        virtual ~V1742Decoder();
        
        virtual void decode(Buffer &buffer);
        
        virtual size_t eventsReady();
        
        virtual void writeOut(H5::H5File &file, size_t nEvents);
        
        virtual void dispatch(int nfd, int *fds);

    protected:
        
        size_t eventBuffer;
        V1742calib *calib;
        V1742Settings &settings;
        
        size_t dispatch_index;
        size_t decode_size;
        size_t group_counter,event_counter,decode_counter;
        struct timespec last_decode_time;
        
        uint32_t trigger_last;
        
        uint32_t nSamples;
        bool grActive[4];
        size_t grGrabbed[4];
        uint16_t *samples[4][8];
        uint16_t *start_index[4];
        uint16_t *patterns[4];
        uint32_t *trigger_count[4];
        uint32_t *trigger_time[4];
        bool trActive[2];
        uint16_t *tr_samples[2];
        
        uint32_t* decode_event_structure(uint32_t *event);
        
        uint32_t* decode_group_structure(uint32_t *group, uint32_t gr);

};

#endif

