# World Atlas

## CountryPicker

An offline country and territory selector with geographic focus, a searchable
directory, and a separate confirmed value. It includes the 249 ISO 3166-1
locations and Kosovo (`XK`, a user-assigned code). Names in the directory are
English. Boundaries and label anchors follow the bundled Natural Earth map.

```cpp
hui::ui::CountryPicker picker;
picker.set_bounds({96, 236, 1728, 716});
picker.set_selected("JP");   // restore a previously saved choice
// During the app's input/update/draw loop:
if (picker.handle(input, feedback) == hui::ui::Event::activated)
    save_country(picker.selected_code());
picker.update(dt);
picker.draw(canvas);
```

Give the widget a wide TV-safe region. Assign the app theme to `style.theme`
and the motion preference to `style.reduced_motion`. The gallery example uses
`1728 × 716` virtual pixels. The map, focus marker, directory and keyboard are
all drawn by the native renderer. A small country retains a full-size marker.

| Input | Map | Directory | Search keyboard |
| --- | --- | --- | --- |
| D-pad or left stick | Move to a country in that direction | Move through results | Navigate keys |
| Confirm | Select the focused country | Select and return to map | Type, or choose Done to show results |
| Square | Cycle world, regional and local scales | | Erase a character |
| Triangle | Open directory at the focused country | Open search | Insert a space |
| Back | Return `Event::cancelled` | Return to map | Cancel the edit and restore prior results |

`focused()` exposes the current code, name, region and coordinates of the
label point. `selected_code()` stays empty until the host sets a choice or the
player confirms one. Both return views into static storage. Use `set_focus` to
preview a country, `set_query` to feed a query from another input method, and
`style.graticule` to toggle the geographic grid.

The picker includes its pre-triangulated map in the compiled library. To rebuild
the geometry, run `tools/bake-country-map.py` with the pinned GeoJSON described
in its header. Baking dependencies are used only when regenerating the data.
