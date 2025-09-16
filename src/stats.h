#pragma once
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

#include "common.h"
#include "utils.h"

namespace tribase {
class Stats {
   public:
    size_t total_count;

    // triangle part
    size_t skip_triangle_count;
    size_t skip_triangle_large_count;

    // subNN part
    size_t check_subnn_L2_ele_count;
    size_t check_subnn_IP_ele_count;
    size_t check_subnn_L2_count;
    size_t check_subnn_IP_count;
    size_t skip_subnn_L2_count;
    size_t skip_subnn_IP_count;

    // pivot part
    size_t check_pivot_count;      // how many pivot checks performed (per element summed)
    size_t skip_pivot_count;       // elements skipped due to any pivot bound

    size_t simi_update_count;
    size_t dis_calculate_count;
    // distance call counters
    size_t dis_calls_qx;  // d(q, x) true candidate distances
    size_t dis_calls_qp;  // d(q, pivot)
    size_t dis_calls_qc;  // d(q, centroid)

    // cluster-level pruning counters
    size_t total_cluster_count;   // clusters considered for cluster-level pruning
    size_t pruned_cluster_count;  // clusters pruned at cluster-level

    size_t nlist;
    size_t nprobe;

    double faiss_query_time;
    double query_time;

    OptLevel opt_level;

    double recall;
    double r2;
    float simi_ratio;

    size_t n_query;

    // summary
   private:
    float pruning_triangle;
    float pruning_triangle_large;
    float pruning_subnn_L2;
    float pruning_subnn_IP;
    float pruning_pivot;
    float check_subnn_L2;
    float check_subnn_IP;

    float simi_update_rate;

    float time_speedup;
    float pruning_speedup;
    double qps;
    // derived stats for distance calls
    double pct_true_distance;        // dis_calls_qx / total_count
    double calls_per_candidate;      // (dis_calls_qx + dis_calls_qp + dis_calls_qc) / total_count

   public:
    void reset() {
        total_count = 0;
        skip_triangle_count = 0;
        skip_triangle_large_count = 0;
        check_subnn_L2_ele_count = 0;
        check_subnn_IP_ele_count = 0;
        check_subnn_L2_count = 0;
        check_subnn_IP_count = 0;
        skip_subnn_L2_count = 0;
        skip_subnn_IP_count = 0;
        simi_update_count = 0;
        check_pivot_count = 0;
        skip_pivot_count = 0;
        dis_calls_qx = 0;
        dis_calls_qp = 0;
        dis_calls_qc = 0;
        total_cluster_count = 0;
        pruned_cluster_count = 0;
    }

    Stats() { reset(); }

    void summary() {
        dis_calculate_count = total_count - skip_triangle_count - skip_triangle_large_count - skip_subnn_L2_count - skip_subnn_IP_count - skip_pivot_count;
        simi_update_rate = dis_calculate_count == 0 ? 0 : 100.0 * simi_update_count / dis_calculate_count;
        pruning_triangle = skip_triangle_count == 0 ? 0 : 100.0 * skip_triangle_count / total_count;
        pruning_triangle_large = skip_triangle_large_count == 0 ? 0 : 100.0 * skip_triangle_large_count / total_count;
        pruning_subnn_L2 = skip_subnn_L2_count == 0 ? 0 : 100.0 * skip_subnn_L2_count / total_count;
        pruning_subnn_IP = skip_subnn_IP_count == 0 ? 0 : 100.0 * skip_subnn_IP_count / total_count;
        pruning_pivot = skip_pivot_count == 0 ? 0 : 100.0 * skip_pivot_count / total_count;
        check_subnn_IP = check_subnn_IP_count == 0 ? 0 : 1.0 * check_subnn_IP_ele_count / check_subnn_IP_count;
        check_subnn_L2 = check_subnn_L2_count == 0 ? 0 : 1.0 * check_subnn_L2_ele_count / check_subnn_L2_count;

        time_speedup = query_time == 0 ? 0 : 100.0 * faiss_query_time / query_time;
        pruning_speedup = dis_calculate_count == 0 ? 0 : 100.0 * total_count / dis_calculate_count;
        qps = static_cast<double>(n_query) / query_time;
        size_t dis_calls_total = dis_calls_qx + dis_calls_qp + dis_calls_qc;
        pct_true_distance = total_count == 0 ? 0.0 : static_cast<double>(dis_calls_qx) / static_cast<double>(total_count);
        calls_per_candidate = total_count == 0 ? 0.0 : static_cast<double>(dis_calls_total) / static_cast<double>(total_count);
    }

