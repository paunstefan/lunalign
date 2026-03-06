# lunalign

A command-line tool for processing lunar astrophotography. It handles the full pipeline from raw video capture to a final stacked image: decoding, debayering, quality rating, frame registration, and stacking.

## Building from source

### Prerequisites

lunalign requires Clang 19 (or newer) with libc++ and optionally OpenMP support.

```bash
sudo apt install clang-19
sudo apt install libc++-19-dev libc++abi-19-dev

sudo apt install libomp-19-dev
```

OpenMP is strongly recommended, as the processing gets extremely slow without it.

### Compiling

```bash
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_BUILD_TYPE=Release -DLUNALIGN_USE_OPENMP=ON ..
make
```

The binary will be at `build/lunalign`.

## Usage

lunalign takes either a single command string or a script file as its only argument:

```bash
# Run a single command
lunalign "debayer -in=process/decoded -out=process/debayered"

# Run a script file with multiple commands
lunalign pipeline.txt
```

### Script files

A script file contains multiple commands separated by semicolons. For example, a full processing pipeline might look like:

```
decode -in=capture.ser -out=process/decoded;
debayer -in=process/decoded -out=process/debayered;
rate -in=process/debayered -percent=70 -out=process/rated;
register -in=process/rated -out=process/registered -rotation=1;
stack -in=process/registered -out=result.fits -method=sigma -sigma=2.5
```

Notice that the `register` command above does not specify `-reference`. It defaults to `$best_frame`, which is set automatically by the preceding `rate` command.

### Pipeline variables

Commands in a script share a pipeline context — a simple key-value store that lives for the duration of the script. A command can write a value into the context, and any later command can reference it with the `$variable` syntax.

Any argument value that starts with `$` is resolved against the context before the command runs. If the variable has not been set by a previous command, the pipeline exits with an error.

Currently defined variables:

| Variable | Set by | Description |
|----------|--------|-------------|
| `best_frame` | `rate` | Filename of the highest-rated frame |

For example, to explicitly reference a pipeline variable:

```
rate -in=process/debayered -percent=70 -out=process/rated;
register -in=process/rated -reference=$best_frame -rotation=1
```

This is equivalent to omitting `-reference` entirely, since its default value is `$best_frame`.

### Commands

**decode** — Decode a SER video file into individual FITS frames.

```
decode -in=capture.ser -out=process/decoded
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `-in` | yes | — | Path to the input SER file |
| `-out` | no | `process/decoded` | Output directory for FITS frames |

This command was not tested thoroughly yet and may contain bugs. I recommend running lunalign directly on FITS files.

**debayer** — Convert raw Bayer-pattern FITS frames into color (RGB) FITS files. The Bayer pattern is read from the `BAYERPAT` FITS header keyword.

```
debayer -in=process/decoded -out=process/debayered
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `-in` | yes | — | Input directory containing FITS files |
| `-out` | no | `process/debayered` | Output directory |

**rate** — Evaluate frame sharpness and copy the best percentage of frames. Uses Laplacian variance on the green channel as a quality metric. Sets the `best_frame` pipeline variable to the filename of the highest-rated frame.

```
rate -in=process/debayered -percent=70 -out=process/rated
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `-in` | yes | — | Input directory containing FITS files |
| `-percent` | yes | — | Percentage of best frames to keep (e.g. `70`) |
| `-out` | no | `process/rated` | Output directory for selected frames |

**register** — Align frames to a reference frame using FFT-based phase correlation. Supports optional rotation detection via log-polar transform.

```
register -in=process/rated -reference=debayered_0001.fits -out=process/registered -rotation=1
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `-in` | yes | — | Input directory containing FITS files |
| `-reference` | no | `$best_frame` | Filename of the reference frame (must be in the input directory). Defaults to the best frame from a preceding `rate` command. |
| `-out` | no | `process/registered` | Output directory |
| `-rotation` | no | `0` | Enable rotation correction (`1` = on, `0` = off) |
| `-scaling` | no | `0` | Enable scale correction (`1` = on, `0` = off) |
| `-highpass` | no | `1` | Use highpass preprocessing (`1`) or gradient magnitude (`0`) |

**stack** — Combine registered frames into a single image. Supports mean, median, and sigma-clipped stacking with optional quality-based weighting.

```
stack -in=process/registered -out=result.fits -method=sigma -sigma=2.5
```

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `-in` | yes | — | Input directory containing FITS files |
| `-out` | no | `process/stacked.fits` | Output file path |
| `-method` | no | `sigma` | Stacking method: `mean`, `median`, or `sigma` |
| `-sigma` | no | `2.5` | Sigma threshold for clipping (only used with `sigma` method) |
| `-weighted` | no | `0` | Weight frames by sharpness (`1` = on, `0` = off) |

## License

Copyright (C) 2025-2026 Stefan Paun. Licensed under the GNU General Public License v3.0 or later. See the license header in source files for details.

## Third-Party Code

* **Siril**: Portions of this software (specifically demosaicing code) were adapted from [Siril](https://free-astro.org/index.php/Siril).
    * *Copyright:* Francois Meyer and team free-astro.
    * *License:* GPLv3