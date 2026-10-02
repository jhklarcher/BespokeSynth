#include "ArrangerClipTiming.h"

#include <cmath>
#include <iostream>

namespace
{
   bool Expect(double actual, double expected, const char* description)
   {
      if (std::abs(actual - expected) < 0.00001)
         return true;
      std::cerr << description << ": expected " << expected << ", got " << actual << '\n';
      return false;
   }
}

int main()
{
   using namespace ArrangerClipTiming;
   bool ok = true;
   ok &= Expect(BarFromMeasure(0), 1, "first ruler bar");
   ok &= Expect(MeasureFromBar(37), 36, "bar 37 uses zero-based measure 36");
   ok &= Expect(MeasureFromNormalized(NormalizedFromMeasure(36.25, 64), 64), 36.25, "normalized position roundtrip");
   ok &= Expect(SnapToBar(5.49), 5, "snap down");
   ok &= Expect(SnapToBar(5.5), 6, "snap up");
   ok &= Expect(ClampMoveStart(-2, 4, 64), 0, "move before start");
   ok &= Expect(ClampMoveStart(63, 4, 64), 60, "move past end");
   ok &= Expect(ClampResizeStart(10, 8, 1), 7, "left resize minimum duration");
   ok &= Expect(ClampResizeEnd(3, 4, 64, 1), 5, "right resize minimum duration");
   ok &= Expect(ClampResizeEnd(70, 62, 64, 1), 64, "right resize arrangement bound");
   ok &= Expect(ClampLength(20, 60, 64, 1.0 / 16), 4, "numeric length arrangement bound");
   ok &= Expect(ClampLength(0, 60, 64, 1.0 / 16), 1.0 / 16, "numeric length positive minimum");
   return ok ? 0 : 1;
}
