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
// the order in which clips were crossed during playback.
class ArrangerSnapshotSelection
{
public:
   ArrangerSnapshotSelection(int lane, double measure)
   : mLane(lane)
   , mMeasure(measure)
   {
   }

   void Consider(int lane, double start, double end, int snapshotIndex)
   {
      if (mMeasure < 0 || lane != mLane || end <= start || start > mMeasure)
         return;

      if (start >= mPreviousStart)
      {
         mPreviousStart = start;
         mPreviousIndex = snapshotIndex;
      }
      if (mMeasure < end && start >= mActiveStart)
      {
         mActiveStart = start;
         mActiveIndex = snapshotIndex;
         mHasActive = true;
      }
   }

   int GetSnapshotIndex() const { return mHasActive ? mActiveIndex : mPreviousIndex; }

private:
   int mLane;
   double mMeasure;
   double mActiveStart{ -std::numeric_limits<double>::infinity() };
   double mPreviousStart{ -std::numeric_limits<double>::infinity() };
   int mActiveIndex{ -1 };
   int mPreviousIndex{ -1 };
   bool mHasActive{ false };
};
