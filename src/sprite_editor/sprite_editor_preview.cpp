#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

namespace {
    // Screen-space pixel radius used for corner and edge hit detection.
    constexpr float kHandleRadius = 6.0f;
    // Minimum frame dimension in image pixels enforced during resize.
    constexpr int kMinFrameDim = 1;

    // The interaction mode for an active frame drag.
    enum class FrameDragOp {
        None,
        Move,
        ResizeTop, ResizeBottom, ResizeLeft, ResizeRight,
        ResizeTopLeft, ResizeTopRight, ResizeBottomLeft, ResizeBottomRight,
    };

    // Map a mouse screen position to a drag operation for the given frame rect.
    // Returns FrameDragOp::None when the mouse is not on or near the rect.
    FrameDragOp HitTestFrame(ImVec2 mouse, ImVec2 imagePos, float zoom,
                             moth::gfx::IntRect const& rect) {
        float const x0 = imagePos.x + (static_cast<float>(rect.left())   * zoom);
        float const y0 = imagePos.y + (static_cast<float>(rect.top())    * zoom);
        float const x1 = imagePos.x + (static_cast<float>(rect.right())  * zoom);
        float const y1 = imagePos.y + (static_cast<float>(rect.bottom()) * zoom);

        constexpr float kH = kHandleRadius;
        bool const nearLeft   = std::abs(mouse.x - x0) <= kH;
        bool const nearRight  = std::abs(mouse.x - x1) <= kH;
        bool const nearTop    = std::abs(mouse.y - y0) <= kH;
        bool const nearBottom = std::abs(mouse.y - y1) <= kH;
        bool const inXSpan    = mouse.x >= x0 - kH && mouse.x <= x1 + kH;
        bool const inYSpan    = mouse.y >= y0 - kH && mouse.y <= y1 + kH;
        bool const inInterior = mouse.x > x0 + kH && mouse.x < x1 - kH &&
                                mouse.y > y0 + kH && mouse.y < y1 - kH;

        // Corners take priority over edges; edges take priority over interior.
        if (nearLeft  && nearTop)    { return FrameDragOp::ResizeTopLeft;     }
        if (nearRight && nearTop)    { return FrameDragOp::ResizeTopRight;    }
        if (nearLeft  && nearBottom) { return FrameDragOp::ResizeBottomLeft;  }
        if (nearRight && nearBottom) { return FrameDragOp::ResizeBottomRight; }
        if (nearTop    && inXSpan)   { return FrameDragOp::ResizeTop;         }
        if (nearBottom && inXSpan)   { return FrameDragOp::ResizeBottom;      }
        if (nearLeft   && inYSpan)   { return FrameDragOp::ResizeLeft;        }
        if (nearRight  && inYSpan)   { return FrameDragOp::ResizeRight;       }
        if (inInterior)              { return FrameDragOp::Move;              }
        return FrameDragOp::None;
    }

    // Return the ImGui cursor shape that communicates the given operation to the user.
    ImGuiMouseCursor CursorForOp(FrameDragOp op) {
        switch (op) {
        case FrameDragOp::Move:
            return ImGuiMouseCursor_ResizeAll;
        case FrameDragOp::ResizeTop:
        case FrameDragOp::ResizeBottom:
            return ImGuiMouseCursor_ResizeNS;
        case FrameDragOp::ResizeLeft:
        case FrameDragOp::ResizeRight:
            return ImGuiMouseCursor_ResizeEW;
        case FrameDragOp::ResizeTopLeft:
        case FrameDragOp::ResizeBottomRight:
            return ImGuiMouseCursor_ResizeNWSE;
        case FrameDragOp::ResizeTopRight:
        case FrameDragOp::ResizeBottomLeft:
            return ImGuiMouseCursor_ResizeNESW;
        default:
            return ImGuiMouseCursor_Arrow;
        }
    }

