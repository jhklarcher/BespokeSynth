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

#include "Arranger.h"
#include "ArrangerSnapshotSelection.h"
#include "CanvasControls.h"
#include "CanvasScrollbar.h"
#include "Checkbox.h"
#include "DropdownList.h"
#include "PatchCableSource.h"
#include "Snapshots.h"
#include "SynthGlobals.h"
#include "TrackOrganizer.h"
#include "Transport.h"

#include <cmath>

SnapshotClipElement::SnapshotClipElement(Canvas* canvas, int col, int row)
: CanvasElement(canvas, col, row, 0, 1)
{
   if (auto* arranger = dynamic_cast<Arranger*>(canvas->GetParent()))
      if (auto* track = arranger->GetTrack(row))
         if (auto* snapshots = track->GetSnapshots())
            mSnapshotIndex = snapshots->GetCurrentSnapshot();

   if (canvas->GetControls() != nullptr)
   {
      mSnapshotSelector = new DropdownList(dynamic_cast<IDropdownListener*>(canvas->GetControls()), "snapshot", 0, 0, &mSnapshotIndex, 170);
      mSnapshotSelector->DrawLabel(true);
      mSnapshotSelector->SetShouldSaveState(false);
      AddElementUIControl(mSnapshotSelector);
   }
}

void SnapshotClipElement::RefreshSnapshotChoices()
{
   if (mSnapshotSelector == nullptr)
      return;

   auto* arranger = dynamic_cast<Arranger*>(mCanvas->GetParent());
   TrackOrganizer* track = arranger ? arranger->GetTrack(mRow) : nullptr;
   Snapshots* snapshots = track ? track->GetSnapshots() : nullptr;
   std::vector<std::pair<int, std::string>> choices;
   bool hasCurrent = false;
   if (snapshots != nullptr)
   {
      for (int index = 0; index < snapshots->GetSize(); ++index)
      {
         if (!snapshots->HasSnapshot(index))
            continue;

         std::string label = snapshots->GetLabel(index);
         const std::string number = ofToString(index);
         choices.emplace_back(index, label.empty() || label == number ? number : number + ": " + label);
         hasCurrent |= index == mSnapshotIndex;
      }
   }

   if (!hasCurrent)
      choices.insert(choices.begin(), { mSnapshotIndex, "missing " + ofToString(mSnapshotIndex) });

   if (choices == mSnapshotChoices)
      return;

   mSnapshotSelector->Clear();
   for (const auto& choice : choices)
      mSnapshotSelector->AddLabel(choice.second, choice.first);
   mSnapshotChoices = std::move(choices);
}

CanvasElement* SnapshotClipElement::CreateDuplicate() const
{
   auto* copy = new SnapshotClipElement(mCanvas, mCol, mRow);
   copy->mOffset = mOffset;
   copy->mLength = mLength;
   copy->mSnapshotIndex = mSnapshotIndex;
   return copy;
}

void SnapshotClipElement::DrawContents(bool clamp, bool wrapped, ofVec2f offset)
{
   ofRectangle rect = GetRect(clamp, wrapped, offset);
   if (rect.width <= 0)
      return;

   auto* arranger = dynamic_cast<Arranger*>(mCanvas->GetParent());
   TrackOrganizer* track = arranger ? arranger->GetTrack(mRow) : nullptr;
   Snapshots* snapshots = track ? track->GetSnapshots() : nullptr;
   ofColor color = track ? track->GetColor() : ofColor(90, 90, 90);
   if (snapshots == nullptr || !snapshots->HasSnapshot(mSnapshotIndex))
      color.setBrightness(color.getBrightness() * .5f);

   ofFill();
   ofSetColor(color);
   ofRect(rect, 2);

   if (rect.width >= 35)
   {
      std::string label = snapshots && snapshots->HasSnapshot(mSnapshotIndex) ? snapshots->GetLabel(mSnapshotIndex) : "missing " + ofToString(mSnapshotIndex);
      while (label.length() > 1 && GetStringWidth(label) > rect.width - 6)
         label.pop_back();
      ofSetColor(255, 255, 255);
      DrawTextNormal(label, rect.x + 3, rect.y + rect.height / 2 + 4, 11);
   }
}

void SnapshotClipElement::SaveState(FileStreamOut& out)
{
   CanvasElement::SaveState(out);
   out << 1;
   out << mSnapshotIndex;
}

