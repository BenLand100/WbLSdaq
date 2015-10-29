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

#include "VMEBridge.hh"
#include "Digitizer.hh"
#include "RunDB.hh"
#include "json.hh"

#ifndef V1730_dpppsd__hh
#define V1730_dpppsd__hh

typedef struct {

    //REG_CHANNEL_ENABLE_MASK
    uint32_t enabled; // 1 bit
    
    //REG_DYNAMIC_RANGE
    uint32_t dynamic_range; // 1 bit
    
    //REG_PRE_TRG
    uint32_t pre_trigger; // 9* bit
    
    //REG_LONG_GATE
    uint32_t long_gate; // 12 bit
    
    //REG_SHORT_GATE
    uint32_t short_gate; // 12 bit
    
    //REG_PRE_GATE
    uint32_t gate_offset; // 8 bit
    
    //REG_DPP_TRG_THRESHOLD
    uint32_t trg_threshold; // 12 bit
    
    //REG_BASELINE_THRESHOLD
    uint32_t fixed_baseline; // 12 bit
    
    //REG_SHAPED_TRIGGER_WIDTH
    uint32_t shaped_trigger_width; // 10 bit
    
    //REG_TRIGGER_HOLDOFF
    uint32_t trigger_holdoff; // 10* bit
    
    //REG_DPP_CTRL
    uint32_t charge_sensitivity; // 3 bit (see docs)
    uint32_t pulse_polarity; // 1 bit (0->positive, 1->negative)
    uint32_t trigger_config; // 2 bit (normal, coincidence, reserved, anticoincidence)
    uint32_t baseline_mean; // 3 bit (fixed, 16, 64, 256, 1024)
    uint32_t self_trigger; // 1 bit (0->enabled, 1->disabled)
    
    //REG_DC_OFFSET
    uint32_t dc_offset; // 16 bit (-1V to 1V)

} V1730_chan_config;

typedef struct {

    //REG_LOCAL_TRIGGER_MANAGEMENT
    uint32_t local_logic; // 2 bit [AND, EVEN, ODD, OR]
    uint32_t valid_logic; // 2 bit [AND, MOTHER, COUPLE, OR]
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t trg_out; // 1 bit
    
    //REG_LOCAL_VALIDATION
    uint32_t valid_mask; // 8 bit
    uint32_t valid_mode; // 2 bit (or, and, majority)
    uint32_t valid_majority; // 3 bit
    
    //REG_RECORD_LENGTH
    uint32_t record_length; // 16* bit
    
    //REG_NEV_AGGREGATE
    uint32_t ev_per_buffer; // 10 bit

} V1730_group_config;

typedef struct {

    //REG_CONFIG
    uint32_t dual_trace; // 1 bit
    uint32_t analog_probe; // 2 bit (see docs)
    uint32_t oscilloscope_mode; // 1 bit
    uint32_t digital_virt_probe_1; // 3 bit (see docs)
    uint32_t digital_virt_probe_2; // 3 bit (see docs)
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t coincidence_window; // 3 bit
    uint32_t global_majority_level; // 3 bit
    uint32_t external_global_trigger; // 1 bit
    uint32_t software_global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t out_logic; // 2 bit (OR,AND,MAJORITY)
    uint32_t out_majority_level; // 3 bit
    uint32_t external_trg_out; // 1 bit
    uint32_t software_trg_out; // 1 bit
    
    //REG_BUFF_ORG
    uint32_t buff_org;
    
    //REG_READOUT_BLT_AGGREGATE_NUMBER
    uint16_t max_board_agg_blt;
    
} V1730_card_config;

class V1730Settings : public DigitizerSettings {
    friend class V1730;

    public:
    
        V1730Settings();
        
        V1730Settings(RunTable &digitizer, RunDB &db);
        
        virtual ~V1730Settings();
        
        void validate();
        
        inline bool getEnabled(uint32_t ch) {
            return chans[ch].enabled;
        }
        
        inline uint32_t getRecordLength(uint32_t ch) {
            return groups[ch/2].record_length;
        }
        
        inline uint32_t getDCOffset(uint32_t ch) {
            return chans[ch].dc_offset;
        }
        
        inline uint32_t getPreSamples(uint32_t ch) {
            return chans[ch].pre_trigger;
        }
        
        inline uint32_t getThreshold(uint32_t ch) {
            return chans[ch].trg_threshold;
        }
        
        inline std::string getIndex() {
            return index;
        }
    
    protected:
    
        V1730_card_config card;
        V1730_group_config groups[8];
        V1730_chan_config chans[16];
        
        void chanDefaults(uint32_t ch);
        
        void groupDefaults(uint32_t gr);
        
};

class V1730 : public Digitizer {
    
    public:
    
