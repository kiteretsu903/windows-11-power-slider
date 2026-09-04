# Security design

Windows 11 Power Slider deliberately keeps a small privilege and dependency surface.

## Trust boundary

- The process runs as the interactive user and never requests elevation.
- The installer is per-user and does not add a certificate to any trust store.
- The app has no network client and does not launch external commands.
- Persistent settings are language choice, one-time startup initialization marker and HKCU Run entry.
- First normal launch enables startup as requested; later launches preserve a disabled startup choice.

## Native APIs

- `powrprof.dll` is loaded with `LOAD_LIBRARY_SEARCH_SYSTEM32`.
- AC/DC power mode access uses dynamically resolved Windows power APIs.
- Acrylic uses dynamically resolved `SetWindowCompositionAttribute` with the acrylic accent policy. This is an undocumented Windows interface; it is gated to Windows 11, honors transparency/high-contrast settings and falls back to solid rendering on failure.
- Transparent content is presented through DirectComposition and a premultiplied-alpha DXGI swap chain.
- Keep Awake uses `SetThreadExecutionState`; no power request survives process termination.
- Keep Awake releases its system/display requirements on battery at or below `max(5%, configured critical level + 2 percentage points)` (capped at 100%), or on a critical battery flag. It also fails safe when power telemetry is unavailable without confirmed AC. The critical level is read only; a failed policy read uses a 5% release threshold. Checks run on power notifications and the existing 1.5-second timer, including when hidden or dragging. Sleep/resume clears requests without re-enabling them. A failed release is retried without showing a false Off state.

## Known limitations

- The battery safeguard releases this app's requests; it does not force sleep, change critical-battery actions, enable hibernation or clear other apps' requests. Windows/firmware remain responsible for emergency action. Abrupt battery failure, missing telemetry, a stalled UI thread or a disabled system critical action cannot be made safe by this app. Never wait for physical exhaustion to save work.
- The executable is unsigned until a publisher signs it with a CA-trusted code-signing certificate. An unsigned portable build may show Microsoft Defender SmartScreen.
- The accent interface is not a stable public API contract; compatibility must be checked on future Windows updates.

Please report security issues privately to the repository owner before public disclosure.
