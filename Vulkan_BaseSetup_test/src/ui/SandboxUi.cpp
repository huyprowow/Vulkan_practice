#include "SandboxUi.hpp"
#include "../samples/Samples.hpp"

#include <imgui.h>
#include <iterator>

int drawSandboxUi(int currentSampleIndex, ISample &sample) {
  int selected = currentSampleIndex;
  
  // ghim góc dưới phải; pivot (1,1) nghĩa là neo bằng góc dưới phải của cửa sổ
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  const float margin = ImGui::GetStyle().WindowPadding.x;
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin,
             viewport->WorkPos.y + viewport->WorkSize.y - margin),
      ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::Begin("Sandbox", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
  ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate,
              1000.0f / ImGui::GetIO().Framerate);
  if (ImGui::BeginCombo("Sample", SAMPLES[currentSampleIndex].name)) {
    for (int i = 0; i < static_cast<int>(std::size(SAMPLES)); ++i) {
      if (ImGui::Selectable(SAMPLES[i].name, i == currentSampleIndex)) {
        selected = i;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::Separator();
  sample.ui();
  ImGui::End();
  return selected;
}