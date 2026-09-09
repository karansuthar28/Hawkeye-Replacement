# HPCA Assignment 01 — Hawkeye in ChampSim

This package is designed to be copied into an **already working ChampSim checkout**. It implements the three independently gradable components required by the assignment and a thin ChampSim replacement-policy adapter, plus scripts for the complete experiment/report pipeline.

## 1. What is implemented

```text
replacement/hawkeye/
  optgen.h / optgen.cc          Standalone OPTgen occupancy-vector model
  predictor.h / predictor.cc    Standalone 8K-entry, 3-bit PC predictor
  rrip.h / rrip.cc              Standalone Hawkeye/RRIP update + victim logic
  hawkeye.h / hawkeye.cc        ChampSim-facing replacement adapter

hawkeye_tests/
  optgen_test.cc
  predictor_test.cc
  rrip_test.cc

configs/
  lru_4way.json, hawkeye_4way.json
  lru_8way.json, hawkeye_8way.json
  lru_16way.json, hawkeye_16way.json

scripts/
  component_tests.sh
  verify_no_debug_prints.sh
  download_required_traces.sh
  smoke_hawkeye.sh
  run_experiments.sh
  parse_results.py
  make_plots.py
  build_report.py
  package_submission.sh
```

The algorithm components contain no ChampSim types. The adapter owns them and delegates OPT labeling, predictor training/prediction, and RRPV operations rather than reimplementing their logic.

## 2. Install into your existing ChampSim checkout

Suppose this package is extracted at `~/Downloads/hpca01_hawkeye_solution` and your working ChampSim checkout is `~/ChampSim`:

```bash
cd ~/Downloads/hpca01_hawkeye_solution
bash install_into_champsim.sh ~/ChampSim
cd ~/ChampSim
```

No baseline files are overwritten except files with the same Hawkeye/config/script names added by this package.

## 3. Run the standalone component checks first

```bash
bash scripts/component_tests.sh
bash scripts/verify_no_debug_prints.sh
```

Expected end messages:

```text
ALL COMPONENT SELF-TESTS PASSED
All three required grading skeletons compile cleanly.
PASS: no debug print statements in replacement/hawkeye
```

The grading drivers themselves remain in the assignment's required form. Their vectors are intentionally empty because the grader substitutes held-out vectors.

## 4. Make sure the three experiment traces exist

If you have not already downloaded them:

```bash
bash scripts/download_required_traces.sh
```

This obtains:

```text
traces/456.hmmer-191B.champsimtrace.xz
traces/429.mcf-22B.champsimtrace.xz
traces/473.astar-42B.champsimtrace.xz
```

Do **not** decompress the `.xz` files.

## 5. Fast integration smoke test

Before spending time on the long experiments, verify that Hawkeye configures, builds, and runs:

```bash
bash scripts/smoke_hawkeye.sh traces/456.hmmer-191B.champsimtrace.xz
```

This uses only 1M warmup + 2M simulation instructions. If this succeeds, the adapter is integrated into your actual ChampSim checkout.

## 6. Install the plotting/report dependency

```bash
python3 -m pip install -r requirements-hawkeye.txt
```

`matplotlib` is the only extra Python dependency used by the supplied plotting/report scripts.

## 7. Run every required experiment automatically

```bash
STUDENT_NAME="Your Name" STUDENT_ID="Your ID" bash scripts/run_experiments.sh
```

Every measured run uses the assignment's required:

```text
--warmup_instructions 20000000
--simulation_instructions 50000000
```

The script performs the required LRU/Hawkeye measurements for:

- 456.hmmer-191B at 4-way / 8192 sets
- 456.hmmer-191B at 8-way / 4096 sets
- 456.hmmer-191B at 16-way / 2048 sets
- 429.mcf-22B at the default 16-way / 2048 sets
- 473.astar-42B at the default 16-way / 2048 sets

The 16-way hmmer measurements are reused between Plot 1 and Plot 2, so there are **10 unique simulations**. The assignment prose says "8 runs" for Plot 1, but its own table specifies three associativities (4, 8, 16), which mathematically requires six Plot-1 runs (three configurations × two policies).

If a run stops halfway, rerun the same command. Existing non-empty logs are skipped. Set `FORCE=1` to rerun everything:

```bash
FORCE=1 STUDENT_NAME="Your Name" STUDENT_ID="Your ID" bash scripts/run_experiments.sh
```

## 8. Files generated after the real runs

```text
results/results.csv
results/reduction.csv
results/plot1_llc_miss_rate_vs_associativity.png
results/plot1_llc_miss_rate_vs_associativity.pdf
results/plot2_miss_rate_reduction_over_lru.png
results/plot2_miss_rate_reduction_over_lru.pdf
report/HPCA_Assignment_01_Hawkeye_Report.pdf
```

The second plot uses exactly:

```text
(LRU miss rate - Hawkeye miss rate) / LRU miss rate * 100
```

The PDF report automatically includes the two measured plots and three implementation/experiment observations. It never substitutes invented experiment numbers.

## 9. Make the code submission zip

After all checks pass:

```bash
bash scripts/package_submission.sh
```

This creates:

```text
HPCA01_Hawkeye_Submission_Code.zip
```

Submit that code archive (or your GitHub repository) together with:

```text
report/HPCA_Assignment_01_Hawkeye_Report.pdf
```

## 10. Implementation map for understanding it later

The full dataflow is:

```text
LLC access (address, PC)
        |
        v
   OPTgen.access()
        |
   past OPT label
        |
        v
 predictor.train(previous PC)
        |
 current PC -> predictor.predict()
        |
 friendly / averse
        |
        v
   update_rrpv()
        |
        v
    find_victim()
```

- **OPTgen** asks whether the previous-to-current reuse interval could have remained resident under an ideal past OPT reconstruction.
- **Predictor** learns whether a PC tends to generate OPT-friendly or OPT-averse accesses.
- **RRIP state** translates that prediction into eviction priority: friendly = protected near RRPV 0, averse = immediate candidate at RRPV 7.
- **hawkeye.cc** is deliberately thin: it connects ChampSim's callbacks to the three components.

## 11. Important submission rule

Do not add debugging output to `replacement/hawkeye/`. The assignment's grader compares test stdout exactly. Run this immediately before submission:

```bash
bash scripts/verify_no_debug_prints.sh
```
