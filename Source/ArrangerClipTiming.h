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

#include <algorithm>
#include <cmath>

// Canvas stores fractions of the arrangement; the ruler and timing entries use
// one-based bars. One Canvas column is one measure in the Arranger.
namespace ArrangerClipTiming
{
   inline double MeasureFromBar(double bar) { return bar - 1; }
   inline double BarFromMeasure(double measure) { return measure + 1; }
   inline double MeasureFromNormalized(double position, int measures) { return position * measures; }
   inline double NormalizedFromMeasure(double measure, int measures) { return measure / measures; }
   inline double SnapToBar(double measure) { return std::round(measure); }
   inline double ClampMoveStart(double proposed, double length, double total) { return std::clamp(proposed, 0.0, std::max(0.0, total - length)); }
   inline double ClampResizeStart(double proposed, double end, double minimumLength) { return std::clamp(proposed, 0.0, std::max(0.0, end - minimumLength)); }
   inline double ClampResizeEnd(double proposed, double start, double total, double minimumLength) { return std::clamp(proposed, std::min(total, start + minimumLength), total); }
   inline double ClampLength(double proposed, double start, double total, double minimumLength) { return std::clamp(proposed, minimumLength, std::max(minimumLength, total - start)); }
}
