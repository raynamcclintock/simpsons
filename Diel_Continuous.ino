#include <EEPROM.h>       //To log each event
#include <Narcoleptic.h>  //for the Sleep function
#include <RTClib.h>

struct Sample {
    // add this to start_time to get the absolute time of the sample
    TimeSpan start;
    // duration for which to pump in milliseconds
    unsigned long dur;
};

// This program is used to only run a single 12 port run and has a sleep cycle
// Variables to modify:
//*****************************************************************
// 220ml/min, 1000ml bag, so 1000/220*60 = 272 seconds
// constexpr unsigned long sample_ms = 3000UL;

// Narcoleptic sleep delay time. max = 30000ms = 30sec
constexpr uint32_t sleep = 1000; // 1 second sleep chunks

// Pump runs at 38ml/min. Not much time is needed to flush the manifold (15ml)
// plus 61cm of 0.16cm radius tubing = total volume of 20ml. A minimum of ~32s 
// or 32000 is enough time to flush once.
constexpr int flush_ms = 32000; //You can use a smaller value like 4000 when testing

// Time at which to start sampling
// DateTime constructor takes  YYYY,MM, DD, HH, MM, SS
DateTime start_time = DateTime(2022, 4, 07, 12, 00, 00);

// specifies when samples are to be taken relative to start_time and for how
// long the pump should run for each sample
// in this example, the first sample is taken 0 seconds after start_time
// the second sample is 10 seconds after start_time, and so on
// entry format is D,H,M,S.
Sample samples[] = {
    Sample{.start=TimeSpan((int32_t)0,0,0,0), 850000UL}, //Sample 1 at T+0 seconds, ends at T+30 mins
    Sample{.start=TimeSpan(0,3,0,0), 850000UL}, //Sample 2 at T+30 minutes, ends at T+1 hr  there is a five minute buffer between
    Sample{.start=TimeSpan(0,6,0,0), 850000UL}, //Sample 3 at T+4 hr (3 hr gap from previous), ends at T+4.5
    Sample{.start=TimeSpan(0,9,0,0), 850000UL}, //Sample 4 at T+4.5 hr, ends at T+5
    Sample{.start=TimeSpan(0,12,0,0), 850000UL}, //Sample 5 at T+8 hr (3 hr gap from previous), ends at T+8.5
    Sample{.start=TimeSpan(0,15,0,0), 850000UL}, //Sample 6 at T+8.5 hr, ends at T+9
    Sample{.start=TimeSpan(0,18,0,0), 850000UL}, //Sample 7 at T+12 hr (3 hr gap from previous), ends at T+12.5
    Sample{.start=TimeSpan(0,21,0,0), 850000UL}, //Sample 8 at T+12.5 hr, ends at T+13 hr
    Sample{.start=TimeSpan(0,24,0,0), 850000UL}, //Sample 9 at T+16 hr (3 hr gap from previous), ends at T+16.5
    Sample{.start=TimeSpan(0,27,0,0), 850000UL}, //Sample 10 at T+16.5 hr, ends at T+17
    Sample{.start=TimeSpan(0,30,0,0), 850000UL}, //Sample 11 at T+20 hr (3 hr gap from previous), ends at T+20.5
    Sample{.start=TimeSpan(0,33,0,0), 850000UL}, //Sample 12 at T+20.5 hr, ends at T+21
};

// If you wish to sample periodically i.e. at regular intervals set this to the
// period of the cycle, otherwise, ignore it.
TimeSpan period = TimeSpan(0,36,0,0);

// If the number of cycles (during which all valves are used to take samples)
// if you wish to sample from each valve only once, set this to 1
constexpr unsigned num_periods = 10;

//*****************************************************************
// DO NOT MODIFY ANYTHING BELOW THIS LINE

static_assert(
    num_periods > 0, 
    "num_periods must be positive, otherwise no samples will be taken."
);

// fancy template that returns the length of an array at compile time
template <typename T, size_t N>
constexpr size_t array_len(T (&)[N]) {
    return N;
}

// define relay ON or OFF
constexpr uint8_t relayOn = 0;
constexpr uint8_t relayOff = 1;
// Valves
constexpr uint8_t flushValve = A3;
constexpr uint8_t pump = A2;
// valve 1,2,3,4,5,6,7,8,9,10,11,12
constexpr uint8_t sample_valves[] = {7, 8, 9, 10, 11, 12, 13, A0, A1, 4, 5, 6};

