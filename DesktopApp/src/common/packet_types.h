#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <QMetaType>

struct ProcessingStats {
    long total_rows = 0;
    long total_attacks = 0;

    // Phase Timings (in seconds)
    double load_time = 0;
    double preprocess_time = 0;
    double inference_time = 0;
    double total_time = 0;

    double rows_per_second = 0;
};

Q_DECLARE_METATYPE(ProcessingStats)

#endif