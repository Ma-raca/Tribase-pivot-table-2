<#
参数说明与可选项:
- BenchmarksPath: 数据集根目录，默认 ./benchmarks
- Dataset: 数据集名，如 sift10k/sift1M/nuswide/msong/iris/ccnews/yahooaq
- OptLevels: 优化级别（可多值，空格分隔）。可选:
  OPT_NONE OPT_TRIANGLE OPT_SUBNN_L2 OPT_SUBNN_IP OPT_PIVOT OPT_TRI_SUBNN_L2 OPT_TRI_SUBNN_IP OPT_SUBNN_ONLY OPT_ALL
- Nprobes: 探测簇数（可多值）。0 表示全簇扫描（自动替换为 nlist）。
- K: 返回近邻数，默认 100
- Subk: 子索引候选近邻列表长度（建议与 pivot_m 对齐）
- Metric: 距离度量，可选 l2 | ip
- Nlist: 聚类数（建库期参数，0=自动 sqrt(nb)）
- Cache: 复用已有索引文件
- TrainOnly: 只建索引不搜索
- Verbose: 详细日志
- Loop: 搜索重复次数（取均值）

Pivot Table:
- PivotM: 每簇 pivot 个数（0 时用 PivotRatio*Subk 推导）
- PivotMethod: 选点方法，可选 fps | fft | random | kmeans | pca | var_ortho（别名: var_orthr/mvoa）
- PivotRatio: 当 PivotM=0 时，PivotM≈round(Subk*PivotRatio)
- PivotSubsetSize: 构造 pivot 空间的行数（0=全量）
- PivotCandidateRatio: 候选列数≈ceil(ratio*PivotM)
- PivotCandidateCap: 候选列上限（0=不封顶）

PCA pivots:
- PivotPcaRadiusAlpha: 半径系数 α，R=α*sqrt(mean(||x-c||^2))
- PivotPcaBothSigns: 是否使用 ± 两个方向

MVOA 扩充（仅当 PivotMethod ∈ {var_ortho,var_orthr,mvoa} 生效）:
- PivotScope: 候选范围，可选 intra | inter | hybrid（别名: internal/self, external/cross, mixed）
- PivotIntraMethod: 本簇采样，可选 fft | fps
- PivotCrossK: 选择最远的 K 个外簇作为候选来源
- PivotCrossPerCluster: 每个外簇随机抽样的候选个数

簇级剪枝（L2）:
- ClusterPrune: 开启簇级剪枝
- ClusterPruneBeta: 保守系数 β（阈值乘以 β）

输出:
- Csv: 结果 CSV 路径（未指定则自动生成包含 dataset/opt/pivot/subk 的文件名）

示例:
  .\script\run_query.ps1 -Build -Dataset sift1M -OptLevels "OPT_PIVOT" -Nprobes @(50,100,300,1000) -Subk 50 -PivotM 50 -PivotMethod var_orthr -PivotScope hybrid -PivotIntraMethod fft -PivotCrossK 8 -PivotCrossPerCluster 8 -ClusterPrune -Verbose -Csv "benchmarks/sift1M/result/log_pivot.csv"
  .\script\run_query.ps1 -Dataset sift1M -OptLevels "OPT_SUBNN_L2" -Nprobes @(50,100,300,1000) -Subk 50 -Verbose -Csv "benchmarks/sift1M/result/log_subnnL2.csv"
