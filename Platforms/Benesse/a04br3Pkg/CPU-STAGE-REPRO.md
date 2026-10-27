# CPU stage firmware reproduction

The reviewed P53 source is commit `1be79ad794d83d4ebe16cd98cca0fdbbe04cf891`.
For the matched comparison, use `DEBUG`, `A04BR3_TEST_CYCLE=1`,
`A04BR3_CHARGER_OWNER=1`, and `A04BR3_CPU_STAGE_DIAG=0` or `1`.
Use the same toolchain and `SOURCE_DATE_EPOCH` for both. Diagnostic output is
non-atomic register state, not a CPU frequency measurement. Hardware read safety
and watchdog recovery must be verified separately.

The source of Mu_Basecore is the submodule commit
`3c9c7b2e211e62d2a954ad760a36fe05adc43aa3` from
`https://github.com/microsoft/mu_basecore.git`. `build_uefi.py` applies the
following files from this repository, in this order, before compiling:

1. `Resources/MuPatches/AsmMacroLib.patch`
2. `Resources/MuPatches/Auth-Service.patch`
3. `Resources/MuPatches/Boot-Manager.patch`
4. `Resources/MuPatches/Timer.patch`
5. `Resources/MuPatches/Usb-Bus.patch`

For independent verification, in a clean checkout of the pinned Mu_Basecore,
apply those five tracked files with `git apply --index` in the order above;
`git write-tree` must print `a1418283bf7daed69def4df3b6e741b91f22c242`.
Run this check in a disposable checkout, not on an existing working tree. The
normal official build applies the files itself; do not preapply them there.
No separately supplied patch is required. Pin Common/Mu at
`b8d46c71610318e36a661819e1e3c0b8bdda413a` and Common/Mu_OEM_Sample at
`b70b4b7bb02c43bf9db2172dfb3ed0457e2361d6`.

The matched build used UniXNU `cc4297af2f0494178a35aa97d0faec60c5ae9f89`,
DarwinBoot `b9063ad22903288df989c3449621b7eb97de7dd2`, LLVM 18, and
`python build_uefi.py -d a04br3 -r DEBUG -c`, setting `UNIXNU_ROOT` to that
UniXNU checkout. This build path and Mu_Basecore pin differ from the earlier
P52 firmware build; compare the matched OFF/ON pair to isolate this option.