void SnapshotClipElement::LoadState(FileStreamIn& in)
{
   CanvasElement::LoadState(in);
   int rev;
   in >> rev;
   LoadStateValidate(rev <= 1);
   in >> mSnapshotIndex;
}

Arranger::Arranger()
: IDrawableModule(620, 365)
{
}

Arranger::~Arranger()
{
   if (mCanvas != nullptr)
      mCanvas->SetListener(nullptr);
}

void Arranger::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   mPlayCheckbox = new Checkbox(this, "play arrangement", 5, 5, &mPlay);
   mLengthEntry = new TextEntry(this, "length", 180, 5, 4, &mNumMeasures, 4, 512);
   mLengthEntry->DrawLabel(true);

   for (int lane = 0; lane < kNumLanes; ++lane)
   {
      mTrackCables[lane] = new PatchCableSource(this, kConnectionType_Special);
      mTrackCables[lane]->AddTypeFilter("trackorganizer");
      mTrackCables[lane]->SetManualPosition(8, kCanvasY + lane * ((mHeight - kBottomMargin) / kNumLanes) + 12);
      AddPatchCableSource(mTrackCables[lane]);
   }

   mCanvas = new Canvas(this, kCanvasX, kCanvasY, mWidth - kCanvasX - kRightMargin, mHeight - kBottomMargin, mNumMeasures, kNumLanes, mNumMeasures, &SnapshotClipElement::Create);
   AddUIControl(mCanvas);
   mCanvas->SetShouldSaveState(false); // Save once, after the module's own state.
   mCanvas->SetNumVisibleRows(kNumLanes);
   mCanvas->SetMajorColumnInterval(4);
   mCanvas->mViewEnd = 8;
   mCanvas->SetListener(this);

   mCanvasControls = new CanvasControls();
   mCanvasControls->SetCanvas(mCanvas);
   mCanvasControls->CreateUIControls();
   mCanvasControls->AllowDragModeSelection(false);
   mCanvasControls->AllowViewRowsEditing(false);
   AddChild(mCanvasControls);

   mCanvasScrollbar = new CanvasScrollbar(mCanvas, "scrollh", CanvasScrollbar::Style::kHorizontal);
   AddUIControl(mCanvasScrollbar);
}

void Arranger::Resize(float width, float height)
{
   mWidth = MAX(width, 430);
   mHeight = MAX(height, 320);
   for (int lane = 0; lane < kNumLanes; ++lane)
   {
      if (mTrackCables[lane] != nullptr)
         mTrackCables[lane]->SetManualPosition(8, kCanvasY + lane * ((mHeight - kBottomMargin) / kNumLanes) + 12);
   }
   if (mCanvas != nullptr)
      mCanvas->SetDimensions(mWidth - kCanvasX - kRightMargin, mHeight - kBottomMargin);
   if (mCanvasControls != nullptr)
      mCanvasControls->Resize(mWidth - kCanvasX - kRightMargin, 92);
}

TrackOrganizer* Arranger::GetTrack(int lane) const
{
   if (lane < 0 || lane >= kNumLanes || mTrackCables[lane] == nullptr)
      return nullptr;
   return dynamic_cast<TrackOrganizer*>(mTrackCables[lane]->GetTarget());
}

int Arranger::ResolveSnapshotAt(int lane, double measure) const
{
   if (mCanvas == nullptr || lane < 0 || lane >= kNumLanes || measure < 0)
      return -1;

   ArrangerSnapshotSelection selection(lane, measure);
   for (auto* element : mCanvas->GetElements())
   {
      auto* clip = dynamic_cast<SnapshotClipElement*>(element);
      if (clip == nullptr)
         continue;

      selection.Consider(clip->mRow, clip->GetStart() * mNumMeasures, clip->GetEnd() * mNumMeasures, clip->GetSnapshotIndex());
   }

   return selection.GetSnapshotIndex();
}

void Arranger::Poll()
{
   if (!mPlay || mCanvas == nullptr)
      return;

   const double measure = TheTransport->GetMeasureTime(gTime);
   for (int lane = 0; lane < kNumLanes; ++lane)
   {
      auto* track = GetTrack(lane);
      auto* snapshots = track ? track->GetSnapshots() : nullptr;
      if (snapshots == nullptr)
         continue;

      const int index = ResolveSnapshotAt(lane, measure);
      if (index >= 0 && snapshots->HasSnapshot(index) && snapshots->GetCurrentSnapshot() != index)
         snapshots->SetSnapshot(index, gTime);
   }
}

