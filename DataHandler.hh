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
#include <string>
#include <vector>

#include "V1730_dpppsd.hh"
#include "Buffer.hh"

#ifndef DataHandler__hh
#define DataHandler__hh

class DataHandler {

    public:
    
        // for data acquisition
        DataHandler(size_t nGrabs, size_t nRepeat, std::string outfile, V1730Settings &_settings);
        
        // for debugging / trigger rate
        DataHandler(V1730Settings &_settings);
        
        virtual ~DataHandler();
        
        bool decode(Buffer &buf);
        
        void init(size_t nGrabs, size_t nRepeat, std::string outfile);

        uint32_t* decode_chan_agg(uint32_t *chanagg, uint32_t group);

        uint32_t* decode_board_agg(uint32_t *boardagg);
        
        void writeout();
        
    protected: 
        
        V1730Settings &settings;
        
        struct timespec last_time;
        double time_int;
        
        size_t cycle;    
        size_t decode_counter;
        size_t chanagg_counter;
        size_t boardagg_counter;
        
        size_t decode_size;
            
        size_t nGrabs, nRepeat;
        std::string outfile;    
        
        std::map<uint32_t,uint32_t> chan2idx,idx2chan;
        std::vector<size_t> nsamples;
        std::vector<size_t> grabbed,lastgrabbed;
        std::vector<uint16_t*> grabs, baselines, qshorts, qlongs;
        std::vector<uint32_t*> times;

};

#endif

