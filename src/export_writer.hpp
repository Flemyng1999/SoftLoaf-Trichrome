#pragma once

#include <QString>

#include "softloaf_trichrome/image_buf.hpp"

namespace softloaf::trichrome::desktop {

// Single export boundary: color encoding, integer quantization, ICC tagging,
// and TIFF container I/O all live here (as in SoftLoaf Negative).
bool WriteExport(const ImageBuf& linear_srgb,
                 const QString& output_color_space,
                 int bit_depth,
                 const QString& path,
                 QString* reason);

}  // namespace softloaf::trichrome::desktop
