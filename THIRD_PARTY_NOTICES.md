# Third-Party Notices

SoftLoaf Trichrome depends on third-party open-source software. This file is a
summary for packagers and contributors; the dependency licenses themselves are
authoritative.

## Qt

The desktop application uses Qt 6 modules including Core, Gui, Qml, Quick,
Quick Controls, Quick Dialogs, and Widgets.

Qt is available under commercial and open-source licenses. Builds of this
project are intended to use the open-source LGPL terms unless a distributor has
a separate Qt commercial license. Distributors must comply with the applicable
Qt license obligations, including dynamic-linking and replacement requirements
where required by the LGPL.

## LibRaw

RAW file decoding uses LibRaw. LibRaw is distributed under dual licensing terms
including LGPL and CDDL options. Distributors must comply with the selected
LibRaw license.

## OpenCV

Image processing uses OpenCV. OpenCV is distributed under the Apache License
2.0.

## Little CMS

ICC profile generation uses Little CMS. Little CMS is distributed under the MIT
License.

## C++ Standard Library And Platform SDKs

Binary packages may also include or link against platform runtime components,
compiler runtimes, and operating-system SDK libraries. Those components are
governed by their respective vendor terms.

## RAW Test Data

Public RAW compatibility fixtures should not be committed to this repository.
Use manifests, checksums, and local caches instead. The suggested local cache is:

```text
/Users/flemyng/Desktop/Film/raw.pixls.us/
```
