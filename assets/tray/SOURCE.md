# Custom tray symbols

- `efficiency.svg`: leaf outline containing a solid lightning bolt.
- `balanced.svg`: full circular dial with one centered upright needle, no ticks.

Created for this project with user approval, under the project MIT license.
Saver and Performance continue to use the upstream Fluent artwork in `../fluent`.

The build-time rasterizer trims transparent artboard space, centers the visible
shape and preserves a 1-DIP safety margin. Embedded sizes range from 16 to 64 px;
the app selects a size using the taskbar DPI and the system small-icon metric.
