#pragma once

class ISample;

/// Cửa sổ "Sandbox" dùng chung cho mọi entry point: FPS, chọn sample, UI riêng
/// của sample. Trả về vị trí sample được chọn trong SAMPLES.
int drawSandboxUi(int currentSampleIndex, ISample &sample);