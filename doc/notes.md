




New SSTV decoder architecture

 * Standardize on 16-bit signed integers.
 * Standardize on sample rate?
 * Allow different demodulators.
 * Must handle real-time feeds.
 * Must be portable, written in c.
 * should be fast.
 * Stateful. Continuously builds image.
 * Adjustable. User can tweak parameters in real-time to adjust image.

Stack:

 1. Radio. (RF -> Samples)
 2. Demodulator. (Samples -> Frequencies)
 3. SSTV Decoder. (Frequencies -> Image)

An SSTV mode is defined by the following properties:

 * 7-bit VIS code
 * Nominal number of lines.
 * Scanline duration (including linesync)
 * Linesync frequency
 * 100% frequency
 * Linesync duration

Use correlation to find the VIS code, and possibly the syncs.



There are two parts:

 1. VIS detector/decoder
 2. SSTV decoder

The VIS detector detects and decodes the VIS. With a VIS is detected, it
can automatically kick off the SSTV decoder. The SSTV decoder can also
be started manually.
