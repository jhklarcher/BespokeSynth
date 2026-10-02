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

#include <limits>

// Resolve the desired snapshot from absolute transport position, independently of
// the order in which clips were crossed during playback. Clips occupy [start, end).
// While clips overlap, the latest start wins (then the later Canvas element).
// At an end boundary an underlying clip resumes; after all clips end, hold the
// snapshot that was active immediately before the gap. Unavailable clips are inert.
class ArrangerSnapshotSelection
{
public:
   ArrangerSnapshotSelection(int lane, double measure)
   : mLane(lane)
   , mMeasure(measure)
   {
   }

   void Consider(int lane, double start, double end, int snapshotIndex, bool available, int clipOrder = -1)
   {
      if (mMeasure < 0 || lane != mLane || end <= start || start > mMeasure || !available)
         return;

      if (end <= mMeasure && (end > mPreviousEnd || (end == mPreviousEnd && start >= mPreviousStart)))
      {
         mPreviousEnd = end;
         mPreviousStart = start;
         mPreviousIndex = snapshotIndex;
      }
      if (mMeasure < end && start >= mActiveStart)
      {
         mActiveStart = start;
         mActiveIndex = snapshotIndex;
         mActiveClipOrder = clipOrder;
         mHasActive = true;
      }
   }

   int GetSnapshotIndex() const { return mHasActive ? mActiveIndex : mPreviousIndex; }
   int GetActiveClipOrder() const { return mHasActive ? mActiveClipOrder : -1; }

private:
   int mLane;
   double mMeasure;
   double mActiveStart{ -std::numeric_limits<double>::infinity() };
   double mPreviousEnd{ -std::numeric_limits<double>::infinity() };
   double mPreviousStart{ -std::numeric_limits<double>::infinity() };
   int mActiveIndex{ -1 };
   int mActiveClipOrder{ -1 };
   int mPreviousIndex{ -1 };
   bool mHasActive{ false };
};