    // Apply (dx, dy) image-space pixel delta to rect according to op.
    // Enforces minimum frame size and clamps to the atlas dimensions.
    void ApplyFrameDelta(moth::gfx::IntRect& rect, FrameDragOp op,
                         int dx, int dy, int imgW, int imgH) {
        switch (op) {
        case FrameDragOp::Move: {
            int const w = rect.w();
            int const h = rect.h();
            rect.topLeft.x     = std::clamp(rect.topLeft.x + dx, 0, imgW - w);
            rect.topLeft.y     = std::clamp(rect.topLeft.y + dy, 0, imgH - h);
            rect.bottomRight.x = rect.topLeft.x + w;
            rect.bottomRight.y = rect.topLeft.y + h;
            break;
        }
        case FrameDragOp::ResizeTop:
            rect.topLeft.y = std::clamp(rect.topLeft.y + dy,
                                        0, rect.bottomRight.y - kMinFrameDim);
            break;
        case FrameDragOp::ResizeBottom:
            rect.bottomRight.y = std::clamp(rect.bottomRight.y + dy,
                                            rect.topLeft.y + kMinFrameDim, imgH);
            break;
        case FrameDragOp::ResizeLeft:
            rect.topLeft.x = std::clamp(rect.topLeft.x + dx,
                                        0, rect.bottomRight.x - kMinFrameDim);
            break;
        case FrameDragOp::ResizeRight:
            rect.bottomRight.x = std::clamp(rect.bottomRight.x + dx,
                                            rect.topLeft.x + kMinFrameDim, imgW);
            break;
        case FrameDragOp::ResizeTopLeft:
            rect.topLeft.x = std::clamp(rect.topLeft.x + dx,
                                        0, rect.bottomRight.x - kMinFrameDim);
            rect.topLeft.y = std::clamp(rect.topLeft.y + dy,
                                        0, rect.bottomRight.y - kMinFrameDim);
            break;
        case FrameDragOp::ResizeTopRight:
            rect.bottomRight.x = std::clamp(rect.bottomRight.x + dx,
                                            rect.topLeft.x + kMinFrameDim, imgW);
            rect.topLeft.y     = std::clamp(rect.topLeft.y     + dy,
                                            0, rect.bottomRight.y - kMinFrameDim);
            break;
        case FrameDragOp::ResizeBottomLeft:
            rect.topLeft.x     = std::clamp(rect.topLeft.x     + dx,
                                            0, rect.bottomRight.x - kMinFrameDim);
            rect.bottomRight.y = std::clamp(rect.bottomRight.y + dy,
                                            rect.topLeft.y + kMinFrameDim, imgH);
            break;
        case FrameDragOp::ResizeBottomRight:
            rect.bottomRight.x = std::clamp(rect.bottomRight.x + dx,
                                            rect.topLeft.x + kMinFrameDim, imgW);
            rect.bottomRight.y = std::clamp(rect.bottomRight.y + dy,
                                            rect.topLeft.y + kMinFrameDim, imgH);
            break;
        default:
            break;
        }
    }
} // namespace