#>
Param(
    [string]$BenchmarksPath = "./benchmarks",
    [string]$Dataset = "sift1M",
    [string]$OptLevels = "OPT_PIVOT",           # 多值空格分隔; 可选: OPT_NONE OPT_TRIANGLE OPT_SUBNN_L2 OPT_SUBNN_IP OPT_PIVOT OPT_TRI_SUBNN_L2 OPT_TRI_SUBNN_IP OPT_SUBNN_ONLY OPT_ALL
    [int[]]$Nprobes = @(0),                       # 多值数组; 如 50,100,300,1000；0=全簇扫描（自动替换为 nlist）
    [int]$K = 100,
    [int]$Subk = 50,
    [string]$Metric = "l2",                      # 可选: l2 | ip
    [int]$Nlist = 0,                              # 0 则程序内部用 sqrt(nb)
    [switch]$Cache,                               # 复用已有索引
    [switch]$TrainOnly,                           # 仅建索引
    [switch]$Verbose,
    [int]$Loop = 1,

    # Pivot Table
    [int]$PivotM = 50,
    [string]$PivotMethod = "var_orthr",         # 可选: fps | fft | random | kmeans | pca | var_ortho(别名: var_orthr/mvoa)
    [double]$PivotRatio = 1.0,
    [int]$PivotSubsetSize = 0,
    [double]$PivotCandidateRatio = 4.0,
    [int]$PivotCandidateCap = 0,

    # PCA pivots
    [double]$PivotPcaRadiusAlpha = 20.0,
    [switch]$PivotPcaBothSigns,

    # MVOA 扩充
    [string]$PivotScope = "hybrid",             # 可选: intra | inter | hybrid（别名: internal/self, external/cross, mixed）
    [string]$PivotIntraMethod = "fft",          # 可选: fft | fps
    [int]$PivotCrossK = 8,
    [int]$PivotCrossPerCluster = 8,

    # 簇级剪枝 (L2)
    [switch]$ClusterPrune,
    [double]$ClusterPruneBeta = 0.98,

    # 输出
    [string]$Csv = "",

    # 是否先编译
    [switch]$Build
)

$ErrorActionPreference = "Stop"

function Join-Args {
    param([string[]]$Args)
    return ($Args | Where-Object { $_ -ne $null -and $_.Trim().Length -gt 0 }) -join ' '
}

if ($Build) {
    Write-Host "[build] building in docker..." -ForegroundColor Cyan
    $buildCmd = "sed -i 's/\r\$//' script/*.sh || true; cmake -B build . && cmake --build build -j | cat"
    docker run --rm -v "${PWD}:/app/tribase" -w /app/tribase tribase bash -lc $buildCmd
}

$nprobeStr = ($Nprobes | ForEach-Object { "$_" }) -join ' '

if (-not $Csv -or $Csv.Trim().Length -eq 0) {
    $safeOpt = $OptLevels -replace '\s+', '_' -replace '[^a-zA-Z0-9_\-]', ''
    $Csv = "benchmarks/$Dataset/result/log_${Dataset}_${safeOpt}_pm${PivotM}_${PivotMethod}_subk${Subk}.csv"
}

$argsList = @()
$argsList += "--benchmarks_path $BenchmarksPath"
$argsList += "--dataset $Dataset"
if ($Nlist -gt 0) { $argsList += "--nlist $Nlist" }
$argsList += "--metric $Metric"
$argsList += "--k $K"
$argsList += "--subk $Subk"
$argsList += "--opt_levels $OptLevels"
$argsList += "--nprobes $nprobeStr"
$argsList += "--loop $Loop"
$argsList += "--csv $Csv"
if ($Cache) { $argsList += "--cache" }
if ($TrainOnly) { $argsList += "--train_only" }
if ($Verbose) { $argsList += "--verbose" }

# pivot common
$argsList += "--pivot_m $PivotM"
$argsList += "--pivot_method $PivotMethod"
$argsList += "--pivot_ratio $PivotRatio"
if ($PivotSubsetSize -gt 0) { $argsList += "--pivot_subset_size $PivotSubsetSize" }
$argsList += "--pivot_candidate_ratio $PivotCandidateRatio"
if ($PivotCandidateCap -gt 0) { $argsList += "--pivot_candidate_cap $PivotCandidateCap" }

# pca
$argsList += "--pivot_pca_radius_alpha $PivotPcaRadiusAlpha"
if ($PivotPcaBothSigns) { $argsList += "--pivot_pca_both_signs" }

# mvoa
$argsList += "--pivot_scope $PivotScope"
$argsList += "--pivot_intra_method $PivotIntraMethod"
$argsList += "--pivot_cross_k $PivotCrossK"
$argsList += "--pivot_cross_per_cluster $PivotCrossPerCluster"

# cluster prune
if ($ClusterPrune) { $argsList += "--cluster_prune" }
$argsList += "--cluster_prune_beta $ClusterPruneBeta"

$argsJoined = Join-Args -Args $argsList

$queryCmd = "./build/bin/query $argsJoined | cat"

Write-Host "[run] dataset=$Dataset opt=[$OptLevels] nprobes=[$nprobeStr] subk=$Subk pivot=($PivotMethod,$PivotM,$PivotScope) -> $Csv" -ForegroundColor Green
docker run --rm -v "${PWD}:/app/tribase" -w /app/tribase tribase bash -lc $queryCmd


