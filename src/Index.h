#ifndef INDEX_H
#define INDEX_H

#include <memory>
#include "Clustering.h"
#include "IVF.h"
#include "IVFScan.hpp"
#include "common.h"

namespace tribase {

class Index {
   public:
    Index(size_t d = 0,
          size_t nlist = 0,
          size_t nprobe = 0,
          MetricType metric = MetricType::METRIC_L2,
          OptLevel opt_level = OptLevel::OPT_NONE,
          size_t sub_k = 0,
          size_t sub_nlist = 1,
          size_t sub_nprobe = 1,
          bool verbose = false,
          EdgeDevice edge_device_enabled = EdgeDevice::EDGEDEVIVE_DISABLED,
          size_t pivot_m = 0,
          PivotMethod pivot_method = PivotMethod::PIVOT_FPS,
          float pivot_ratio = 1.0f,
          size_t pivot_subset_size = 0,
          float pivot_candidate_ratio = 4.0f,
          size_t pivot_candidate_cap = 0);

    Index& operator=(Index&& other) noexcept;

    void train(size_t n, const float* codes, bool faiss = false, bool lite = false);

    void single_thread_nearest_cluster_search(size_t n, const float* queries, float* distances, idx_t* labels);
    void single_thread_search(size_t n, const float* queries, size_t k, float* distances, idx_t* labels, float ratio, Stats* stats);

    void add(size_t n, const float* codes);

    Stats search(size_t n, const float* queries, size_t k, float* distances, idx_t* labels, float ratio = 1.0);
    void save_index(std::string path) const;
    void load_index(std::string path);
    void load_SPANN(std::string path);

    // 其他查询方法的声明

   private:
    std::unique_ptr<IVFScanBase> get_scanner(MetricType metric, OptLevel opt_level, size_t k, EdgeDevice edge_device_enabled = EdgeDevice::EDGEDEVIVE_DISABLED);

   public:
    size_t d;
    size_t nlist;
    size_t nprobe;
    MetricType metric;
    OptLevel opt_level;
    OptLevel added_opt_level;

    size_t sub_k;
    size_t sub_nlist;
    size_t sub_nprobe;

    // pivot config
    size_t pivot_m = 0;            // 0 表示使用 sub_k 或由 pivot_ratio 推导
    PivotMethod pivot_method = PivotMethod::PIVOT_FPS;
    float pivot_ratio = 1.0f;      // 当 pivot_m==0 时，可用 ratio*sub_k 推导
    size_t pivot_subset_size = 0;  // 0 表示使用 list_size 全量
    float pivot_candidate_ratio = 4.0f; // 预筛候选列数 = ceil(ratio * pivot_m)
    size_t pivot_candidate_cap = 0;     // 0 表示不限制

    // PCA params
    float pca_radius_alpha = 20.0f;     // R = alpha * sqrt(mean(||x-c||^2))
    bool pca_both_signs = true;         // 生成 ±R·u_i

    // Cluster-level pruning (L2) switch & params
    bool cluster_prune = false;
    float cluster_prune_beta = 0.98f;

    // MVOA cross-cluster candidate settings
    PivotScope pivot_scope = PivotScope::PIVOT_SCOPE_INTRA; // intra|inter|hybrid
    PivotIntraMethod pivot_intra_method = PivotIntraMethod::INTRA_FFT; // fft|fps for intra candidates
    size_t pivot_cross_k = 8;                 // number of farthest clusters to draw from
    size_t pivot_cross_per_cluster = 8;       // samples per external cluster

    bool verbose;
    EdgeDevice edge_device_enabled;

    std::unique_ptr<IVF[]> lists;
    std::unique_ptr<float[]> centroid_codes;
    std::unique_ptr<idx_t[]> centroid_ids;
};

}  // namespace tribase

#endif  // INDEX_H