static_assert(
    array_len(samples) == array_len(sample_valves),
    "There must be exactly the same number of samples as valves to sample from"
);

RTC_DS3231 rtc;

void setup() {
    // Sets all pins to output mode
    for (uint8_t pin: sample_valves) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, relayOff);
    }

    pinMode(pump, OUTPUT);
    pinMode(flushValve, OUTPUT);

    digitalWrite(pump, relayOff);
    digitalWrite(flushValve, relayOff);

    Serial.begin(9600);
    if (!rtc.begin()) {
        Serial.println(F("Real time clock was not found"));
    }
    // adjust RTC to the compile time of this sketch
    rtc.adjust(DateTime(__DATE__, __TIME__));

    //check that samples do not overlap
    DateTime prev_sampling_end = start_time + samples[0].start - TimeSpan(1);
    for (size_t valve_idx = 0; valve_idx < array_len(samples); valve_idx++) {
        Sample sample = samples[valve_idx];
        if (start_time + sample.start < prev_sampling_end) {
            Serial.println(F("Error - sampling overlap"));
            Serial.print(F("Sample with index "));
            Serial.print(valve_idx-1);
            Serial.println(F(" overlaps with next sample"));
        }
        prev_sampling_end = start_time + sample.start + TimeSpan(sample.dur/1000 + 1);
    }
}

void loop() {
    // Modify the lines below to write to SD card for nano, substitute
    // Serial.print with myFile.print
    DateTime setup_time = rtc.now();
    char setup_timestamp[] = "YYYY-MM-DD hh:mm:ss";
    setup_time.toString(setup_timestamp);

    Serial.println(F("********Parameter Header********"));
    Serial.println(setup_timestamp);
    Serial.print(F("Number of samples per period = "));
    Serial.println(array_len(samples));
    Serial.print(F("Number of sampling periods = "));
    Serial.println(num_periods);
    Serial.print(F("Flush Time (milliseconds) = "));
    Serial.println(flush_ms);
    Serial.println(F("********************************"));
    delay(100);

    while (rtc.now() <= start_time) {}

    for (unsigned period_idx = 0; period_idx<num_periods; period_idx++) {
        DateTime period_start_time = start_time + TimeSpan(period.totalseconds() * period_idx);

        Serial.print(F("Sample period "));
        Serial.print(period_idx + 1);
        Serial.print(F(" of "));
        Serial.print(num_periods);
        Serial.print(F(" Scheduled for "));
        char period_timestamp[] = "YYYY-MM-DD hh:mm:ss";
        period_start_time.toString(period_timestamp);
        Serial.println(period_timestamp);

        // loop over the valves
        for (size_t valve_idx = 0; valve_idx < array_len(samples); valve_idx++) {
            Sample sample = samples[valve_idx];
            DateTime sample_start_time = period_start_time + sample.start;

            char sample_scheduled_timestamp[] = "YYYY-MM-DD hh:mm:ss";
            sample_start_time.toString(sample_scheduled_timestamp);
            Serial.print(F("Sample "));
            Serial.print(valve_idx + 1);
            Serial.print(F(" period "));
            Serial.print(period_idx + 1);
            Serial.print(F(" scheduled for "));
            Serial.println(sample_scheduled_timestamp);

            if (rtc.now() > sample_start_time) {
                Serial.print(F("Warning: sampling started "));
                Serial.print((rtc.now() - sample_start_time).totalseconds());
                Serial.println(F(" seconds late"));
            }

            while (rtc.now() <= sample_start_time) {}

            //flush
            digitalWrite(flushValve, relayOn);
            digitalWrite(pump, relayOn);
            delay(flush_ms);
            //sample
            digitalWrite(sample_valves[valve_idx], relayOn);
            digitalWrite(flushValve, relayOff);
            delay(sample.dur);
            digitalWrite(pump, relayOff);
            digitalWrite(sample_valves[valve_idx], relayOff);

            // Modify the lines below to write to SD card for nano, substitute
            // Serial.print with myFile.print
            DateTime sample_time = rtc.now();
            char sample_timestamp[] = "YYYY-MM-DD hh:mm:ss";
            sample_time.toString(sample_timestamp);

            Serial.print(sample_timestamp);
            Serial.print(' ');
            Serial.print(valve_idx + 1);
            Serial.println(F("_Sample collected"));
            delay(100);
        }
    }

    while (true) {
        Narcoleptic.delay(sleep);
    }
}
//Code by Max Blachman and David Mucciarone
//Stanford Underwater Laboratory
//January 28, 2021 