        //system wide
        static constexpr uint32_t REG_CONFIG = 0x8000;
        static constexpr uint32_t REG_CONFIG_SET = 0x8004;
        static constexpr uint32_t REG_CONFIG_CLEAR = 0x8008;
        static constexpr uint32_t REG_BUFF_ORG = 0x800C;
        static constexpr uint32_t REG_CHANNEL_CALIB = 0x809C;
        static constexpr uint32_t REG_FRONT_PANEL_CONTROL = 0x811C;
        static constexpr uint32_t REG_LVDS_NEW_FEATURES = 0x81A0;
        static constexpr uint32_t REG_DUMMY = 0xEF20;
        static constexpr uint32_t REG_SOFTWARE_RESET = 0xEF24;
        static constexpr uint32_t REG_SOFTWARE_CLEAR = 0xEF28;
        static constexpr uint32_t REG_BOARD_CONFIGURATION_RELOAD = 0xEF34;

        //per channel, or with = 0x0n00
        static constexpr uint32_t REG_RECORD_LENGTH = 0x1020;
        static constexpr uint32_t REG_DYNAMIC_RANGE = 0x1024;
        static constexpr uint32_t REG_NEV_AGGREGATE = 0x1034;
        static constexpr uint32_t REG_PRE_TRG = 0x1038;
        static constexpr uint32_t REG_SHORT_GATE = 0x1054;
        static constexpr uint32_t REG_LONG_GATE = 0x1058;
        static constexpr uint32_t REG_PRE_GATE = 0x105C;
        static constexpr uint32_t REG_DPP_TRG_THRESHOLD = 0x1060;
        static constexpr uint32_t REG_BASELINE_THRESHOLD = 0x1064;
        static constexpr uint32_t REG_SHAPED_TRIGGER_WIDTH = 0x1070;
        static constexpr uint32_t REG_TRIGGER_HOLDOFF = 0x1074;
        static constexpr uint32_t REG_DPP_CTRL = 0x1080;
        static constexpr uint32_t REG_TRIGGER_CTRL = 0x1084;
        static constexpr uint32_t REG_DC_OFFSET = 0x1098;
        static constexpr uint32_t REG_CHANNEL_TEMP = 0x10A8;
        
        //per couple, add 4*couple 
        static constexpr uint32_t REG_LOCAL_VALIDATION = 0x8180; 

        //acquisition 
        static constexpr uint32_t REG_ACQUISITION_CONTROL = 0x8100;
        static constexpr uint32_t REG_ACQUISITION_STATUS = 0x8104;
        static constexpr uint32_t REG_SOFTWARE_TRIGGER = 0x8108;
        static constexpr uint32_t REG_GLOBAL_TRIGGER_MASK = 0x810C;
        static constexpr uint32_t REG_TRIGGER_OUT_MASK = 0x8110;
        static constexpr uint32_t REG_CHANNEL_ENABLE_MASK = 0x8120;

        //readout
        static constexpr uint32_t REG_EVENT_SIZE = 0x814C;
        static constexpr uint32_t REG_READOUT_CONTROL = 0xEF00;
        static constexpr uint32_t REG_READOUT_STATUS = 0xEF04;
        static constexpr uint32_t REG_VME_ADDRESS_RELOCATION = 0xEF10;
        static constexpr uint32_t REG_READOUT_BLT_AGGREGATE_NUMBER = 0xEF1C;
        
        V1730(VMEBridge &bridge, uint32_t baseaddr);
        
        virtual ~V1730();
        
        virtual void calib();

        virtual bool program(DigitizerSettings &settings);
        
        virtual void startAcquisition();
        
        virtual void stopAcquisition();
        
        virtual bool acquisitionRunning();
        
        virtual bool readoutReady();
        
        virtual bool checkTemps(std::vector<uint32_t> &temps, uint32_t danger);
        
    protected:
        
        size_t readoutBLT_evtsz(char *buffer, size_t buffer_size);

};

class V1730Decoder : public Decoder {

    public: 
    
        V1730Decoder(size_t eventBuffer, V1730Settings &settings);
        
        virtual ~V1730Decoder();
        
        virtual void decode(Buffer &buffer);
        
        virtual size_t eventsReady();
        
        virtual void writeOut(H5::H5File &file, size_t nEvents);

    protected:
        
        size_t eventBuffer;
        V1730Settings &settings;
        
        size_t decode_counter;
        size_t chanagg_counter;
        size_t boardagg_counter;
        
        size_t decode_size;
        struct timespec last_decode_time;
        
        std::map<uint32_t,uint32_t> chan2idx,idx2chan;
        std::vector<size_t> nsamples;
        std::vector<size_t> grabbed;
        std::vector<uint16_t*> grabs, baselines, qshorts, qlongs, patterns;
        std::vector<uint64_t*> times;

        uint32_t* decode_chan_agg(uint32_t *chanagg, uint32_t group, uint16_t pattern);

        uint32_t* decode_board_agg(uint32_t *boardagg);

};

#endif