    void print() {
        summary();
        size_t dis_calls_total = dis_calls_qx + dis_calls_qp + dis_calls_qc;
        double avg_qc = n_query == 0 ? 0.0 : static_cast<double>(dis_calls_qc) / static_cast<double>(n_query);
        double avg_qp = n_query == 0 ? 0.0 : static_cast<double>(dis_calls_qp) / static_cast<double>(n_query);
        double avg_qx = n_query == 0 ? 0.0 : static_cast<double>(dis_calls_qx) / static_cast<double>(n_query);
        double avg_total = n_query == 0 ? 0.0 : static_cast<double>(dis_calls_total) / static_cast<double>(n_query);
        std::cout << std::format("nprobe:{} opt_level: {} simi_ratio: {}\n", nprobe, static_cast<int>(opt_level), simi_ratio)
                  << std::format("tri: {}({:.2f}%) tri_large: {}({:.2f}%) subnn_L2: {}({:.2f}%) subnn_IP: {}({:.2f}%) pivot: {}({:.2f}%)\n", skip_triangle_count, pruning_triangle, skip_triangle_large_count, pruning_triangle_large, skip_subnn_L2_count, pruning_subnn_L2, skip_subnn_IP_count, pruning_subnn_IP, skip_pivot_count, pruning_pivot)
                  << std::format("simi_update_rate: {:.2f}% check_L2: {} check_IP: {}\n", simi_update_rate, check_subnn_L2, check_subnn_IP)
                  << std::format("time_speedup: {:.2f}% pruning_speedup: {:.2f}% faiss_query_time: {:.6f} query_time: {:.6f} qps: {:f}\n", time_speedup, pruning_speedup, faiss_query_time, query_time, qps)
                  << std::format("dis_calls_qc: {} dis_calls_qp: {} dis_calls_qx: {} total_dis_calls: {} pct_true_distance: {:.4f} calls_per_candidate: {:.4f}\n", dis_calls_qc, dis_calls_qp, dis_calls_qx, dis_calls_total, pct_true_distance, calls_per_candidate)
                  << std::format("avg_per_query - dis_calls_qc: {:.2f} dis_calls_qp: {:.2f} dis_calls_qx: {:.2f} total_dis_calls: {:.2f}\n", avg_qc, avg_qp, avg_qx, avg_total)
                  << std::format("recall: {} r2: {}\n", recall, r2);
    }

    void toCsv(std::string filename, bool append, std::string dataset = "Unknown") {
        CsvWriter writer(filename,
                         {"dataset", "nlist", "nprobe", "opt_level", "simi_ratio",
                          "tri", "tri_large", "subnn_L2", "subnn_IP", "pivot_skip", "pivot_rate", "simi_update_rate",
                          "check_L2", "check_IP",
                          "time_speedup", "pruning_speedup", "query_time", "qps",
                          "recall", "r2",
                          "dis_calls_qc", "dis_calls_qp", "dis_calls_qx", "pct_true_distance", "calls_per_candidate"},
                         append, false);
        summary();
        writer << dataset << nlist << nprobe << static_cast<int>(opt_level) << simi_ratio
               << skip_triangle_count << skip_triangle_large_count << skip_subnn_L2_count << skip_subnn_IP_count << skip_pivot_count << pruning_pivot / 100 << simi_update_rate / 100
               << check_subnn_L2 << check_subnn_IP
               << time_speedup / 100 << pruning_speedup / 100 << query_time << qps
               << recall << r2
               << dis_calls_qc << dis_calls_qp << dis_calls_qx << pct_true_distance << calls_per_candidate
               << std::endl;
    }
};

inline Stats mergeStats(std::vector<Stats>& stats) {
    Stats merged;
    for (auto& s : stats) {
        merged.total_count += s.total_count;
        merged.skip_triangle_count += s.skip_triangle_count;
        merged.skip_triangle_large_count += s.skip_triangle_large_count;
        merged.check_subnn_L2_ele_count += s.check_subnn_L2_ele_count;
        merged.check_subnn_IP_ele_count += s.check_subnn_IP_ele_count;
        merged.check_subnn_L2_count += s.check_subnn_L2_count;
        merged.check_subnn_IP_count += s.check_subnn_IP_count;
        merged.skip_subnn_L2_count += s.skip_subnn_L2_count;
        merged.skip_subnn_IP_count += s.skip_subnn_IP_count;
        merged.simi_update_count += s.simi_update_count;
        merged.check_pivot_count += s.check_pivot_count;
        merged.skip_pivot_count += s.skip_pivot_count;
        merged.dis_calls_qx += s.dis_calls_qx;
        merged.dis_calls_qp += s.dis_calls_qp;
        merged.dis_calls_qc += s.dis_calls_qc;
        merged.total_cluster_count += s.total_cluster_count;
        merged.pruned_cluster_count += s.pruned_cluster_count;
    }
    return merged;
}
}  // namespace tribase