void SpriteEditor::DrawPreview() {
    if (!m_spriteSheet) {
        ImGui::TextDisabled("No sprite sheet loaded.");
        return;
    }

    auto const& image = m_spriteSheet->GetImage();
    if (!image) {
        ImGui::TextDisabled("Use File > Import Sheet to add a sheet image.");
        return;
    }

    float const imgW = static_cast<float>(image.GetWidth());
    float const imgH = static_cast<float>(image.GetHeight());

    // Auto-fit on first draw after a load
    ImVec2 const totalAvail = ImGui::GetContentRegionAvail();
    auto const fitZoom = [&]() {
        float const fitH = totalAvail.y - ImGui::GetFrameHeightWithSpacing();
        if (imgW > 0.0f && imgH > 0.0f && totalAvail.x > 0.0f && fitH > 0.0f) {
            m_zoom = std::min(totalAvail.x / imgW, fitH / imgH);
        }
    };
    if (m_zoom < 0.0f) {
        fitZoom();
        // If avail was zero fitZoom() couldn't compute a value — keep the sentinel
        // so it retries on the next frame instead of falling back to 1:1.
    }

    // Toolbar. New Cell comes first so the zoom text changing width does not move it.
    ImGui::BeginDisabled(m_newCellMode);
    if (ImGui::Button("New Cell")) {
        m_newCellMode = true;
        m_newCellAnchor.reset();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        fitZoom();
    }
    ImGui::SameLine();
    if (ImGui::Button("1:1")) {
        m_zoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::Text("%.0f%%", m_zoom * 100.0f);
    if (m_newCellMode) {
        ImGui::SameLine();
        ImGui::TextDisabled("Drag on the sheet to create a cell (Esc to cancel)");
    }

    // Scrollable viewport
    ImVec2 const childSize = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##preview_scroll", childSize, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Mouse-wheel zoom centered on cursor
    if (ImGui::IsWindowHovered()) {
        float const wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 const mouse   = ImGui::GetMousePos();
            ImVec2 const winPos  = ImGui::GetWindowPos();
            float const mouseRelX = (mouse.x - winPos.x) + ImGui::GetScrollX();
            float const mouseRelY = (mouse.y - winPos.y) + ImGui::GetScrollY();
            float const imgSpaceX = mouseRelX / m_zoom;
            float const imgSpaceY = mouseRelY / m_zoom;

            float const factor = (wheel > 0.0f) ? 1.1f : (1.0f / 1.1f);
            m_zoom = std::clamp(m_zoom * factor, 0.05f, 32.0f);

            ImGui::SetScrollX((imgSpaceX * m_zoom) - (mouse.x - winPos.x));
            ImGui::SetScrollY((imgSpaceY * m_zoom) - (mouse.y - winPos.y));
        }
    }

    float const displayW = imgW * m_zoom;
    float const displayH = imgH * m_zoom;

    // Draw the atlas image, then overlay an InvisibleButton at the same position so
    // ImGui owns all left-button interactions (drag, cursor changes) on the canvas.
    ImVec2 const imagePos = ImGui::GetCursorScreenPos();
    DrawImage(image, { static_cast<int>(displayW), static_cast<int>(displayH) });
    ImGui::SetCursorScreenPos(imagePos);
    ImGui::InvisibleButton("##canvas_interact", ImVec2{ displayW, displayH });
    bool const canvasHovered = ImGui::IsItemHovered();

    int const imgWi = static_cast<int>(imgW);
    int const imgHi = static_cast<int>(imgH);
    ImVec2 const mouse = ImGui::GetMousePos();

    // New Cell mode: drag out a rect in image space; releasing adds it as a frame.
    // Edges snap to the nearest pixel boundary and are clamped to the atlas.
    auto const mouseToImage = [&]() {
        return moth::gfx::IntVec2{
            std::clamp(static_cast<int>(std::round((mouse.x - imagePos.x) / m_zoom)), 0, imgWi),
            std::clamp(static_cast<int>(std::round((mouse.y - imagePos.y) / m_zoom)), 0, imgHi) };
    };
    auto const newCellRect = [&]() {
        moth::gfx::IntVec2 const a = *m_newCellAnchor;
        moth::gfx::IntVec2 const b = mouseToImage();
        return moth::gfx::MakeRect(std::min(a.x, b.x), std::min(a.y, b.y),
                                   std::abs(b.x - a.x), std::abs(b.y - a.y));
    };
    if (m_newCellMode) {
        if (ImGui::IsItemActivated()) {
            m_newCellAnchor = mouseToImage();
        }
        if (ImGui::IsItemDeactivated() && m_newCellAnchor.has_value()) {
            moth::gfx::IntRect const rect = newCellRect();
            m_newCellAnchor.reset();
            // A plain click makes an empty rect; stay in the mode so the user can drag again.
            if (rect.w() >= kMinFrameDim && rect.h() >= kMinFrameDim) {
                AppendFrames({ rect });
                m_newCellMode = false;
            }
        }
    }

    // Show a drag/resize cursor whenever the mouse is over the selected frame border.
    bool const selectedInRange = (m_selectedFrame >= 0 &&
                                  m_selectedFrame < static_cast<int>(m_frames.size()));
    if (selectedInRange && !m_newCellMode && ImGui::IsItemHovered()) {
        FrameDragOp const hoverOp = HitTestFrame(mouse, imagePos, m_zoom,
                                                  m_frames[m_selectedFrame].rect);
        if (hoverOp != FrameDragOp::None) {
            ImGui::SetMouseCursor(CursorForOp(hoverOp));
        }
    }

    if (ImGui::IsItemActivated() && !m_newCellMode) {
        // Priority 1: start a drag/resize on the already-selected frame if the mouse
        // is on its border or interior.
        FrameDragOp startOp = FrameDragOp::None;
        if (selectedInRange) {
            startOp = HitTestFrame(mouse, imagePos, m_zoom,
                                   m_frames[m_selectedFrame].rect);
        }

        if (startOp != FrameDragOp::None) {
            m_frameDrag = { static_cast<int>(startOp), m_frames };
        } else {
            // Priority 2: click on any other frame to select it and begin a move.
            int hit = -1;
            float const relX = (mouse.x - imagePos.x) / m_zoom;
            float const relY = (mouse.y - imagePos.y) / m_zoom;
            for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
                auto const& fr = m_frames[i];
                if (relX >= static_cast<float>(fr.rect.left())   &&
                    relX <  static_cast<float>(fr.rect.right())   &&
                    relY >= static_cast<float>(fr.rect.top())     &&
                    relY <  static_cast<float>(fr.rect.bottom())) {
                    hit = i;
                    break;
                }
            }
            m_selectedFrame = hit;
            if (hit >= 0) {
                m_frameDrag = { static_cast<int>(FrameDragOp::Move), m_frames };
            }
        }
    }

    if (ImGui::IsItemActive() && m_frameDrag.has_value()) {
        // Apply the total drag delta from the button's activation point to the
        // snapshot rect every frame — avoids accumulating rounding error.
        ImVec2 const totalDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        int const dx = static_cast<int>(std::round(totalDelta.x / m_zoom));
        int const dy = static_cast<int>(std::round(totalDelta.y / m_zoom));
        if (m_selectedFrame >= 0 &&
            m_selectedFrame < static_cast<int>(m_frameDrag->snapshot.size())) {
            moth::gfx::IntRect r = m_frameDrag->snapshot[m_selectedFrame].rect;
            ApplyFrameDelta(r, static_cast<FrameDragOp>(m_frameDrag->op), dx, dy, imgWi, imgHi);
            m_frames[m_selectedFrame].rect = r;
        }
    }

    if (ImGui::IsItemDeactivated() && m_frameDrag.has_value()) {
        // Push an undo action only if the rect actually moved.
        bool changed = false;
        if (m_selectedFrame >= 0 &&
            m_selectedFrame < static_cast<int>(m_frames.size()) &&
            m_selectedFrame < static_cast<int>(m_frameDrag->snapshot.size())) {
            changed = (m_frames[m_selectedFrame].rect !=
                       m_frameDrag->snapshot[m_selectedFrame].rect);
        }
        if (changed) {
            PushFrameAction(std::move(m_frameDrag->snapshot),
                            m_selectedFrame, m_selectedFrame);
        }
        m_frameDrag.reset();
    }

    // -----------------------------------------------------------------------
    // Frame rect overlays and handle squares — drawn on the child's draw list
    // so they clip to the scroll viewport.
    // -----------------------------------------------------------------------
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    auto const& cfg = m_config;
    auto const toU32 = [](moth::gfx::Color const& c) {
        return ImGui::ColorConvertFloat4ToU32(
            ImVec4{ c.data[0], c.data[1], c.data[2], c.data[3] });
    };
    ImU32 const normalU32   = toU32(cfg.SpriteEditorNormalColor);
    ImU32 const selectedU32 = toU32(cfg.SpriteEditorSelectedColor);

    for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
        auto const& fr = m_frames[i];
        bool const selected = (i == m_selectedFrame);
        ImU32 const color     = selected ? selectedU32 : normalU32;
        float const thickness = static_cast<float>(cfg.SpriteEditorRectThickness);

        float const rx0 = imagePos.x + (static_cast<float>(fr.rect.left())   * m_zoom);
        float const ry0 = imagePos.y + (static_cast<float>(fr.rect.top())    * m_zoom);
        float const rx1 = imagePos.x + (static_cast<float>(fr.rect.right())  * m_zoom);
        float const ry1 = imagePos.y + (static_cast<float>(fr.rect.bottom()) * m_zoom);

        // AddRect centers its stroke on the boundary; offset by half-thickness
        // outward so the inner edge of the line exactly borders the frame.
        float const half = thickness * 0.5f;
        drawList->AddRect({ rx0 - half, ry0 - half }, { rx1 + half, ry1 + half },
                          color, 0.0f, 0, thickness);

        // Pivot cross — pivot is relative to frame top-left, fixed 5px arm length
        float const px = imagePos.x + (static_cast<float>(fr.rect.left() + fr.pivot.x) * m_zoom);
        float const py = imagePos.y + (static_cast<float>(fr.rect.top()  + fr.pivot.y) * m_zoom);
        constexpr float kArm = 5.0f;
        drawList->AddLine({ px - kArm, py }, { px + kArm, py }, color, 1.5f);
        drawList->AddLine({ px, py - kArm }, { px, py + kArm }, color, 1.5f);
    }

    // In-progress New Cell rect.
    if (m_newCellMode && m_newCellAnchor.has_value()) {
        moth::gfx::IntRect const r = newCellRect();
        float const thickness = static_cast<float>(cfg.SpriteEditorRectThickness);
        float const half = thickness * 0.5f;
        drawList->AddRect({ imagePos.x + (static_cast<float>(r.left())   * m_zoom) - half,
                            imagePos.y + (static_cast<float>(r.top())    * m_zoom) - half },
                          { imagePos.x + (static_cast<float>(r.right())  * m_zoom) + half,
                            imagePos.y + (static_cast<float>(r.bottom()) * m_zoom) + half },
                          selectedU32, 0.0f, 0, thickness);
    }

    // ImGui has no crosshair cursor shape, so hide the OS cursor and draw one.
    // The foreground list keeps it visible when a drag leaves the viewport.
    if (m_newCellMode && (canvasHovered || m_newCellAnchor.has_value())) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        ImDrawList* const fg = ImGui::GetForegroundDrawList();
        constexpr float kCrossArm = 10.0f;
        ImVec2 const p{ std::floor(mouse.x) + 0.5f, std::floor(mouse.y) + 0.5f };
        // Dark outline under a light line so the cross reads on any sheet colors.
        for (auto const& [col, width] : { std::pair{ IM_COL32(0, 0, 0, 255), 3.0f },
                                          std::pair{ IM_COL32(255, 255, 255, 255), 1.0f } }) {
            fg->AddLine({ p.x - kCrossArm, p.y }, { p.x + kCrossArm + 1.0f, p.y }, col, width);
            fg->AddLine({ p.x, p.y - kCrossArm }, { p.x, p.y + kCrossArm + 1.0f }, col, width);
        }
    }

    ImGui::EndChild();
}
