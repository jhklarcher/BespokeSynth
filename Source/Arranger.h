/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2026 Bespoke Synth contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#pragma once

#include "Canvas.h"
#include "IDrawableModule.h"
#include "TextEntry.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

class CanvasControls;
class CanvasScrollbar;
class Checkbox;
class DropdownList;
class PatchCableSource;
class TrackOrganizer;

class SnapshotClipElement : public CanvasElement
{
public:
   SnapshotClipElement(Canvas* canvas, int col, int row);
   static CanvasElement* Create(Canvas* canvas, int col, int row) { return new SnapshotClipElement(canvas, col, row); }
   CanvasElement* CreateDuplicate() const override;

   int GetSnapshotIndex() const { return mSnapshotIndex; }
   void RefreshSnapshotChoices();
   void SaveState(FileStreamOut& out) override;
   void LoadState(FileStreamIn& in) override;

private:
   void DrawContents(bool clamp, bool wrapped, ofVec2f offset) override;

   int mSnapshotIndex{ 0 };
   DropdownList* mSnapshotSelector{ nullptr };
   std::vector<std::pair<int, std::string>> mSnapshotChoices;
};

class Arranger : public IDrawableModule, public ICanvasListener, public ITextEntryListener
{
public:
   Arranger();
   ~Arranger() override;
   static IDrawableModule* Create() { return new Arranger(); }
   static bool AcceptsAudio() { return false; }
   static bool AcceptsNotes() { return false; }
   static bool AcceptsPulses() { return false; }

   void CreateUIControls() override;
   bool IsResizable() const override { return mHasSerializedDimensions; }
   void Resize(float width, float height) override;
   void Poll() override;
   TrackOrganizer* GetTrack(int lane) const;
   int ResolveSnapshotAt(int lane, double measure) const;

   void CanvasUpdated(Canvas* canvas) override {}
   void TextEntryComplete(TextEntry* entry) override;

   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;
   void SaveLayout(ofxJSONElement& moduleInfo) override;
   void SaveState(FileStreamOut& out) override;
   void LoadState(FileStreamIn& in, int rev) override;
   int GetModuleSaveStateRev() const override { return 2; }

   bool IsEnabled() const override { return true; }

private:
   void DrawModule() override;
   void OnClicked(float x, float y, bool right) override;
   void UpdateCanvasLength();

   static constexpr int kNumLanes = 8;
   static constexpr int kCanvasX = 110;
   static constexpr int kCanvasY = 60;
   static constexpr int kRightMargin = 10;
   static constexpr int kBottomMargin = 173;

   std::array<PatchCableSource*, kNumLanes> mTrackCables{};
   Canvas* mCanvas{ nullptr };
   CanvasControls* mCanvasControls{ nullptr };
   CanvasScrollbar* mCanvasScrollbar{ nullptr };
   TextEntry* mLengthEntry{ nullptr };
   Checkbox* mPlayCheckbox{ nullptr };
   int mNumMeasures{ 64 };
   bool mPlay{ true };
   bool mHasSerializedDimensions{ true };
};
