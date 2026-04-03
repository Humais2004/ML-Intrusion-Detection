#include "preprocessor.h"
#include <vector>
#include <omp.h>
#include <iostream>
#include <cmath>      
#include <algorithm>  
#include <mpi.h> // <--- CRITICAL: Fixes the "MPI_Wtime is undefined" error

const int NUM_RAW_FEATURES = 80;
const int NUM_CLEAN_FEATURES = 39; // Exactly 39 inputs. No Label.

namespace Col {
    constexpr int BwdPktLenCV = 0;
    constexpr int FwdPktLenCV = 1;
    constexpr int PktLenCV = 2;
    constexpr int DstPort = 3;
    constexpr int FlowDuration = 4;
    constexpr int TotFwdPkts = 5;
    constexpr int TotBwdPkts = 6;
    constexpr int FlowBytsPers = 7;
    constexpr int FlowPktsPers = 8;
    constexpr int FwdPktsPers = 9;
    constexpr int BwdPktsPers = 10;
    constexpr int PktLenMean = 11;
    constexpr int FwdPktLenMean = 12;
    constexpr int BwdPktLenMean = 13;
    constexpr int InitFwdWinByts = 14;
    constexpr int InitBwdWinByts = 15;
    constexpr int ActiveMean = 16;
    constexpr int IdleMean = 17;
    constexpr int FlowIATMean = 18;
    constexpr int FlowIATStd = 19;
    constexpr int FwdIATMean = 20;
    constexpr int BwdIATMean = 21;
    constexpr int DownPerUpRatio = 22;
    constexpr int FwdBwdPktRatio = 23;
    constexpr int TrafficSymmetry = 24;
    constexpr int ResponseRatio = 25;
    constexpr int BidirectionalActive = 26;
    constexpr int IatRegularity = 27;
    constexpr int FlowRate = 28;
    constexpr int TimingAsymmetry = 29;
    constexpr int FwdBwdSizeDiff = 30;
    constexpr int IsSmallPacket = 31;
    constexpr int IsRiskyPort = 32;
    constexpr int IsWebTraffic = 33;
    constexpr int PortCategory = 34;
    constexpr int AnomalyScore = 35;
    constexpr int Protocol_0_0 = 36;
    constexpr int Protocol_17_0 = 37;
    constexpr int Protocol_6_0 = 38;
}

