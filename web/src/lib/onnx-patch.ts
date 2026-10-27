// D-27: onnxruntime can only return a model's declared outputs, but the T9 embedding is the
// internal tensor after global average pooling (server.exe reads it by layer name through
// OpenCV dnn). Rather than re-hosting a modified 47 MB model, the browser downloads the
// original from the ONNX zoo, checks its SHA-256 (D-18, D-19), and declares that tensor as an
// extra graph output by splicing one protobuf field into the bytes. Weights are untouched.

// The zoo's LFS host directly: the github.com/.../raw/ URL in data/models/README.md redirects
// here, and that redirect carries no CORS header, so browsers refuse to follow it.
export const MODEL_URL =
  "https://media.githubusercontent.com/media/onnx/models/main/validated/vision/classification/resnet/model/resnet18-v2-7.onnx";
export const MODEL_SHA256 = "3b4d048057854022232aa20bf68262e31718ab7119a17f920b8a2dc1433f824b";
export const EMBEDDING_OUTPUT = "resnetv22_flatten0_reshape0";

// ValueInfoProto{name: EMBEDDING_OUTPUT, type: float tensor [1, 512]}, serialized by the onnx
// Python package (helper.make_tensor_value_info).
const EMBEDDING_VALUE_INFO = hex(
  "0a1b7265736e65747632325f666c617474656e305f7265736861706530120f0a0d080112090a0208010a03088004",
);

const MODEL_GRAPH_FIELD = 7; // ModelProto.graph
const GRAPH_OUTPUT_FIELD = 12; // GraphProto.output (repeated ValueInfoProto)
const LENGTH_DELIMITED = 2;

function hex(s: string): Uint8Array {
  return Uint8Array.from(s.match(/../g)!.map((b) => parseInt(b, 16)));
}

function readVarint(bytes: Uint8Array, pos: number): [number, number] {
  let value = 0;
  let scale = 1; // multiplication, not shifts: lengths can exceed 2^31 in principle
  for (;;) {
    const b = bytes[pos++];
    value += (b & 0x7f) * scale;
    if (b < 0x80) return [value, pos];
    scale *= 128;
  }
}

function varint(n: number): number[] {
  const out: number[] = [];
  do {
    let b = n % 128;
    n = Math.floor(n / 128);
    if (n > 0) b |= 0x80;
    out.push(b);
  } while (n > 0);
  return out;
}

// Appends the embedding output to ModelProto.graph.output. Protobuf allows a repeated field's
// elements anywhere in the message, so appending at the end of the graph payload is valid.
export function declareEmbeddingOutput(model: Uint8Array): Uint8Array {
  let pos = 0;
  while (pos < model.length) {
    const [key, afterKey] = readVarint(model, pos);
    const field = Math.floor(key / 8);
    const wire = key % 8;
    if (wire === LENGTH_DELIMITED) {
      const [len, start] = readVarint(model, afterKey);
      if (field === MODEL_GRAPH_FIELD) {
        const extra = [
          (GRAPH_OUTPUT_FIELD << 3) | LENGTH_DELIMITED,
          ...varint(EMBEDDING_VALUE_INFO.length),
          ...EMBEDDING_VALUE_INFO,
        ];
        const lenBytes = varint(len + extra.length);
        const out = new Uint8Array(model.length - (start - afterKey) + lenBytes.length + extra.length);
        let o = 0;
        out.set(model.subarray(0, afterKey), o);
        o += afterKey;
        out.set(lenBytes, o);
        o += lenBytes.length;
        out.set(model.subarray(start, start + len), o);
        o += len;
        out.set(extra, o);
        o += extra.length;
        out.set(model.subarray(start + len), o);
        return out;
      }
      pos = start + len;
    } else if (wire === 0) {
      pos = readVarint(model, afterKey)[1];
    } else if (wire === 1) {
      pos = afterKey + 8;
    } else if (wire === 5) {
      pos = afterKey + 4;
    } else {
      throw new Error(`unsupported protobuf wire type ${wire}`);
    }
  }
  throw new Error("ONNX model has no graph");
}

export async function sha256Hex(bytes: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", bytes as BufferSource);
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}
