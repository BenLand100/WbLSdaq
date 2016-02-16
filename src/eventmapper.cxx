#include <H5Cpp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <cstring>
#include <string>
#include <math.h>
#include <unistd.h>
#include <glob.h>

using namespace std;
using namespace H5;

// Extracts patterns dataset from a group in an hdf5 file, stores in patterns argument
void getPatterns(H5File &file, const string &gname, vector<uint16_t> &patterns) {
    Group group = file.openGroup(gname);
    DataSet dataset = group.openDataSet("patterns");

    hsize_t dims[1];
    DataSpace dataspace = dataset.getSpace();
    dataspace.getSimpleExtentDims(dims);
    
    patterns.resize(dims[0]);
    dataset.read(&patterns[0],PredType::NATIVE_UINT16,dataspace);
}

typedef struct {
    int file, index;
    uint16_t pattern;
} locator;

typedef struct {
    locator master, fast;
} event;


[[noreturn]] void help() {
    cout << "eventmapper reads all files that match `${prefix}.${index}.h5` ";
    cout << "respecting the optional bounds on the index and generates ";
    cout << "an event map as `${prefix}.map.csv` containg correlated events ";
    cout << "determined by scanning the files and matching sequential events ";
    cout << "that are equal according to the test mask or ophans which are ";
    cout << "the lesser of two events according to the comparison mask excepting ";
    cout << "cases where the offset according to the comparion mask is greater ";
    cout << "than the specified max offset and the larger is orphaned. In ";
    cout << "exceptional cases (with -r) when the raw offset is more than the max offset ";
    cout << "and the patterns do not pass equality events from both cards will ";
    cout << "be skipped to try to get back on track. This only happens when ";
    cout << "there are faulty connections on the LVDS inputs." << endl; 
    cout << "./eventmapper [options] prefix" << endl;
    cout << "\t-v            enable verbose mode" << endl;
    cout << "\t-r            resync by skipping events until things look right" << endl;
    cout << "\t-s index      force start file index [least]" << endl;
    cout << "\t-e index      force end file index [greatest]" << endl;
    cout << "\t-t mask       set equality test mask [0xEF]" << endl;
    cout << "\t-c mask       set comparison mask [0x0F]" << endl;
    cout << "\t-o offset     set max offset used for comparison [8]" << endl;
    cout << "\t-m group      set master pattern group [/fast/gr0/]" << endl;
    cout << "\t-f group      set fast pattern group [/master/ch0/]" << endl;
    exit(1);
}