void Arranger::DrawModule()
{
   if (Minimized() || !IsVisible())
      return;

   mPlayCheckbox->Draw();
   mLengthEntry->Draw();
   DrawTextNormal("shift-click: add clip   drag: move/resize   delete: remove", 5, 38, 11);

   if (auto* clip = dynamic_cast<SnapshotClipElement*>(mCanvasControls->GetSelectedElement()))
      clip->RefreshSnapshotChoices();

   const float viewStart = mCanvas->mViewStart;
   const float viewEnd = mCanvas->mViewEnd;
   ofPushStyle();
   ofSetColor(150, 150, 150);
   for (int measure = (int)std::ceil(viewStart); measure <= (int)std::floor(viewEnd); ++measure)
   {
      float x = kCanvasX + (measure - viewStart) / (viewEnd - viewStart) * mCanvas->GetWidth();
      ofLine(x, kCanvasY - 16, x, kCanvasY - 2);
      DrawTextNormal(ofToString(measure + 1), x + 2, kCanvasY - 4, 10);
   }
   ofPopStyle();

   for (int lane = 0; lane < kNumLanes; ++lane)
   {
      TrackOrganizer* track = GetTrack(lane);
      ofColor color = track ? track->GetColor() : ofColor(90, 90, 90);
      color.a = 55;
      mCanvas->SetRowColor(lane, color);
      ofSetColor(track ? track->GetColor() : ofColor(150, 150, 150));
      std::string name = track ? track->GetTrackName() : "track " + ofToString(lane + 1);
      while (name.length() > 1 && GetStringWidth(name) > kCanvasX - 25)
         name.pop_back();
      DrawTextNormal(name, 20, kCanvasY + lane * (mCanvas->GetHeight() / kNumLanes) + 16, 11);
   }

   mCanvas->SetCursorPos(TheTransport->GetMeasureTime(gTime) / mNumMeasures);
   mCanvas->Draw();
   mCanvasScrollbar->Draw();
   mCanvasControls->Draw();
}

void Arranger::OnClicked(float x, float y, bool right)
{
   if (!right && x >= kCanvasX && x < kCanvasX + mCanvas->GetWidth() && y >= kCanvasY - 16 && y < kCanvasY)
   {
      double measure = mCanvas->mViewStart + (x - kCanvasX) / mCanvas->GetWidth() * (mCanvas->mViewEnd - mCanvas->mViewStart);
      TheTransport->SetMeasureTime(std::floor(measure));
      return;
   }
   IDrawableModule::OnClicked(x, y, right);
}

void Arranger::UpdateCanvasLength()
{
   mNumMeasures = ofClamp(mNumMeasures, 4, 512);
   if (mCanvas == nullptr)
      return;
   mCanvas->SetLength(mNumMeasures);
   mCanvas->SetNumCols(mNumMeasures);
   float viewLength = mCanvas->mViewEnd - mCanvas->mViewStart;
   viewLength = ofClamp(viewLength, 1, (float)mNumMeasures);
   mCanvas->mViewStart = ofClamp(mCanvas->mViewStart, 0, (float)mNumMeasures - viewLength);
   mCanvas->mViewEnd = mCanvas->mViewStart + viewLength;
   mCanvas->mLoopEnd = mNumMeasures;
}

void Arranger::TextEntryComplete(TextEntry* entry)
{
   if (entry == mLengthEntry)
      UpdateCanvasLength();
}

void Arranger::LoadLayout(const ofxJSONElement& moduleInfo)
{
   SetUpFromSaveData();
}

void Arranger::SetUpFromSaveData()
{
}

void Arranger::SaveLayout(ofxJSONElement& moduleInfo)
{
}

void Arranger::SaveState(FileStreamOut& out)
{
   out << GetModuleSaveStateRev();

   IDrawableModule::SaveState(out);
   mCanvas->SaveState(out);
   out << mCanvas->mViewStart;
   out << mCanvas->mViewEnd;
}

void Arranger::LoadState(FileStreamIn& in, int rev)
{
   mCanvasControls->SetElement(nullptr);
   mHasSerializedDimensions = rev >= 2;
   IDrawableModule::LoadState(in, rev);
   mHasSerializedDimensions = true;
   LoadStateValidate(rev <= GetModuleSaveStateRev());
   mCanvas->LoadState(in);
   in >> mCanvas->mViewStart;
   in >> mCanvas->mViewEnd;
   UpdateCanvasLength();
}