void runPreprocessor(BatchManager<std::vector<float>>& raw_queue,
    BatchManager<std::vector<float>>& clean_queue, double& active_time) {

    std::vector<float> raw_batch;

    // START INTERNAL TIMER: Captures only the time spent processing
    double start = MPI_Wtime();

    while (raw_queue.consume(raw_batch)) {
        int total_rows = raw_batch.size() / NUM_RAW_FEATURES;
        std::vector<float> clean_batch(total_rows * NUM_CLEAN_FEATURES);

        #pragma omp parallel for
        for (int i = 0; i < total_rows; ++i) {
            int r_idx = i * NUM_RAW_FEATURES;
            int c_idx = i * NUM_CLEAN_FEATURES;

            // Basic metrics
            float tot_fwd = raw_batch[r_idx + 4];
            float tot_bwd = raw_batch[r_idx + 5];
            float total_pkts = tot_fwd + tot_bwd;
            int dst_port = static_cast<int>(raw_batch[r_idx + 0]);

            // Feature Engineering Calculations
            clean_batch[c_idx + Col::BwdPktLenCV] = raw_batch[r_idx + 15] / (raw_batch[r_idx + 14] + 1e-5f);
            clean_batch[c_idx + Col::FwdPktLenCV] = raw_batch[r_idx + 11] / (raw_batch[r_idx + 10] + 1e-5f);
            clean_batch[c_idx + Col::PktLenCV] = raw_batch[r_idx + 43] / (raw_batch[r_idx + 42] + 1e-5f);
            clean_batch[c_idx + Col::DstPort] = raw_batch[r_idx + 0];
            clean_batch[c_idx + Col::FlowDuration] = raw_batch[r_idx + 3];
            clean_batch[c_idx + Col::TotFwdPkts] = tot_fwd;
            clean_batch[c_idx + Col::TotBwdPkts] = tot_bwd;
            clean_batch[c_idx + Col::FlowBytsPers] = raw_batch[r_idx + 16];
            clean_batch[c_idx + Col::FlowPktsPers] = raw_batch[r_idx + 17];
            clean_batch[c_idx + Col::FwdPktsPers] = raw_batch[r_idx + 38];
            clean_batch[c_idx + Col::BwdPktsPers] = raw_batch[r_idx + 39];
            clean_batch[c_idx + Col::PktLenMean] = raw_batch[r_idx + 42];
            clean_batch[c_idx + Col::FwdPktLenMean] = raw_batch[r_idx + 10];
            clean_batch[c_idx + Col::BwdPktLenMean] = raw_batch[r_idx + 14];
            clean_batch[c_idx + Col::InitFwdWinByts] = raw_batch[r_idx + 67];
            clean_batch[c_idx + Col::InitBwdWinByts] = raw_batch[r_idx + 68];
            clean_batch[c_idx + Col::ActiveMean] = raw_batch[r_idx + 71];
            clean_batch[c_idx + Col::IdleMean] = raw_batch[r_idx + 75];
            clean_batch[c_idx + Col::FlowIATMean] = raw_batch[r_idx + 18];
            clean_batch[c_idx + Col::FlowIATStd] = raw_batch[r_idx + 19];
            clean_batch[c_idx + Col::FwdIATMean] = raw_batch[r_idx + 23];
            clean_batch[c_idx + Col::BwdIATMean] = raw_batch[r_idx + 28];
            clean_batch[c_idx + Col::DownPerUpRatio] = raw_batch[r_idx + 53];

            clean_batch[c_idx + Col::FwdBwdPktRatio] = tot_fwd / (tot_bwd + 1.0f);
            clean_batch[c_idx + Col::TrafficSymmetry] = std::min(tot_fwd / (total_pkts + 1.0f), tot_bwd / (total_pkts + 1.0f));
            clean_batch[c_idx + Col::ResponseRatio] = tot_bwd / (tot_fwd + 1.0f);
            clean_batch[c_idx + Col::BidirectionalActive] = (tot_fwd > 0.0f && tot_bwd > 0.0f) ? 1.0f : 0.0f;
            clean_batch[c_idx + Col::IatRegularity] = raw_batch[r_idx + 20] / (raw_batch[r_idx + 19] + 1.0f);

            float flow_rate = total_pkts / (raw_batch[r_idx + 3] + 0.001f);
            clean_batch[c_idx + Col::FlowRate] = flow_rate;
            clean_batch[c_idx + Col::TimingAsymmetry] = std::abs(raw_batch[r_idx + 23] - raw_batch[r_idx + 28]);
            clean_batch[c_idx + Col::FwdBwdSizeDiff] = std::abs(raw_batch[r_idx + 10] - raw_batch[r_idx + 14]);

            float is_small_packet = (raw_batch[r_idx + 42] < 100.0f) ? 1.0f : 0.0f;
            clean_batch[c_idx + Col::IsSmallPacket] = is_small_packet;

            // Port Analysis
            bool risky = (dst_port == 21 || dst_port == 22 || dst_port == 23 || dst_port == 25 ||
                dst_port == 445 || dst_port == 3389 || dst_port == 1433 || dst_port == 3306);
            bool web = (dst_port == 80 || dst_port == 443 || dst_port == 8080 || dst_port == 8443);
            clean_batch[c_idx + Col::IsRiskyPort] = risky ? 1.0f : 0.0f;
            clean_batch[c_idx + Col::IsWebTraffic] = web ? 1.0f : 0.0f;

            if (dst_port <= 1024) clean_batch[c_idx + Col::PortCategory] = 0.0f;
            else if (dst_port <= 49151) clean_batch[c_idx + Col::PortCategory] = 1.0f;
            else clean_batch[c_idx + Col::PortCategory] = 2.0f;

            // Anomaly Scoring
            float anomaly_score = 0.0f;
            if (clean_batch[c_idx + Col::TrafficSymmetry] < 0.1f) anomaly_score += 1.0f;
            anomaly_score += is_small_packet;
            if (flow_rate > 1000.0f) anomaly_score += 1.0f;
            if (clean_batch[c_idx + Col::IatRegularity] < 0.05f) anomaly_score += 1.0f;
            if (risky) anomaly_score += 1.0f;
            clean_batch[c_idx + Col::AnomalyScore] = anomaly_score;

            // Protocol Encoding
            float raw_protocol = raw_batch[r_idx + 1];
            clean_batch[c_idx + Col::Protocol_0_0] = (raw_protocol == 0.0f) ? 1.0f : 0.0f;
            clean_batch[c_idx + Col::Protocol_17_0] = (raw_protocol == 17.0f) ? 1.0f : 0.0f;
            clean_batch[c_idx + Col::Protocol_6_0] = (raw_protocol == 6.0f) ? 1.0f : 0.0f;

            // ========================================================================
            // FINAL SANITY GUARD
            // This prevents inf/nan from ever reaching XGBoost
            // ========================================================================
            for (int j = 0; j < NUM_CLEAN_FEATURES; ++j) {
                float& val = clean_batch[c_idx + j];

                if (!std::isfinite(val)) {
                    val = 0.0f;
                }

                // Prevent extreme outliers from floating point math errors
                if (val > 1e10f) val = 1e10f;
                if (val < -1e10f) val = -1e10f;
            }
        }

        clean_queue.produce(std::move(clean_batch));
    }

    // STOP INTERNAL TIMER
    active_time = MPI_Wtime() - start;

    clean_queue.setFinished();
    std::cout << "[PREPROCESSOR] Work finished in " << active_time << " seconds." << std::endl;
}