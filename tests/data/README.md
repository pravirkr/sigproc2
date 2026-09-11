# Test filterbank fixtures

## `tiny.fil`

Synthetic original-format SIGPROC filterbank (no telescope data).

| Key | Value |
|---|---|
| `source_name` | `TINY` |
| `data_type` | 1 (filterbank) |
| `nchans` | 8 |
| `nbits` | 8 |
| `nifs` | 1 |
| `nsamples` | 16 |
| `tsamp` | 0.001 s |
| `fch1` | 1400 MHz |
| `foff` | -1 MHz |
| `tstart` | 50000 MJD |

Payload: 128 unsigned 8-bit samples `0..127` in sample-major order.

String keys use **int32** length prefixes (original C `send_string`). Never
regenerate this file with an 8-byte-length encoder.

### Regeneration

After the library encoder is available:

```text
sigproc::io::SigprocHeader hdr;
hdr.set("source_name", std::string("TINY"));
hdr.set("data_type", 1);
hdr.set("nchans", 8);
hdr.set("nbits", 8);
hdr.set("nifs", 1);
hdr.set("nsamples", 16);
hdr.set("fch1", 1400.0);
hdr.set("foff", -1.0);
hdr.set("tsamp", 0.001);
hdr.set("tstart", 50000.0);
// write header then bytes 0..127
```

Python equivalent (little-endian int32 lengths, `kEncodeOrder`):

```python
import struct
def token(s):
    b = s.encode("ascii")
    return struct.pack("<i", len(b)) + b
# HEADER_START, source_name TINY, machine_id 0, telescope_id 0, data_type 1,
# tstart 50000, tsamp 0.001, nbits 8, nsamples 16, fch1 1400, foff -1,
# nchans 8, nifs 1, HEADER_END, then bytes(range(128))
```

License: synthetic fixture, not observational data.