// Assumes two cards (fast w/ g0 patterns, master w/ c0 petterns) with card-wide 
// triggers that have stored LVDS patterns of which some bits are an external
// trigger count. Missed triggers will have -1 in the locator fields.
int main(int argc, char **argv) {
    
    // The bitmask used to compare two patterns for equality
    uint16_t test_mask = 0xEF; // bit 4 acts up on master, only 8 bits of counter
    // The bitmask applied before magnitude comparison when patterns don't match up
    uint16_t comp_mask = 0x0F; // if they don't match... compare the first four
    // If the magnitude is greater than this assume the count rolled over
    size_t max_offset = 8;
    // Group to read master patterns from
    string master_group = "/master/ch0/";
    // Group to read fast patterns from
    string fast_group = "/fast/gr0/";
    // Start index
    int startidx = -1;
    // End index
    int endidx = -1;
    // File prefix to read from
    string fprefix;
    // enable skipping to resync
    bool resync = false;
    // enable auto-orphaning detected retriggers
    bool retrigger = true;
    // Extra debug flag
    bool verbose = false;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, ":s:e:vrt:c:o:m:f:")) != -1) {
        switch (c) {
            case 's':
                startidx = stoull(optarg,NULL,0);
                break;
            case 'e':
                endidx = stoull(optarg,NULL,0);
                break;
            case 'v':
                verbose = true;
                break;
            case 'r':
                resync = true;
                break;
            case 't':
                test_mask = stoul(optarg,NULL,0) & 0xFFFF;
                break;
            case 'c':
                comp_mask = stoul(optarg,NULL,0) & 0xFFFF;
                break;
            case 'o':
                max_offset = stoull(optarg,NULL,9);
                break;
            case 'm':
                master_group = string(optarg);
                break;
            case 'f':
                fast_group = string(optarg);
                break;
            case ':':
                cout << "-" << optopt << " requires an argument" << endl;
                help();
            case '?':
                cout << "-" << optopt << " is an unknown option" << endl;
                help();
            default:
                cout << "Unexpected result from getopt" << endl;
                help();
        }
    }

    if (argc - optind != 1) help();
    fprefix = argv[optind];

    if (startidx < 0 || endidx < 0) {
        glob_t files;
        string pattern = fprefix+".*.h5";
        glob(pattern.c_str(),0,NULL,&files);
        size_t prelen = fprefix.length();
        size_t min = INT_MAX, max = 0;
        for (size_t i = 0; i < files.gl_pathc; i++) {
            int len = strlen(files.gl_pathv[i]);
            string fname(&(files.gl_pathv[i])[prelen+1],len-prelen-4);
            size_t j = stoull(fname);
            if (j < min) min = j;
            if (j > max) max = j;
        }
        if (startidx < 0) startidx = min;
        if (endidx < 0) endidx = max;
    }

    vector<event> events;
    list<locator> master_overflow, fast_overflow;
    size_t skipped = 0, total_skipped = 0, orphans = 0, master_orphans = 0, fast_orphans = 0, total_events = 0;
    
    //loop over files
    for (int fidx = startidx; fidx <= endidx; fidx++) {
    
        string fname = fprefix+"."+to_string(fidx)+".h5";
        if (verbose) cout << "Opening file " << fname << endl;
        H5File file(fname, H5F_ACC_RDONLY);
        
        vector<uint16_t> master_patterns, fast_patterns;
        getPatterns(file, master_group, master_patterns);
        getPatterns(file, fast_group, fast_patterns);
        
        bool lastorphaned = false;

        //loop over triggers
        size_t mi = 0, fi = 0;
        while ((mi < master_patterns.size() && fi < fast_patterns.size()) || (master_overflow.size() && fast_overflow.size())) {
           
            if (orphans > max_offset) {
                cout << "Too many orphans in a row - bailing out." << endl;
                return 1;
            }


            // Populate event with pending or next independently
            event ev;
            if (master_overflow.size()) {
                ev.master = master_overflow.front();
                master_overflow.pop_front();
            } else {
                ev.master.file = fidx;
                ev.master.index = mi;
                ev.master.pattern = master_patterns[mi];
                mi++;
            }
            if (fast_overflow.size()) {
                ev.fast = fast_overflow.front();
                fast_overflow.pop_front();
            } else {
                ev.fast.file = fidx;
                ev.fast.index = fi;
                ev.fast.pattern = fast_patterns[fi];
                fi++;
            }
            
            // One of the two triggers might be an orphan
            if ((ev.master.pattern & test_mask) != (ev.fast.pattern & test_mask)) {
                
                cout << "Discontinuity found - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask); 
                cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
                
                if ((ev.master.pattern ) - 99 == (ev.fast.pattern) ) {
                    cout << "\tmaster -99 bug..." << endl;
                    continue;
                } else if (ev.master.pattern - 99 + 16 == ev.fast.pattern) {
                    cout << "\tmaster -99+16 bug..." << endl;
                    continue;
                } else if (ev.master.pattern - 100 == ev.fast.pattern) {
                    cout << "\tmaster -100 bug..." << endl;
                    continue;
                } else if (ev.master.pattern - 100 + 16 == ev.fast.pattern) {
                    cout << "\tmaster -100+16 bug..." << endl;
                    continue;
                } else if (ev.master.pattern + 156 == ev.fast.pattern) {
                    cout << "\tmaster +156 bug..." << endl;
                    continue;
                } else if (ev.master.pattern + 156 + 16  == ev.fast.pattern) {
                    cout << "\tmaster -100+16 bug..." << endl;
                    continue;
                }
               int mdelta = (ev.master.pattern) - (events.back().master.pattern);
               int fdelta = (ev.fast.pattern) - (events.back().fast.pattern);
               int delta = (ev.fast.pattern & comp_mask) - (ev.master.pattern & comp_mask);
               
               if (!lastorphaned && abs(mdelta) == 1 && abs(fdelta) > max_offset) {
                   master_overflow.push_front(ev.master);
                   cout << "\tfast jump was orphaned" << endl;
                   ev.master.file = -1;
                   ev.master.index = -1;
                   ev.master.pattern = -1;
                   fast_orphans++;
                   orphans++;
                   continue;
               } else if (!lastorphaned && abs(fdelta) == 1 && abs(mdelta) > max_offset) {
                    cout << "\tmaster jump was orphaned" << endl;
                    fast_overflow.push_front(ev.fast);
                    ev.fast.file = -1;
                    ev.fast.index = -1;
                    ev.fast.pattern = -1;
                    master_orphans++;
                    orphans++;
                    continue;
               }

               if (resync) { 
                    if (skipped && abs(ev.fast.pattern-ev.master.pattern) < max_offset) {
                        cout << "\tPossibly back on track..." << endl;
                    } else if ((abs(delta) != 1) && (abs(mdelta) > max_offset+skipped || abs(fdelta) > max_offset+skipped)) {
                        cout << "\tWay off track - skipping (" << skipped << ") events as last ditch effort" << endl;
                        skipped++;
                        total_skipped++;
                        if (skipped > 255) {
                            cout << "\tWe skipped more events than we can count, bailing out." << endl;
                            return -1;
                        }
                        continue;
                    }    
                    skipped = 0;
                }

                if (retrigger) {
                    if ((ev.master.pattern & test_mask) == (events.back().master.pattern & test_mask)) {
                        cout << "\tmaster retrigger was orphaned" << endl;
                        fast_overflow.push_front(ev.fast);
                        ev.fast.file = -1;
                        ev.fast.index = -1;
                        ev.fast.pattern = -1;
                        master_orphans++;
                        continue;
                    } else if ((ev.fast.pattern & test_mask) == (events.back().fast.pattern & test_mask)) {
                        master_overflow.push_front(ev.master);
                        cout << "\tfast retrigger was orphaned" << endl;
                        ev.master.file = -1;
                        ev.master.index = -1;
                        ev.master.pattern = -1;
                        fast_orphans++;
                        continue;
                    }

                }
            
                bool invert = abs((ev.fast.pattern & comp_mask) - (ev.master.pattern & comp_mask)) > 8;
                    if (((ev.fast.pattern & comp_mask) > (ev.master.pattern & comp_mask)) != invert) {
                        cout << "\tmaster was orphaned" << endl;
                        fast_overflow.push_front(ev.fast);
                        ev.fast.file = -1;
                        ev.fast.index = -1;
                        ev.fast.pattern = -1;
                        master_orphans++;
                    } else {
                        master_overflow.push_front(ev.master);
                        cout << "\tfast was orphaned" << endl;
                        ev.master.file = -1;
                        ev.master.index = -1;
                        ev.master.pattern = -1;
                        fast_orphans++;
                    }
                    orphans++;

                lastorphaned = true;
            } else {
                lastorphaned = false;
                if (verbose) {
                    cout << "Good event - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                    cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                    cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask);
                    cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
                }
                orphans = 0;
            }
             
            // Add the final event to the list
            events.push_back(ev);
            total_events++;
        }
        
        for (; mi < master_patterns.size(); mi++) {
            locator m;
            m.file = fidx;
            m.index = mi;
            m.pattern = master_patterns[mi];
            master_overflow.push_back(m);
        }
        
        for (; fi < fast_patterns.size(); fi++) {
            locator f;
            f.file = fidx;
            f.index = fi;
            f.pattern = fast_patterns[fi];
            fast_overflow.push_back(f);
        }
        
    }
    
    cout << "Events: " << total_events << ", Master Orphans: " << master_orphans << ", Fast Orphans: " << fast_orphans << ", Skipped: " << total_skipped << endl;
    
    //May have some orphans in one or the other overflow (but not both so no checks for correlations)
    while (master_overflow.size()) {
        event ev;
        ev.master = master_overflow.front();
        master_overflow.pop_front();
        ev.fast.file = -1;
        ev.fast.index = -1;
        ev.fast.pattern = -1;
        events.push_back(ev);
        master_orphans++;
    }
    while (fast_overflow.size()) {
        event ev;
        ev.fast = fast_overflow.front();
        fast_overflow.pop_front();
        ev.master.file = -1;
        ev.master.index = -1;
        ev.master.pattern = -1;
        events.push_back(ev);
        fast_orphans++;
    }
    
    ofstream evmap;
    evmap.open(fprefix+".map.csv");
    evmap << "\"event_index\", \"master_file\", \"master_index\", \"fast_file\", \"fast_index\"" << endl;
    for (size_t i = 0; i < events.size(); i++) {
        evmap << i << ", ";
        evmap << events[i].master.file << ", ";
        evmap << events[i].master.index << ", ";
        evmap << events[i].fast.file << ", ";
        evmap << events[i].fast.index << endl;
    }
    evmap.close();
    
}
    
    
