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
#include "ArrangerClipTiming.h"
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

#include <algorithm>
#include <cmath>

SnapshotClipElement::SnapshotClipElement(Canvas* canvas, int col, int row)
: CanvasElement(canvas, col, row, 0, 1)
{
   if (auto* arranger = dynamic_cast<Arranger*>(canvas->GetParent()))
      if (auto* track = arranger->GetTrack(row))
         if (auto* snapshots = track->GetSnapshots())
         {
            mSnapshotIndex = snapshots->GetCurrentSnapshot();
            if (!snapshots->HasSnapshot(mSnapshotIndex))
               for (int index = 0; index < snapshots->GetSize(); ++index)
                  if (snapshots->HasSnapshot(index))
                  {
                     mSnapshotIndex = index;
                     break;
                  }
         }

   if (canvas->GetControls() != nullptr)
   {
      mSnapshotSelector = new DropdownList(dynamic_cast<IDropdownListener*>(canvas->GetControls()), "snapshot", 0, 0, &mSnapshotIndex, 170);
      mSnapshotSelector->DrawLabel(true);
      mSnapshotSelector->SetShouldSaveState(false);
      AddElementUIControl(mSnapshotSelector);

      auto* textListener = dynamic_cast<ITextEntryListener*>(canvas->GetControls());
      mStartEntry = new TextEntry(textListener, "start bar", 0, 0, 7, &mStartBar, 1.0f, 512.0f);
      mLengthEntry = new TextEntry(textListener, "length", 0, 0, 7, &mLengthMeasures, 1.0f / 16, 512.0f);
      for (auto* entry : { mStartEntry, mLengthEntry })
      {
         entry->DrawLabel(true);
         entry->SetShouldSaveState(false);
         AddElementUIControl(entry);
      }
   }
}

void SnapshotClipElement::RefreshTimingControls()
{
   if (mStartEntry != nullptr && IKeyboardFocusListener::GetActiveKeyboardFocus() != mStartEntry)
      mStartBar = ArrangerClipTiming::BarFromMeasure(ArrangerClipTiming::MeasureFromNormalized(GetStart(), mCanvas->GetNumCols()));
   if (mLengthEntry != nullptr && IKeyboardFocusListener::GetActiveKeyboardFocus() != mLengthEntry)
      mLengthMeasures = ArrangerClipTiming::MeasureFromNormalized(GetEnd() - GetStart(), mCanvas->GetNumCols());
}

