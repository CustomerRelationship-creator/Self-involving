# M3 cognition gateway protocol

The ESP32 connects to a configured `wss://` endpoint with a revocable device
credential in the `Authorization: Bearer` header. The gateway owns STT, model,
memory, tool permissions and TTS credentials; provider keys never enter firmware.

## Control messages

Device to gateway:

- `hello`: protocol 3, board and firmware identity.
- `session.start`: PCM signed 16-bit little-endian, 24 kHz, mono, 20 ms.
- `session.cancel`: immediately discard queued input and pending response.

Gateway to device:

- `response.thinking`: remote processing has started.
- `session.ended`: playback is complete or the session was cancelled.

Control messages are UTF-8 JSON WebSocket text frames. Unknown fields must be
ignored; unknown message types must not change physical device state.

## Audio frames

Audio uses one binary WebSocket message per 20 ms frame. The packed 20-byte
little-endian header is:

| Field | Bytes | Meaning |
|---|---:|---|
| magic | 4 | `SIV3` / `0x53495633` |
| version | 1 | `3` |
| kind | 1 | `1` microphone, `2` speaker |
| flags | 2 | reserved, zero |
| sequence | 4 | monotonic within a session |
| sample_rate | 4 | `24000` |
| payload_bytes | 2 | at most 960 |
| reserved | 2 | zero |

The payload immediately follows the header. The gateway must apply backpressure,
discard audio after cancellation and send reply audio in order. The device queue
is bounded; stale audio is dropped rather than growing memory use.

## Required gateway behavior

- authenticate a token to exactly one device identity;
- enforce session time, byte and tool limits;
- stop inference and TTS on `session.cancel`;
- send `response.thinking` before a long model wait;
- finish with `session.ended` even after a recoverable error;
- keep raw audio only according to an explicit user retention policy;
- require separate confirmation for high-impact external actions.
