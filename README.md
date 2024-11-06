# µStreamer-Visca

µStreamer-Visca is a fork of [µStreamer](https://github.com/pikvm/ustreamer) that add support for Sony VISCA over UDP to control V4L2 devices remotly.

I plan to use this modified tool with the [Obsbot Tiny 4k](https://www.obsbot.com/fr/obsbot-tiny-4k-webcam) and have only tested it with this camera but it should work with others (you will see errors if the v4l2 controls are not supported by your camera)

-----
# Installation

See https://github.com/pikvm/ustreamer?tab=readme-ov-file#installation

-----
# Visca

Protocol has been tested successfuly with vMix.

Currently implemented is:
  - Pan / Tilt
  - Zoom
  - Focus
  - Autofocus
  - Tally light (partial)

-----
# License
Copyright (C) 2018-2024 by Maxim Devaev mdevaev@gmail.com
Copyright (C) 2024-2024 by Samuel samuel@thestaticturtle.fr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.
