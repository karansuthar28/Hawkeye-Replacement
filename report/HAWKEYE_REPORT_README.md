# Report generation

Do **not** submit invented numbers. After the real ChampSim experiment logs exist, run:

```bash
python3 scripts/parse_results.py
python3 scripts/make_plots.py
STUDENT_NAME="Your Name" STUDENT_ID="Your ID" python3 scripts/build_report.py
```

The final file is:

`report/HPCA_Assignment_01_Hawkeye_Report.pdf`

The report automatically inserts the measured miss rates, the two required plots, and three implementation/experiment learnings.