void SnapshotClipElement::TextEntryComplete(TextEntry* entry)
{
   const int measures = mCanvas->GetNumCols();
   const double start = ArrangerClipTiming::MeasureFromNormalized(GetStart(), measures);
   const double length = ArrangerClipTiming::MeasureFromNormalized(GetEnd() - GetStart(), measures);
   if (entry == mStartEntry)
   {
      const double newStart = ArrangerClipTiming::ClampMoveStart(ArrangerClipTiming::MeasureFromBar(mStartBar), length, measures);
      SetStart(ArrangerClipTiming::NormalizedFromMeasure(newStart, measures), true);
   }
   else if (entry == mLengthEntry)
   {
      const double newLength = ArrangerClipTiming::ClampLength(mLengthMeasures, start, measures, 1.0 / 16);
      SetEnd(ArrangerClipTiming::NormalizedFromMeasure(start + newLength, measures));
   }
   RefreshTimingControls();
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

   // Mark only the shared time region. Every overlapping clip draws its own
   // marking, so the top Canvas element does not hide the overlap cue.
   for (auto* element : mCanvas->GetElements())
   {
      auto* other = dynamic_cast<SnapshotClipElement*>(element);
      if (other == nullptr || other == this || other->mRow != mRow)
         continue;
      const float overlapStart = std::max(GetStart(), other->GetStart());
      const float overlapEnd = std::min(GetEnd(), other->GetEnd());
      if (overlapEnd <= overlapStart)
         continue;
      const float viewStart = mCanvas->mViewStart / mCanvas->GetLength();
      const float viewEnd = mCanvas->mViewEnd / mCanvas->GetLength();
      const float x0 = ofMap(overlapStart, viewStart, viewEnd, 0, mCanvas->GetWidth(), true) + offset.x;
      const float x1 = ofMap(overlapEnd, viewStart, viewEnd, 0, mCanvas->GetWidth(), true) + offset.x;
      if (x1 <= x0)
         continue;
      ofSetColor(0, 0, 0, 55);
      ofRect(x0, rect.y + 1, x1 - x0, rect.height - 2);
      ofSetColor(255, 255, 255, 95);
      for (float x = x0 + 4; x < x1; x += 8)
         ofLine(x, rect.y + 2, std::min(x + 4, x1), rect.y + 6);
   }

   if (rect.width >= 35)
   {
      std::string label = snapshots && snapshots->HasSnapshot(mSnapshotIndex) ? snapshots->GetLabel(mSnapshotIndex) : "missing " + ofToString(mSnapshotIndex);
      while (label.length() > 1 && GetStringWidth(label) > rect.width - 6)
         label.pop_back();
      ofSetColor(255, 255, 255);
      DrawTextNormal(label, rect.x + 3, rect.y + rect.height / 2 + 4, 11);
   }

   if (GetHighlighted())
   {
      const float handleWidth = std::min(7.0f, rect.width / 3);
      ofSetColor(25, 25, 25, 150);
      ofRect(rect.x, rect.y + 2, handleWidth, rect.height - 4);
      ofRect(rect.x + rect.width - handleWidth, rect.y + 2, handleWidth, rect.height - 4);
      ofSetColor(255, 225, 125);
      ofLine(rect.x + handleWidth / 2, rect.y + 4, rect.x + handleWidth / 2, rect.y + rect.height - 4);
      ofLine(rect.x + rect.width - handleWidth / 2, rect.y + 4, rect.x + rect.width - handleWidth / 2, rect.y + rect.height - 4);
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
      const float rowHeight = (mHeight - kBottomMargin) / kNumLanes;
      mAddClipButtons[lane] = new ClickButton(this, ("add clip " + ofToString(lane + 1)).c_str(), 88, kCanvasY + lane * rowHeight + (rowHeight - 16) / 2, ButtonDisplayStyle::kPlus);
      mAddClipButtons[lane]->SetDimensions(16, 16);
      mAddClipButtons[lane]->SetCableTargetable(false);
   }

   mCanvas = new Canvas(this, kCanvasX, kCanvasY, mWidth - kCanvasX - kRightMargin, mHeight - kBottomMargin, mNumMeasures, kNumLanes, mNumMeasures, &SnapshotClipElement::Create);
   AddUIControl(mCanvas);
   mCanvas->SetShouldSaveState(false); // Save once, after the module's own state.
   mCanvas->SetNumVisibleRows(kNumLanes);
   mCanvas->SetMajorColumnInterval(4);
   mCanvas->SetBoundedBarEditing(true);
   mCanvas->SetResizeHitWidth(9);
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
      if (mAddClipButtons[lane] != nullptr)
      {
         const float rowHeight = (mHeight - kBottomMargin) / kNumLanes;
         mAddClipButtons[lane]->SetPosition(88, kCanvasY + lane * rowHeight + (rowHeight - 16) / 2);
      }
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

bool Arranger::CanCreateClip(int lane) const
{
   auto* track = GetTrack(lane);
   auto* snapshots = track ? track->GetSnapshots() : nullptr;
   if (snapshots == nullptr)
      return false;
   for (int index = 0; index < snapshots->GetSize(); ++index)
      if (snapshots->HasSnapshot(index))
         return true;
   return false;
}

void Arranger::ButtonClicked(ClickButton* button, double time)
{
   for (int lane = 0; lane < kNumLanes; ++lane)
   {
      if (button != mAddClipButtons[lane])
         continue;
      if (!CanCreateClip(lane))
         return;

      const double playhead = TheTransport->GetMeasureTime(gTime);
      const int col = ofClamp((int)ArrangerClipTiming::SnapToBar(playhead), 0, mNumMeasures - 1);
      auto* clip = mCanvas->CreateElement(col, lane);
      mCanvas->AddElement(clip);
      mCanvas->SelectElement(clip);
      if (col < mCanvas->mViewStart || col + 1 > mCanvas->mViewEnd)
      {
         const float viewLength = mCanvas->mViewEnd - mCanvas->mViewStart;
         mCanvas->mViewStart = ofClamp(col - viewLength / 2, 0, mNumMeasures - viewLength);
         mCanvas->mViewEnd = mCanvas->mViewStart + viewLength;
      }
      return;
   }
}

ArrangerSnapshotSelection Arranger::ResolveSelectionAt(int lane, double measure) const
{
   ArrangerSnapshotSelection selection(lane, measure);
   if (mCanvas == nullptr || lane < 0 || lane >= kNumLanes || measure < 0)
      return selection;

   auto* track = GetTrack(lane);
   auto* snapshots = track ? track->GetSnapshots() : nullptr;
   if (snapshots == nullptr)
      return selection;

   const auto& elements = mCanvas->GetElements();
   for (int order = 0; order < elements.size(); ++order)
   {
      auto* clip = dynamic_cast<SnapshotClipElement*>(elements[order]);
      if (clip == nullptr || clip->mRow != lane)
         continue;

      selection.Consider(clip->mRow, clip->GetStart() * mNumMeasures, clip->GetEnd() * mNumMeasures, clip->GetSnapshotIndex(), snapshots->HasSnapshot(clip->GetSnapshotIndex()), order);
   }

   return selection;
}

int Arranger::ResolveSnapshotAt(int lane, double measure) const
{
   return ResolveSelectionAt(lane, measure).GetSnapshotIndex();
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
   DrawTextNormal("+: add clip   shift-click: add   drag: move/resize   alt: fine", 5, 38, 11);

   if (auto* clip = dynamic_cast<SnapshotClipElement*>(mCanvasControls->GetSelectedElement()))
   {
      clip->RefreshSnapshotChoices();
      clip->RefreshTimingControls();
   }

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
      const bool canCreate = CanCreateClip(lane);
      mAddClipButtons[lane]->SetShowing(canCreate);
      ofColor color = track ? track->GetColor() : ofColor(90, 90, 90);
      color.a = 55;
      mCanvas->SetRowColor(lane, color);
      ofSetColor(track ? track->GetColor() : ofColor(150, 150, 150));
      std::string name = track ? track->GetTrackName() : "connect track";
      if (track != nullptr && !canCreate)
         name = "add snapshot";
      while (name.length() > 1 && GetStringWidth(name) > 68)
         name.pop_back();
      DrawTextNormal(name, 20, kCanvasY + lane * (mCanvas->GetHeight() / kNumLanes) + 16, 11);
      if (canCreate)
         mAddClipButtons[lane]->Draw();
   }

   mCanvas->SetCursorPos(TheTransport->GetMeasureTime(gTime) / mNumMeasures);
   mCanvas->Draw();

   if (mPlay)
   {
      const double measure = TheTransport->GetMeasureTime(gTime);
      ofPushStyle();
      ofNoFill();
      ofSetLineWidth(2);
      ofSetColor(110, 245, 220);
      const auto& elements = mCanvas->GetElements();
      for (int lane = 0; lane < kNumLanes; ++lane)
      {
         const int order = ResolveSelectionAt(lane, measure).GetActiveClipOrder();
         if (order < 0 || order >= elements.size())
            continue; // A held gap state has no active clip to outline.
         const ofRectangle rect = elements[order]->GetRect(true, false);
         if (rect.width > 2 && rect.height > 2)
            ofRect(kCanvasX + rect.x + 1, kCanvasY + rect.y + 1, rect.width - 2, rect.height - 2);
      }
      ofPopStyle();
   }
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
   for (auto* element : mCanvas->GetElements())
   {
      const double originalStart = ArrangerClipTiming::MeasureFromNormalized(element->GetStart(), mNumMeasures);
      const double originalLength = ArrangerClipTiming::MeasureFromNormalized(element->GetEnd() - element->GetStart(), mNumMeasures);
      const double length = ArrangerClipTiming::ClampLength(originalLength, 0, mNumMeasures, 1.0 / 16);
      const double start = ArrangerClipTiming::ClampMoveStart(originalStart, length, mNumMeasures);
      element->SetStart(ArrangerClipTiming::NormalizedFromMeasure(start, mNumMeasures), true);
      element->SetEnd(ArrangerClipTiming::NormalizedFromMeasure(start + length, mNumMeasures));
   }
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
