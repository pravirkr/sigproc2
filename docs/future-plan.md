# SIGPROC C++23 rewrite — future search stack

> **DO NOT IMPLEMENT FROM THIS DOCUMENT.**
>
> This is a structured future RFC, **not** an agent-orchestration spec. Coding agents must **not** open PRs for tools listed here until a follow-up fills numeric oracles (output format, worked example, phase convention, `.prd` columns, tree recurrence, birdie file, harmonic normalisation).
>
> Staffable work lives in **`docs/implementation-plan.md`** (Phase 0–4 filterbank manipulation + FBH5 I/O).

| Field | Value |
|---|---|
| **Title** | Later search / astronomy stack (dedisperse, tree, fold, seek, RFI, filmerge, TEMPO) |
| **Date** | 2026-09-11 |
| **Status** | Draft — **not staffable** |
| **Companion** | `docs/implementation-plan.md` |
| **Behaviour source** | [FRBs/sigproc](https://github.com/FRBs/sigproc) @ `8f3a9a6632f15a146dd9e058a9a1ec807d1f992d` |
| **Constraint file** | `Agents.md` |

---

## Scope

**In a future staffing RFC (not now):**

| Tool | Mapped binary | Notes |
|---|---|---|
| `dedisperse` | `sig_dedisperse` | Incoherent DM / subbands → `.tim` |
| `tree` | `sig_tree` | Taylor tree; extra vs Agents.md Next |
| `fold` | `sig_fold` | Profiles; ASCII + `data_type=3` |
| `seek` | `sig_seek` | Periodicity + single-pulse |
| `rfi_analyse` algorithm | `sig_rfi` | **No PGPLOT** |
| `filmerge` | `sig_filmerge` | Time merge (splice already does freq) |
| `barycentre` / `depolyco` | later | TEMPO, cwd side effects |
| `FilterbankBlock` / `FourierSeries` / `FoldedCube` | types | **Only when a CLI needs ownership** |

**Out forever (this repo iteration):** Python, PGPLOT UIs, historical converters, PSRFITS (separate later format RFC).

---

## Why deferred

- Fortran `seek` / `sumhrm` / `dosearch` is large and poorly specified without a `.prd` column oracle.
- Fold phase convention (bin 0 = left edge vs centre) is not written down in one place.
- Tree recurrence vs DM index vs pc cm⁻³ needs a worked numeric example.
- `rfi_analyse` is glued to PGPLOT; the algorithm must be extracted without the UI.
- TEMPO writes files in cwd (`tssb.par`, `polyco.bar`) — product policy, not a weekend PR.
- `get_dm_delays("top")` currently throws in `sigproc2`; fix it **with** the dedisp RFC so the reference frequency is tested.

---

## Architecture sketch (when staffed)

Reuse Phase 0–4 I/O: gulp + skipback + `std::span<float>` kernels. **Do not** add `FilterbankBlock` speculatively. Introduce it only if a CLI needs owning 2-D storage that `span` cannot express (same bar as `TimeSeries` in the implementation plan).

```
.fil → FilterbankReader → float gulp
     → delay table (OQ-5 constant)
     → shift-and-sum / tree / fold / r2c FFT
     → TimeSeries / ASCII / data_type=3
```

OpenMP on time loops. FFTW already linked; keep FFTW out of public headers. No Python.

---

## Per-tool notes

### `sig_dedisperse`

- **Sources:** [dedisperse.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/dedisperse.c), [dmdelay.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/dmdelay.c), [dmshift.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/dmshift.c), [dedisperse_header.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/dedisp/dedisperse_header.c), [dedisperse_data.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/dedisp/dedisperse_data.c)
- **Known:** `data_type=2` time series; `-d dm`, `-b nbands`, `-i ignore`, `-headerless`, `-nobaseline`, `-subzero`. Layout sample-major.
- **Required oracles before staffing:**
  - **OQ-5 DM constant:** (a) L&K `4.1488080e3` (current `kDispConst`) (b) original `4148.741601` (c) `--dmconst`. **Still open.** Do not silently change `sig_fake` (K28 in the implementation plan).
  - Worked example: two-channel chirp, expected peak sample after DM.
  - `ref_freq` `"top"` vs `"ch1"` vs `"center"`; fix `get_dm_delays("top")`.
  - Ignore-channel file 1-based vs 0-based.
  - Output header keys (`refdm`, `nchans=1` or `nbands`).

### `sig_tree`

- **Sources:** [tree.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/tree.c) (if present at SHA; else `applications` tree in the mixed tree is not truth — confirm on GitHub).
- **Known:** `-s -r -l -u -noflip`; UniqueID output stem.
- **Required oracles:** recurrence (Taylor tree), DM index vs pc cm⁻³, power-of-two `nchans` constraint, output file naming.

### `sig_fold`

- **Sources:** [fold.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/fold.c), [fold_data.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/fold_data.c), [fold_header.c](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/fold_header.c)
- **Known:** `-p` period ms, `-n` nbins, ASCII default, `data_type=3` binary. Skip `-psrfits`, `-epn`, PGPLOT.
- **Required oracles:** phase convention (bin 0 left edge vs centre); acceleration `-a`; polyco later (implementation plan already deferred polyco).
- **Type:** `FoldedCube` only in the PR that first needs nsub×nchan×nbin ownership.

### `sig_seek`

- **Sources:** [seek.f](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/seek/seek.f), [seekin.f](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/seek/seekin.f), [sumhrm.f](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/seek/sumhrm.f), [dosearch.f](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/seek/dosearch.f), [singlepulse.f](https://raw.githubusercontent.com/FRBs/sigproc/8f3a9a6632f15a146dd9e058a9a1ec807d1f992d/src/seek/singlepulse.f)
- **Known:** `.tim`/`.ser`/`.dis`/`.fft`; `-pulse`, `-fftw` (this rewrite **only** has FFTW), harmonic folds 1,2,4,8,16.
- **Required oracles:** `.prd` column order; pad-to-2^n vs `-nopow2`; birdie/mask file syntax; harmonic-sum normalisation; reconstructed S/N (`-recon`) in or out.
- **Type:** `FourierSeries` in the FFT PR, not earlier.

### `sig_rfi`

- **Sources:** `rfi_analyse` is PGPLOT + FFTW. Algorithm only.
- **Required oracles:** per-channel statistic, σ threshold, mask ASCII format, whether to zero or replace with noise.
- **Non-goal:** `cpgplot.h`, interactive windows, `wisdom.txt`.

### `sig_filmerge`

- Time-domain merge of overlapping `.fil`. Frequency concat is already `sig_splice`. Needs alignment rules and a numeric example.

### TEMPO / barycentre

- Original shells out to `tempo`, writes `tssb.par` / `polyco.bar` in cwd. Future RFC must decide: parse polyco without TEMPO vs optional external binary. Not a Phase 0–4 concern.

---

## Sketched future PRs (placeholders only)

Do **not** treat these IDs as staffable:

1. Delay table + `get_dm_delays("top")` + OQ-5 decision
2. Dedisp shift-and-sum kernel + `sig_dedisperse` MVP (filterbank → `.tim`)
3. Tree kernel + `sig_tree`
4. `FoldedCube` + fold kernel + `sig_fold`
5. `FourierSeries` + FFTW RAII + harmonic sum
6. `sig_seek` periodicity
7. Single-pulse path
8. RFI stats + `sig_rfi`
9. `sig_filmerge` (optional)

Each needs Catch2 oracles **before** coding starts.

---

## Open question (future RFC)

**OQ-5 — DM constant for `sig_dedisperse`:** (a) L&K `4.1488080e3` (b) original `4148.741601` (c) `--dmconst`. Fake pulse physics in the implementation plan **already** uses `fake.c` (`8.3e3` smear, `4148.741601` delays) and must not be “fixed” to L&K without a new decision.

---

## Non-goals

Python; PGPLOT; historical converters; PSRFITS; implementing any of the above from this text.

---

## References

- `docs/implementation-plan.md`
- FRBs/sigproc @ `8f3a9a6632f15a146dd9e058a9a1ec807d1f992d` (paths above)
- `Agents.md`
